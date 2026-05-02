// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryViewModel.h"

#include "MetaStory.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryDelegates.h"
#include "Debugger/MetaStoryDebugger.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "HAL/PlatformApplicationMisc.h"
#include "UnrealExporter.h"
#include "Exporters/Exporter.h"
#include "Factories.h"
#include "MetaStoryTaskBase.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryConsiderationBase.h"
#include "MetaStoryPropertyHelpers.h"
#include "MetaStoryEditorDataClipboardHelpers.h"
#include "Misc/NotNull.h"
#include "Misc/StringOutputDevice.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryViewModel)

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

namespace UE::MetaStory::Editor
{
	class FMetaStoryStateTextFactory : public FCustomizableTextObjectFactory
	{
	public:
		FMetaStoryStateTextFactory()
			: FCustomizableTextObjectFactory(GWarn)
		{}

		virtual bool CanCreateClass(UClass* InObjectClass, bool& bOmitSubObjs) const override
		{
			return InObjectClass->IsChildOf(UMetaStoryState::StaticClass())
				|| InObjectClass->IsChildOf(UMetaStoryClipboardBindings::StaticClass());
		}

		virtual void ProcessConstructedObject(UObject* NewObject) override
		{
			if (UMetaStoryState* State = Cast<UMetaStoryState>(NewObject))
			{
				States.Add(State);
			}
			else if (UMetaStoryClipboardBindings* Bindings = Cast<UMetaStoryClipboardBindings>(NewObject))
			{
				ClipboardBindings = Bindings;
			}
		}

	public:
		TArray<UMetaStoryState*> States;
		UMetaStoryClipboardBindings* ClipboardBindings = nullptr;
	};


	void CollectBindingsCopiesRecursive(UMetaStoryEditorData* TreeData, UMetaStoryState* State, TArray<FMetaStoryPropertyPathBinding>& AllBindings)
	{
		if (!State)
		{
			return;
		}
		
		TreeData->VisitStateNodes(*State, [TreeData, &AllBindings](const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
		{
			TArray<const FPropertyBindingBinding*> NodeBindings;
			TreeData->GetPropertyEditorBindings()->FPropertyBindingBindingCollection::GetBindingsFor(Desc.ID, NodeBindings);
			Algo::Transform(NodeBindings, AllBindings, [](const FPropertyBindingBinding* BindingPtr) { return *static_cast<const FMetaStoryPropertyPathBinding*>(BindingPtr); });
			return EMetaStoryVisitor::Continue;
		});

		for (UMetaStoryState* ChildState : State->Children)
		{
			CollectBindingsCopiesRecursive(TreeData, ChildState, AllBindings);
		}
	}

	FString ExportStatesToText(UMetaStoryEditorData* TreeData, const TArrayView<UMetaStoryState*> States)
	{
		if (States.IsEmpty())
		{
			return FString();
		}

		// Clear the mark state for saving.
		UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

		FStringOutputDevice Archive;
		const FExportObjectInnerContext Context;

		UMetaStoryClipboardBindings* ClipboardBindings = NewObject<UMetaStoryClipboardBindings>();
		check(ClipboardBindings);

		for (UMetaStoryState* State : States)
		{
			UObject* ThisOuter = State->GetOuter();
			UExporter::ExportToOutputDevice(&Context, State, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, ThisOuter);

			CollectBindingsCopiesRecursive(TreeData, State, ClipboardBindings->Bindings);
		}

		UExporter::ExportToOutputDevice(&Context, ClipboardBindings, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false);

		return *Archive;
	}

	void CollectStateLinks(const UStruct* Struct, void* Memory, TArray<FMetaStoryStateLink*>& Links)
	{
		for (TPropertyValueIterator<FStructProperty> It(Struct, Memory); It; ++It)
		{
			if (It->Key->Struct == TBaseStructure<FMetaStoryStateLink>::Get())
			{
				FMetaStoryStateLink* StateLink = static_cast<FMetaStoryStateLink*>(const_cast<void*>(It->Value));
				Links.Add(StateLink);
			}
		}
	}

	// todo: Should refactor it into FMetaStoryScopedEditorDataFixer
	void FixNodesAfterDuplication(TArrayView<FMetaStoryEditorNode> Nodes, TMap<FGuid, FGuid>& IDsMap, TArray<FMetaStoryStateLink*>& Links)
	{
		for (FMetaStoryEditorNode& Node : Nodes)
		{
			const FGuid NewNodeID = FGuid::NewGuid();
			IDsMap.Emplace(Node.ID, NewNodeID);
			Node.ID = NewNodeID;

			if (Node.Node.IsValid())
			{
				CollectStateLinks(Node.Node.GetScriptStruct(), Node.Node.GetMutableMemory(), Links);
			}
			if (Node.Instance.IsValid())
			{
				CollectStateLinks(Node.Instance.GetScriptStruct(), Node.Instance.GetMutableMemory(), Links);
			}
			if (Node.InstanceObject)
			{
				CollectStateLinks(Node.InstanceObject->GetClass(), Node.InstanceObject, Links);
			}
			if (Node.ExecutionRuntimeData.IsValid())
			{
				CollectStateLinks(Node.ExecutionRuntimeData.GetScriptStruct(), Node.ExecutionRuntimeData.GetMutableMemory(), Links);
			}
			if (Node.ExecutionRuntimeDataObject)
			{
				CollectStateLinks(Node.ExecutionRuntimeDataObject->GetClass(), Node.ExecutionRuntimeDataObject, Links);
			}
		}
	}

	// todo: Should refactor it into FMetaStoryScopedEditorDataFixer
	void FixStateAfterDuplication(UMetaStoryState* State, UMetaStoryState* NewParentState, TMap<FGuid, FGuid>& IDsMap, TArray<FMetaStoryStateLink*>& Links, TArray<UMetaStoryState*>& NewStates)
	{
		State->Modify();

		const FGuid NewStateID = FGuid::NewGuid();
		IDsMap.Emplace(State->ID, NewStateID);
		State->ID = NewStateID;
		
		const FGuid NewParametersID = FGuid::NewGuid();
		IDsMap.Emplace(State->Parameters.ID, NewParametersID);
		State->Parameters.ID = NewParametersID;
		
		State->Parent = NewParentState;
		NewStates.Add(State);
		
		if (State->Type == EMetaStoryStateType::Linked)
		{
			Links.Emplace(&State->LinkedSubtree);
		}

		FixNodesAfterDuplication(TArrayView<FMetaStoryEditorNode>(&State->SingleTask, 1), IDsMap, Links);
		FixNodesAfterDuplication(State->Tasks, IDsMap, Links);
		FixNodesAfterDuplication(State->EnterConditions, IDsMap, Links);
		FixNodesAfterDuplication(State->Considerations, IDsMap, Links);

		for (FMetaStoryTransition& Transition : State->Transitions)
		{
			// Transition Ids are not used by nodes so no need to add to 'IDsMap'
			Transition.ID = FGuid::NewGuid();

			FixNodesAfterDuplication(Transition.Conditions, IDsMap, Links);
			Links.Emplace(&Transition.State);
		}

		for (UMetaStoryState* Child : State->Children)
		{
			FixStateAfterDuplication(Child, State, IDsMap, Links, NewStates);
		}
	}

	// Removes states from the array which are children of any other state.
	void RemoveContainedChildren(TArray<UMetaStoryState*>& States)
	{
		TSet<UMetaStoryState*> UniqueStates;
		for (UMetaStoryState* State : States)
		{
			UniqueStates.Add(State);
		}

		for (int32 i = 0; i < States.Num(); )
		{
			UMetaStoryState* State = States[i];

			// Walk up the parent state sand if the current state
			// exists in any of them, remove it.
			UMetaStoryState* StateParent = State->Parent;
			bool bShouldRemove = false;
			while (StateParent)
			{
				if (UniqueStates.Contains(StateParent))
				{
					bShouldRemove = true;
					break;
				}
				StateParent = StateParent->Parent;
			}

			if (bShouldRemove)
			{
				States.RemoveAt(i);
			}
			else
			{
				i++;
			}
		}
	}

	// Returns true if the state is child of parent state.
	bool IsChildOf(const UMetaStoryState* ParentState, const UMetaStoryState* State)
	{
		for (const UMetaStoryState* Child : ParentState->Children)
		{
			if (Child == State)
			{
				return true;
			}
			if (IsChildOf(Child, State))
			{
				return true;
			}
		}
		return false;
	}

namespace Private
{
	/* Short-lived helper struct for node manipulation in the editor */
	struct FMetaStoryStateNodeEditorHandle
	{
		const TNotNull<UMetaStoryEditorData*> EditorData;
		const TNotNull<UMetaStoryState*> OwnerState;

	private:
		FStringView NodePath;
		int32 ArrayIndex = INDEX_NONE;
		void* TargetArray = nullptr;
		void* TargetNode = nullptr;
		bool bIsTransition = false;

	public:
		FMetaStoryStateNodeEditorHandle(const TNotNull<UMetaStoryEditorData*> InEditorData, const TNotNull<UMetaStoryState*> InOwnerState, const FGuid& NodeID)
			: EditorData(InEditorData),
			OwnerState(InOwnerState)
		{
			auto FindNode = [Self = this, &NodeID]<typename T>(T& Nodes, FStringView Path)
			{
				for (int32 Index = 0; Index < Nodes.Num(); ++Index)
				{
					if (NodeID == Nodes[Index].ID)
					{
						Self->ArrayIndex = Index;
						Self->TargetArray = &Nodes;
						Self->TargetNode = &Nodes[Self->ArrayIndex];
						Self->NodePath = Path;

						return true;
					}
				}

				return false;
			};

			if (!NodeID.IsValid())
			{
				return;
			}

			if (FindNode(OwnerState->EnterConditions, TEXT("EnterConditions")))
			{
				return;
			}

			if (FindNode(OwnerState->Tasks, TEXT("Tasks")))
			{
				return;
			}

			if (NodeID == OwnerState->SingleTask.ID)
			{
				NodePath = TEXT("SingleTask");
				TargetNode = &OwnerState->SingleTask;
				return;
			}

			if (FindNode(OwnerState->Considerations, TEXT("Considerations")))
			{
				return;
			}

			if (FindNode(OwnerState->Transitions, TEXT("Transitions")))
			{
				bIsTransition = true;
				return;
			}
		}

		bool IsValid() const
		{
			return TargetNode != nullptr;
		}

		bool IsTransition() const
		{
			return bIsTransition;
		}

		FMetaStoryEditorNode& GetEditorNode() const
		{
			check(IsValid() && !IsTransition());

			return *static_cast<FMetaStoryEditorNode*>(TargetNode);
		}

		TArray<FMetaStoryEditorNode>& GetEditorNodeArray() const
		{
			check(IsValid() && !IsTransition() && GetNodeIndex() != INDEX_NONE);

			return *static_cast<TArray<FMetaStoryEditorNode>*>(TargetArray);
		}

		FMetaStoryTransition& GetTransition() const
		{
			check(IsValid() && IsTransition());

			return *static_cast<FMetaStoryTransition*>(TargetNode);
		}

		TArray<FMetaStoryTransition>& GetTransitionArray() const
		{
			check(IsValid() && IsTransition() && GetNodeIndex() != INDEX_NONE);

			return *static_cast<TArray<FMetaStoryTransition>*>(TargetArray);
		}

		FStringView GetNodePath() const
		{
			return NodePath;
		}

		int32 GetNodeIndex() const
		{
			return ArrayIndex;
		}
	};
}

};

FMetaStoryViewModel::FMetaStoryViewModel()
	: TreeDataWeak(nullptr)
#if WITH_METASTORY_TRACE_DEBUGGER
	, Debugger(MakeShareable(new FMetaStoryDebugger))
#endif // WITH_METASTORY_TRACE_DEBUGGER
{
}

FMetaStoryViewModel::~FMetaStoryViewModel()
{
	if (GEditor)
	{
	    GEditor->UnregisterForUndo(this);
	}

	UE::MetaStory::Delegates::OnIdentifierChanged.RemoveAll(this);
}

void FMetaStoryViewModel::Init(UMetaStoryEditorData* InTreeData)
{
	TreeDataWeak = InTreeData;

	GEditor->RegisterForUndo(this);

	UE::MetaStory::Delegates::OnIdentifierChanged.AddSP(this, &FMetaStoryViewModel::HandleIdentifierChanged);
	
#if WITH_METASTORY_TRACE_DEBUGGER
	UE::MetaStory::Delegates::OnBreakpointsChanged.AddSP(this, &FMetaStoryViewModel::HandleBreakpointsChanged);
	UE::MetaStory::Delegates::OnPostCompile.AddSP(this, &FMetaStoryViewModel::HandlePostCompile);

	Debugger->SetAsset(GetStateTree());
	BindToDebuggerDelegates();
	RefreshDebuggerBreakpoints();
#endif // WITH_METASTORY_TRACE_DEBUGGER
}

const UMetaStory* FMetaStoryViewModel::GetStateTree() const
{
	if (const UMetaStoryEditorData* TreeData = TreeDataWeak.Get())
	{
		return TreeData->GetTypedOuter<UMetaStory>();
	}

	return nullptr;
}

const UMetaStoryEditorData* FMetaStoryViewModel::GetStateTreeEditorData() const
{
	return TreeDataWeak.Get();
}

const UMetaStoryState* FMetaStoryViewModel::GetStateByID(const FGuid StateID) const
{
	if (const UMetaStoryEditorData* TreeData = TreeDataWeak.Get())
	{
		return const_cast<UMetaStoryState*>(TreeData->GetStateByID(StateID));
	}
	return nullptr;
}

UMetaStoryState* FMetaStoryViewModel::GetMutableStateByID(const FGuid StateID) const
{
	if (UMetaStoryEditorData* TreeData = TreeDataWeak.Get())
	{
		return TreeData->GetMutableStateByID(StateID);
	}
	return nullptr;
}

void FMetaStoryViewModel::HandleIdentifierChanged(const UMetaStory& MetaStory) const
{
	if (GetStateTree() == &MetaStory)
	{
		OnAssetChanged.Broadcast();
	}
}

#if WITH_METASTORY_TRACE_DEBUGGER

void FMetaStoryViewModel::ToggleStateBreakpoints(TConstArrayView<TWeakObjectPtr<>> States, EMetaStoryBreakpointType Type)
{
	UMetaStoryEditorData* EditorData = TreeDataWeak.Get();
	if (EditorData == nullptr || States.IsEmpty())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ToggleStateBreakpoint", "Toggle State Breakpoint"));
	EditorData->Modify(/*bAlwaysMarkDirty*/false);

	for (const TWeakObjectPtr<>& WeakStateObject : States)
	{
		if (const UMetaStoryState* State = static_cast<const UMetaStoryState*>(WeakStateObject.Get()))
		{
			const bool bRemoved = EditorData->RemoveBreakpoint(State->ID, Type);
			if (bRemoved == false)
			{
				EditorData->AddBreakpoint(State->ID, Type);
			}
		}
	}
}

void FMetaStoryViewModel::ToggleTaskBreakpoint(const FGuid ID, const EMetaStoryBreakpointType Type)
{
	if (UMetaStoryEditorData* EditorData = TreeDataWeak.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("ToggleTaskBreakpoint", "Toggle Task Breakpoint"));
		EditorData->Modify(/*bAlwaysMarkDirty*/false);

		const bool bRemoved = EditorData->RemoveBreakpoint(ID, Type);
		if (bRemoved == false)
		{
			EditorData->AddBreakpoint(ID, Type);
		}
	}
}

