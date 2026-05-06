// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorData.h"
#include "MetaStoryMetaplotTopology.h"
#include "MetaStory.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryConsiderationBase.h"
#include "MetaStoryEditorDataExtension.h"
#include "MetaStoryEditorModule.h"
#include "MetaStoryEditorSchema.h"
#include "MetaStoryEvaluatorBase.h"
#include "MetaStoryNodeClassCache.h"
#include "MetaStoryPropertyFunctionBase.h"
#include "MetaStoryPropertyHelpers.h"
#include "MetaStoryTaskBase.h"
#include "Algo/LevenshteinDistance.h"
#include "Customizations/MetaStoryBindingExtension.h"
#include "Customizations/MetaStoryEditorNodeUtils.h"
#include "Modules/ModuleManager.h"
#include "UObject/UE5SpecialProjectStreamObjectVersion.h"

#if WITH_EDITOR
#include "StructUtilsDelegates.h"
#include "StructUtils/UserDefinedStruct.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEditorData)

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

namespace UE::MetaStory::Editor
{
	FAutoConsoleVariable CVarLogEnableBindingSelectionNodeToInstanceData(
		TEXT("MetaStory.Compiler.EnableBindingSelectionNodeToInstanceData"),
		true,
		TEXT("Enable binding from enter condition, utility/consideration and state argument to bind to task instance data.\n")
		TEXT("The task instance data is only available once the transition is completed.")
		TEXT("A parent state can enter a child state during state selection (before the transition completes).")
	);

	const FString GlobalStateName(TEXT("Global"));
	const FString PropertyFunctionStateName(TEXT("Property Functions"));
	const FName ParametersNodeName(TEXT("Parameters"));

	bool IsPropertyFunctionOwnedByNode(FGuid NodeID, FGuid PropertyFuncID, const FMetaStoryEditorPropertyBindings& EditorBindings)
	{
		for (const FPropertyBindingBinding& Binding : EditorBindings.GetBindings())
		{
			const FGuid TargetID = Binding.GetTargetPath().GetStructID();
			if (TargetID == NodeID)
			{
				return true;
			}

			FConstStructView NodeView = Binding.GetPropertyFunctionNode();
			if (const FMetaStoryEditorNode* Node = NodeView.GetPtr<const FMetaStoryEditorNode>())
			{
				if (Node->ID == PropertyFuncID)
				{
					PropertyFuncID = TargetID;
				}
			}
		}

		return false;
	}

	FMetaStoryEditorColor CreateDefaultColor()
	{
		FMetaStoryEditorColor DefaultColor;
		DefaultColor.ColorRef = FMetaStoryEditorColorRef();
		DefaultColor.Color = FLinearColor(FColor(31, 151, 167));
		DefaultColor.DisplayName = TEXT("Default Color");
		return DefaultColor;
	}
}

UMetaStoryEditorData::UMetaStoryEditorData()
{
	Colors.Add(UE::MetaStory::Editor::CreateDefaultColor());

	EditorBindings.SetBindingsOwner(this);
}

void UMetaStoryEditorData::PostInitProperties()
{
	Super::PostInitProperties();

	if(!IsTemplate())
	{
		RootParametersGuid = FGuid::NewGuid();
	}

#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		OnParametersChangedHandle = UE::MetaStory::Delegates::OnParametersChanged.AddUObject(this, &UMetaStoryEditorData::OnParametersChanged);
		OnStateParametersChangedHandle = UE::MetaStory::Delegates::OnStateParametersChanged.AddUObject(this, &UMetaStoryEditorData::OnStateParametersChanged);
	}
	
#endif
}

void UMetaStoryEditorData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);	
	Ar.UsingCustomVersion(FUE5SpecialProjectStreamObjectVersion::GUID);
}

#if WITH_EDITOR

void UMetaStoryEditorData::BeginDestroy()
{
	if (OnParametersChangedHandle.IsValid())
	{
		UE::MetaStory::Delegates::OnParametersChanged.Remove(OnParametersChangedHandle);
		OnParametersChangedHandle.Reset();
	}
	if (OnStateParametersChangedHandle.IsValid())
	{
		UE::MetaStory::Delegates::OnStateParametersChanged.Remove(OnStateParametersChangedHandle);
		OnStateParametersChangedHandle.Reset();
	}
	
	Super::BeginDestroy();
}

void UMetaStoryEditorData::OnParametersChanged(const UMetaStory& MetaStory)
{
	if (const UMetaStory* OwnerMetaStory = GetTypedOuter<UMetaStory>())
	{
		if (OwnerMetaStory == &MetaStory)
		{
			UpdateBindingsInstanceStructs();
		}
	}
}

void UMetaStoryEditorData::OnStateParametersChanged(const UMetaStory& MetaStory, const FGuid StateID)
{
	if (const UMetaStory* OwnerMetaStory = GetTypedOuter<UMetaStory>())
	{
		if (OwnerMetaStory == &MetaStory)
		{
			UpdateBindingsInstanceStructs();
		}
	}
}

void UMetaStoryEditorData::EnsureEmbeddedMetaplotFlow()
{
	if (!bUseMetaplotFlowTopology || MetaplotFlow)
	{
		return;
	}

	Modify();
	MetaplotFlow = NewObject<UMetaplotFlow>(this, TEXT("MetaplotFlow"), RF_Transactional);

	FMetaplotNode StartNode;
	StartNode.NodeId = FGuid::NewGuid();
	StartNode.NodeType = EMetaplotNodeType::Start;
	StartNode.NodeName = INVTEXT("Start");
	StartNode.StageIndex = 0;
	StartNode.LayerIndex = 0;
	MetaplotFlow->Nodes.Add(StartNode);
	MetaplotFlow->StartNodeId = StartNode.NodeId;
	MetaplotFlow->SyncNodeStatesWithNodes();
}

void UMetaStoryEditorData::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerCustomVersion(FUE5SpecialProjectStreamObjectVersion::GUID) < FUE5SpecialProjectStreamObjectVersion::StateTreeGlobalParameterChanges)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		RootParameterPropertyBag = RootParameters.Parameters;
		RootParametersGuid = RootParameters.ID;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	// Ensure the schema and states have had their PostLoad() fixed applied as we may need them in the later calls (or MetaStory compile which might be calling this).
	if (Schema)
	{
		Schema->ConditionalPostLoad();
	}
	if (EditorSchema)
	{
		EditorSchema->ConditionalPostLoad();
	}

	for (UMetaStoryEditorDataExtension* Extension : Extensions)
	{
		if (Extension)
		{
			Extension->ConditionalPostLoad();
		}
	}

	if (bUseMetaplotFlowTopology)
	{
		EnsureEmbeddedMetaplotFlow();
	}

	if (bUseMetaplotFlowTopology && MetaplotFlow)
	{
		MetaplotFlow->ConditionalPostLoad();
		if (!UE::MetaStory::MetaplotTopology::RebuildShadowStates(*this, nullptr))
		{
			UE_LOG(LogMetaStoryEditor, Error, TEXT("Metaplot topology: RebuildShadowStates failed during PostLoad."));
		}
	}

	VisitHierarchy([](UMetaStoryState& State, UMetaStoryState* ParentState) mutable 
	{
		State.ConditionalPostLoad();
		return EMetaStoryVisitor::Continue;
	});
	CallPostLoadOnNodes();

	ReparentStates();
	FixObjectNodes();
	FixDuplicateIDs();
	UpdateBindingsInstanceStructs();
}

