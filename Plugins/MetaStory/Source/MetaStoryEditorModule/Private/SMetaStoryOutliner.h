// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class UMetaStory;
class FMetaStoryViewModel;
struct FPropertyChangedEvent;
class UMetaStoryState;
class FUICommandList;

namespace UE::MetaStory
{
class SCompactTreeEditorView;
}

class SMetaStoryOutliner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaStoryOutliner) {}
	SLATE_END_ARGS()

	SMetaStoryOutliner();
	virtual ~SMetaStoryOutliner() override;

	void Construct(const FArguments& InArgs, TSharedRef<FMetaStoryViewModel> MetaStoryViewModel, const TSharedRef<FUICommandList>& InCommandList);

private:
	// ViewModel handlers
	void HandleModelAssetChanged();
	void HandleModelStatesRemoved(const TSet<UMetaStoryState*>& AffectedParents);
	void HandleModelStatesMoved(const TSet<UMetaStoryState*>& AffectedParents, const TSet<UMetaStoryState*>& MovedStates);
	void HandleModelStateAdded(UMetaStoryState* ParentState, UMetaStoryState* NewState);
	void HandleModelStatesChanged(const TSet<UMetaStoryState*>& AffectedStates, const FPropertyChangedEvent& PropertyChangedEvent);
	void HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates);
	void HandleTreeViewSelectionChanged(TConstArrayView<FGuid> SelectedStateIDs);
	void HandleVisualThemeChanged(const UMetaStory& MetaStory);

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	TSharedPtr<SWidget> HandleContextMenuOpening();

	// Action handlers
	// @todo: these are also defined in the SMetaStoryView, figure out how to share code.
	UMetaStoryState* GetFirstSelectedState() const;
	void HandleAddSiblingState();
	void HandleAddChildState();
	void HandleCutSelectedStates();
	void HandleCopySelectedStates();
	void HandlePasteStatesAsSiblings();
	void HandlePasteStatesAsChildren();
	void HandleDuplicateSelectedStates();
	void HandleDeleteStates();
	void HandleEnableSelectedStates();
	void HandleDisableSelectedStates();

	bool HasSelection() const;
	bool CanPaste() const;
	bool CanEnableStates() const;
	bool CanDisableStates() const;

	void BindCommands();

	TSharedPtr<FMetaStoryViewModel> MetaStoryViewModel;

	TSharedPtr<UE::MetaStory::SCompactTreeEditorView> CompactTreeView;

	TSharedPtr<FUICommandList> CommandList;

	bool bItemsDirty = false;
	bool bUpdatingSelection = false;
};
