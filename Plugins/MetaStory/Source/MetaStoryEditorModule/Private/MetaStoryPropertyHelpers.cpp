// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryPropertyHelpers.h"
#include "MetaStoryEditorNode.h"
#include "ScopedTransaction.h"
#include "Hash/Blake3.h"
#include "Misc/EnumerateRange.h"
#include "Misc/StringBuilder.h"
#include "String/ParseTokens.h"
#include "UObject/Field.h"
#include "MetaStoryPropertyBindings.h"
#include "MetaStoryEditorData.h"

namespace UE::MetaStory::PropertyHelpers
{
namespace Internal
{
	void DispatchPostEditToEditorNode(FPropertyChangedChainEvent& InPropertyChangedEvent, const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* InEditorNodeInChain, FMetaStoryEditorNode& InEditorNode)
	{
		if (FMetaStoryNodeBase* MetaStoryNode = InEditorNode.Node.GetMutablePtr<FMetaStoryNodeBase>())
		{
			// Check that the path contains EditorNode's: Node, Instance or Instance Object
			if (const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* EditorNodeMemberPropNode = InEditorNodeInChain->GetNextNode())
			{
				// Check that we have a changed property on one of the above properties.
				if (const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* ActiveMemberPropNode = EditorNodeMemberPropNode->GetNextNode()) 
				{
					// Update the event
					const FProperty* EditorNodeChildMember = EditorNodeMemberPropNode->GetValue();
					check(EditorNodeChildMember);

					// Take copy of the event, we'll modify it.
					FEditPropertyChain PropertyChainCopy;
					for (const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* Node = InPropertyChangedEvent.PropertyChain.GetHead(); Node; Node = Node->GetNextNode())
					{
						PropertyChainCopy.AddTail(Node->GetValue());
					}
					FPropertyChangedChainEvent PropertyChangedEvent(PropertyChainCopy, InPropertyChangedEvent);

					PropertyChangedEvent.SetActiveMemberProperty(ActiveMemberPropNode->GetValue());
					PropertyChangedEvent.PropertyChain.SetActiveMemberPropertyNode(PropertyChangedEvent.MemberProperty);

					// To be consistent with the other property chain callbacks, do not cross object boundary.
					const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* ActivePropNode = ActiveMemberPropNode;
					while (ActivePropNode->GetNextNode())
					{
						if (CastField<FObjectProperty>(ActivePropNode->GetValue()))
						{
							break;
						}
						ActivePropNode = ActivePropNode->GetNextNode();
					}
							
					PropertyChangedEvent.Property = ActivePropNode->GetValue();
					PropertyChangedEvent.PropertyChain.SetActivePropertyNode(PropertyChangedEvent.Property);

					if (EditorNodeChildMember->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, Node))
					{
						MetaStoryNode->PostEditNodeChangeChainProperty(PropertyChangedEvent, InEditorNode.GetInstance());
					}
					else if (EditorNodeChildMember->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, Instance))
					{
						if (InEditorNode.Instance.IsValid())
						{
							MetaStoryNode->PostEditInstanceDataChangeChainProperty(PropertyChangedEvent, FMetaStoryDataView(InEditorNode.Instance));
						}
					}
					else if (EditorNodeChildMember->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, InstanceObject))
					{
						if (InEditorNode.InstanceObject)
						{
							MetaStoryNode->PostEditInstanceDataChangeChainProperty(PropertyChangedEvent, FMetaStoryDataView(InEditorNode.InstanceObject));
						}
					}
					else if (EditorNodeChildMember->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, ExecutionRuntimeData))
					{
						if (InEditorNode.ExecutionRuntimeData.IsValid())
						{
							MetaStoryNode->PostEditInstanceDataChangeChainProperty(PropertyChangedEvent, FMetaStoryDataView(InEditorNode.ExecutionRuntimeData));
						}
					}
					else if (EditorNodeChildMember->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, ExecutionRuntimeDataObject))
					{
						if (InEditorNode.ExecutionRuntimeDataObject)
						{
							MetaStoryNode->PostEditInstanceDataChangeChainProperty(PropertyChangedEvent, FMetaStoryDataView(InEditorNode.ExecutionRuntimeDataObject));
						}
					}
				}
			}
		}
	}
}


