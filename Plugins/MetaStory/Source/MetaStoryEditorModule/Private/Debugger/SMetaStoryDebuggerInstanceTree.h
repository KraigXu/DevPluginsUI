// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_METASTORY_TRACE_DEBUGGER

#include "Widgets/Views/STreeView.h"

namespace RewindDebugger
{
	class FRewindDebuggerTrack;
}

struct FMetaStoryDebugger;

class SMetaStoryDebuggerInstanceTree : public SCompoundWidget
{
	using FOnSelectionChanged = STreeView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>::FOnSelectionChanged;
	using FOnMouseButtonDoubleClick = STreeView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>::FOnMouseButtonDoubleClick;

public:
	SLATE_BEGIN_ARGS(SMetaStoryDebuggerInstanceTree) { }
	SLATE_ARGUMENT(TArray<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>*, InstanceTracks);
	SLATE_ARGUMENT(TSharedPtr< SScrollBar >, ExternalScrollBar);
	SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_EVENT(FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick)
	SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)
	SLATE_EVENT(FSimpleDelegate, OnExpansionChanged)
	SLATE_EVENT(FOnTableViewScrolled, OnScrolled)
	SLATE_END_ARGS()

	SMetaStoryDebuggerInstanceTree();
	virtual ~SMetaStoryDebuggerInstanceTree();

	void Construct(const FArguments& InArgs);

	void Refresh();
	void RestoreExpansion();

	TSharedPtr<RewindDebugger::FRewindDebuggerTrack> GetSelection() const;
	void SetSelection(const TSharedPtr<RewindDebugger::FRewindDebuggerTrack>& SelectedItem) const;
	void ScrollTo(const TSharedPtr<RewindDebugger::FRewindDebuggerTrack>& SelectedItem) const;
	void ScrollTo(double ScrollOffset) const;

private:
	TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	void TreeExpansionChanged(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> Item, bool bShouldBeExpanded) const;
	
	TArray<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>* InstanceTracks = nullptr;

	TSharedPtr<STreeView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>> TreeView;

	FSimpleDelegate OnExpansionChanged;	
};


#endif // WITH_METASTORY_TRACE_DEBUGGER