void FMetaStoryViewModel::ToggleTransitionBreakpoint(const TConstArrayView<TNotNull<const FMetaStoryTransition*>> Transitions, const ECheckBoxState ToggledState)
{
	UMetaStoryEditorData* EditorData = TreeDataWeak.Get();
	if (EditorData == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("TransitionBreakpointToggled", "Transition Breakpoint Toggled"));
	EditorData->Modify(/*bAlwaysMarkDirty*/false);

	for (const FMetaStoryTransition* Transition : Transitions)
	{
		const bool bHasBreakpoint = EditorData->HasBreakpoint(Transition->ID, EMetaStoryBreakpointType::OnTransition);
		if (ToggledState == ECheckBoxState::Checked && bHasBreakpoint == false)
		{
			EditorData->AddBreakpoint(Transition->ID, EMetaStoryBreakpointType::OnTransition);
		}
		else if (ToggledState == ECheckBoxState::Unchecked && bHasBreakpoint)
		{
			EditorData->RemoveBreakpoint(Transition->ID, EMetaStoryBreakpointType::OnTransition);
		}
	}
}

bool FMetaStoryViewModel::HasBreakpoint(const FGuid ID, const EMetaStoryBreakpointType Type)
{
	if (UMetaStoryEditorData* TreeData = TreeDataWeak.Get())
	{
		return TreeData->HasBreakpoint(ID, Type);
	}

	return false;
}

bool FMetaStoryViewModel::CanProcessBreakpoints() const
{
	return Debugger.Get().CanProcessBreakpoints();
}