void DispatchPostEditToNodes(UObject& Owner, FPropertyChangedChainEvent& InPropertyChangedEvent, UMetaStoryEditorData& EditorData)
{
	// Walk through changed property chain and look for first FMetaStoryEditorNode, and call the node specific post edit methods.
	
	const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* CurrentPropNode = InPropertyChangedEvent.PropertyChain.GetHead();
	const FProperty* HeadProperty = CurrentPropNode->GetValue();
	check(HeadProperty);
	if (HeadProperty->GetOwnerClass() != Owner.GetClass())
	{
		return;
	}
	
	FMetaStoryEditorNode* LastEditorNode = nullptr;
	const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* LastEditorNodeInChain = nullptr;

	uint8* CurrentAddress = reinterpret_cast<uint8*>(&Owner);
	FPropertyBindingPath TargetPath;
	while (CurrentPropNode)
	{
		const FProperty* CurrentProperty = CurrentPropNode->GetValue();
		check(CurrentProperty);
		CurrentAddress = CurrentAddress + CurrentProperty->GetOffset_ForInternal();

		while (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(CurrentProperty))
		{
			FScriptArrayHelper Helper(ArrayProperty, CurrentAddress);
			const int32 Index = InPropertyChangedEvent.GetArrayIndex(ArrayProperty->GetName());
			if (!Helper.IsValidIndex(Index))
			{
				check(CurrentPropNode->GetNextNode() == nullptr);
				break;
			}

			if (TargetPath.GetStructID().IsValid())
			{
				TargetPath.AddPathSegment(ArrayProperty->GetFName(), Index);
			}

			CurrentAddress = Helper.GetRawPtr(Index);
			CurrentProperty = ArrayProperty->Inner;
		}

		FPropertyBindingPathSegment PathSegment(CurrentProperty->GetFName());
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(CurrentProperty))
		{
			if (StructProperty->Struct == FInstancedStruct::StaticStruct())
			{
				FInstancedStruct& InstancedStruct = *reinterpret_cast<FInstancedStruct*>(CurrentAddress);
				CurrentAddress = InstancedStruct.GetMutableMemory();

				PathSegment.SetInstanceStruct(InstancedStruct.GetScriptStruct());
			}
			else if (StructProperty->Struct == FMetaStoryEditorNode::StaticStruct())
			{
				if (TargetPath.GetStructID().IsValid())
				{
					FPropertyBindingBinding* FoundBinding = EditorData.GetPropertyEditorBindings()->GetMutableBindings().FindByPredicate([&TargetPath](const FPropertyBindingBinding& Binding)
					{
						return TargetPath == Binding.GetTargetPath();
					});

					if (!ensure(FoundBinding && FoundBinding->GetPropertyFunctionNode().IsValid()))
					{
						return;
					}

					CurrentAddress = FoundBinding->GetMutablePropertyFunctionNode().GetMemory();
					TargetPath.Reset();
				}

				LastEditorNode = reinterpret_cast<FMetaStoryEditorNode*>(CurrentAddress);
				LastEditorNodeInChain = CurrentPropNode;
				TargetPath.SetStructID(LastEditorNode->ID);

				CurrentPropNode = CurrentPropNode->GetNextNode();
				if (CurrentPropNode)
				{
					const FName EditorNodeChildMemberName = CurrentPropNode->GetValue()->GetFName();
					if (EditorNodeChildMemberName == GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, Instance)
						|| EditorNodeChildMemberName == GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, InstanceObject))
					{
						CurrentAddress = static_cast<uint8*>(LastEditorNode->GetInstance().GetMutableMemory());
						CurrentPropNode = CurrentPropNode->GetNextNode();
						continue;
					}
					if (EditorNodeChildMemberName == GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, ExecutionRuntimeData)
						|| EditorNodeChildMemberName == GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, ExecutionRuntimeDataObject))
					{
						CurrentAddress = static_cast<uint8*>(LastEditorNode->GetExecutionRuntimeData().GetMutableMemory());
						CurrentPropNode = CurrentPropNode->GetNextNode();
						continue;
					}
				}

				break;
			}
			else if (StructProperty->Struct == FMetaStoryStateParameters::StaticStruct())
			{
				FMetaStoryStateParameters& StateParameters = *reinterpret_cast<FMetaStoryStateParameters*>(CurrentAddress);
				check(!TargetPath.GetStructID().IsValid());
				TargetPath.SetStructID(StateParameters.ID);

				CurrentPropNode = CurrentPropNode->GetNextNode();
				if (CurrentPropNode && CurrentPropNode->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaStoryStateParameters, Parameters))
				{
					CurrentPropNode = CurrentPropNode->GetNextNode();
					if (CurrentPropNode && CurrentPropNode->GetValue()->GetFName() == TEXT("Value"))
					{
						CurrentAddress = StateParameters.Parameters.GetMutableValue().GetMemory();
						CurrentPropNode = CurrentPropNode->GetNextNode();
						continue;
					}
				}

				return;
			}
		}
		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(CurrentProperty))
		{
			if (!TargetPath.GetStructID().IsValid())
			{
				return;
			}

			if (UObject* Object = *reinterpret_cast<UObject**>(CurrentAddress))
			{
				CurrentAddress = reinterpret_cast<uint8*>(Object);
				PathSegment.SetInstanceStruct(Object->GetClass(), EPropertyBindingPropertyAccessType::ObjectInstance);
			}
			else
			{
				break;
			}
		}

		if (TargetPath.GetStructID().IsValid())
		{
			TargetPath.AddPathSegment(PathSegment);
		}

		CurrentPropNode = CurrentPropNode->GetNextNode();
	}

	if (LastEditorNode && LastEditorNodeInChain)
	{
		Internal::DispatchPostEditToEditorNode(InPropertyChangedEvent, LastEditorNodeInChain, *LastEditorNode);
	}
}