void UMetaStoryEditorData::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* Property = PropertyChangedEvent.Property;
	FProperty* MemberProperty = nullptr;
	if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode())
	{
		MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
	}

	if (MemberProperty && Property)
	{
		const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
		checkf(MetaStory, TEXT("UMetaStoryEditorData should only be allocated within a UMetaStory"));
		
		const FName MemberName = MemberProperty->GetFName();
		if (MemberName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, Schema)
			|| MemberName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, EditorSchema))
		{
			if (EditorSchema && !EditorSchema->AllowExtensions())
			{
				Extensions.Reset();
			}
			UE::MetaStory::Delegates::OnSchemaChanged.Broadcast(*MetaStory);
		}
		else if (MemberName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, RootParameterPropertyBag))
		{
			UE::MetaStory::Delegates::OnParametersChanged.Broadcast(*MetaStory);
		}
		else if (MemberName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, bUseMetaplotFlowTopology))
		{
			if (bUseMetaplotFlowTopology)
			{
				EnsureEmbeddedMetaplotFlow();
				if (MetaplotFlow)
				{
					(void)UE::MetaStory::MetaplotTopology::RebuildShadowStates(*this, nullptr);
				}
				UE::MetaStory::Delegates::OnGlobalDataChanged.Broadcast(*MetaStory);
			}
		}

		// Ensure unique ID on duplicated items.
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
		{
			if (MemberName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, Evaluators))
			{
				const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(MemberProperty->GetFName().ToString());
				if (Evaluators.IsValidIndex(ArrayIndex))
				{
					const FGuid OldStructID = Evaluators[ArrayIndex].ID;
					Evaluators[ArrayIndex].ID = FGuid::NewGuid();
					EditorBindings.CopyBindings(OldStructID, Evaluators[ArrayIndex].ID);
				}
			}
			else if (MemberName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, GlobalTasks))
			{
				const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(MemberProperty->GetFName().ToString());
				if (GlobalTasks.IsValidIndex(ArrayIndex))
				{
					const FGuid OldStructID = GlobalTasks[ArrayIndex].ID;
					GlobalTasks[ArrayIndex].ID = FGuid::NewGuid();
					EditorBindings.CopyBindings(OldStructID, GlobalTasks[ArrayIndex].ID);
				}
			}
		}
		else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove)
		{
			if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, Evaluators)
				|| MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, GlobalTasks))
			{
				TMap<FGuid, const FPropertyBindingDataView> AllStructValues;
				GetAllStructValues(AllStructValues);
				Modify();
				EditorBindings.RemoveInvalidBindings(AllStructValues);
			}
		}

		// Notify that the global data changed (will need to update binding widgets, etc)
		if (MemberName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, Evaluators)
			|| MemberName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, GlobalTasks))
		{
			UE::MetaStory::Delegates::OnGlobalDataChanged.Broadcast(*MetaStory);
		}

		// Notify that the color data has changed and fix existing data
		if (MemberName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, Colors))
		{
			if (Colors.IsEmpty())
			{
				// Add default color
				Colors.Add(UE::MetaStory::Editor::CreateDefaultColor());
			}
			VisitHierarchy([Self=this](UMetaStoryState& State, UMetaStoryState* ParentState)
			{
				if (!Self->FindColor(State.ColorRef))
				{
					State.Modify();
					State.ColorRef = FMetaStoryEditorColorRef();
				}
				return EMetaStoryVisitor::Continue;
			});

			UE::MetaStory::Delegates::OnVisualThemeChanged.Broadcast(*MetaStory);
		}
	}

	UE::MetaStory::PropertyHelpers::DispatchPostEditToNodes(*this, PropertyChangedEvent, *this);
}

void UMetaStoryEditorData::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	EditorBindings.SetBindingsOwner(this);
	DuplicateIDs();
}
#endif // WITH_EDITOR