bool FMetaStoryViewModel::CanAddStateBreakpoint(const EMetaStoryBreakpointType Type) const
{
	if (!CanProcessBreakpoints())
	{
		return false;
	}

	const UMetaStoryEditorData* EditorData = TreeDataWeak.Get();
	if (!ensure(EditorData != nullptr))
	{
		return false;
	}

	for (const TWeakObjectPtr<UMetaStoryState>& WeakState : SelectedStates)
	{
		if (const UMetaStoryState* State = WeakState.Get())
		{
			if (EditorData->HasBreakpoint(State->ID, Type) == false)
			{
				return true;
			}
		}
	}

	return false;
}

bool FMetaStoryViewModel::CanRemoveStateBreakpoint(const EMetaStoryBreakpointType Type) const
{
	const UMetaStoryEditorData* EditorData = TreeDataWeak.Get();
	if (!ensure(EditorData != nullptr))
	{
		return false;
	}

	for (const TWeakObjectPtr<UMetaStoryState>& WeakState : SelectedStates)
	{
		if (const UMetaStoryState* State = WeakState.Get())
		{
			if (EditorData->HasBreakpoint(State->ID, Type))
			{
				return true;
			}
		}
	}

	return false;
}


ECheckBoxState FMetaStoryViewModel::GetStateBreakpointCheckState(const EMetaStoryBreakpointType Type) const
{
	const bool bCanAdd = CanAddStateBreakpoint(Type);
	const bool bCanRemove = CanRemoveStateBreakpoint(Type);
	if (bCanAdd && bCanRemove)
	{
		return ECheckBoxState::Undetermined;
	}

	if (bCanRemove)
	{
		return ECheckBoxState::Checked;
	}

	if (bCanAdd)
	{
		return ECheckBoxState::Unchecked;
	}

	// Should not happen since action is not visible in this case
	return ECheckBoxState::Undetermined;
}

void FMetaStoryViewModel::HandleEnableStateBreakpoint(EMetaStoryBreakpointType Type)
{
	TArray<UMetaStoryState*> ValidatedSelectedStates;
	GetSelectedStates(ValidatedSelectedStates);
	if (ValidatedSelectedStates.IsEmpty())
	{
		return;
	}

	UMetaStoryEditorData* EditorData = TreeDataWeak.Get();
	if (!ensure(EditorData != nullptr))
	{
		return;
	}

	TBitArray<> HasBreakpoint;
	HasBreakpoint.Reserve(ValidatedSelectedStates.Num());
	for (const UMetaStoryState* SelectedState : ValidatedSelectedStates)
	{
		HasBreakpoint.Add(SelectedState != nullptr && EditorData->HasBreakpoint(SelectedState->ID, Type));
	}

	check(HasBreakpoint.Num() == ValidatedSelectedStates.Num());

	// Process CanAdd first so in case of undetermined state (mixed selection) we add by default. 
	if (CanAddStateBreakpoint(Type))
	{
		const FScopedTransaction Transaction(LOCTEXT("AddStateBreakpoint", "Add State Breakpoint(s)"));
		EditorData->Modify();
		for (int Index = 0; Index < ValidatedSelectedStates.Num(); ++Index)
		{
			const UMetaStoryState* SelectedState = ValidatedSelectedStates[Index];
			if (HasBreakpoint[Index] == false && SelectedState != nullptr)
			{
				EditorData->AddBreakpoint(SelectedState->ID, Type);	
			}
		}
	}
	else if (CanRemoveStateBreakpoint(Type))
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveStateBreakpoint", "Remove State Breakpoint(s)"));
		EditorData->Modify();
		for (int Index = 0; Index < ValidatedSelectedStates.Num(); ++Index)
		{
			const UMetaStoryState* SelectedState = ValidatedSelectedStates[Index];
			if (HasBreakpoint[Index] && SelectedState != nullptr)
			{
				EditorData->RemoveBreakpoint(SelectedState->ID, Type);	
			}
		}
	}
}

UMetaStoryState* FMetaStoryViewModel::FindStateAssociatedToBreakpoint(FMetaStoryDebuggerBreakpoint Breakpoint) const
{
	UMetaStoryEditorData* EditorData = TreeDataWeak.Get();
	if (EditorData == nullptr)
	{
		return nullptr;
	}
	const UMetaStory* MetaStory = GetStateTree();
	if (MetaStory == nullptr)
	{
		return nullptr;
	}

	UMetaStoryState* MetaStoryState = nullptr;

	if (const FMetaStoryStateHandle* StateHandle = Breakpoint.ElementIdentifier.TryGet<FMetaStoryStateHandle>())
	{
		const FGuid StateId = MetaStory->GetStateIdFromHandle(*StateHandle);
		MetaStoryState = EditorData->GetMutableStateByID(StateId);
	}
	else if (const FMetaStoryDebuggerBreakpoint::FMetaStoryTaskIndex* TaskIndex = Breakpoint.ElementIdentifier.TryGet<FMetaStoryDebuggerBreakpoint::FMetaStoryTaskIndex>())
	{
		const FGuid TaskId = MetaStory->GetNodeIdFromIndex(TaskIndex->Index);

		EditorData->VisitHierarchy([&TaskId, &MetaStoryState](UMetaStoryState& State, UMetaStoryState* /*ParentState*/)
			{
				for (const FMetaStoryEditorNode& EditorNode : State.Tasks)
				{
					if (EditorNode.ID == TaskId)
					{
						MetaStoryState = &State;
						return EMetaStoryVisitor::Break;
					}
				}
				return EMetaStoryVisitor::Continue;
			});
	}
	else if (const FMetaStoryDebuggerBreakpoint::FMetaStoryTransitionIndex* TransitionIndex = Breakpoint.ElementIdentifier.TryGet<FMetaStoryDebuggerBreakpoint::FMetaStoryTransitionIndex>())
	{
		const FGuid TransitionId = MetaStory->GetTransitionIdFromIndex(TransitionIndex->Index);

		EditorData->VisitHierarchy([&TransitionId, &MetaStoryState](UMetaStoryState& State, UMetaStoryState* /*ParentState*/)
			{
				for (const FMetaStoryTransition& StateTransition : State.Transitions)
				{
					if (StateTransition.ID == TransitionId)
					{
						MetaStoryState = &State;
						return EMetaStoryVisitor::Break;
					}
				}
				return EMetaStoryVisitor::Continue;
			});
	}

	return MetaStoryState;
}

void FMetaStoryViewModel::HandleBreakpointsChanged(const UMetaStory& MetaStory)
{
	if (GetStateTree() == &MetaStory)
	{
		RefreshDebuggerBreakpoints();
	}
}

void FMetaStoryViewModel::HandlePostCompile(const UMetaStory& MetaStory)
{
	if (GetStateTree() == &MetaStory)
	{
		RefreshDebuggerBreakpoints();
	}
}

void FMetaStoryViewModel::RemoveAllBreakpoints()
{
	if (UMetaStoryEditorData* TreeData = TreeDataWeak.Get())
	{
		Debugger->ClearAllBreakpoints();

		TreeData->RemoveAllBreakpoints();
	}
}

void FMetaStoryViewModel::RefreshDebuggerBreakpoints()
{
	const UMetaStory* MetaStory = GetStateTree();
	const UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (MetaStory != nullptr && TreeData != nullptr)
	{
		Debugger->ClearAllBreakpoints();

		for (const FMetaStoryEditorBreakpoint& Breakpoint : TreeData->Breakpoints)
		{
			// Test if the ID is associated to a task
			const FMetaStoryIndex16 Index = MetaStory->GetNodeIndexFromId(Breakpoint.ID);
			if (Index.IsValid())
			{
				Debugger->SetTaskBreakpoint(Index, Breakpoint.BreakpointType);
			}
			else
			{
				// Then test if the ID is associated to a State
				FMetaStoryStateHandle StateHandle = MetaStory->GetStateHandleFromId(Breakpoint.ID);
				if (StateHandle.IsValid())
				{
					Debugger->SetStateBreakpoint(StateHandle, Breakpoint.BreakpointType);
				}
				else
				{
					// Then test if the ID is associated to a transition
					const FMetaStoryIndex16 TransitionIndex = MetaStory->GetTransitionIndexFromId(Breakpoint.ID);
					if (TransitionIndex.IsValid())
					{
						Debugger->SetTransitionBreakpoint(TransitionIndex, Breakpoint.BreakpointType);
					}
				}
			}
		}
	}
}

#endif // WITH_METASTORY_TRACE_DEBUGGER

void FMetaStoryViewModel::NotifyAssetChangedExternally() const
{
	OnAssetChanged.Broadcast();
}

void FMetaStoryViewModel::NotifyStatesChangedExternally(const TSet<UMetaStoryState*>& ChangedStates, const FPropertyChangedEvent& PropertyChangedEvent) const
{
	OnStatesChanged.Broadcast(ChangedStates, PropertyChangedEvent);
}

