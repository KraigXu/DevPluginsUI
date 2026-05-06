// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryState.h"
#include "MetaStory.h"
#include "MetaStoryCustomVersions.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryConsiderationBase.h"
#include "MetaStoryTaskBase.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryPropertyHelpers.h"
#include "Customizations/MetaStoryEditorNodeUtils.h"
#include "Flow/MetaStoryFlow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryState)

#if WITH_EDITOR
/** 将 MetaStoryFlowRoot 下影子 State 的显示名/描述写回内嵌 UMetaStoryFlow 节点，供流程图 SMetaStoryFlowGraph 与资产一致。 */
static void MetaStorySyncEmbeddedFlowNodeDisplayFromShadowState(UMetaStoryState& State)
{
	UMetaStoryState* ParentState = State.Parent;
	if (!ParentState || ParentState->Name != FName(TEXT("MetaStoryFlowRoot")))
	{
		return;
	}
	UMetaStoryEditorData* EditorData = State.GetTypedOuter<UMetaStoryEditorData>();
	if (!EditorData || !EditorData->MetaStoryFlow || !State.ID.IsValid())
	{
		return;
	}
	UMetaStoryFlow* Flow = EditorData->MetaStoryFlow;
	for (FMetaStoryFlowNode& Node : Flow->Nodes)
	{
		if (Node.NodeId != State.ID)
		{
			continue;
		}
		Flow->Modify();
		Node.NodeName = FText::FromName(State.Name);
		Node.Description = FText::FromString(State.Description);
		if (UMetaStory* MetaStory = EditorData->GetTypedOuter<UMetaStory>())
		{
			UE::MetaStory::Delegates::OnGlobalDataChanged.Broadcast(*MetaStory);
		}
		return;
	}
}
#endif


//////////////////////////////////////////////////////////////////////////
// FMetaStoryStateParameters

