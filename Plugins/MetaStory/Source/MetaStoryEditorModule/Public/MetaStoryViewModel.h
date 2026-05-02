// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtr.h"
#include "EditorUndoClient.h"
#include "IMetaStoryEditorHost.h"
#include "MetaStoryViewModel.generated.h"

#define UE_API METASTORYEDITORMODULE_API

namespace UE::MetaStoryEditor
{
	struct FMetaStoryClipboardEditorData;
}

struct FMetaStoryTransition;
class FMenuBuilder;
class UMetaStory;
class UMetaStoryEditorData;
class UMetaStoryState;

enum class ECheckBoxState : uint8;
enum class EMetaStoryBreakpointType : uint8;

struct FPropertyChangedEvent;
struct FMetaStoryDebugger;
struct FMetaStoryDebuggerBreakpoint;
struct FMetaStoryEditorBreakpoint;
struct FMetaStoryPropertyPathBinding;

enum class EMetaStoryViewModelInsert : uint8
{
	Before,
	After,
	Into,
};

enum class UE_DEPRECATED(5.6, "Use the enum with the E prefix") FMetaStoryViewModelInsert : uint8
{
	Before,
	After,
	Into,
};

/**
 * ModelView for editing MetaStoryEditorData.
 */
class FMetaStoryViewModel : public FEditorUndoClient, public TSharedFromThis<FMetaStoryViewModel>
{
public:

	DECLARE_MULTICAST_DELEGATE(FOnAssetChanged);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStatesChanged, const TSet<UMetaStoryState*>& /*AffectedStates*/, const FPropertyChangedEvent& /*PropertyChangedEvent*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStateAdded, UMetaStoryState* /*ParentState*/, UMetaStoryState* /*NewState*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStatesRemoved, const TSet<UMetaStoryState*>& /*AffectedParents*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStatesMoved, const TSet<UMetaStoryState*>& /*AffectedParents*/, const TSet<UMetaStoryState*>& /*MovedStates*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStateNodesChanged, const UMetaStoryState* /*AffectedState*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectionChanged, const TArray<TWeakObjectPtr<UMetaStoryState>>& /*SelectedStates*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBringNodeToFocus, const UMetaStoryState* /*State*/, const FGuid /*NodeID*/);

	UE_API FMetaStoryViewModel();
	UE_API virtual ~FMetaStoryViewModel() override;

	UE_API void Init(UMetaStoryEditorData* InTreeData);

	//~ FEditorUndoClient
	UE_API virtual void PostUndo(bool bSuccess) override;
	UE_API virtual void PostRedo(bool bSuccess) override;

	// Selection handling.
	UE_API void ClearSelection();
	UE_API void SetSelection(UMetaStoryState* Selected);
	UE_API void SetSelection(const TArray<TWeakObjectPtr<UMetaStoryState>>& InSelection);
	UE_API bool IsSelected(const UMetaStoryState* State) const;
	UE_API bool IsChildOfSelection(const UMetaStoryState* State) const;
	UE_API void GetSelectedStates(TArray<UMetaStoryState*>& OutSelectedStates) const;
	UE_API void GetSelectedStates(TArray<TWeakObjectPtr<UMetaStoryState>>& OutSelectedStates) const;
	UE_API bool HasSelection() const;

	UE_API void BringNodeToFocus(UMetaStoryState* State, const FGuid NodeID);
	
	// Returns associated MetaStory asset.
	UE_API const UMetaStory* GetMetaStory() const;

	UE_API const UMetaStoryEditorData* GetMetaStoryEditorData() const;

	UE_API const UMetaStoryState* GetStateByID(const FGuid StateID) const;
	UE_API UMetaStoryState* GetMutableStateByID(const FGuid StateID) const;
	
	// Returns array of subtrees to edit.
	UE_API TArray<TObjectPtr<UMetaStoryState>>* GetSubTrees() const;
	UE_API int32 GetSubTreeCount() const;
	UE_API void GetSubTrees(TArray<TWeakObjectPtr<UMetaStoryState>>& OutSubtrees) const;

	/** Find the states that are linked to the provided StateID. */
	UE_API void GetLinkStates(FGuid StateID, TArray<FGuid>& LinkingIn, TArray<FGuid>& LinkedOut) const;

	// Gets and sets MetaStory view expansion state store in the asset.
	UE_API void SetPersistentExpandedStates(TSet<TWeakObjectPtr<UMetaStoryState>>& InExpandedStates);
	UE_API void GetPersistentExpandedStates(TSet<TWeakObjectPtr<UMetaStoryState>>& OutExpandedStates);

	// State manipulation
	UE_API void AddState(UMetaStoryState* AfterState);
	UE_API void AddChildState(UMetaStoryState* ParentState);
	UE_API void RenameState(UMetaStoryState* State, FName NewName);
	UE_API void RemoveSelectedStates();
	UE_API void CopySelectedStates();
	UE_API bool CanPasteStatesFromClipboard() const;
	UE_API void PasteStatesFromClipboard(UMetaStoryState* AfterState);
	UE_API void PasteStatesAsChildrenFromClipboard(UMetaStoryState* ParentState);
	UE_API void DuplicateSelectedStates();
	UE_API void MoveSelectedStatesBefore(UMetaStoryState* TargetState);
	UE_API void MoveSelectedStatesAfter(UMetaStoryState* TargetState);
	UE_API void MoveSelectedStatesInto(UMetaStoryState* TargetState);
	UE_API bool CanEnableStates() const;
	UE_API bool CanDisableStates() const;
	UE_API bool CanPasteNodesToSelectedStates() const;
	UE_API void SetSelectedStatesEnabled(bool bEnable);

	// EditorNode and Transition manipulation
	// @todo: support ReplaceWith and Rename
	UE_API void DeleteNode(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID);
	UE_API void DeleteAllNodes(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID);
	UE_API void CopyNode(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID);
	UE_API void CopyAllNodes(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID);
	UE_API void PasteNode(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID);
	UE_API void PasteNodesToSelectedStates();
	UE_API void DuplicateNode(TWeakObjectPtr<UMetaStoryState> State, const FGuid& ID);

	// Force to update the view externally.
	UE_API void NotifyAssetChangedExternally() const;
	UE_API void NotifyStatesChangedExternally(const TSet<UMetaStoryState*>& ChangedStates, const FPropertyChangedEvent& PropertyChangedEvent) const;

	// Debugging