TArray<TObjectPtr<UMetaStoryState>>* FMetaStoryViewModel::GetSubTrees() const
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	return TreeData != nullptr ? &TreeData->SubTrees : nullptr;
}

int32 FMetaStoryViewModel::GetSubTreeCount() const
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	return TreeData != nullptr ? TreeData->SubTrees.Num() : 0;
}

void FMetaStoryViewModel::GetSubTrees(TArray<TWeakObjectPtr<UMetaStoryState>>& OutSubtrees) const
{
	OutSubtrees.Reset();
	if (UMetaStoryEditorData* TreeData = TreeDataWeak.Get())
	{
		for (UMetaStoryState* Subtree : TreeData->SubTrees)
		{
			OutSubtrees.Add(Subtree);
		}
	}
}

void FMetaStoryViewModel::GetLinkStates(FGuid StateID, TArray<FGuid>& LinkingIn, TArray<FGuid>& LinkedOut) const
{
	const UMetaStoryState* State = GetStateByID(StateID);
	if (State == nullptr)
	{
		return;
	}

	if (UMetaStoryEditorData* TreeData = TreeDataWeak.Get())
	{
		TreeData->VisitHierarchy([&LinkingIn, &LinkedOut, StateID = State->ID](UMetaStoryState& State, UMetaStoryState* ParentState)
			{
				if (State.ID == StateID)
				{
					return EMetaStoryVisitor::Continue;
				}
				if (State.Type == EMetaStoryStateType::Linked && StateID == State.LinkedSubtree.ID)
				{
					LinkingIn.AddUnique(State.ID);
				}
				else
				{
					for (const FMetaStoryTransition& Transition : State.Transitions)
					{
						if (Transition.State.ID == StateID)
						{
							LinkingIn.AddUnique(State.ID);
						}
					}
				}
				return EMetaStoryVisitor::Continue;
			});

		if (State->Type == EMetaStoryStateType::Linked)
		{
			LinkedOut.AddUnique(State->LinkedSubtree.ID);
		}

		for (const FMetaStoryTransition& Transition : State->Transitions)
		{
			LinkedOut.AddUnique(Transition.State.ID);
		}
	}
}

void FMetaStoryViewModel::PostUndo(bool bSuccess)
{
	// TODO: see if we can narrow this down.
	OnAssetChanged.Broadcast();
}

void FMetaStoryViewModel::PostRedo(bool bSuccess)
{
	OnAssetChanged.Broadcast();
}

void FMetaStoryViewModel::ClearSelection()
{
	if (SelectedStates.IsEmpty())
	{
		return;
	}

	SelectedStates.Reset();

	const TArray<TWeakObjectPtr<UMetaStoryState>> SelectedStatesArr;
	OnSelectionChanged.Broadcast(SelectedStatesArr);
}

void FMetaStoryViewModel::SetSelection(UMetaStoryState* Selected)
{
	if (SelectedStates.Num() == 1 && SelectedStates.Contains(Selected))
	{
		return;
	}

	SelectedStates.Reset();
	if (Selected != nullptr)
	{
		SelectedStates.Add(Selected);
	}

	TArray<TWeakObjectPtr<UMetaStoryState>> SelectedStatesArr;
	SelectedStatesArr.Add(Selected);
	OnSelectionChanged.Broadcast(SelectedStatesArr);
}

void FMetaStoryViewModel::DeleteNode(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID)
{
	if (UMetaStoryEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UMetaStoryState* OwnerState = State.Get())
		{
			const UE::MetaStory::Editor::Private::FMetaStoryStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			auto DeleteFunc = [&StateNodeHandle](TNotNull<UMetaStoryState*> InOwnerState, TNotNull<UMetaStoryEditorData*> InEditorData, const FMetaStoryEditPropertyPath& InPropertyPath)
				{
					StateNodeHandle.EditorData->Modify();

					if (StateNodeHandle.IsTransition())
					{
						StateNodeHandle.GetTransitionArray().RemoveAt(StateNodeHandle.GetNodeIndex());
					}
					else
					{
						StateNodeHandle.GetEditorNodeArray().RemoveAt(StateNodeHandle.GetNodeIndex());
					}

					UE::MetaStoryEditor::RemoveInvalidBindings(InEditorData);
				};

			UE::MetaStory::PropertyHelpers::ModifyStateInPreAndPostEdit(
				LOCTEXT("DeleteNodeTransaction", "Delete Node"),
				StateNodeHandle.OwnerState,
				StateNodeHandle.EditorData,
				StateNodeHandle.GetNodePath(),
				DeleteFunc,
				StateNodeHandle.GetNodeIndex(),
				EPropertyChangeType::ArrayRemove);


			OnStateNodesChanged.Broadcast(OwnerState);
		}
	}
}

void FMetaStoryViewModel::DeleteAllNodes(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID)
{
	if (UMetaStoryEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UMetaStoryState* OwnerState = State.Get())
		{
			UE::MetaStory::Editor::Private::FMetaStoryStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			auto DeleteAllFunc = [&StateNodeHandle](const TNotNull<UMetaStoryState*> InOwnerState, const TNotNull<UMetaStoryEditorData*> InEditorData, const FMetaStoryEditPropertyPath& InPropertyPath)
				{
					StateNodeHandle.EditorData->Modify();

					if (StateNodeHandle.IsTransition())
					{
						StateNodeHandle.GetTransitionArray().Empty();
					}
					else
					{
						StateNodeHandle.GetEditorNodeArray().Empty();
					}

					UE::MetaStoryEditor::RemoveInvalidBindings(InEditorData);
				};

			UE::MetaStory::PropertyHelpers::ModifyStateInPreAndPostEdit(
				LOCTEXT("DeleteAllNodesTransaction", "Delete All Nodes"),
				StateNodeHandle.OwnerState,
				StateNodeHandle.EditorData,
				StateNodeHandle.GetNodePath(),
				DeleteAllFunc,
				INDEX_NONE, // Pass Invalid Index to Array Clear Op
				EPropertyChangeType::ArrayClear);


			OnStateNodesChanged.Broadcast(OwnerState);
		}
	}
}

void FMetaStoryViewModel::CopyNode(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID)
{
	if (UMetaStoryEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UMetaStoryState* OwnerState = State.Get())
		{
			UE::MetaStory::Editor::Private::FMetaStoryStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			UE::MetaStoryEditor::FMetaStoryClipboardEditorData ClipboardEditorData;
			if (StateNodeHandle.IsTransition())
			{
				ClipboardEditorData.Append(EditorData, TConstArrayView<FMetaStoryTransition>(&StateNodeHandle.GetTransition(), 1));
			}
			else
			{
				ClipboardEditorData.Append(EditorData, TConstArrayView<FMetaStoryEditorNode>(&StateNodeHandle.GetEditorNode(), 1));
			}
			UE::MetaStoryEditor::ExportTextAsClipboardEditorData(ClipboardEditorData);
		}
	}
}

void FMetaStoryViewModel::CopyAllNodes(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID)
{
	if (UMetaStoryEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UMetaStoryState* OwnerState = State.Get())
		{
			UE::MetaStory::Editor::Private::FMetaStoryStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			UE::MetaStoryEditor::FMetaStoryClipboardEditorData ClipboardEditorData;
			if (StateNodeHandle.IsTransition())
			{
				ClipboardEditorData.Append(EditorData, StateNodeHandle.GetTransitionArray());
			}
			else
			{
				ClipboardEditorData.Append(EditorData, StateNodeHandle.GetEditorNodeArray());
			}

			UE::MetaStoryEditor::ExportTextAsClipboardEditorData(ClipboardEditorData);
		}
	}
}