void UMetaStoryEditorData::GetBindableStructs(const FGuid TargetStructID, TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const
{
	// Find the states that are updated before the current state.
	TArray<const UMetaStoryState*> Path;
	const UMetaStoryState* State = GetStateByStructID(TargetStructID);
	while (State != nullptr)
	{
		Path.Insert(State, 0);

		// Stop at subtree root.
		if (State->Type == EMetaStoryStateType::Subtree)
		{
			break;
		}

		State = State->Parent;
	}
	
	GetAccessibleStructsInExecutionPath(Path, TargetStructID, OutStructDescs);
}

void UMetaStoryEditorData::GetAccessibleStructsInExecutionPath(const TConstArrayView<const UMetaStoryState*> Path, const FGuid TargetStructID, TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const
{
	const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
	checkf(MetaStory, TEXT("UMetaStoryEditorData should only be allocated within a UMetaStory"));

	bool bAcceptTaskInstanceData = true;
	TInstancedStruct<FPropertyBindingBindableStructDescriptor> TargetStructDesc;
	bool bIsTargetPropertyFunction = false;
	if (GetBindableStructByID(TargetStructID, TargetStructDesc))
	{
		bIsTargetPropertyFunction = TargetStructDesc.Get<FMetaStoryBindableStructDesc>().DataSource == EMetaStoryBindableStructSource::PropertyFunction;
		if (!UE::MetaStory::Editor::CVarLogEnableBindingSelectionNodeToInstanceData->GetBool())
		{
			bAcceptTaskInstanceData = UE::MetaStory::AcceptTaskInstanceData(TargetStructDesc.Get<FMetaStoryBindableStructDesc>().DataSource);
		}
	}

	EMetaStoryVisitor BaseProgress = VisitGlobalNodes([&OutStructDescs, TargetStructID]
		(const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
	{
		if (Desc.ID == TargetStructID)
		{
			return EMetaStoryVisitor::Break;
		}
		
		OutStructDescs.Add(TInstancedStruct<FMetaStoryBindableStructDesc>::Make(Desc));
		
		return EMetaStoryVisitor::Continue;
	});

	if (BaseProgress == EMetaStoryVisitor::Continue)
	{
		TArray<TInstancedStruct<FMetaStoryBindableStructDesc>, TInlineAllocator<32>> BindableDescs;

		for (const UMetaStoryState* State : Path)
		{
			if (State == nullptr)
			{
				continue;
			}
			
			const EMetaStoryVisitor StateProgress = VisitStateNodes(*State,
				[&OutStructDescs, &BindableDescs, &Path, TargetStructID, bIsTargetPropertyFunction, bAcceptTaskInstanceData, this]
				(const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
				{
					// Stop iterating as soon as we find the target node.
					if (Desc.ID == TargetStructID)
					{
						OutStructDescs.Append(BindableDescs);
						return EMetaStoryVisitor::Break;
					}

					// Not at target yet, collect all bindable source accessible so far.
					switch (Desc.DataSource)
					{
						case EMetaStoryBindableStructSource::StateParameter:
						case EMetaStoryBindableStructSource::StateEvent:
							BindableDescs.Add(TInstancedStruct<FMetaStoryBindableStructDesc>::Make(Desc));
							break;

						case EMetaStoryBindableStructSource::Task:
							if (bAcceptTaskInstanceData)
							{
								BindableDescs.Add(TInstancedStruct<FMetaStoryBindableStructDesc>::Make(Desc));
							}
							break;

						case EMetaStoryBindableStructSource::TransitionEvent:
						{
							// Checking if BindableStruct's owning Transition contains the Target.
							if (State == Path.Last())
							{
								for (const FMetaStoryTransition& Transition : State->Transitions)
								{
									bool bFoundOwningTransition = false;
									for (const FMetaStoryEditorNode& ConditionNode : Transition.Conditions)
									{
										if (ConditionNode.ID == TargetStructID
											|| (bIsTargetPropertyFunction && UE::MetaStory::Editor::IsPropertyFunctionOwnedByNode(ConditionNode.ID, TargetStructID, EditorBindings)))
										{
											if (Transition.GetEventID() == Desc.ID)
											{
												BindableDescs.Add(TInstancedStruct<FMetaStoryBindableStructDesc>::Make(Desc));
											}

											bFoundOwningTransition = true;
											break;
										}
									}

									if (bFoundOwningTransition)
									{
										break;
									}
								}
							}
							break;
						}

						case EMetaStoryBindableStructSource::PropertyFunction:
						{
							if (State == Path.Last())
							{
								if (UE::MetaStory::Editor::IsPropertyFunctionOwnedByNode(TargetStructID, Desc.ID, EditorBindings))
								{
									BindableDescs.Add(TInstancedStruct<FMetaStoryBindableStructDesc>::Make(Desc));
								}
							}
						}
					}
							
					return EMetaStoryVisitor::Continue;
				});
			
			if (StateProgress == EMetaStoryVisitor::Break)
			{
				break;
			}
		}
	}
}

UMetaStoryEditorDataExtension* UMetaStoryEditorData::K2_GetExtension(TSubclassOf<UMetaStoryEditorDataExtension> InExtensionType)
{
	for (UMetaStoryEditorDataExtension* Extension : Extensions)
	{
		if (Extension && Extension->IsA(InExtensionType))
		{
			return Extension;
		}
	}
	return nullptr;
}

FMetaStoryBindableStructDesc UMetaStoryEditorData::FindContextData(const UStruct* ObjectType, const FString ObjectNameHint) const
{
	if (Schema == nullptr)
	{
		return FMetaStoryBindableStructDesc();
	}

	// Find candidates based on type.
	TArray<FMetaStoryBindableStructDesc> Candidates;
	for (const FMetaStoryExternalDataDesc& Desc : Schema->GetContextDataDescs())
	{
		if (Desc.Struct != nullptr
			&& Desc.Struct->IsChildOf(ObjectType))
		{
			Candidates.Emplace(UE::MetaStory::Editor::GlobalStateName, Desc.Name, Desc.Struct, FMetaStoryDataHandle(), EMetaStoryBindableStructSource::Context, Desc.ID);
		}
	}

	// Handle trivial cases.
	if (Candidates.IsEmpty())
	{
		return FMetaStoryBindableStructDesc();
	}

	if (Candidates.Num() == 1)
	{
		return Candidates[0];
	}
	
	check(!Candidates.IsEmpty());
	
	// Multiple candidates, pick one that is closest match based on name.
	auto CalculateScore = [](const FString& Name, const FString& CandidateName)
	{
		if (CandidateName.IsEmpty())
		{
			return 1.0f;
		}
		const float WorstCase = static_cast<float>(Name.Len() + CandidateName.Len());
		return 1.0f - (static_cast<float>(Algo::LevenshteinDistance(Name, CandidateName)) / WorstCase);
	};
	
	const FString ObjectNameLowerCase = ObjectNameHint.ToLower();
	
	int32 HighestScoreIndex = 0;
	float HighestScore = CalculateScore(ObjectNameLowerCase, Candidates[0].Name.ToString().ToLower());
	
	for (int32 Index = 1; Index < Candidates.Num(); Index++)
	{
		const float Score = CalculateScore(ObjectNameLowerCase, Candidates[Index].Name.ToString().ToLower());
		if (Score > HighestScore)
		{
			HighestScore = Score;
			HighestScoreIndex = Index;
		}
	}
	
	return Candidates[HighestScoreIndex];
}

EMetaStoryVisitor UMetaStoryEditorData::EnumerateBindablePropertyFunctionNodes(TFunctionRef<EMetaStoryVisitor(const UScriptStruct* NodeStruct, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const
{
	if (Schema == nullptr)
	{
		return EMetaStoryVisitor::Continue;
	}

	FMetaStoryEditorModule& EditorModule = FModuleManager::GetModuleChecked<FMetaStoryEditorModule>(TEXT("MetaStoryEditorModule"));
	FMetaStoryNodeClassCache* ClassCache = EditorModule.GetNodeClassCache().Get();
	check(ClassCache);

	TArray<TSharedPtr<FMetaStoryNodeClassData>> StructNodes;
	ClassCache->GetStructs(FMetaStoryPropertyFunctionBase::StaticStruct(), StructNodes);
	for (const TSharedPtr<FMetaStoryNodeClassData>& NodeClassData : StructNodes)
	{
		if (const UScriptStruct* NodeStruct = NodeClassData->GetScriptStruct())
		{
			if (NodeStruct == FMetaStoryPropertyFunctionBase::StaticStruct() || NodeStruct->HasMetaData(TEXT("Hidden")))
			{
				continue;
			}

			if (Schema->IsStructAllowed(NodeStruct))
			{
				if (const UStruct* InstanceDataStruct = NodeClassData->GetInstanceDataStruct())
				{
					FMetaStoryBindableStructDesc Desc;
					Desc.Struct = InstanceDataStruct;
					Desc.ID = FGuid::NewDeterministicGuid(NodeStruct->GetName());
					Desc.DataSource = EMetaStoryBindableStructSource::PropertyFunction;
					Desc.Name = FName(NodeStruct->GetDisplayNameText().ToString());
					Desc.StatePath = UE::MetaStory::Editor::PropertyFunctionStateName;
					Desc.Category = NodeStruct->GetMetaData(TEXT("Category"));

					if (InFunc(NodeStruct, Desc, FMetaStoryDataView(InstanceDataStruct, nullptr)) == EMetaStoryVisitor::Break)
					{
						return EMetaStoryVisitor::Break;
					}
				}
			}
		}
	}

	return EMetaStoryVisitor::Continue;
}

void UMetaStoryEditorData::AppendBindablePropertyFunctionStructs(TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& InOutStructs) const
{
	EnumerateBindablePropertyFunctionNodes([&InOutStructs](const UScriptStruct*, const FMetaStoryBindableStructDesc& Desc, const FPropertyBindingDataView)
		{
			InOutStructs.Add(TInstancedStruct<FMetaStoryBindableStructDesc>::Make(Desc));
			return EMetaStoryVisitor::Continue;
		});
}

bool UMetaStoryEditorData::CanCreateParameter(const FGuid StructID) const
{
	if (RootParametersGuid == StructID)
	{
		return true;
	}

	bool bFoundStructID = false;

	VisitHierarchy([&StructID, &bFoundStructID](UMetaStoryState& State, UMetaStoryState* ParentState)->EMetaStoryVisitor
	{
		if (State.Parameters.ID == StructID)
		{
			bFoundStructID = true;
			return EMetaStoryVisitor::Break;
		}
		return EMetaStoryVisitor::Continue;
	});

	return bFoundStructID;
}

void UMetaStoryEditorData::CreateParametersForStruct(const FGuid StructID, TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs)
{
	if (InOutCreationDescs.IsEmpty())
	{
		return;
	}

	const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
	checkf(MetaStory, TEXT("UMetaStoryEditorData should only be allocated within a UMetaStory"));

	if (RootParametersGuid == StructID)
	{
		CreateRootProperties(InOutCreationDescs);
		UE::MetaStory::Delegates::OnParametersChanged.Broadcast(*MetaStory);
		return;
	}

	VisitHierarchy([&StructID, MetaStory, &InOutCreationDescs](UMetaStoryState& State, UMetaStoryState* ParentState)->EMetaStoryVisitor
	{
		if (State.Parameters.ID == StructID)
		{
			UE::PropertyBinding::CreateUniquelyNamedPropertiesInPropertyBag(InOutCreationDescs,State.Parameters.Parameters);
			UE::MetaStory::Delegates::OnStateParametersChanged.Broadcast(*MetaStory, State.ID);
			return EMetaStoryVisitor::Break;
		}
		return EMetaStoryVisitor::Continue;
	});
}

void UMetaStoryEditorData::OnPropertyBindingChanged(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
{
	UE::MetaStory::PropertyBinding::OnMetaStoryPropertyBindingChanged.Broadcast(InSourcePath, InTargetPath);
}

bool UMetaStoryEditorData::GetBindableStructByID(const FGuid StructID, TInstancedStruct<FPropertyBindingBindableStructDescriptor>& OutStructDesc) const
{
	VisitAllNodes([&OutStructDesc, StructID](const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
	{
		if (Desc.ID == StructID)
		{
			OutStructDesc = TInstancedStruct<FMetaStoryBindableStructDesc>::Make(Desc);
			return EMetaStoryVisitor::Break;
		}
		return EMetaStoryVisitor::Continue;
	});
	
	return OutStructDesc.IsValid();
}

bool UMetaStoryEditorData::GetBindingDataViewByID(const FGuid StructID, FPropertyBindingDataView& OutDataView) const
{
	bool bFound = false;
	VisitAllNodes([&OutDataView, &bFound, StructID](const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
	{
		if (Desc.ID == StructID)
		{
			bFound = true;
			OutDataView = Value;
			return EMetaStoryVisitor::Break;
		}
		return EMetaStoryVisitor::Continue;
	});

	return bFound;
}

const UMetaStoryState* UMetaStoryEditorData::GetStateByStructID(const FGuid TargetStructID) const
{
	const UMetaStoryState* Result = nullptr;

	VisitHierarchyNodes([&Result, TargetStructID](const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
		{
			if (Desc.ID == TargetStructID)
			{
				Result = State;
				return EMetaStoryVisitor::Break;
			}
			return EMetaStoryVisitor::Continue;
			
		});

	return Result;
}

const UMetaStoryState* UMetaStoryEditorData::GetStateByID(const FGuid StateID) const
{
	return const_cast<UMetaStoryEditorData*>(this)->GetMutableStateByID(StateID);
}

UMetaStoryState* UMetaStoryEditorData::GetMutableStateByID(const FGuid StateID)
{
	UMetaStoryState* Result = nullptr;
	
	VisitHierarchy([&Result, &StateID](UMetaStoryState& State, UMetaStoryState* /*ParentState*/)
	{
		if (State.ID == StateID)
		{
			Result = &State;
			return EMetaStoryVisitor::Break;
		}

		return EMetaStoryVisitor::Continue;
	});

	return Result;
}

void UMetaStoryEditorData::GetAllStructValues(TMap<FGuid, const FPropertyBindingDataView>& OutAllValues) const
{
	OutAllValues.Reset();

	const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
	checkf(MetaStory, TEXT("UMetaStoryEditorData should only be allocated within a UMetaStory"));

	VisitAllNodes([&OutAllValues](const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
		{
			OutAllValues.Emplace(Desc.ID, Value);
			return EMetaStoryVisitor::Continue;
		});
}

void UMetaStoryEditorData::GetAllStructValues(TMap<FGuid, const FMetaStoryDataView>& OutAllValues) const
{
	OutAllValues.Reset();

	const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
	checkf(MetaStory, TEXT("UMetaStoryEditorData should only be allocated within a UMetaStory"));

	VisitAllNodes([&OutAllValues](const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
		{
			OutAllValues.Emplace(Desc.ID, Value);
			return EMetaStoryVisitor::Continue;
		});
}

void UMetaStoryEditorData::ReparentStates()
{
	VisitHierarchy([TreeData = this](UMetaStoryState& State, UMetaStoryState* ParentState) mutable 
	{
		UObject* ExpectedOuter = ParentState ? Cast<UObject>(ParentState) : Cast<UObject>(TreeData);
		if (State.GetOuter() != ExpectedOuter)
		{
			UE_LOG(LogMetaStoryEditor, Log, TEXT("%s: Fixing outer on state %s."), *TreeData->GetFullName(), *GetNameSafe(&State));
			State.Rename(nullptr, ExpectedOuter, REN_DontCreateRedirectors | REN_DoNotDirty);
		}
		
		State.Parent = ParentState;
		
		return EMetaStoryVisitor::Continue;
	});
}

void UMetaStoryEditorData::FixObjectInstances(TSet<UObject*>& SeenObjects, UObject& Outer, FMetaStoryEditorNode& Node)
{
	auto NewInstances = [this, &Node, &Outer](const UStruct* Struct, const UStruct* ExecutionRuntimeStruct)
		{
			if (Struct && !Node.Instance.IsValid() && Node.InstanceObject == nullptr)
			{
				if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Struct))
				{
					Node.Instance.InitializeAs(InstanceType);
				}
				else if (const UClass* InstanceClass = Cast<const UClass>(Struct))
				{
					Node.InstanceObject = NewObject<UObject>(&Outer, InstanceClass);
				}
			}
			if (ExecutionRuntimeStruct && !Node.ExecutionRuntimeData.IsValid() && Node.ExecutionRuntimeDataObject == nullptr)
			{
				if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(ExecutionRuntimeStruct))
				{
					Node.ExecutionRuntimeData.InitializeAs(InstanceType);
				}
				else if (const UClass* InstanceClass = Cast<const UClass>(ExecutionRuntimeStruct))
				{
					Node.ExecutionRuntimeDataObject = NewObject<UObject>(&Outer, InstanceClass);
				}
			}
		};
	auto FixInstance = [this, &SeenObjects, &Outer](FInstancedStruct& Instance, const UStruct* Struct)
		{
			if (Instance.IsValid())
			{
				if (Instance.GetScriptStruct() != Struct)
				{
					Instance.Reset();
				}
			}
		};
	auto FixInstanceObject = [this, &SeenObjects, &Outer](TObjectPtr<UObject>& InstanceObject, const UStruct* Struct)
		{
			if (InstanceObject)
			{
				if (InstanceObject->GetClass() != Struct)
				{
					InstanceObject = nullptr;
				}
				// Found a duplicate reference to an object, make unique copy.
				else if(SeenObjects.Contains(InstanceObject))				
				{
					UE_LOG(LogMetaStoryEditor, Log, TEXT("%s: Making duplicate node instance %s unique."), *GetFullName(), *GetNameSafe(InstanceObject));
					InstanceObject = DuplicateObject(InstanceObject, &Outer);
				}
				else
				{
					// Make sure the instance object is property outered.
					if (InstanceObject->GetOuter() != &Outer)
					{
						UE_LOG(LogMetaStoryEditor, Log, TEXT("%s: Fixing outer on node instance %s."), *GetFullName(), *GetNameSafe(InstanceObject));
						InstanceObject->Rename(nullptr, &Outer, REN_DontCreateRedirectors | REN_DoNotDirty);
					}
				}
				SeenObjects.Add(InstanceObject);
			}
		};

	const FMetaStoryNodeBase* NodeBase = Node.Node.GetPtr<FMetaStoryNodeBase>();
	if (NodeBase)
	{
		FixInstance(Node.Instance, NodeBase->GetInstanceDataType());
		FixInstanceObject(Node.InstanceObject, NodeBase->GetInstanceDataType());
		FixInstance(Node.ExecutionRuntimeData, NodeBase->GetExecutionRuntimeDataType());
		FixInstanceObject(Node.ExecutionRuntimeDataObject, NodeBase->GetExecutionRuntimeDataType());
		NewInstances(NodeBase->GetInstanceDataType(), NodeBase->GetExecutionRuntimeDataType());
	}
	else
	{
		Node.Instance.Reset();
		Node.InstanceObject = nullptr;
		Node.ExecutionRuntimeData.Reset();
		Node.ExecutionRuntimeDataObject = nullptr;
	}
};

void UMetaStoryEditorData::FixObjectNodes()
{
	// Older version of MetaStorys had all instances outered to the editor data. This causes issues with State copy/paste.
	// Instance data does not get duplicated but the copied state will reference the object on the source state instead.
	//
	// Ensure that all node objects are parented to their states, and make duplicated instances unique.

	TSet<UObject*> SeenObjects;
	
	VisitHierarchy([&SeenObjects, TreeData = this](UMetaStoryState& State, UMetaStoryState* ParentState) mutable 
	{

		// Enter conditions
		for (FMetaStoryEditorNode& Node : State.EnterConditions)
		{
			TreeData->FixObjectInstances(SeenObjects, State, Node);
		}
		
		// Tasks
		for (FMetaStoryEditorNode& Node : State.Tasks)
		{
			TreeData->FixObjectInstances(SeenObjects, State, Node);
		}

		TreeData->FixObjectInstances(SeenObjects, State, State.SingleTask);


		// Transitions
		for (FMetaStoryTransition& Transition : State.Transitions)
		{
			for (FMetaStoryEditorNode& Node : Transition.Conditions)
			{
				TreeData->FixObjectInstances(SeenObjects, State, Node);
			}
		}
		
		return EMetaStoryVisitor::Continue;
	});

	for (FMetaStoryEditorNode& Node : Evaluators)
	{
		FixObjectInstances(SeenObjects, *this, Node);
	}

	for (FMetaStoryEditorNode& Node : GlobalTasks)
	{
		FixObjectInstances(SeenObjects, *this, Node);
	}
}

FText UMetaStoryEditorData::GetNodeDescription(const FMetaStoryEditorNode& Node, const EMetaStoryNodeFormatting Formatting) const
{
	if (const FMetaStoryNodeBase* NodePtr = Node.Node.GetPtr<FMetaStoryNodeBase>())
	{
		// If the node has name override, return it.
		if (!NodePtr->Name.IsNone())
		{
			return FText::FromName(NodePtr->Name);
		}

		// If the node has automatic description, return it.
		const FMetaStoryBindingLookup BindingLookup(this);
		const FMetaStoryDataView InstanceData = Node.GetInstance();
		if (InstanceData.IsValid())
		{
			
			const FText Description = NodePtr->GetDescription(Node.ID, InstanceData, BindingLookup, Formatting);
			if (!Description.IsEmpty())
			{
				return Description;
			}
		}

		// As last resort, return node's display name.
		check(Node.Node.GetScriptStruct());
		return Node.Node.GetScriptStruct()->GetDisplayNameText();
	}

	// The node is not initialized.
	return LOCTEXT("EmptyNode", "None");
}

void UMetaStoryEditorData::FixDuplicateIDs()
{
	// Around version 5.1-5.3 we had issue that copy/paste or some duplication methods could create nodes with duplicate IDs.
	// This code tries to fix that, it looks for duplicates, makes them unique, and duplicates the bindings when ID changes.
	TSet<FGuid> FoundNodeIDs;

	// Evaluators
	for (int32 Index = 0; Index < Evaluators.Num(); Index++)
	{
		FMetaStoryEditorNode& Node = Evaluators[Index];
		if (const FMetaStoryEvaluatorBase* Evaluator = Node.Node.GetPtr<FMetaStoryEvaluatorBase>())
		{
			const FGuid OldID = Node.ID; 
			if (FoundNodeIDs.Contains(Node.ID))
			{
				Node.ID = UE::MetaStory::PropertyHelpers::MakeDeterministicID(*this, TEXT("Evaluators"), Index);
				
				UE_LOG(LogMetaStoryEditor, Log, TEXT("%s: Found Evaluator '%s' with duplicate ID, changing ID:%s to ID:%s."),
					*GetFullName(), *Node.GetName().ToString(), *OldID.ToString(), *Node.ID.ToString());
				EditorBindings.CopyBindings(OldID, Node.ID);
			}
			FoundNodeIDs.Add(Node.ID);
		}
	}
	
	// Global Tasks
	for (int32 Index = 0; Index < GlobalTasks.Num(); Index++)
	{
		FMetaStoryEditorNode& Node = GlobalTasks[Index];
		if (const FMetaStoryTaskBase* Task = Node.Node.GetPtr<FMetaStoryTaskBase>())
		{
			const FGuid OldID = Node.ID; 
			if (FoundNodeIDs.Contains(Node.ID))
			{
				Node.ID = UE::MetaStory::PropertyHelpers::MakeDeterministicID(*this, TEXT("GlobalTasks"), Index);
				
				UE_LOG(LogMetaStoryEditor, Log, TEXT("%s: Found GlobalTask '%s' with duplicate ID, changing ID:%s to ID:%s."),
					*GetFullName(), *Node.GetName().ToString(), *OldID.ToString(), *Node.ID.ToString());
				EditorBindings.CopyBindings(OldID, Node.ID);
			}
			FoundNodeIDs.Add(Node.ID);
		}
	}
	
	VisitHierarchy([&FoundNodeIDs, &EditorBindings = EditorBindings, &Self = *this](UMetaStoryState& State, UMetaStoryState* ParentState)
	{
		// Enter conditions
		for (int32 Index = 0; Index < State.EnterConditions.Num(); Index++)
		{
			FMetaStoryEditorNode& Node = State.EnterConditions[Index];
			if (const FMetaStoryConditionBase* Cond = Node.Node.GetPtr<FMetaStoryConditionBase>())
			{
				const FGuid OldID = Node.ID;
				
				bool bIsAlreadyInSet = false;
				FoundNodeIDs.Add(Node.ID, &bIsAlreadyInSet);
				if (bIsAlreadyInSet)
				{
					Node.ID = UE::MetaStory::PropertyHelpers::MakeDeterministicID(State, TEXT("EnterConditions"), Index);
					
					UE_LOG(LogMetaStoryEditor, Log, TEXT("%s: Found Enter Condition '%s' with duplicate ID on state '%s', changing ID:%s to ID:%s."),
						*Self.GetFullName(), *Node.GetName().ToString(), *GetNameSafe(&State), *OldID.ToString(), *Node.ID.ToString());
					EditorBindings.CopyBindings(OldID, Node.ID);
				}
			}
		}

		// Tasks
		for (int32 Index = 0; Index < State.Tasks.Num(); Index++)
		{
			FMetaStoryEditorNode& Node = State.Tasks[Index];
			if (const FMetaStoryTaskBase* Task = Node.Node.GetPtr<FMetaStoryTaskBase>())
			{
				const FGuid OldID = Node.ID;
				
				bool bIsAlreadyInSet = false;
				FoundNodeIDs.Add(Node.ID, &bIsAlreadyInSet);
				if (bIsAlreadyInSet)
				{
					Node.ID = UE::MetaStory::PropertyHelpers::MakeDeterministicID(State, TEXT("Tasks"), Index);

					UE_LOG(LogMetaStoryEditor, Log, TEXT("%s: Found Task '%s' with duplicate ID on state '%s', changing ID:%s to ID:%s."),
						*Self.GetFullName(), *Node.GetName().ToString(), *GetNameSafe(&State), *OldID.ToString(), *Node.ID.ToString());
					EditorBindings.CopyBindings(OldID, Node.ID);
				}
			}
		}

		if (FMetaStoryTaskBase* Task = State.SingleTask.Node.GetMutablePtr<FMetaStoryTaskBase>())
		{
			const FGuid OldID = State.SingleTask.ID;

			bool bIsAlreadyInSet = false;
			FoundNodeIDs.Add(State.SingleTask.ID, &bIsAlreadyInSet);
			if (bIsAlreadyInSet)
			{
				State.SingleTask.ID = UE::MetaStory::PropertyHelpers::MakeDeterministicID(State, TEXT("SingleTask"), 0);

				UE_LOG(LogMetaStoryEditor, Log, TEXT("%s: Found enter condition '%s' with duplicate ID on state '%s', changing ID:%s to ID:%s."),
					*Self.GetFullName(), *State.SingleTask.GetName().ToString(), *GetNameSafe(&State), *OldID.ToString(), *State.SingleTask.ID.ToString());
				EditorBindings.CopyBindings(OldID, State.SingleTask.ID);
			}
		}

		// Transitions
		for (int32 TransitionIndex = 0; TransitionIndex < State.Transitions.Num(); TransitionIndex++)
		{
			FMetaStoryTransition& Transition = State.Transitions[TransitionIndex];
			for (int32 Index = 0; Index < Transition.Conditions.Num(); Index++)
			{
				FMetaStoryEditorNode& Node = Transition.Conditions[Index];
				if (const FMetaStoryConditionBase* Cond = Node.Node.GetPtr<FMetaStoryConditionBase>())
				{
					const FGuid OldID = Node.ID; 
					bool bIsAlreadyInSet = false;
					FoundNodeIDs.Add(Node.ID, &bIsAlreadyInSet);
					if (bIsAlreadyInSet)
					{
						Node.ID = UE::MetaStory::PropertyHelpers::MakeDeterministicID(State, TEXT("TransitionConditions"), ((uint64)TransitionIndex << 32) | (uint64)Index);

						UE_LOG(LogMetaStoryEditor, Log, TEXT("%s: Found transition condition '%s' with duplicate ID on state '%s', changing ID:%s to ID:%s."),
							*Self.GetFullName(), *Node.GetName().ToString(), *GetNameSafe(&State), *OldID.ToString(), *Node.ID.ToString());
						EditorBindings.CopyBindings(OldID, Node.ID);
					}
				}
			}
		}
		
		return EMetaStoryVisitor::Continue;
	});

	// It is possible that the user has changed the node type so some of the bindings might not make sense anymore, clean them up.
	TMap<FGuid, const FPropertyBindingDataView> AllValues;
	GetAllStructValues(AllValues);
	EditorBindings.RemoveInvalidBindings(AllValues);
}

void UMetaStoryEditorData::DuplicateIDs()
{
	TMap<FGuid, FGuid> OldToNewIDs;

	// Visit and create new ids
	{
		auto AddId = [&OldToNewIDs](const FGuid& OldID, bool bTestIfContains = true)
		{
			ensureAlwaysMsgf(bTestIfContains == false || !OldToNewIDs.Contains(OldID), TEXT("The id is duplicated and FixDuplicateIDs failed to fix it."));

			if (OldID.IsValid())
			{
				const FGuid NewID = FGuid::NewGuid();
				OldToNewIDs.Add(OldID, NewID);
			}
		};

		auto AddIds = [&AddId](TArrayView<FMetaStoryEditorNode> Nodes)
		{
			for (FMetaStoryEditorNode& Node : Nodes)
			{
				AddId(Node.ID);
			}
		};

		// Do not use the VisitGlobalNodes because the schema should not be included in OldToNewIDs 
		OldToNewIDs.Add(RootParametersGuid, FGuid::NewGuid());
		AddIds(Evaluators);
		AddIds(GlobalTasks);
		for (FMetaStoryEditorColor& Color : Colors)
		{
			AddId(Color.ColorRef.ID);
		}

		VisitHierarchy([Self = this, &AddId, &OldToNewIDs](const UMetaStoryState& State, UMetaStoryState* ParentState)
		{
			AddId(State.ID);
			AddId(State.Parameters.ID);

			for (const FMetaStoryTransition& Transition : State.Transitions)
			{
				AddId(Transition.ID);
			}

			return Self->VisitStateNodes(State, [&AddId](const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
			{
				AddId(Desc.ID, /*bTestIfContains*/false);
				return EMetaStoryVisitor::Continue;
			});
		});

		// Confirms that we collected everything.
		{
			TMap<FGuid, const FMetaStoryDataView> AllStructValues;
			GetAllStructValues(AllStructValues);
			// Schema ids are not duplicated
			if (Schema != nullptr)
			{
				for (const FMetaStoryExternalDataDesc& ContextDesc : Schema->GetContextDataDescs())
				{
					AllStructValues.Remove(ContextDesc.ID);
				}
			}
			for (auto& StructValue : AllStructValues)
			{
				ensureMsgf(OldToNewIDs.Contains(StructValue.Key), TEXT("An ID container was not duplicated for asset '%s'."), *GetOutermost()->GetName());
			}
		}
	}

	// Remap ids properties to the new generated ids
	{
		TArray<const UObject*> ObjectToSearch;
		TSet<const UObject*> ObjectSearched;
		ObjectToSearch.Add(this);
		ObjectSearched.Add(this);
		while (ObjectToSearch.Num())
		{
			const UObject* CurrentObject = ObjectToSearch.Pop();
			for (TPropertyValueIterator<FProperty> It(CurrentObject->GetClass(), CurrentObject, EPropertyValueIteratorFlags::FullRecursion, EFieldIteratorFlags::ExcludeDeprecated); It; ++It)
			{
				const FProperty* Property = It.Key();
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					if (StructProperty->Struct == TBaseStructure<FGuid>::Get())
					{
						// Skip the guid properties.
						It.SkipRecursiveProperty();

						// Modify the value if needed.
						// We const_cast because we don't change the layout and the iterator advance is still going to work.
						FGuid* GuidValue = reinterpret_cast<FGuid*>(const_cast<void*>(It.Value()));
						if (FGuid* NewGuidValue = OldToNewIDs.Find(*GuidValue))
						{
							*GuidValue = *NewGuidValue;
						}
					}
				}
				else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					if (const UObject* ObjectValue = ObjectProperty->GetPropertyValue(It.Value()))
					{
						if (!ObjectSearched.Contains(ObjectValue))
						{
							// Add the inner properties of this instanced object.
							bool bAddObject = false;
							if (Property->HasAllPropertyFlags(CPF_ExportObject) && ObjectValue->GetClass()->HasAllClassFlags(CLASS_EditInlineNew))
							{
								bAddObject = true;
							}
							else if (Property->HasAllPropertyFlags(CPF_InstancedReference))
							{
								bAddObject = true;
							}

							if (bAddObject)
							{
								ObjectSearched.Add(ObjectValue);
								ObjectToSearch.Add(ObjectValue);
							}
						}
					}
				}
			}
		}
	}
}

void UMetaStoryEditorData::UpdateBindingsInstanceStructs()
{
	TMap<FGuid, const FMetaStoryDataView> AllValues;
	GetAllStructValues(AllValues);
	for (FPropertyBindingBinding& Binding : EditorBindings.GetMutableBindings())
	{
		if (AllValues.Contains(Binding.GetSourcePath().GetStructID()))
		{
			Binding.GetMutableSourcePath().UpdateSegmentsFromValue(AllValues[Binding.GetSourcePath().GetStructID()]);
		}

		if (AllValues.Contains(Binding.GetTargetPath().GetStructID()))
		{
			Binding.GetMutableTargetPath().UpdateSegmentsFromValue(AllValues[Binding.GetTargetPath().GetStructID()]);
		}
	}
}

void UMetaStoryEditorData::CallPostLoadOnNodes()
{
	for (FMetaStoryEditorNode& EvaluatorEditorNode : Evaluators)
	{
		if (FMetaStoryNodeBase* EvaluatorNode = EvaluatorEditorNode.Node.GetMutablePtr<FMetaStoryNodeBase>())
		{
			UE::MetaStoryEditor::EditorNodeUtils::ConditionalUpdateNodeInstanceData(EvaluatorEditorNode, *this);
			EvaluatorNode->PostLoad(EvaluatorEditorNode.GetInstance());
		}
	}

	for (FMetaStoryEditorNode& GlobalTaskEditorNode : GlobalTasks)
	{
		if (FMetaStoryNodeBase* GlobalTaskNode = GlobalTaskEditorNode.Node.GetMutablePtr<FMetaStoryNodeBase>())
		{
			UE::MetaStoryEditor::EditorNodeUtils::ConditionalUpdateNodeInstanceData(GlobalTaskEditorNode, *this);
			GlobalTaskNode->PostLoad(GlobalTaskEditorNode.GetInstance());
		}
	}
}

EMetaStoryVisitor UMetaStoryEditorData::VisitStateNodes(const UMetaStoryState& State, TFunctionRef<EMetaStoryVisitor(const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const
{
	auto VisitFuncNodes = [&InFunc, &State, this](const FGuid StructID, const FName NodeName)
	{
		const FString StatePath = FString::Printf(TEXT("%s/%s"), *State.GetPath(), *NodeName.ToString());
		return VisitStructBoundPropertyFunctions(StructID, StatePath, [&InFunc, &State](const FMetaStoryEditorNode& EditorNode, const FMetaStoryBindableStructDesc& Desc, FMetaStoryDataView Value)
		{
			return InFunc(&State, Desc, Value);
		});
	};

	bool bContinue = true;

	const FString StatePath = State.GetPath();
	
	if (bContinue)
	{
		// Bindable state parameters
		if (State.Parameters.Parameters.IsValid())
		{
			if (VisitFuncNodes(State.Parameters.ID, UE::MetaStory::Editor::ParametersNodeName) == EMetaStoryVisitor::Break)
			{
				bContinue = false;
			}
			else
			{
				FMetaStoryBindableStructDesc Desc;
				Desc.StatePath = StatePath;
				Desc.Struct = State.Parameters.Parameters.GetPropertyBagStruct();
				Desc.Name = FName("Parameters");
				Desc.ID = State.Parameters.ID;
				Desc.DataSource = EMetaStoryBindableStructSource::StateParameter;

				if (InFunc(&State, Desc, FMetaStoryDataView(const_cast<FInstancedPropertyBag&>(State.Parameters.Parameters).GetMutableValue())) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
				}
			}
		}
	}

	const FString StatePathWithConditions = StatePath + TEXT("/EnterConditions");
	
	if (bContinue)
	{
		if (State.bHasRequiredEventToEnter )
		{
			FMetaStoryBindableStructDesc Desc;
			Desc.StatePath = StatePathWithConditions;
			Desc.Struct = FMetaStoryEvent::StaticStruct();
			Desc.Name = FName("Enter Event");
			Desc.ID = State.GetEventID();
			Desc.DataSource = EMetaStoryBindableStructSource::StateEvent;

			if (InFunc(&State, Desc, FMetaStoryDataView(FStructView::Make(const_cast<UMetaStoryState&>(State).RequiredEventToEnter.GetTemporaryEvent()))) == EMetaStoryVisitor::Break)
			{
				bContinue = false;
			}
		}
	}

	if (bContinue)
	{
		// Enter conditions
		for (const FMetaStoryEditorNode& Node : State.EnterConditions)
		{
			if (VisitFuncNodes(Node.ID, Node.GetName()) == EMetaStoryVisitor::Break)
			{
				bContinue = false;
				break;
			}
			else if (const FMetaStoryConditionBase* Cond = Node.Node.GetPtr<FMetaStoryConditionBase>())
			{
				FMetaStoryBindableStructDesc Desc;
				Desc.StatePath = StatePathWithConditions;
				Desc.Struct = Cond->GetInstanceDataType();
				Desc.Name = Node.GetName();
				Desc.ID = Node.ID;
				Desc.DataSource = EMetaStoryBindableStructSource::Condition;

				if (InFunc(&State, Desc, Node.GetInstance()) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
					break;
				}

				FMetaStoryBindableStructDesc NodeDesc;
				NodeDesc.StatePath = StatePathWithConditions;
				NodeDesc.Struct = Node.Node.GetScriptStruct();
				NodeDesc.Name = Node.GetName();
				NodeDesc.ID = Node.GetNodeID();
				NodeDesc.DataSource = EMetaStoryBindableStructSource::Condition;

				if (InFunc(&State, NodeDesc, Node.GetNode()) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
					break;
				}
			}
		}
	}
	if (bContinue)
	{
		const FString StatePathWithConsiderations = StatePath + TEXT("/Considerations");
		// Utility Considerations
		for (const FMetaStoryEditorNode& Node : State.Considerations)
		{
			if (VisitFuncNodes(Node.ID, Node.GetName()) == EMetaStoryVisitor::Break)
			{
				bContinue = false;
				break;
			}
			else if (const FMetaStoryConsiderationBase* Consideration = Node.Node.GetPtr<FMetaStoryConsiderationBase>())
			{
				FMetaStoryBindableStructDesc Desc;
				Desc.StatePath = StatePathWithConsiderations;
				Desc.Struct = Consideration->GetInstanceDataType();
				Desc.Name = Node.GetName();
				Desc.ID = Node.ID;
				Desc.DataSource = EMetaStoryBindableStructSource::Consideration;

				if (InFunc(&State, Desc, Node.GetInstance()) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
					break;
				}

				FMetaStoryBindableStructDesc NodeDesc;
				NodeDesc.StatePath = StatePathWithConsiderations;
				NodeDesc.Struct = Node.Node.GetScriptStruct();
				NodeDesc.Name = Node.GetName();
				NodeDesc.ID = Node.GetNodeID();
				NodeDesc.DataSource = EMetaStoryBindableStructSource::Consideration;

				if (InFunc(&State, NodeDesc, Node.GetNode()) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
					break;
				}
			}
		}
	}
	if (bContinue)
	{
		// Tasks
		for (const FMetaStoryEditorNode& Node : State.Tasks)
		{
			if (VisitFuncNodes(Node.ID, Node.GetName()) == EMetaStoryVisitor::Break)
			{
				bContinue = false;
				break;
			}
			else if (const FMetaStoryTaskBase* Task = Node.Node.GetPtr<FMetaStoryTaskBase>())
			{
				FMetaStoryBindableStructDesc Desc;
				Desc.StatePath = StatePath;
				Desc.Struct = Task->GetInstanceDataType();
				Desc.Name = Node.GetName();
				Desc.ID = Node.ID;
				Desc.DataSource = EMetaStoryBindableStructSource::Task;

				if (InFunc(&State, Desc, Node.GetInstance()) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
					break;
				}

				FMetaStoryBindableStructDesc NodeDesc;
				NodeDesc.StatePath = StatePath;
				NodeDesc.Struct = Node.Node.GetScriptStruct();
				NodeDesc.Name = Node.GetName();
				NodeDesc.ID = Node.GetNodeID();
				NodeDesc.DataSource = EMetaStoryBindableStructSource::Task;

				if (InFunc(&State, NodeDesc, Node.GetNode()) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
					break;
				}
			}
		}
	}
	if (bContinue)
	{
		if (const FMetaStoryTaskBase* Task = State.SingleTask.Node.GetPtr<FMetaStoryTaskBase>())
		{
			if (VisitFuncNodes(State.SingleTask.ID, State.SingleTask.GetName()) == EMetaStoryVisitor::Break)
			{
				bContinue = false;
			}
			else
			{
				FMetaStoryBindableStructDesc Desc;
				Desc.StatePath = StatePath;
				Desc.Struct = Task->GetInstanceDataType();
				Desc.Name = State.SingleTask.GetName();
				Desc.ID = State.SingleTask.ID;
				Desc.DataSource = EMetaStoryBindableStructSource::Task;

				if (InFunc(&State, Desc, State.SingleTask.GetInstance()) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
				}

				FMetaStoryBindableStructDesc NodeDesc;
				NodeDesc.StatePath = StatePath;
				NodeDesc.Struct = State.SingleTask.Node.GetScriptStruct();
				NodeDesc.Name = State.SingleTask.GetName();
				NodeDesc.ID = State.SingleTask.GetNodeID();
				NodeDesc.DataSource = EMetaStoryBindableStructSource::Task;

				if (InFunc(&State, NodeDesc, State.SingleTask.GetNode()) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
				}
			}
		}

	}
	if (bContinue)
	{
		// Transitions
		for (int32 TransitionIndex = 0; TransitionIndex < State.Transitions.Num(); TransitionIndex++)
		{
			const FMetaStoryTransition& Transition = State.Transitions[TransitionIndex];
			const FString StatePathWithTransition = StatePath + FString::Printf(TEXT("/Transition[%d]"), TransitionIndex);

			{
				FMetaStoryBindableStructDesc Desc;
				Desc.StatePath = StatePathWithTransition;
				Desc.Struct = FMetaStoryTransition::StaticStruct();
				Desc.Name = FName(TEXT("Transition"));
				Desc.ID = Transition.ID;
				Desc.DataSource = EMetaStoryBindableStructSource::Transition;

				if (InFunc(&State, Desc, FMetaStoryDataView(FStructView::Make(const_cast<FMetaStoryTransition&>(Transition)))) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
					break;
				}
			}

			if (Transition.Trigger == EMetaStoryTransitionTrigger::OnEvent)
			{
				FMetaStoryBindableStructDesc Desc;
				Desc.StatePath = StatePathWithTransition;
				Desc.Struct = FMetaStoryEvent::StaticStruct();
				Desc.Name = FName(TEXT("Transition Event"));
				Desc.ID = Transition.GetEventID();
				Desc.DataSource = EMetaStoryBindableStructSource::TransitionEvent;

				if (InFunc(&State, Desc, FMetaStoryDataView(FStructView::Make(const_cast<FMetaStoryTransition&>(Transition).RequiredEvent.GetTemporaryEvent()))) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
					break;
				}
			}

			for (const FMetaStoryEditorNode& Node : Transition.Conditions)
			{
				if (VisitFuncNodes(Node.ID, Node.GetName()) == EMetaStoryVisitor::Break)
				{
					bContinue = false;
					break;
				}
				else if (const FMetaStoryConditionBase* Cond = Node.Node.GetPtr<FMetaStoryConditionBase>())
				{
					FMetaStoryBindableStructDesc Desc;
					Desc.StatePath = StatePathWithTransition;
					Desc.Struct = Cond->GetInstanceDataType();
					Desc.Name = Node.GetName();
					Desc.ID = Node.ID;
					Desc.DataSource = EMetaStoryBindableStructSource::Condition;

					if (InFunc(&State, Desc, Node.GetInstance()) == EMetaStoryVisitor::Break)
					{
						bContinue = false;
						break;
					}

					FMetaStoryBindableStructDesc NodeDesc;
					NodeDesc.StatePath = StatePathWithTransition;
					NodeDesc.Struct = Node.Node.GetScriptStruct();
					NodeDesc.Name = Node.GetName();
					NodeDesc.ID = Node.GetNodeID();
					NodeDesc.DataSource = EMetaStoryBindableStructSource::Condition;

					if (InFunc(&State, NodeDesc, Node.GetNode()) == EMetaStoryVisitor::Break)
					{
						bContinue = false;
						break;
					}
				}
			}
		}
	}

	return bContinue ? EMetaStoryVisitor::Continue : EMetaStoryVisitor::Break;
}

EMetaStoryVisitor UMetaStoryEditorData::VisitStructBoundPropertyFunctions(FGuid StructID, const FString& StatePath, TFunctionRef<EMetaStoryVisitor(const FMetaStoryEditorNode& EditorNode, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const
{
	TArray<const FPropertyBindingBinding*> Bindings;
	EditorBindings.FPropertyBindingBindingCollection::GetBindingsFor(StructID, Bindings);

	for (const FPropertyBindingBinding* Binding : Bindings)
	{
		const FConstStructView FunctionNodeView = Binding->GetPropertyFunctionNode();
		if (const FMetaStoryEditorNode* FunctionNode = FunctionNodeView.GetPtr<const FMetaStoryEditorNode>())
		{
			if (VisitStructBoundPropertyFunctions(FunctionNode->ID, StatePath, InFunc) == EMetaStoryVisitor::Break)
			{
				return EMetaStoryVisitor::Break;
			}

			FMetaStoryBindableStructDesc Desc;
			Desc.Struct = FunctionNode->GetInstance().GetStruct();
			if (const UStruct* NodeStruct = FunctionNode->Node.GetScriptStruct())
			{
				Desc.ID = FunctionNode->ID;
				Desc.DataSource = EMetaStoryBindableStructSource::PropertyFunction;
				Desc.Name = FName(NodeStruct->GetDisplayNameText().ToString());
				Desc.StatePath = FString::Printf(TEXT("%s/%s"), *StatePath, *UE::MetaStory::Editor::PropertyFunctionStateName);

				if (InFunc(*FunctionNode, Desc, FunctionNode->GetInstance()) == EMetaStoryVisitor::Break)
				{
					return EMetaStoryVisitor::Break;
				}
			}
		}
	}

	return EMetaStoryVisitor::Continue;
}

EMetaStoryVisitor UMetaStoryEditorData::VisitHierarchy(TFunctionRef<EMetaStoryVisitor(UMetaStoryState& State, UMetaStoryState* ParentState)> InFunc) const
{
	using FStatePair = TTuple<UMetaStoryState*, UMetaStoryState*>; 
	TArray<FStatePair> Stack;
	bool bContinue = true;

	for (UMetaStoryState* SubTree : SubTrees)
	{
		if (!SubTree)
		{
			continue;
		}

		Stack.Add( FStatePair(nullptr, SubTree));

		while (!Stack.IsEmpty() && bContinue)
		{
			FStatePair Current = Stack[0];
			UMetaStoryState* ParentState = Current.Get<0>();
			UMetaStoryState* State = Current.Get<1>();
			check(State);

			Stack.RemoveAt(0);

			bContinue = InFunc(*State, ParentState) == EMetaStoryVisitor::Continue;
			
			if (bContinue)
			{
				// Children
				for (UMetaStoryState* ChildState : State->Children)
				{
					Stack.Add(FStatePair(State, ChildState));
				}
			}
		}
		
		if (!bContinue)
		{
			break;
		}
	}

	return EMetaStoryVisitor::Continue;
}

EMetaStoryVisitor UMetaStoryEditorData::VisitGlobalNodes(TFunctionRef<EMetaStoryVisitor(const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const
{
	// Root parameters
	{
		FMetaStoryBindableStructDesc Desc;
		Desc.StatePath = UE::MetaStory::Editor::GlobalStateName;
		Desc.Struct = GetRootParametersPropertyBag().GetPropertyBagStruct();
		Desc.Name = FName(TEXT("Parameters"));
		Desc.ID = RootParametersGuid;
		Desc.DataSource = EMetaStoryBindableStructSource::Parameter;
		
		if (InFunc(nullptr, Desc, FMetaStoryDataView(const_cast<FInstancedPropertyBag&>(GetRootParametersPropertyBag()).GetMutableValue())) == EMetaStoryVisitor::Break)
		{
			return EMetaStoryVisitor::Break;
		}
	}

	// All named external data items declared by the schema
	if (Schema != nullptr)
	{
		for (const FMetaStoryExternalDataDesc& ContextDesc : Schema->GetContextDataDescs())
		{
			if (ContextDesc.Struct)
			{
				FMetaStoryBindableStructDesc Desc;
				Desc.StatePath = UE::MetaStory::Editor::GlobalStateName;
				Desc.Struct = ContextDesc.Struct;
				Desc.Name = ContextDesc.Name;
				Desc.ID = ContextDesc.ID;
				Desc.DataSource = EMetaStoryBindableStructSource::Context;

				// We don't have value for the external objects, but return the type and null value so that users of GetAllStructValues() can use the type.
				if (InFunc(nullptr, Desc, FMetaStoryDataView(Desc.Struct, nullptr)) == EMetaStoryVisitor::Break)
				{
					return EMetaStoryVisitor::Break;
				}
			}
		}
	}

	auto VisitFuncNodesFunc = [&InFunc, this](const FMetaStoryEditorNode& Node)
	{
		const FString StatePath = FString::Printf(TEXT("%s/%s"), *UE::MetaStory::Editor::GlobalStateName, *Node.GetName().ToString());
		return VisitStructBoundPropertyFunctions(Node.ID, StatePath, [&InFunc](const FMetaStoryEditorNode& EditorNode, const FMetaStoryBindableStructDesc& Desc, FMetaStoryDataView Value)
		{
			return InFunc(nullptr, Desc, Value);
		});
	};

	// Evaluators
	for (const FMetaStoryEditorNode& Node : Evaluators)
	{
		if (VisitFuncNodesFunc(Node) == EMetaStoryVisitor::Break)
		{
			return EMetaStoryVisitor::Break;
		}

		if (const FMetaStoryEvaluatorBase* Evaluator = Node.Node.GetPtr<FMetaStoryEvaluatorBase>())
		{
			FMetaStoryBindableStructDesc Desc;
			Desc.StatePath = UE::MetaStory::Editor::GlobalStateName;
			Desc.Struct = Evaluator->GetInstanceDataType();
			Desc.Name = Node.GetName();
			Desc.ID = Node.ID;
			Desc.DataSource = EMetaStoryBindableStructSource::Evaluator;

			if (InFunc(nullptr, Desc, Node.GetInstance()) == EMetaStoryVisitor::Break)
			{
				return EMetaStoryVisitor::Break;
			}

			FMetaStoryBindableStructDesc NodeDesc;
			NodeDesc.StatePath = UE::MetaStory::Editor::GlobalStateName;
			NodeDesc.Struct = Node.Node.GetScriptStruct();
			NodeDesc.Name = Node.GetName();
			NodeDesc.ID = Node.GetNodeID();
			NodeDesc.DataSource = EMetaStoryBindableStructSource::Evaluator;

			if (InFunc(nullptr, NodeDesc, Node.GetNode()) == EMetaStoryVisitor::Break)
			{
				return EMetaStoryVisitor::Break;
			}
		}
	}

	// Global tasks
	for (const FMetaStoryEditorNode& Node : GlobalTasks)
	{
		if (VisitFuncNodesFunc(Node) == EMetaStoryVisitor::Break)
		{
			return EMetaStoryVisitor::Break;
		}

		if (const FMetaStoryTaskBase* Task = Node.Node.GetPtr<FMetaStoryTaskBase>())
		{
			FMetaStoryBindableStructDesc Desc;
			Desc.StatePath = UE::MetaStory::Editor::GlobalStateName;
			Desc.Struct = Task->GetInstanceDataType();
			Desc.Name = Node.GetName();
			Desc.ID = Node.ID;
			Desc.DataSource = EMetaStoryBindableStructSource::GlobalTask;

			if (InFunc(nullptr, Desc, Node.GetInstance()) == EMetaStoryVisitor::Break)
			{
				return EMetaStoryVisitor::Break;
			}

			FMetaStoryBindableStructDesc NodeDesc;
			NodeDesc.StatePath = UE::MetaStory::Editor::GlobalStateName;
			NodeDesc.Struct = Node.Node.GetScriptStruct();
			NodeDesc.Name = Node.GetName();
			NodeDesc.ID = Node.GetNodeID();
			NodeDesc.DataSource = EMetaStoryBindableStructSource::GlobalTask;

			if (InFunc(nullptr, NodeDesc, Node.GetNode()) == EMetaStoryVisitor::Break)
			{
				return EMetaStoryVisitor::Break;
			}
		}
	}

	return  EMetaStoryVisitor::Continue;
}

EMetaStoryVisitor UMetaStoryEditorData::VisitHierarchyNodes(TFunctionRef<EMetaStoryVisitor(const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const
{
	return VisitHierarchy([this, &InFunc](const UMetaStoryState& State, UMetaStoryState* /*ParentState*/)
	{
		return VisitStateNodes(State, InFunc);
	});
}

EMetaStoryVisitor UMetaStoryEditorData::VisitAllNodes(TFunctionRef<EMetaStoryVisitor(const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const
{
	if (VisitGlobalNodes(InFunc) == EMetaStoryVisitor::Break)
	{
		return EMetaStoryVisitor::Break;
	}

	if (VisitHierarchyNodes(InFunc) == EMetaStoryVisitor::Break)
	{
		return EMetaStoryVisitor::Break;
	}

	return EMetaStoryVisitor::Continue;
}

#if WITH_METASTORY_TRACE_DEBUGGER
bool UMetaStoryEditorData::HasAnyBreakpoint(const FGuid ID) const
{
	return Breakpoints.ContainsByPredicate([ID](const FMetaStoryEditorBreakpoint& Breakpoint) { return Breakpoint.ID == ID; });
}

bool UMetaStoryEditorData::HasBreakpoint(const FGuid ID, const EMetaStoryBreakpointType BreakpointType) const
{
	return GetBreakpoint(ID, BreakpointType) != nullptr;
}

const FMetaStoryEditorBreakpoint* UMetaStoryEditorData::GetBreakpoint(const FGuid ID, const EMetaStoryBreakpointType BreakpointType) const
{
	return Breakpoints.FindByPredicate([ID, BreakpointType](const FMetaStoryEditorBreakpoint& Breakpoint)
		{
			return Breakpoint.ID == ID && Breakpoint.BreakpointType == BreakpointType;
		});
}

void UMetaStoryEditorData::AddBreakpoint(const FGuid ID, const EMetaStoryBreakpointType BreakpointType)
{
	Breakpoints.Emplace(ID, BreakpointType);

	const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
	checkf(MetaStory, TEXT("UMetaStoryEditorData should only be allocated within a UMetaStory"));
	UE::MetaStory::Delegates::OnBreakpointsChanged.Broadcast(*MetaStory);
}

bool UMetaStoryEditorData::RemoveBreakpoint(const FGuid ID, const EMetaStoryBreakpointType BreakpointType)
{
	const int32 Index = Breakpoints.IndexOfByPredicate([ID, BreakpointType](const FMetaStoryEditorBreakpoint& Breakpoint)
		{
			return Breakpoint.ID == ID && Breakpoint.BreakpointType == BreakpointType;
		});
		
	if (Index != INDEX_NONE)
	{
		Breakpoints.RemoveAtSwap(Index);
		
		const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
		checkf(MetaStory, TEXT("UMetaStoryEditorData should only be allocated within a UMetaStory"));
		UE::MetaStory::Delegates::OnBreakpointsChanged.Broadcast(*MetaStory);
	}

	return Index != INDEX_NONE;
}

void UMetaStoryEditorData::RemoveAllBreakpoints()
{
	Breakpoints.Reset();

	const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
	checkf(MetaStory, TEXT("UMetaStoryEditorData should only be allocated within a UMetaStory"));
	UE::MetaStory::Delegates::OnBreakpointsChanged.Broadcast(*MetaStory);
}

#endif // WITH_METASTORY_TRACE_DEBUGGER

#undef LOCTEXT_NAMESPACE