#if WITH_METASTORY_TRACE_DEBUGGER
	UE_API bool HasBreakpoint(FGuid ID, EMetaStoryBreakpointType Type);
	UE_API bool CanProcessBreakpoints() const;
	UE_API bool CanAddStateBreakpoint(EMetaStoryBreakpointType Type) const;
	UE_API bool CanRemoveStateBreakpoint(EMetaStoryBreakpointType Type) const;
	UE_API ECheckBoxState GetStateBreakpointCheckState(EMetaStoryBreakpointType Type) const;
	UE_API void HandleEnableStateBreakpoint(EMetaStoryBreakpointType Type);
	UE_API void ToggleStateBreakpoints(TConstArrayView<TWeakObjectPtr<>> States, EMetaStoryBreakpointType Type);
	UE_API void ToggleTaskBreakpoint(FGuid ID, EMetaStoryBreakpointType Type);
	UE_API void ToggleTransitionBreakpoint(TConstArrayView<TNotNull<const FMetaStoryTransition*>> Transitions, ECheckBoxState ToggledState);

	UE_API UMetaStoryState* FindStateAssociatedToBreakpoint(FMetaStoryDebuggerBreakpoint Breakpoint) const;

	TSharedRef<FMetaStoryDebugger> GetDebugger() const
	{
		return Debugger;
	}

	UE_API void RemoveAllBreakpoints();
	UE_API void RefreshDebuggerBreakpoints();
#endif // WITH_METASTORY_TRACE_DEBUGGER

	UE_API bool IsStateActiveInDebugger(const UMetaStoryState& State) const;

	// Called when the whole asset is updated (i.e. undo/redo).
	FOnAssetChanged& GetOnAssetChanged()
	{
		return OnAssetChanged;
	}
	
	// Called when States are changed (i.e. change name or properties).
	FOnStatesChanged& GetOnStatesChanged()
	{
		return OnStatesChanged;
	}
	
	// Called each time a state is added.
	FOnStateAdded& GetOnStateAdded()
	{
		return OnStateAdded;
	}

	// Called each time a states are removed.
	FOnStatesRemoved& GetOnStatesRemoved()
	{
		return OnStatesRemoved;
	}

	// Called each time a state is removed.
	FOnStatesMoved& GetOnStatesMoved()
	{
		return OnStatesMoved;
	}

	// Called each time a state's Editor nodes or transitions are changed except from the DetailsView.
	FOnStateNodesChanged& GetOnStateNodesChanged()
	{
		return OnStateNodesChanged;
	}

	// Called each time the selection changes.
	FOnSelectionChanged& GetOnSelectionChanged()
	{
		return OnSelectionChanged;
	}

	FOnBringNodeToFocus& GetOnBringNodeToFocus()
	{
		return OnBringNodeToFocus;
	}

protected:
	UE_API void GetExpandedStatesRecursive(UMetaStoryState* State, TSet<TWeakObjectPtr<UMetaStoryState>>& ExpandedStates);

	UE_API void MoveSelectedStates(UMetaStoryState* TargetState, const EMetaStoryViewModelInsert RelativeLocation);

	UE_API void PasteStatesAsChildrenFromText(const FString& TextToImport, UMetaStoryState* ParentState, const int32 IndexToInsertAt);

	UE_API void HandleIdentifierChanged(const UMetaStory& MetaStory) const;
	
	UE_API void BindToDebuggerDelegates();

	UE_API void PasteNodesToState(TNotNull<UMetaStoryEditorData*> InEditorData, TNotNull<UMetaStoryState*> InState, UE::MetaStoryEditor::FMetaStoryClipboardEditorData& InProcessedClipboard);

	TWeakObjectPtr<UMetaStoryEditorData> TreeDataWeak;
	TSet<TWeakObjectPtr<UMetaStoryState>> SelectedStates;

#if WITH_METASTORY_TRACE_DEBUGGER
	UE_API void HandleBreakpointsChanged(const UMetaStory& MetaStory);
	UE_API void HandlePostCompile(const UMetaStory& MetaStory);

	TSharedRef<FMetaStoryDebugger> Debugger;
	TArray<FGuid> ActiveStates;
#endif // WITH_METASTORY_TRACE_DEBUGGER
	
	FOnAssetChanged OnAssetChanged;
	FOnStatesChanged OnStatesChanged;
	FOnStateAdded OnStateAdded;
	FOnStatesRemoved OnStatesRemoved;
	FOnStatesMoved OnStatesMoved;
	FOnStateNodesChanged OnStateNodesChanged;
	FOnSelectionChanged OnSelectionChanged;
	FOnBringNodeToFocus OnBringNodeToFocus;
};

/** Helper class to allow to copy bindings into clipboard. */
UCLASS(MinimalAPI, Hidden)
class UMetaStoryClipboardBindings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FMetaStoryPropertyPathBinding> Bindings;
};

#undef UE_API