void FMetaStoryViewModel::PasteNode(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID)
{
	if (UMetaStoryEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UMetaStoryState* OwnerState = State.Get())
		{
			UE::MetaStory::Editor::Private::FMetaStoryStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			const UScriptStruct* TargetType = nullptr;
			if (StateNodeHandle.IsTransition())
			{
				TargetType = TBaseStructure<FMetaStoryTransition>::Get();
			}
			else
			{
				static const UScriptStruct* TaskBaseStruct = FMetaStoryTaskBase::StaticStruct();
				static const UScriptStruct* ConditionBaseStruct = FMetaStoryConditionBase::StaticStruct();
				static const UScriptStruct* ConsiderationBaseStruct = FMetaStoryConsiderationBase::StaticStruct();

				const UScriptStruct* BaseNodeScriptStruct = nullptr;
				const UScriptStruct* NodeScriptStruct = StateNodeHandle.GetEditorNode().GetNode().GetScriptStruct();
				if (NodeScriptStruct->IsChildOf(TaskBaseStruct))
				{
					BaseNodeScriptStruct = TaskBaseStruct;
				}
				else if (NodeScriptStruct->IsChildOf(ConditionBaseStruct))
				{
					BaseNodeScriptStruct = ConditionBaseStruct;
				}
				else if (NodeScriptStruct->IsChildOf(ConsiderationBaseStruct))
				{
					BaseNodeScriptStruct = ConsiderationBaseStruct;
				}

				TargetType = BaseNodeScriptStruct;
			}

			UE::MetaStoryEditor::FMetaStoryClipboardEditorData ClipboardEditorData;
			const bool bSucceeded = UE::MetaStoryEditor::ImportTextAsClipboardEditorData(TargetType, EditorData, OwnerState, ClipboardEditorData);

			if (!bSucceeded)
			{
				return;
			}

			if (StateNodeHandle.IsTransition() && ClipboardEditorData.GetTransitionsInBuffer().IsEmpty())
			{
				return;
			}

			if (!StateNodeHandle.IsTransition() && ClipboardEditorData.GetEditorNodesInBuffer().IsEmpty())
			{
				return;
			}

			auto PasteFunc = [&StateNodeHandle, &ClipboardEditorData](const TNotNull<UMetaStoryState*> InOwnerState, const TNotNull<UMetaStoryEditorData*> InEditorData, const FMetaStoryEditPropertyPath& InPropertyPath)
				{
					auto PasteFuncInternal = [InEditorData, &ClipboardEditorData]<typename T>(TArray<T>& Dest, TArrayView<T> Source, int32 DestIndex) requires
						std::is_same_v<T, FMetaStoryEditorNode> || std::is_same_v<T, FMetaStoryTransition>
					{
						check(Source.Num());

						const int32 DestStartIndex = DestIndex;
						Dest[DestIndex++] = MoveTemp(Source[0]);

						Dest.InsertUninitialized(DestIndex, Source.Num() - 1);
						for (int32 SourceIndex = 1; SourceIndex < Source.Num(); SourceIndex++, DestIndex++)
						{
							new (Dest.GetData() + DestIndex) T(MoveTemp(Source[SourceIndex]));
						}

						for (FMetaStoryPropertyPathBinding& Binding : ClipboardEditorData.GetBindingsInBuffer())
						{
							InEditorData->GetPropertyEditorBindings()->AddStateTreeBinding(MoveTemp(Binding));
						}
					};

					InEditorData->Modify();

					if (StateNodeHandle.IsTransition())
					{
						PasteFuncInternal(StateNodeHandle.GetTransitionArray(), ClipboardEditorData.GetTransitionsInBuffer(), StateNodeHandle.GetNodeIndex());
					}
					else
					{
						PasteFuncInternal(StateNodeHandle.GetEditorNodeArray(), ClipboardEditorData.GetEditorNodesInBuffer(), StateNodeHandle.GetNodeIndex());
					}
				};

			UE::MetaStory::PropertyHelpers::ModifyStateInPreAndPostEdit(
				LOCTEXT("PasteNodeTransaction", "Paste Node"),
				StateNodeHandle.OwnerState,
				StateNodeHandle.EditorData,
				StateNodeHandle.GetNodePath(),
				PasteFunc,
				INDEX_NONE, // Value Set Op, skip the index
				EPropertyChangeType::ValueSet);

			OnStateNodesChanged.Broadcast(OwnerState);
		}
	}
}

void FMetaStoryViewModel::PasteNodesToSelectedStates()
{
	if (UMetaStoryEditorData* EditorData = TreeDataWeak.Get())
	{
		TArray<UMetaStoryState*> CurrentlySelectedStates;
		GetSelectedStates(CurrentlySelectedStates);

		if (CurrentlySelectedStates.Num())
		{
			using namespace UE::MetaStoryEditor;
			TArray<FMetaStoryClipboardEditorData, TInlineAllocator<4>> Clipboards;
			Clipboards.AddDefaulted(CurrentlySelectedStates.Num());

			for (int32 Idx = 0; Idx < CurrentlySelectedStates.Num(); ++Idx)
			{
				FMetaStoryClipboardEditorData& Clipboard = Clipboards[Idx];

				// any editor nodes and transitions
				constexpr const UScriptStruct* TargetType = nullptr;
				ImportTextAsClipboardEditorData(TargetType, EditorData, CurrentlySelectedStates[Idx], Clipboard);

				if (!Clipboard.IsValid())
				{
					return;
				}
			}

			FScopedTransaction ScopedTransaction(LOCTEXT("PasteNodesToSelectedStatesTransaction", "Paste Nodes To Selected States"));
			EditorData->Modify();

			for (int32 Idx = 0; Idx < CurrentlySelectedStates.Num(); ++Idx)
			{
				UMetaStoryState* SelectedState = CurrentlySelectedStates[Idx];

				SelectedState->Modify();

				PasteNodesToState(EditorData, SelectedState, Clipboards[Idx]);
			}
		}
	}
}

void FMetaStoryViewModel::DuplicateNode(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID)
{
	if (UMetaStoryEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UMetaStoryState* OwnerState = State.Get())
		{
			UE::MetaStory::Editor::Private::FMetaStoryStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			auto DuplicateFunc = [&StateNodeHandle](const TNotNull<UMetaStoryState*> InOwnerState, const TNotNull<UMetaStoryEditorData*> InEditorData, const FMetaStoryEditPropertyPath& InPropertyPath)
				{
					int32 DestIndex = StateNodeHandle.GetNodeIndex();
					UE::MetaStoryEditor::FMetaStoryClipboardEditorData Clipboard;

					auto DuplicateFuncInternal = [InEditorData, &Clipboard]<typename T>(TArray<T>& Dest, int32 Index, TArrayView<T> Source)
						requires std::is_same_v<T, FMetaStoryEditorNode> || std::is_same_v<T, FMetaStoryTransition>
					{
						check(Source.Num());
						Dest.InsertUninitialized(Index, Source.Num());

						T* BaseAddress = Dest.GetData() + Index;
						for (int32 SourceIdx = 0; SourceIdx < Source.Num(); ++SourceIdx)
						{
							::new (BaseAddress + SourceIdx) T(MoveTemp(Source[SourceIdx]));
						}

						for (FMetaStoryPropertyPathBinding& Binding : Clipboard.GetBindingsInBuffer())
						{
							InEditorData->GetPropertyEditorBindings()->AddStateTreeBinding(MoveTemp(Binding));
						}
					};

					InEditorData->Modify();

					if (StateNodeHandle.IsTransition())
					{
						FMetaStoryTransition& SourceTransition = StateNodeHandle.GetTransition();

						Clipboard.Append(InEditorData, TConstArrayView<FMetaStoryTransition>(&SourceTransition, 1));
						Clipboard.ProcessBuffer(nullptr, InEditorData, InOwnerState);

						if (!Clipboard.IsValid())
						{
							return;
						}

						DuplicateFuncInternal(StateNodeHandle.GetTransitionArray(), DestIndex, Clipboard.GetTransitionsInBuffer());
					}
					else
					{
						FMetaStoryEditorNode& SourceEditorNode = StateNodeHandle.GetEditorNode();

						Clipboard.Append(InEditorData, TConstArrayView<FMetaStoryEditorNode>(&SourceEditorNode, 1));
						Clipboard.ProcessBuffer(nullptr, InEditorData, InOwnerState);

						if (!Clipboard.IsValid())
						{
							return;
						}

						DuplicateFuncInternal(StateNodeHandle.GetEditorNodeArray(), DestIndex, Clipboard.GetEditorNodesInBuffer());
					}
				};

			UE::MetaStory::PropertyHelpers::ModifyStateInPreAndPostEdit(
				LOCTEXT("DuplicateNodeTransaction", "Duplicate Node"),
				StateNodeHandle.OwnerState,
				StateNodeHandle.EditorData,
				StateNodeHandle.GetNodePath(),
				DuplicateFunc,
				StateNodeHandle.GetNodeIndex(),
				EPropertyChangeType::Duplicate);

			OnStateNodesChanged.Broadcast(OwnerState);
		}
	}
}

void FMetaStoryViewModel::SetSelection(const TArray<TWeakObjectPtr<UMetaStoryState>>& InSelectedStates)
{
	if (SelectedStates.Num() == InSelectedStates.Num() && SelectedStates.Array() == InSelectedStates)
	{
		return;
	}

	SelectedStates.Reset();

	for (const TWeakObjectPtr<UMetaStoryState>& State : InSelectedStates)
	{
		if (State.Get())
		{
			SelectedStates.Add(State);
		}
	}

	OnSelectionChanged.Broadcast(InSelectedStates);
}