void ModifyStateInPreAndPostEdit(
	const FText& TransactionDescription, 
	TNotNull<UMetaStoryState*> State,
	TNotNull<UMetaStoryEditorData*> EditorData,
	FStringView RelativeNodePath, 
	TFunctionRef<void(TNotNull<UMetaStoryState*> OwnerState, TNotNull<UMetaStoryEditorData*> EditorData, const FMetaStoryEditPropertyPath& PropertyPath)> Func,
	int32 ArrayIndex, 
	EPropertyChangeType::Type ChangeType)
{
	FScopedTransaction ScopedTransaction(TransactionDescription);

	FEditPropertyChain PropertyChain;

	FMetaStoryEditPropertyPath PropertyPath = FMetaStoryEditPropertyPath(State->GetClass(), RelativeNodePath);
	PropertyPath.MakeEditPropertyChain(PropertyChain);

	State->PreEditChange(PropertyChain);

	Func(State, EditorData, PropertyPath);

	FProperty* ActiveNode = PropertyChain.GetActiveNode()->GetValue();
	TArray<TMap<FString, int32>> ArrayIndicesPerObject;

	if (ArrayIndex != INDEX_NONE)
	{
		ArrayIndicesPerObject.Add(TMap<FString, int32>());
		ArrayIndicesPerObject[0].Add(ActiveNode->GetName(), ArrayIndex);
	}

	FPropertyChangedEvent ChangedEvent(PropertyChain.GetActiveNode()->GetValue(), ChangeType);
	ChangedEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);

	FPropertyChangedChainEvent ChainEvent(PropertyChain, ChangedEvent);
	State->PostEditChangeChainProperty(ChainEvent);
}

FGuid MakeDeterministicID(const UObject& Owner, const FString& PropertyPath, const uint64 Seed)
{
	// From FGuid::NewDeterministicGuid(FStringView ObjectPath, uint64 Seed)
	
	// Convert the objectpath to utf8 so that whether TCHAR is UTF8 or UTF16 does not alter the hash.
	TUtf8StringBuilder<1024> Utf8ObjectPath(InPlace, Owner.GetPathName());
	TUtf8StringBuilder<1024> Utf8PropertyPath(InPlace, PropertyPath);

	FBlake3 Builder;

	// Hash this as the namespace of the Version 3 UUID, to avoid collisions with any other guids created using Blake3.
	static FGuid BaseVersion(TEXT("bf324a38-a445-45a4-8921-249554b58189"));
	Builder.Update(&BaseVersion, sizeof(FGuid));
	Builder.Update(Utf8ObjectPath.GetData(), Utf8ObjectPath.Len() * sizeof(UTF8CHAR));
	Builder.Update(Utf8PropertyPath.GetData(), Utf8PropertyPath.Len() * sizeof(UTF8CHAR));
	Builder.Update(&Seed, sizeof(Seed));

	const FBlake3Hash Hash = Builder.Finalize();

	return FGuid::NewGuidFromHash(Hash);
}