void FMetaStoryStateParameters::RemoveUnusedOverrides()
{
	// Remove overrides that do not exists anymore
	if (!PropertyOverrides.IsEmpty())
	{
		if (const UPropertyBag* Bag = Parameters.GetPropertyBagStruct())
		{
			for (TArray<FGuid>::TIterator It = PropertyOverrides.CreateIterator(); It; ++It)
			{
				if (!Bag->FindPropertyDescByID(*It))
				{
					It.RemoveCurrentSwap();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FMetaStoryTransition

FMetaStoryTransition::FMetaStoryTransition(const EMetaStoryTransitionTrigger InTrigger, const EMetaStoryTransitionType InType, const UMetaStoryState* InState)
	: Trigger(InTrigger)
{
	State = InState ? InState->GetLinkToState() : FMetaStoryStateLink(InType);
}

FMetaStoryTransition::FMetaStoryTransition(const EMetaStoryTransitionTrigger InTrigger, const FGameplayTag InEventTag, const EMetaStoryTransitionType InType, const UMetaStoryState* InState)
	: Trigger(InTrigger)
	, RequiredEvent{InEventTag}
{
	State = InState ? InState->GetLinkToState() : FMetaStoryStateLink(InType);
}

void FMetaStoryTransition::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (EventTag_DEPRECATED.IsValid())
	{
		RequiredEvent.Tag = EventTag_DEPRECATED;
		EventTag_DEPRECATED = FGameplayTag();
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
}

//////////////////////////////////////////////////////////////////////////
// UMetaStoryState

UMetaStoryState::UMetaStoryState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if(!IsTemplate())
	{
		ID = FGuid::NewGuid();
		Parameters.ID = FGuid::NewGuid();
	}
}

UMetaStoryState::~UMetaStoryState()
{
	UE::MetaStory::Delegates::OnPostCompile.RemoveAll(this);
}

void UMetaStoryState::PostInitProperties()
{
	Super::PostInitProperties();
	
	UE::MetaStory::Delegates::OnPostCompile.AddUObject(this, &UMetaStoryState::OnTreeCompiled);
}

void UMetaStoryState::OnTreeCompiled(const UMetaStory& MetaStory)
{
	if (&MetaStory == LinkedAsset)
	{
		UpdateParametersFromLinkedSubtree();
	}
}

void UMetaStoryState::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	const FMetaStoryEditPropertyPath PropertyChainPath(PropertyAboutToChange);

	static const FMetaStoryEditPropertyPath StateTypePath(UMetaStoryState::StaticClass(), TEXT("Type"));

	if (PropertyChainPath.IsPathExact(StateTypePath))
	{
		// If transitioning from linked state, reset the parameters
		if (Type == EMetaStoryStateType::Linked
			|| Type == EMetaStoryStateType::LinkedAsset)
		{
			Parameters.ResetParametersAndOverrides();
		}
	}
}

void UMetaStoryState::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FMetaStoryEditPropertyPath ChangePropertyPath(PropertyChangedEvent);

	static const FMetaStoryEditPropertyPath StateNamePath(UMetaStoryState::StaticClass(), TEXT("Name"));
	static const FMetaStoryEditPropertyPath StateDescriptionPath(UMetaStoryState::StaticClass(), TEXT("Description"));
	static const FMetaStoryEditPropertyPath StateTypePath(UMetaStoryState::StaticClass(), TEXT("Type"));
	static const FMetaStoryEditPropertyPath SelectionBehaviorPath(UMetaStoryState::StaticClass(), TEXT("SelectionBehavior"));
	static const FMetaStoryEditPropertyPath StateLinkedSubtreePath(UMetaStoryState::StaticClass(), TEXT("LinkedSubtree"));
	static const FMetaStoryEditPropertyPath StateLinkedAssetPath(UMetaStoryState::StaticClass(), TEXT("LinkedAsset"));
	static const FMetaStoryEditPropertyPath StateParametersPath(UMetaStoryState::StaticClass(), TEXT("Parameters"));
	static const FMetaStoryEditPropertyPath StateTasksPath(UMetaStoryState::StaticClass(), TEXT("Tasks"));
	static const FMetaStoryEditPropertyPath StateEnterConditionsPath(UMetaStoryState::StaticClass(), TEXT("EnterConditions"));
	static const FMetaStoryEditPropertyPath StateConsiderationsPath(UMetaStoryState::StaticClass(), TEXT("Considerations"));
	static const FMetaStoryEditPropertyPath StateTransitionsPath(UMetaStoryState::StaticClass(), TEXT("Transitions"));
	static const FMetaStoryEditPropertyPath StateTransitionsConditionsPath(UMetaStoryState::StaticClass(), TEXT("Transitions.Conditions"));
	static const FMetaStoryEditPropertyPath StateTransitionsIDPath(UMetaStoryState::StaticClass(), TEXT("Transitions.ID"));

#if WITH_EDITOR
	if (ChangePropertyPath.IsPathExact(StateNamePath) || ChangePropertyPath.IsPathExact(StateDescriptionPath))
	{
		MetaStorySyncEmbeddedFlowNodeDisplayFromShadowState(*this);
	}
#endif

	// Broadcast name changes so that the UI can update.
	if (ChangePropertyPath.IsPathExact(StateNamePath))
	{
		const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
		if (ensure(MetaStory))
		{
			UE::MetaStory::Delegates::OnIdentifierChanged.Broadcast(*MetaStory);
		}
	}

	if (ChangePropertyPath.IsPathExact(SelectionBehaviorPath))
	{
		// Broadcast selection type changes so that the UI can update.
		const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
		if (ensure(MetaStory))
		{
			UE::MetaStory::Delegates::OnIdentifierChanged.Broadcast(*MetaStory);
		}
	}
	
	if (ChangePropertyPath.IsPathExact(StateTypePath))
	{
		if (Type == EMetaStoryStateType::Group)
		{
			// Group should not have tasks.
			Tasks.Reset();
		}

		const bool bHasPredefinedSelectionBehavior = Type == EMetaStoryStateType::Linked || Type == EMetaStoryStateType::LinkedAsset;
		if (bHasPredefinedSelectionBehavior)
		{
			// Reset Selection Behavior back to Try Enter State for group and linked types
			SelectionBehavior = EMetaStoryStateSelectionBehavior::TryEnterState;
			// Remove any tasks when they are not used.
			Tasks.Reset();
		}

		// If transitioning from linked state, reset the linked state.
		if (Type != EMetaStoryStateType::Linked)
		{
			LinkedSubtree = FMetaStoryStateLink();
		}
		if (Type != EMetaStoryStateType::LinkedAsset)
		{
			LinkedAsset = nullptr;
		}

		if (Type == EMetaStoryStateType::Linked
			|| Type == EMetaStoryStateType::LinkedAsset)
		{
			// Linked parameter layout is fixed, and copied from the linked target state.
			Parameters.bFixedLayout = true;
			UpdateParametersFromLinkedSubtree();
		}
		else
		{
			// Other layouts can be edited
			Parameters.bFixedLayout = false;
		}
	}

	// When switching to new state, update the parameters.
	if (ChangePropertyPath.IsPathExact(StateLinkedSubtreePath))
	{
		if (Type == EMetaStoryStateType::Linked)
		{
			UpdateParametersFromLinkedSubtree();
		}
	}
	
	if (ChangePropertyPath.IsPathExact(StateLinkedAssetPath))
	{
		if (Type == EMetaStoryStateType::LinkedAsset)
		{
			UpdateParametersFromLinkedSubtree();
		}
	}

	// Broadcast subtree parameter layout edits so that the linked states can adapt, and bindings can update.
	if (ChangePropertyPath.IsPathExact(StateParametersPath))
	{
		const UMetaStory* MetaStory = GetTypedOuter<UMetaStory>();
		if (ensure(MetaStory))
		{
			UE::MetaStory::Delegates::OnStateParametersChanged.Broadcast(*MetaStory, ID);
		}
	}

	// Reset delay on completion transitions
	if (ChangePropertyPath.ContainsPath(StateTransitionsPath))
	{
		const int32 TransitionsIndex = ChangePropertyPath.GetPropertyArrayIndex(StateTransitionsPath);
		if (Transitions.IsValidIndex(TransitionsIndex))
		{
			FMetaStoryTransition& Transition = Transitions[TransitionsIndex];

			if (EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnStateCompleted))
			{
				Transition.bDelayTransition = false;
			}
		}
	}

	// Set default state to root and Id on new transitions.
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
	{
		if (ChangePropertyPath.IsPathExact(StateTransitionsPath))
		{
			const int32 TransitionsIndex = ChangePropertyPath.GetPropertyArrayIndex(StateTransitionsPath);
			if (Transitions.IsValidIndex(TransitionsIndex))
			{
				FMetaStoryTransition& Transition = Transitions[TransitionsIndex];
				Transition.Trigger = EMetaStoryTransitionTrigger::OnStateCompleted;
				const UMetaStoryState* RootState = GetRootState();
				Transition.State = RootState->GetLinkToState();
				Transition.ID = FGuid::NewGuid();
			}
		}
	}

	if (UMetaStoryEditorData* TreeData = GetTypedOuter<UMetaStoryEditorData>())
	{
		UE::MetaStory::PropertyHelpers::DispatchPostEditToNodes(*this, PropertyChangedEvent, *TreeData);
	}
}

void UMetaStoryState::PostLoad()
{
	Super::PostLoad();

	// Make sure state has transactional flags to make it work with undo (to fix a bug where root states were created without this flag).
	if (!HasAnyFlags(RF_Transactional))
	{
		SetFlags(RF_Transactional);
	}

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const int32 CurrentMetaStoryCustomVersion = UE::MetaStory::CustomVersions::GetEffectiveAssetLinkerVersion(this);
	constexpr int32 AddedTransitionIdsVersion = FMetaStoryCustomVersion::AddedTransitionIds;
	constexpr int32 OverridableStateParametersVersion = FMetaStoryCustomVersion::OverridableStateParameters;
	constexpr int32 AddedCheckingParentsPrerequisitesVersion = FMetaStoryCustomVersion::AddedCheckingParentsPrerequisites;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (CurrentMetaStoryCustomVersion < AddedTransitionIdsVersion)
	{
		// Make guids for transitions. These need to be deterministic when upgrading because of cooking.
		for (int32 Index = 0; Index < Transitions.Num(); Index++)
		{
			FMetaStoryTransition& Transition = Transitions[Index];
			Transition.ID = FGuid::NewDeterministicGuid(GetPathName(), Index);
		}
	}

	if (CurrentMetaStoryCustomVersion < OverridableStateParametersVersion)
	{
		// In earlier versions, all parameters were overwritten.
		if (const UPropertyBag* Bag = Parameters.Parameters.GetPropertyBagStruct())
		{
			for (const FPropertyBagPropertyDesc& Desc : Bag->GetPropertyDescs())
			{
				Parameters.PropertyOverrides.Add(Desc.ID);
			}
		}
	}

	if (CurrentMetaStoryCustomVersion < AddedCheckingParentsPrerequisitesVersion)
	{
		bCheckPrerequisitesWhenActivatingChildDirectly = false;
	}
	
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	for (FMetaStoryEditorNode& EnterConditionEditorNode : EnterConditions)
	{
		if (FMetaStoryNodeBase* ConditionNode = EnterConditionEditorNode.Node.GetMutablePtr<FMetaStoryNodeBase>())
		{
			UE::MetaStoryEditor::EditorNodeUtils::ConditionalUpdateNodeInstanceData(EnterConditionEditorNode, *this);
			ConditionNode->PostLoad(EnterConditionEditorNode.GetInstance());
		}
	}

	for (FMetaStoryEditorNode& ConsiderationEditorNode : Considerations)
	{
		if (FMetaStoryNodeBase* ConsiderationNode = ConsiderationEditorNode.Node.GetMutablePtr<FMetaStoryNodeBase>())
		{
			UE::MetaStoryEditor::EditorNodeUtils::ConditionalUpdateNodeInstanceData(ConsiderationEditorNode, *this);
			ConsiderationNode->PostLoad(ConsiderationEditorNode.GetInstance());
		}
	}

	for (FMetaStoryEditorNode& TaskEditorNode : Tasks)
	{
		if (FMetaStoryNodeBase* TaskNode = TaskEditorNode.Node.GetMutablePtr<FMetaStoryNodeBase>())
		{
			UE::MetaStoryEditor::EditorNodeUtils::ConditionalUpdateNodeInstanceData(TaskEditorNode, *this);
			TaskNode->PostLoad(TaskEditorNode.GetInstance());
		}
	}

	if (FMetaStoryNodeBase* SingleTaskNode = SingleTask.Node.GetMutablePtr<FMetaStoryNodeBase>())
	{
		UE::MetaStoryEditor::EditorNodeUtils::ConditionalUpdateNodeInstanceData(SingleTask, *this);
		SingleTaskNode->PostLoad(SingleTask.GetInstance());
	}

	for (FMetaStoryTransition& Transition : Transitions)
	{
		for (FMetaStoryEditorNode& TransitionConditionEditorNode : Transition.Conditions)
		{
			if (FMetaStoryNodeBase* ConditionNode = TransitionConditionEditorNode.Node.GetMutablePtr<FMetaStoryNodeBase>())
			{
				UE::MetaStoryEditor::EditorNodeUtils::ConditionalUpdateNodeInstanceData(TransitionConditionEditorNode, *this);
				ConditionNode->PostLoad(TransitionConditionEditorNode.GetInstance());
			}
		}
	}
#endif // WITH_EDITOR
}

void UMetaStoryState::UpdateParametersFromLinkedSubtree()
{
	if (const FInstancedPropertyBag* DefaultParameters = GetDefaultParameters())
	{
		Parameters.Parameters.MigrateToNewBagInstanceWithOverrides(*DefaultParameters, Parameters.PropertyOverrides);
		Parameters.RemoveUnusedOverrides();
	}
	else
	{
		Parameters.ResetParametersAndOverrides();
	}
}

void UMetaStoryState::SetParametersPropertyOverridden(const FGuid PropertyID, const bool bIsOverridden)
{
	if (bIsOverridden)
	{
		Parameters.PropertyOverrides.AddUnique(PropertyID);
	}
	else
	{
		Parameters.PropertyOverrides.Remove(PropertyID);
		UpdateParametersFromLinkedSubtree();

		// Remove binding when override is removed.
		if (UMetaStoryEditorData* EditorData = GetTypedOuter<UMetaStoryEditorData>())
		{
			if (FMetaStoryEditorPropertyBindings* Bindings = EditorData->GetPropertyEditorBindings())
			{
				if (const UPropertyBag* ParametersBag = Parameters.Parameters.GetPropertyBagStruct())
				{
					if (const FPropertyBagPropertyDesc* Desc = ParametersBag->FindPropertyDescByID(PropertyID))
					{
						check(Desc->CachedProperty);

						EditorData->Modify();

						FPropertyBindingPath Path(Parameters.ID, Desc->CachedProperty->GetFName());
						Bindings->RemoveBindings(Path);
					}
				}
			}
		}
	}
}

const FInstancedPropertyBag* UMetaStoryState::GetDefaultParameters() const
{
	if (Type == EMetaStoryStateType::Linked)
	{
		if (const UMetaStoryEditorData* TreeData = GetTypedOuter<UMetaStoryEditorData>())
		{
			if (const UMetaStoryState* LinkTargetState = TreeData->GetStateByID(LinkedSubtree.ID))
			{
				return &LinkTargetState->Parameters.Parameters;
			}
		}
	}
	else if (Type == EMetaStoryStateType::LinkedAsset)
	{
		if (LinkedAsset)
		{
			return &LinkedAsset->GetDefaultParameters();
		}
	}

	return nullptr;
}

const UMetaStoryState* UMetaStoryState::GetRootState() const
{
	const UMetaStoryState* RootState = this;
	while (RootState->Parent != nullptr)
	{
		RootState = RootState->Parent;
	}
	return RootState;
}

const UMetaStoryState* UMetaStoryState::GetNextSiblingState() const
{
	if (!Parent)
	{
		return nullptr;
	}
	for (int32 ChildIdx = 0; ChildIdx < Parent->Children.Num(); ChildIdx++)
	{
		if (Parent->Children[ChildIdx] == this)
		{
			const int NextIdx = ChildIdx + 1;

			// Select the next enabled sibling
			if (NextIdx < Parent->Children.Num() && Parent->Children[NextIdx]->bEnabled)
			{
				return Parent->Children[NextIdx];
			}
			break;
		}
	}
	return nullptr;
}

const UMetaStoryState* UMetaStoryState::GetNextSelectableSiblingState() const
{
	if (!Parent)
	{
		return nullptr;
	}

	const int32 StartChildIndex = Parent->Children.IndexOfByKey(this);
	if (StartChildIndex == INDEX_NONE)
	{
		return nullptr;
	}
	
	for (int32 ChildIdx = StartChildIndex + 1; ChildIdx < Parent->Children.Num(); ChildIdx++)
	{
		// Select the next enabled and selectable sibling
		const UMetaStoryState* State =Parent->Children[ChildIdx];
		if (State->SelectionBehavior != EMetaStoryStateSelectionBehavior::None
			&& State->bEnabled)
		{
			return State;
		}
	}
	
	return nullptr;
}

FString UMetaStoryState::GetPath() const
{
	TArray<const UMetaStoryState*> States;
	for (const UMetaStoryState* CurrState = this; CurrState; CurrState = CurrState->Parent)
	{
		States.Add(CurrState);
	}
	Algo::Reverse(States);
	
	FStringBuilderBase Result;
	for (const UMetaStoryState* CurrState : States)
	{
		if (Result.Len() > 0)
		{
			Result.Append(TEXT("/"));
		}
		Result.Append(CurrState->Name.ToString());
	}

	return Result.ToString();
}

FMetaStoryStateLink UMetaStoryState::GetLinkToState() const
{
	FMetaStoryStateLink Link(EMetaStoryTransitionType::GotoState);
	Link.Name = Name;
	Link.ID = ID;
	return Link;
}

TSubclassOf<UMetaStorySchema> UMetaStoryState::GetSchema() const
{
	if (const UMetaStoryEditorData* EditorData = GetTypedOuter<UMetaStoryEditorData>())
	{
		if (EditorData->Schema)
		{
			return EditorData->Schema->GetClass();
		}
	}
	return nullptr;
}

void UMetaStoryState::SetLinkedState(FMetaStoryStateLink InStateLink)
{
	check(Type == EMetaStoryStateType::Linked);
	LinkedSubtree = InStateLink;

	Tasks.Reset();
	LinkedAsset = nullptr;
	Parameters.bFixedLayout = true;
	UpdateParametersFromLinkedSubtree();
	SelectionBehavior = EMetaStoryStateSelectionBehavior::TryEnterState;
}

void UMetaStoryState::SetLinkedStateAsset(UMetaStory* InLinkedAsset)
{
	check(Type == EMetaStoryStateType::LinkedAsset);
	LinkedAsset = InLinkedAsset;

	Tasks.Reset();
	LinkedSubtree = FMetaStoryStateLink();
	Parameters.bFixedLayout = true;
	UpdateParametersFromLinkedSubtree();
	SelectionBehavior = EMetaStoryStateSelectionBehavior::TryEnterState;
}