bool FMetaStoryViewModel::IsSelected(const UMetaStoryState* State) const
{
	const TWeakObjectPtr<UMetaStoryState> WeakState = const_cast<UMetaStoryState*>(State);
	return SelectedStates.Contains(WeakState);
}

bool FMetaStoryViewModel::IsChildOfSelection(const UMetaStoryState* State) const
{
	for (const TWeakObjectPtr<UMetaStoryState>& WeakSelectedState : SelectedStates)
	{
		if (const UMetaStoryState* SelectedState = Cast<UMetaStoryState>(WeakSelectedState.Get()))
		{
			if (SelectedState == State)
			{
				return true;
			}
			
			if (UE::MetaStory::Editor::IsChildOf(SelectedState, State))
			{
				return true;
			}
		}
	}
	return false;
}

void FMetaStoryViewModel::GetSelectedStates(TArray<UMetaStoryState*>& OutSelectedStates) const
{
	OutSelectedStates.Reset();
	for (const TWeakObjectPtr<UMetaStoryState>& WeakState : SelectedStates)
	{
		if (UMetaStoryState* State = WeakState.Get())
		{
			OutSelectedStates.Add(State);
		}
	}
}

void FMetaStoryViewModel::GetSelectedStates(TArray<TWeakObjectPtr<UMetaStoryState>>& OutSelectedStates) const
{
	OutSelectedStates.Reset();
	for (const TWeakObjectPtr<UMetaStoryState>& WeakState : SelectedStates)
	{
		if (WeakState.Get())
		{
			OutSelectedStates.Add(WeakState);
		}
	}
}

bool FMetaStoryViewModel::HasSelection() const
{
	return SelectedStates.Num() > 0;
}

void FMetaStoryViewModel::BringNodeToFocus(UMetaStoryState* State, const FGuid NodeID)
{
	SetSelection(State);
	OnBringNodeToFocus.Broadcast(State, NodeID);
}

void FMetaStoryViewModel::GetPersistentExpandedStates(TSet<TWeakObjectPtr<UMetaStoryState>>& OutExpandedStates)
{
	OutExpandedStates.Reset();
	if (UMetaStoryEditorData* TreeData = TreeDataWeak.Get())
	{
		for (UMetaStoryState* SubTree : TreeData->SubTrees)
		{
			GetExpandedStatesRecursive(SubTree, OutExpandedStates);
		}
	}
}

void FMetaStoryViewModel::GetExpandedStatesRecursive(UMetaStoryState* State, TSet<TWeakObjectPtr<UMetaStoryState>>& OutExpandedStates)
{
	if (State->bExpanded)
	{
		OutExpandedStates.Add(State);
	}
	for (UMetaStoryState* Child : State->Children)
	{
		GetExpandedStatesRecursive(Child, OutExpandedStates);
	}
}


void FMetaStoryViewModel::SetPersistentExpandedStates(TSet<TWeakObjectPtr<UMetaStoryState>>& InExpandedStates)
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TreeData->Modify();

	for (TWeakObjectPtr<UMetaStoryState>& WeakState : InExpandedStates)
	{
		if (UMetaStoryState* State = WeakState.Get())
		{
			State->bExpanded = true;
		}
	}
}


void FMetaStoryViewModel::AddState(UMetaStoryState* AfterState)
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddStateTransaction", "Add State"));

	UMetaStoryState* NewState = NewObject<UMetaStoryState>(TreeData, FName(), RF_Transactional);
	UMetaStoryState* ParentState = nullptr;

	if (AfterState == nullptr)
	{
		// If no subtrees, add a subtree, or add to the root state.
		if (TreeData->SubTrees.IsEmpty())
		{
			TreeData->Modify();
			TreeData->SubTrees.Add(NewState);
		}
		else
		{
			UMetaStoryState* RootState = TreeData->SubTrees[0];
			if (ensureMsgf(RootState, TEXT("%s: Root state is empty."), *GetNameSafe(TreeData->GetOuter())))
			{
				RootState->Modify();
				RootState->Children.Add(NewState);
				NewState->Parent = RootState;
				ParentState = RootState;
			}
		}
	}
	else
	{
		ParentState = AfterState->Parent;
		if (ParentState != nullptr)
		{
			ParentState->Modify();
		}
		else
		{
			TreeData->Modify();
		}

		TArray<TObjectPtr<UMetaStoryState>>& ParentArray = ParentState ? ParentState->Children : TreeData->SubTrees;

		const int32 TargetIndex = ParentArray.Find(AfterState);
		if (TargetIndex != INDEX_NONE)
		{
			// Insert After
			ParentArray.Insert(NewState, TargetIndex + 1);
			NewState->Parent = ParentState;
		}
		else
		{
			// Fallback, should never happen.
			ensureMsgf(false, TEXT("%s: Failed to find specified target state %s on state %s while adding new state."), *GetNameSafe(TreeData->GetOuter()), *GetNameSafe(AfterState), *GetNameSafe(ParentState));
			ParentArray.Add(NewState);
			NewState->Parent = ParentState;
		}
	}

	OnStateAdded.Broadcast(ParentState, NewState);
}

void FMetaStoryViewModel::AddChildState(UMetaStoryState* ParentState)
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr || ParentState == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddChildStateTransaction", "Add Child State"));

	UMetaStoryState* NewState = NewObject<UMetaStoryState>(ParentState, FName(), RF_Transactional);

	ParentState->Modify();
	
	ParentState->Children.Add(NewState);
	NewState->Parent = ParentState;

	OnStateAdded.Broadcast(ParentState, NewState);
}

void FMetaStoryViewModel::RenameState(UMetaStoryState* State, FName NewName)
{
	if (State == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RenameTransaction", "Rename"));
	State->Modify();
	State->Name = NewName;

	TSet<UMetaStoryState*> AffectedStates;
	AffectedStates.Add(State);

	FProperty* NameProperty = FindFProperty<FProperty>(UMetaStoryState::StaticClass(), GET_MEMBER_NAME_CHECKED(UMetaStoryState, Name));
	FPropertyChangedEvent PropertyChangedEvent(NameProperty, EPropertyChangeType::ValueSet);
	OnStatesChanged.Broadcast(AffectedStates, PropertyChangedEvent);
}

void FMetaStoryViewModel::RemoveSelectedStates()
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TArray<UMetaStoryState*> States;
	GetSelectedStates(States);

	// Remove items whose parent also exists in the selection.
	UE::MetaStory::Editor::RemoveContainedChildren(States);

	if (States.Num() > 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteStateTransaction", "Delete State"));

		TSet<UMetaStoryState*> AffectedParents;

		for (UMetaStoryState* StateToRemove : States)
		{
			if (StateToRemove)
			{
				StateToRemove->Modify();

				UMetaStoryState* ParentState = StateToRemove->Parent;
				if (ParentState != nullptr)
				{
					AffectedParents.Add(ParentState);
					ParentState->Modify();
				}
				else
				{
					AffectedParents.Add(nullptr);
					TreeData->Modify();
				}
				
				TArray<TObjectPtr<UMetaStoryState>>& ArrayToRemoveFrom = ParentState ? ParentState->Children : TreeData->SubTrees;
				const int32 ItemIndex = ArrayToRemoveFrom.Find(StateToRemove);
				if (ItemIndex != INDEX_NONE)
				{
					ArrayToRemoveFrom.RemoveAt(ItemIndex);
					StateToRemove->Parent = nullptr;
				}

			}
		}

		OnStatesRemoved.Broadcast(AffectedParents);

		ClearSelection();
	}
}

void FMetaStoryViewModel::CopySelectedStates()
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TArray<UMetaStoryState*> States;
	GetSelectedStates(States);
	UE::MetaStory::Editor::RemoveContainedChildren(States);
	
	FString ExportedText = UE::MetaStory::Editor::ExportStatesToText(TreeData, States);
	
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool FMetaStoryViewModel::CanPasteStatesFromClipboard() const
{
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	UE::MetaStory::Editor::FMetaStoryStateTextFactory Factory;
	return Factory.CanCreateObjectsFromText(TextToImport);
}

void FMetaStoryViewModel::PasteStatesFromClipboard(UMetaStoryState* AfterState)
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}
	
	if (AfterState)
	{
		const int32 Index = AfterState->Parent ? AfterState->Parent->Children.Find(AfterState) : TreeData->SubTrees.Find(AfterState);
		if (Index != INDEX_NONE)
		{
			FString TextToImport;
			FPlatformApplicationMisc::ClipboardPaste(TextToImport);
			
			const FScopedTransaction Transaction(LOCTEXT("PasteStatesTransaction", "Paste State(s)"));
			PasteStatesAsChildrenFromText(TextToImport, AfterState->Parent, Index + 1);
		}
	}
}

