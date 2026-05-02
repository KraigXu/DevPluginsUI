// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorDataClipboardHelpers.h"

#include "Customizations/MetaStoryEditorNodeUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryConsiderationBase.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryTaskBase.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

namespace UE::MetaStoryEditor
{

/** Helper class to detect if there were issues when calling ImportText() */
class FDefaultValueImportErrorContext : public FOutputDevice
{
public:

	int32 NumErrors = 0;

	FDefaultValueImportErrorContext() = default;

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		++NumErrors;
	}
};

void ExportTextAsClipboardEditorData(const FMetaStoryClipboardEditorData& InClipboardEditorData)
{
	FString Value;

	// Use PPF_Copy so that all properties get copied.
	constexpr EPropertyPortFlags PortFlags = PPF_Copy;
	TBaseStructure<FMetaStoryClipboardEditorData>::Get()->ExportText(Value, &InClipboardEditorData, nullptr, nullptr, PortFlags, nullptr);

	FPlatformApplicationMisc::ClipboardCopy(*Value);
}

bool ImportTextAsClipboardEditorData(const UScriptStruct* InTargetType, TNotNull<UMetaStoryEditorData*> InTargetTree, TNotNull<UObject*> InOwner,
	FMetaStoryClipboardEditorData& OutClipboardEditorData, bool bProcessBuffer /*true*/)
{
	OutClipboardEditorData.Reset();

	FString PastedText;

	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	if (PastedText.IsEmpty())
	{
		return false;
	}

	const UScriptStruct* ScriptStruct = TBaseStructure<FMetaStoryClipboardEditorData>::Get();
	FDefaultValueImportErrorContext ErrorPipe;
	ScriptStruct->ImportText(*PastedText, &OutClipboardEditorData, nullptr, PPF_None, &ErrorPipe, ScriptStruct->GetName());

	if (bProcessBuffer)
	{
		OutClipboardEditorData.ProcessBuffer(InTargetType, InTargetTree, InOwner);
	}

	return !bProcessBuffer || OutClipboardEditorData.IsValid();
}

void RemoveInvalidBindings(TNotNull<UMetaStoryEditorData*> InEditorData)
{
	if (FMetaStoryEditorPropertyBindings* Bindings = InEditorData->GetPropertyEditorBindings())
	{
		TMap<FGuid, const FPropertyBindingDataView> AllStructValues;
		InEditorData->GetAllStructValues(AllStructValues);
		Bindings->RemoveInvalidBindings(AllStructValues);
	}
}

void AddErrorNotification(const FText& InText, float InExpiredDuration)
{
	FNotificationInfo NotificationInfo(FText::GetEmpty());
	NotificationInfo.Text = InText;
	NotificationInfo.ExpireDuration = InExpiredDuration;
	FSlateNotificationManager::Get().AddNotification(NotificationInfo);
}

struct FScopedEditorDataFixer
{
	explicit FScopedEditorDataFixer(TNotNull<UMetaStoryEditorData*> InEditorData, TNotNull<UObject*> InOwner, bool InbShouldRegenerateGUID = true,
		bool InbShouldReinstantiateInstanceData = true)
		: EditorData(InEditorData)
		, Owner(InOwner)
		, bShouldRegenerateGUID(InbShouldRegenerateGUID)
		, bShouldReinstantiateInstanceData(InbShouldReinstantiateInstanceData)
	{
	}

	explicit FScopedEditorDataFixer(TNotNull<UMetaStoryEditorData*> InEditorData, TNotNull<UObject*> InOwner, FMetaStoryClipboardEditorData& InClipboardEditorData)
		: FScopedEditorDataFixer(InEditorData, InOwner)
	{

		EditorNodesToFix = InClipboardEditorData.GetEditorNodesInBuffer();
		TransitionsToFix = InClipboardEditorData.GetTransitionsInBuffer();
		BindingsToFix = InClipboardEditorData.GetBindingsInBuffer();
	}

