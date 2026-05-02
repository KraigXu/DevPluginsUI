// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class FMetaStoryViewModel;
class ITableRow;
class SScrollBar;
class STableViewBase;
namespace ESelectInfo { enum Type : int; }
struct FPropertyChangedEvent;
class UMetaStoryState;
class SScrollBox;
class FUICommandList;

class SMetaStoryView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaStoryView) {}
	SLATE_END_ARGS()

	SMetaStoryView();
	virtual ~SMetaStoryView() override;

	void Construct(const FArguments& InArgs, TSharedRef<FMetaStoryViewModel> MetaStoryViewModel, const TSharedRef<FUICommandList>& InCommandList);

	void SavePersistentExpandedStates();

	TSharedPtr<FMetaStoryViewModel> GetViewModel() const;

	void SetSelection(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates) const;

private:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void UpdateTree(bool bExpandPersistent = false);

	//~ Editor/User settings handlers
	void HandleUserSettingsChanged();

	//~ ViewModel handlers
	void HandleModelAssetChanged();
	void HandleModelStatesRemoved(const TSet<UMetaStoryState*>& AffectedParents);
	void HandleModelStatesMoved(const TSet<UMetaStoryState*>& AffectedParents, const TSet<UMetaStoryState*>& MovedStates);
	void HandleModelStateAdded(UMetaStoryState* ParentState, UMetaStoryState* NewState);
	void HandleModelStatesChanged(const TSet<UMetaStoryState*>& AffectedStates, const FPropertyChangedEvent& PropertyChangedEvent);
	void HandleModelStateNodesChanged(const UMetaStoryState* AffectedState);
	void HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates);

	//~ Treeview handlers
	TSharedRef<ITableRow> HandleGenerateRow(TWeakObjectPtr<UMetaStoryState> InState, const TSharedRef<STableViewBase>& InOwnerTableView);
	void HandleGetChildren(TWeakObjectPtr<UMetaStoryState> InParent, TArray<TWeakObjectPtr<UMetaStoryState>>& OutChildren);
	void HandleTreeSelectionChanged(TWeakObjectPtr<UMetaStoryState> InSelectedItem, ESelectInfo::Type SelectionType);
	void HandleTreeExpansionChanged(TWeakObjectPtr<UMetaStoryState> InSelectedItem, bool bExpanded);
	
	TSharedPtr<SWidget> HandleContextMenuOpening();
	TSharedRef<SWidget> HandleGenerateSettingsMenu();

	//~ Action handlers
	//~ @todo: these are also defined in the outliner, figure out how to share code.
	UMetaStoryState* GetFirstSelectedState() const;
	FReply HandleAddStateButton();
	void HandleAddSiblingState();
	void HandleAddChildState();
	void HandleCutSelectedStates();
	void HandleCopySelectedStates();
	void HandlePasteStatesAsSiblings();
	void HandlePasteStatesAsChildren();
	void HandleDuplicateSelectedStates();
	void HandlePasteNodesToState();
	void HandleRenameState();
	void HandleDeleteStates();
	void HandleEnableSelectedStates();
	void HandleDisableSelectedStates();

	bool HasSelection() const;
	bool CanPasteStates() const;
	bool CanEnableStates() const;
	bool CanDisableStates() const;
	bool CanPasteNodesToSelectedStates() const;

	void BindCommands();

	TSharedPtr<FMetaStoryViewModel> MetaStoryViewModel;

	TSharedPtr<STreeView<TWeakObjectPtr<UMetaStoryState>>> TreeView;
	TSharedPtr<SScrollBar> ExternalScrollbar;
	TSharedPtr<SScrollBox> ViewBox;
	TArray<TWeakObjectPtr<UMetaStoryState>> Subtrees;

	TSharedPtr<FUICommandList> CommandList;

	UMetaStoryState* RequestedRenameState;
	FDelegateHandle SettingsChangedHandle;
	bool bItemsDirty;
	bool bUpdatingSelection;
};