void FMetaStoryViewModel::PasteStatesAsChildrenFromClipboard(UMetaStoryState* ParentState)
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}
	
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	const FScopedTransaction Transaction(LOCTEXT("PasteStatesTransaction", "Paste State(s)"));
	PasteStatesAsChildrenFromText(TextToImport, ParentState, INDEX_NONE);
}

void FMetaStoryViewModel::PasteStatesAsChildrenFromText(const FString& TextToImport, UMetaStoryState* ParentState, const int32 IndexToInsertAt)
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	UObject* Outer = ParentState ? static_cast<UObject*>(ParentState) : static_cast<UObject*>(TreeData);
	Outer->Modify();

	UE::MetaStory::Editor::FMetaStoryStateTextFactory Factory;
	Factory.ProcessBuffer(Outer, RF_Transactional, TextToImport);

	TArray<TObjectPtr<UMetaStoryState>>& ParentArray = ParentState ? ParentState->Children : TreeData->SubTrees;
	const int32 TargetIndex = (IndexToInsertAt == INDEX_NONE) ? ParentArray.Num() : IndexToInsertAt;
	ParentArray.Insert(Factory.States, TargetIndex);

	TArray<FMetaStoryStateLink*> Links;
	TMap<FGuid, FGuid> IDsMap;
	TArray<UMetaStoryState*> NewStates;

	for (UMetaStoryState* State : Factory.States)
	{		
		UE::MetaStory::Editor::FixStateAfterDuplication(State, ParentState, IDsMap, Links, NewStates);
	}

	// Copy property bindings for the duplicated states.
	if (Factory.ClipboardBindings)
	{
		for (FMetaStoryPropertyPathBinding& Binding : Factory.ClipboardBindings->Bindings)
		{
			if (Binding.GetPropertyFunctionNode().IsValid())
			{
				UE::MetaStory::Editor::FixNodesAfterDuplication(TArrayView<FMetaStoryEditorNode>(Binding.GetMutablePropertyFunctionNode().GetPtr<FMetaStoryEditorNode>(), 1), IDsMap, Links);
			}
		}

		for (const TPair<FGuid, FGuid>& Entry : IDsMap)
		{
			const FGuid OldTargetID = Entry.Key;
			const FGuid NewTargetID = Entry.Value;
			
			for (FMetaStoryPropertyPathBinding& Binding : Factory.ClipboardBindings->Bindings)
			{
				if (Binding.GetTargetPath().GetStructID() == OldTargetID)
				{
					Binding.GetMutableTargetPath().SetStructID(NewTargetID);

					if (const FGuid* NewSourceID = IDsMap.Find(Binding.GetSourcePath().GetStructID()))
					{
						Binding.GetMutableSourcePath().SetStructID(*NewSourceID);
					}
					
					TreeData->GetPropertyEditorBindings()->AddStateTreeBinding(MoveTemp(Binding));
				}
			}
		}
	}

	// Patch IDs in state links.
	for (FMetaStoryStateLink* Link : Links)
	{
		if (FGuid* NewID = IDsMap.Find(Link->ID))
		{
			Link->ID = *NewID;
		}
	}

	for (UMetaStoryState* State : NewStates)
	{
		OnStateAdded.Broadcast(State->Parent, State);
	}
}

void FMetaStoryViewModel::DuplicateSelectedStates()
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TArray<UMetaStoryState*> States;
	GetSelectedStates(States);
	UE::MetaStory::Editor::RemoveContainedChildren(States);

	if (States.IsEmpty())
	{
		return;
	}
	
	FString ExportedText = UE::MetaStory::Editor::ExportStatesToText(TreeData, States);

	// Place duplicates after first selected state.
	UMetaStoryState* AfterState = States[0];
	
	const int32 Index = AfterState->Parent ? AfterState->Parent->Children.Find(AfterState) : TreeData->SubTrees.Find(AfterState);
	if (Index != INDEX_NONE)
	{
		const FScopedTransaction Transaction(LOCTEXT("DuplicateStatesTransaction", "Duplicate State(s)"));
		PasteStatesAsChildrenFromText(ExportedText, AfterState->Parent, Index + 1);
	}
}


void FMetaStoryViewModel::MoveSelectedStatesBefore(UMetaStoryState* TargetState)
{
	MoveSelectedStates(TargetState, EMetaStoryViewModelInsert::Before);
}

void FMetaStoryViewModel::MoveSelectedStatesAfter(UMetaStoryState* TargetState)
{
	MoveSelectedStates(TargetState, EMetaStoryViewModelInsert::After);
}

void FMetaStoryViewModel::MoveSelectedStatesInto(UMetaStoryState* TargetState)
{
	MoveSelectedStates(TargetState, EMetaStoryViewModelInsert::Into);
}

bool FMetaStoryViewModel::CanEnableStates() const
{
	TArray<UMetaStoryState*> States;
	GetSelectedStates(States);

	for (const UMetaStoryState* State : States)
	{
		// Stop if at least one state can be enabled
		if (State->bEnabled == false)
		{
			return true;
		}
	}

	return false;
}

bool FMetaStoryViewModel::CanDisableStates() const
{
	TArray<UMetaStoryState*> States;
	GetSelectedStates(States);

	for (const UMetaStoryState* State : States)
	{
		// Stop if at least one state can be disabled
		if (State->bEnabled)
		{
			return true;
		}
	}

	return false;
}

bool FMetaStoryViewModel::CanPasteNodesToSelectedStates() const
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (!TreeData)
	{
		return false;
	}

	TArray<UMetaStoryState*> States;
	GetSelectedStates(States);
	if (States.IsEmpty())
	{
		return false;
	}

	UE::MetaStoryEditor::FMetaStoryClipboardEditorData ClipboardEditorData;

	constexpr const UScriptStruct* TargetType = nullptr;
	constexpr bool bProcessBuffer = false;
	const bool bSucceeded = UE::MetaStoryEditor::ImportTextAsClipboardEditorData(TargetType, TreeData, States[0], ClipboardEditorData, bProcessBuffer);

	return bSucceeded && (ClipboardEditorData.GetEditorNodesInBuffer().Num() || ClipboardEditorData.GetTransitionsInBuffer().Num());
}

void FMetaStoryViewModel::SetSelectedStatesEnabled(const bool bEnable)
{
	TArray<UMetaStoryState*> States;
	GetSelectedStates(States);

	if (States.Num() > 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetStatesEnabledTransaction", "Set State Enabled"));

		for (UMetaStoryState* State : States)
		{
			State->Modify();
			State->bEnabled = bEnable;
		}

		OnAssetChanged.Broadcast();
	}
}

void FMetaStoryViewModel::MoveSelectedStates(UMetaStoryState* TargetState, const EMetaStoryViewModelInsert RelativeLocation)
{
	UMetaStoryEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr || TargetState == nullptr)
	{
		return;
	}

	TArray<UMetaStoryState*> States;
	GetSelectedStates(States);

	// Remove child items whose parent also exists in the selection.
	UE::MetaStory::Editor::RemoveContainedChildren(States);

	// Remove states which contain target state as child.
	States.RemoveAll([TargetState](const UMetaStoryState* State)
	{
		return UE::MetaStory::Editor::IsChildOf(State, TargetState);
	});

	if (States.Num() > 0 && TargetState != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("MoveTransaction", "Move"));

		TSet<UMetaStoryState*> AffectedParents;
		TSet<UMetaStoryState*> AffectedStates;

		UMetaStoryState* TargetParent = TargetState->Parent;
		if (RelativeLocation == EMetaStoryViewModelInsert::Into)
		{
			AffectedParents.Add(TargetState);
		}
		else
		{
			AffectedParents.Add(TargetParent);
		}
		
		for (int32 i = States.Num() - 1; i >= 0; i--)
		{
			if (UMetaStoryState* State = States[i])
			{
				State->Modify();
				AffectedParents.Add(State->Parent);
			}
		}

		if (RelativeLocation == EMetaStoryViewModelInsert::Into)
		{
			// Move into
			TargetState->Modify();
		}
		
		for (UMetaStoryState* Parent : AffectedParents)
		{
			if (Parent)
			{
				Parent->Modify();
			}
			else
			{
				TreeData->Modify();
			}
		}

		// Add in reverse order to keep the original order.
		for (int32 i = States.Num() - 1; i >= 0; i--)
		{
			if (UMetaStoryState* SelectedState = States[i])
			{
				AffectedStates.Add(SelectedState);

				UMetaStoryState* SelectedParent = SelectedState->Parent;

				// Remove from current parent
				TArray<TObjectPtr<UMetaStoryState>>& ArrayToRemoveFrom = SelectedParent ? SelectedParent->Children : TreeData->SubTrees;
				const int32 ItemIndex = ArrayToRemoveFrom.Find(SelectedState);
				if (ItemIndex != INDEX_NONE)
				{
					ArrayToRemoveFrom.RemoveAt(ItemIndex);
					SelectedState->Parent = nullptr;
				}

				// Insert to new parent
				if (RelativeLocation == EMetaStoryViewModelInsert::Into)
				{
					// Into
					TargetState->Children.Insert(SelectedState, /*Index*/0);
					SelectedState->Parent = TargetState;
				}
				else
				{
					TArray<TObjectPtr<UMetaStoryState>>& ArrayToMoveTo = TargetParent ? TargetParent->Children : TreeData->SubTrees;
					const int32 TargetIndex = ArrayToMoveTo.Find(TargetState);
					if (TargetIndex != INDEX_NONE)
					{
						if (RelativeLocation == EMetaStoryViewModelInsert::Before)
						{
							// Before
							ArrayToMoveTo.Insert(SelectedState, TargetIndex);
							SelectedState->Parent = TargetParent;
						}
						else if (RelativeLocation == EMetaStoryViewModelInsert::After)
						{
							// After
							ArrayToMoveTo.Insert(SelectedState, TargetIndex + 1);
							SelectedState->Parent = TargetParent;
						}
					}
					else
					{
						// Fallback, should never happen.
						ensureMsgf(false, TEXT("%s: Failed to find specified target state %s on state %s while moving a state."), *GetNameSafe(TreeData->GetOuter()), *GetNameSafe(TargetState), *GetNameSafe(SelectedParent));
						ArrayToMoveTo.Add(SelectedState);
						SelectedState->Parent = TargetParent;
					}
				}
			}
		}

		OnStatesMoved.Broadcast(AffectedParents, AffectedStates);

		TArray<TWeakObjectPtr<UMetaStoryState>> WeakStates;
		for (UMetaStoryState* State : States)
		{
			WeakStates.Add(State);
		}

		SetSelection(WeakStates);
	}
}