	~FScopedEditorDataFixer()
	{
		// OldID -> NewID
		TMap<FGuid, FGuid> IDsMap;

		auto UpdateBindings = [Self = this, &IDsMap]()
			{
				for (FMetaStoryPropertyPathBinding& Binding : Self->BindingsToFix)
				{
					if (FGuid* NewSourceID = IDsMap.Find(Binding.GetSourcePath().GetStructID()))
					{
						Binding.GetMutableSourcePath().SetStructID(*NewSourceID);
					}

					if (FGuid* NewTargetID = IDsMap.Find(Binding.GetTargetPath().GetStructID()))
					{
						Binding.GetMutableTargetPath().SetStructID(*NewTargetID);
					}
				}
			};

		auto ReinstantiateEditorNodeInstanceData = [Self = this](FMetaStoryEditorNode& EditorNode)
			{
				EditorNodeUtils::InstantiateStructSubobjects(*Self->Owner, EditorNode.Node);
				if (EditorNode.InstanceObject)
				{
					EditorNode.InstanceObject = DuplicateObject(EditorNode.InstanceObject, Self->Owner);
				}
				else
				{
					EditorNodeUtils::InstantiateStructSubobjects(*Self->Owner, EditorNode.Instance);
				}
			};

		auto FixEditorNode = [Self = this, &IDsMap, &ReinstantiateEditorNodeInstanceData](FMetaStoryEditorNode& EditorNode)
			{
				FGuid OldInstanceID = EditorNode.ID;
				FGuid OldTemplateID = EditorNode.GetNodeID();
				if (Self->bShouldRegenerateGUID)
				{
					EditorNode.ID = FGuid::NewGuid();

					IDsMap.Add(OldInstanceID, EditorNode.ID);
					IDsMap.Add(OldTemplateID, EditorNode.GetNodeID());
				}

				if (Self->bShouldReinstantiateInstanceData)
				{
					ReinstantiateEditorNodeInstanceData(EditorNode);
				}
			};

		for (FMetaStoryEditorNode& EditorNode : EditorNodesToFix)
		{
			FixEditorNode(EditorNode);
		}

		for (FMetaStoryTransition& Transition : TransitionsToFix)
		{
			if (bShouldRegenerateGUID)
			{
				FGuid OldTransitionID = Transition.ID;
				Transition.ID = FGuid::NewGuid();
				IDsMap.Add(OldTransitionID, Transition.ID);
			}

			for (FMetaStoryEditorNode& CondNode : Transition.Conditions)
			{
				FixEditorNode(CondNode);
			}
		}

		for (FMetaStoryPropertyPathBinding& Binding : BindingsToFix)
		{
			FStructView PropertyFunctionView = Binding.GetMutablePropertyFunctionNode();
			if (FMetaStoryEditorNode* PropertyFunctionNode = PropertyFunctionView.GetPtr<FMetaStoryEditorNode>())
			{
				FixEditorNode(*PropertyFunctionNode);
			}
		}

		UpdateBindings();
	}

private:
	TArrayView<FMetaStoryEditorNode> EditorNodesToFix;
	TArrayView<FMetaStoryTransition> TransitionsToFix;
	TArrayView<FMetaStoryPropertyPathBinding> BindingsToFix;

	TNotNull<UMetaStoryEditorData*> EditorData;
	TNotNull<UObject*> Owner;

	uint8 bShouldRegenerateGUID : 1;
	uint8 bShouldReinstantiateInstanceData : 1;
};

void FMetaStoryClipboardEditorData::Append(TNotNull<const UMetaStoryEditorData*> InStateTree, TConstArrayView<FMetaStoryEditorNode> InEditorNodes)
{
	bBufferProcessed = false;
	EditorNodesBuffer.Append(InEditorNodes);
	
	CollectBindingsForEditorNodes(InStateTree, InEditorNodes);
}

void FMetaStoryClipboardEditorData::Append(TNotNull<const UMetaStoryEditorData*> InStateTree, TConstArrayView<FMetaStoryTransition> InTransitions)
{
	bBufferProcessed = false;
	TransitionsBuffer.Append(InTransitions);

	for (const FMetaStoryTransition& Transition : InTransitions)
	{
		CollectBindingsForEditorNodes(InStateTree, Transition.Conditions);
	}
}

void FMetaStoryClipboardEditorData::Append(TNotNull<const UMetaStoryEditorData*> InStateTree, TConstArrayView<const FPropertyBindingBinding*> InBindingPtrs)
{
	bBufferProcessed = false;
	for (const FPropertyBindingBinding* BindingPtr : InBindingPtrs)
	{
		if (BindingsBuffer.ContainsByPredicate([BindingPtr](const FMetaStoryPropertyPathBinding& InBinding)
			{
				return BindingPtr && BindingPtr->GetSourcePath() == InBinding.GetSourcePath() && BindingPtr->GetTargetPath() == InBinding.GetTargetPath();
			}))
		{
			continue;
		}

		BindingsBuffer.Add(*static_cast<const FMetaStoryPropertyPathBinding*>(BindingPtr));
		const FConstStructView FunctionNodeView = BindingPtr->GetPropertyFunctionNode();
		if (const FMetaStoryEditorNode* FunctionNode = FunctionNodeView.GetPtr<const FMetaStoryEditorNode>())
		{
			CollectBindingsForEditorNodes(InStateTree, { FunctionNode, 1 });
		}
	}
}

void FMetaStoryClipboardEditorData::Reset()
{
	EditorNodesBuffer.Reset();
	TransitionsBuffer.Reset();
	BindingsBuffer.Reset();
	bBufferProcessed = false;
}