bool HasOptionalMetadata(const FProperty& Property)
{
	return Property.HasMetaData(TEXT("Optional"));
}

}; // UE::MetaStory::PropertyHelpers

// ------------------------------------------------------------------------------
// FMetaStoryEditPropertyPath
// ------------------------------------------------------------------------------
FMetaStoryEditPropertyPath::FMetaStoryEditPropertyPath(const UStruct* BaseStruct, FStringView InPath)
{
	TArray<FStringView> PathSegments;
	UE::String::ParseTokens(InPath, TEXT("."), PathSegments, UE::String::EParseTokensOptions::SkipEmpty);

	const UStruct* CurrBase = BaseStruct;
	for (FStringView Segment : PathSegments)
	{
		const FName PropertyName(Segment);
		if (FProperty* Property = CurrBase->FindPropertyByName(PropertyName))
		{
			Path.Emplace(Property, PropertyName);

			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				Property = ArrayProperty->Inner;
			}

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				CurrBase = StructProperty->Struct;
			}
			else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
			{
				CurrBase = ObjectProperty->PropertyClass;
			}
		}
		else
		{
			checkf(false, TEXT("Path %s id not part of type %s."), InPath.GetData(), *GetNameSafe(BaseStruct));
			Path.Reset();
			break;
		}
	}
}

FMetaStoryEditPropertyPath::FMetaStoryEditPropertyPath(const FPropertyChangedChainEvent& PropertyChangedEvent)
{
	FEditPropertyChain::TDoubleLinkedListNode* PropertyNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();
	while (PropertyNode != nullptr)
	{
		if (FProperty* Property = PropertyNode->GetValue())
		{
			const FName PropertyName = Property->GetFName(); 
			const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(PropertyName.ToString());
			Path.Emplace(Property, PropertyName, ArrayIndex);
		}
		PropertyNode = PropertyNode->GetNextNode();
	}
}

FMetaStoryEditPropertyPath::FMetaStoryEditPropertyPath(const FEditPropertyChain& PropertyChain)
{
	FEditPropertyChain::TDoubleLinkedListNode* PropertyNode = PropertyChain.GetActiveMemberNode();
	while (PropertyNode != nullptr)
	{
		if (FProperty* Property = PropertyNode->GetValue())
		{
			const FName PropertyName = Property->GetFName(); 
			Path.Emplace(Property, PropertyName, INDEX_NONE);
		}
		PropertyNode = PropertyNode->GetNextNode();
	}
}

void FMetaStoryEditPropertyPath::MakeEditPropertyChain(FEditPropertyChain& OutPropertyChain) const
{
	OutPropertyChain.Empty();

	for (const FMetaStoryEditPropertySegment& Segment : Path)
	{
		OutPropertyChain.AddTail(Segment.Property);
	}

	OutPropertyChain.SetActiveMemberPropertyNode(Path[0].Property);
}

bool FMetaStoryEditPropertyPath::ContainsPath(const FMetaStoryEditPropertyPath& InPath) const
{
	if (InPath.Path.Num() > Path.Num())
    {
    	return false;
    }

    for (TConstEnumerateRef<FMetaStoryEditPropertySegment> Segment : EnumerateRange(InPath.Path))
    {
    	if (Segment->PropertyName != Path[Segment.GetIndex()].PropertyName)
    	{
    		return false;
    	}
    }
    return true;
}

/** @return true if the property path is exactly the specified path. */
bool FMetaStoryEditPropertyPath::IsPathExact(const FMetaStoryEditPropertyPath& InPath) const
{
	if (InPath.Path.Num() != Path.Num())
	{
		return false;
	}

	for (TConstEnumerateRef<FMetaStoryEditPropertySegment> Segment : EnumerateRange(InPath.Path))
	{
		if (Segment->PropertyName != Path[Segment.GetIndex()].PropertyName)
		{
			return false;
		}
	}
	return true;
}