void FMetaStoryViewModel::BindToDebuggerDelegates()
{
#if WITH_METASTORY_TRACE_DEBUGGER
	Debugger->OnActiveStatesChanged.BindSPLambda(this, [this](const FMetaStoryTraceActiveStates& NewActiveStates)
	{
		if (NewActiveStates.PerAssetStates.Num() == 0)
		{
			ActiveStates.Empty();
		}
		else if (const UMetaStory* OuterStateTree = GetStateTree())
		{
			for (const FMetaStoryTraceActiveStates::FAssetActiveStates& AssetActiveStates : NewActiveStates.PerAssetStates)
			{
				// Only track states owned by the MetaStory associated to the view model (skip linked assets)
				if (AssetActiveStates.WeakStateTree == OuterStateTree)
				{
					ActiveStates.Reset(AssetActiveStates.ActiveStates.Num());
					for (const FMetaStoryStateHandle Handle : AssetActiveStates.ActiveStates)
					{
						ActiveStates.Add(OuterStateTree->GetStateIdFromHandle(Handle));
					}
				}
			}
		}
	});
#endif // WITH_METASTORY_TRACE_DEBUGGER
}

void FMetaStoryViewModel::PasteNodesToState(TNotNull<UMetaStoryEditorData*> InEditorData, TNotNull<UMetaStoryState*> InState, UE::MetaStoryEditor::FMetaStoryClipboardEditorData& InProcessedClipboard)
{
	auto AppendFunc = [&InProcessedClipboard]<typename T>(TNotNull<UMetaStoryState*> InOwnerState, TNotNull<UMetaStoryEditorData*> InEditorData, const FMetaStoryEditPropertyPath & InPropertyPath)
		requires TIsDerivedFrom<T, FMetaStoryNodeBase>::IsDerived || std::is_same_v<T, FMetaStoryTransition>
	{
		if constexpr (std::is_same_v<T, FMetaStoryTransition>)
		{
			TArrayView<FMetaStoryTransition> TransitionsInBuffer = InProcessedClipboard.GetTransitionsInBuffer();

			for (FMetaStoryTransition& Transition : TransitionsInBuffer)
			{
				InOwnerState->Transitions.Add(MoveTemp(Transition));
			}
		}
		else
		{
			const UScriptStruct* NodeType = TBaseStructure<T>::Get();

			TArray<FMetaStoryEditorNode>* TargetArray = nullptr;
			if (NodeType->IsChildOf<FMetaStoryTaskBase>())
			{
				TargetArray = &InOwnerState->Tasks;
			}
			else if (NodeType->IsChildOf<FMetaStoryConditionBase>())
			{
				TargetArray = &InOwnerState->EnterConditions;
			}
			else if (NodeType->IsChildOf<FMetaStoryConsiderationBase>())
			{
				TargetArray = &InOwnerState->Considerations;
			}

			TArrayView<FMetaStoryEditorNode> EditorNodesInBuffer = InProcessedClipboard.GetEditorNodesInBuffer();

			for (FMetaStoryEditorNode& EditorNode : EditorNodesInBuffer)
			{
				TStructView<FMetaStoryNodeBase> NodeView = EditorNode.GetNode();
				if (NodeView.IsValid() && NodeView.GetScriptStruct()->IsChildOf(NodeType))
				{
					TargetArray->Add(MoveTemp(EditorNode));
				}
			}
		}
	};

	FScopedTransaction ScopedTransaction(LOCTEXT("PasteNodesToStateTransaction", "Paste Nodes To State"));

	// Property Change requires one property at a time
	using namespace UE::MetaStory::PropertyHelpers;

	// Enter Conditions
	ModifyStateInPreAndPostEdit(
		LOCTEXT("PasteNodesToStateTransaction", "Paste Nodes To State"),
		InState,
		InEditorData,
		TEXT("EnterConditions"),
		[&AppendFunc](TNotNull<UMetaStoryState*> InOwnerState, TNotNull<UMetaStoryEditorData*> InEditorData, const FMetaStoryEditPropertyPath& InPropertyPath)
		{ AppendFunc.operator() < FMetaStoryConditionBase > (InOwnerState, InEditorData, InPropertyPath); },
		INDEX_NONE,
		EPropertyChangeType::ValueSet);

	// Considerations
	ModifyStateInPreAndPostEdit(
		LOCTEXT("PasteNodesToStateTransaction", "Paste Nodes To State"),
		InState,
		InEditorData,
		TEXT("Considerations"),
		[&AppendFunc](TNotNull<UMetaStoryState*> InOwnerState, TNotNull<UMetaStoryEditorData*> InEditorData, const FMetaStoryEditPropertyPath& InPropertyPath)
		{ AppendFunc.operator() < FMetaStoryConsiderationBase > (InOwnerState, InEditorData, InPropertyPath); },
		INDEX_NONE,
		EPropertyChangeType::ValueSet);

	// Tasks
	ModifyStateInPreAndPostEdit(
		LOCTEXT("PasteNodesToStateTransaction", "Paste Nodes To State"),
		InState,
		InEditorData,
		TEXT("Tasks"),
		[&AppendFunc](TNotNull<UMetaStoryState*> InOwnerState, TNotNull<UMetaStoryEditorData*> InEditorData, const FMetaStoryEditPropertyPath& InPropertyPath)
		{ AppendFunc.operator() < FMetaStoryTaskBase > (InOwnerState, InEditorData, InPropertyPath); },
		INDEX_NONE,
		EPropertyChangeType::ValueSet);

	// Transitions
	ModifyStateInPreAndPostEdit(
		LOCTEXT("PasteNodesToStateTransaction", "Paste Nodes To State"),
		InState,
		InEditorData,
		TEXT("Transitions"),
		[&AppendFunc](TNotNull<UMetaStoryState*> InOwnerState, TNotNull<UMetaStoryEditorData*> InEditorData, const FMetaStoryEditPropertyPath& InPropertyPath)
		{ AppendFunc.operator() < FMetaStoryTransition > (InOwnerState, InEditorData, InPropertyPath); },
		INDEX_NONE,
		EPropertyChangeType::ValueSet);

	// Dump fixed property bindings in the end to avoid being cleaned up before their corresponding nodes are pushed in
	for (FMetaStoryPropertyPathBinding& Binding : InProcessedClipboard.GetBindingsInBuffer())
	{
		InEditorData->GetPropertyEditorBindings()->AddStateTreeBinding(MoveTemp(Binding));
	}

	OnStateNodesChanged.Broadcast(InState);
}

bool FMetaStoryViewModel::IsStateActiveInDebugger(const UMetaStoryState& State) const
{
#if WITH_METASTORY_TRACE_DEBUGGER
	return ActiveStates.Contains(State.ID);
#else
	return false;
#endif // WITH_METASTORY_TRACE_DEBUGGER
}

#undef LOCTEXT_NAMESPACE