void FMetaStoryClipboardEditorData::CollectBindingsForEditorNodes(TNotNull<const UMetaStoryEditorData*> InStateTree, TConstArrayView<FMetaStoryEditorNode> InEditorNodes)
{
	TArray<const FPropertyBindingBinding*> TempBindingPtrs;
	
	for (const FMetaStoryEditorNode& EditorNode : InEditorNodes)
	{
		InStateTree->GetPropertyEditorBindings()->GetBindingsFor(EditorNode.ID, TempBindingPtrs);

		// recursively collect property function node bindings
		constexpr auto EmptyStatePath = TEXT("");
		InStateTree->VisitStructBoundPropertyFunctions(EditorNode.ID, EmptyStatePath, 
			[InStateTree, &TempBindingPtrs](const FMetaStoryEditorNode& EditorNode, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
			{
				TArray<const FPropertyBindingBinding*> StructBindingsPtr;
				InStateTree->GetPropertyEditorBindings()->GetBindingsFor(Desc.ID, StructBindingsPtr);
				TempBindingPtrs.Append(MoveTemp(StructBindingsPtr));

				return EMetaStoryVisitor::Continue;
			});
	}

	Algo::Transform(TempBindingPtrs, BindingsBuffer, [](const FPropertyBindingBinding* BindingPtr) { return *static_cast<const FMetaStoryPropertyPathBinding*>(BindingPtr); });
}

void FMetaStoryClipboardEditorData::ProcessBuffer(const UScriptStruct* InTargetType, TNotNull<UMetaStoryEditorData*> InEditorData, TNotNull<UObject*> InTargetOwner)
{
	bBufferProcessed = false;
	check(!InTargetType || InTargetType->IsChildOf(TBaseStructure<FMetaStoryNodeBase>::Get()) || InTargetType->IsChildOf(TBaseStructure<FMetaStoryTransition>::Get()));
	check(InTargetOwner->IsA<UMetaStoryState>() || InTargetOwner->IsA<UMetaStoryEditorData>());

	auto ValidateEditorNodes = [&InEditorData](TConstArrayView<FMetaStoryEditorNode> InEditorNodes, const UScriptStruct* InRequiredType)
		{
			for (const FMetaStoryEditorNode& EditorNode : InEditorNodes)
			{
				FStructView NodeView = EditorNode.GetNode();
				FMetaStoryDataView InstanceView = EditorNode.GetInstance();
				if (!NodeView.IsValid() || !InstanceView.IsValid())
				{
					AddErrorNotification(LOCTEXT("MalformedNode", "Clipboard text contains invalid data."));
					return false;
				}

				if (InRequiredType && !NodeView.GetScriptStruct()->IsChildOf(InRequiredType))
				{
					AddErrorNotification(FText::Format(LOCTEXT("NotSupportedByType", "This property only accepts nodes of type {0}."), InRequiredType->GetDisplayNameText()));
					return false;
				}

				if (const UMetaStorySchema* TargetSchema = InEditorData->Schema)
				{
					bool bIsNodeAllowed = true;
					const UStruct* InstanceType = InstanceView.GetStruct();
					if (const UScriptStruct* InstanceTypeStruct = Cast<UScriptStruct>(InstanceType))
					{
						if (!TargetSchema->IsStructAllowed(NodeView.GetScriptStruct()))
						{
							bIsNodeAllowed = false;
						}
					}
					else if (const UClass* InstanceTypeClass = Cast<UClass>(InstanceType))
					{
						if (!TargetSchema->IsClassAllowed(InstanceTypeClass))
						{
							bIsNodeAllowed = false;
						}
					}

					if (!bIsNodeAllowed)
					{
						AddErrorNotification(FText::Format(LOCTEXT("NotSupportedBySchema", "Node {0} is not supported by {1} schema."),
							NodeView.GetScriptStruct()->GetDisplayNameText(), TargetSchema->GetClass()->GetDisplayNameText()));

						return false;
					}
				}
			}

			return true;
		};

	auto ValidateTransitions = [Self = this, &ValidateEditorNodes](TConstArrayView<FMetaStoryTransition> InTransitions, const UScriptStruct* InRequiredType)
		{
			if (Self->TransitionsBuffer.Num() && InRequiredType && InRequiredType != TBaseStructure<FMetaStoryTransition>::Get())
			{
				AddErrorNotification(FText::Format(LOCTEXT("NotSupportedByType", "This property only accepts nodes of type {0}."), InRequiredType->GetDisplayNameText()));
				return false;
			}

			for (const FMetaStoryTransition& Transition : InTransitions)
			{
				static const UScriptStruct* ConditionRequiredType = TBaseStructure<FMetaStoryConditionBase>::Get();
				if (!ValidateEditorNodes(Transition.Conditions, ConditionRequiredType))
				{
					return false;
				}
			}

			return true;
		};

	if (ValidateEditorNodes(EditorNodesBuffer, InTargetType) && ValidateTransitions(TransitionsBuffer, InTargetType))
	{
		FScopedEditorDataFixer Fixer(InEditorData, InTargetOwner, *this);
		bBufferProcessed = true;
	}
	else
	{
		Reset();
	}
}
}
#undef LOCTEXT_NAMESPACE
