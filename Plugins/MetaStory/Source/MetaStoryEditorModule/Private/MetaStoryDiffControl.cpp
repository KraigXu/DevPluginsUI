// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryDiffControl.h"
#include "SMetaStoryView.h"
#include "MetaStory.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryViewModel.h"
#include "MetaStoryDiffHelper.h"
#include "Framework/Commands/UICommandList.h"

#define LOCTEXT_NAMESPACE "SMetaStoryDif"

namespace UE::MetaStory::Diff
{

FText FDiffControl::RightRevision = LOCTEXT("OlderRevisionIdentifier", "Right Revision");

TSharedRef<SWidget> FDiffControl::GenerateSingleEntryWidget(FSingleDiffEntry DiffEntry, const FText ObjectName)
{
	return SNew(STextBlock)
		.Text(GetMetaStoryDiffMessage(DiffEntry, ObjectName, true))
		.ToolTipText(GetMetaStoryDiffMessage(DiffEntry, ObjectName))
		.ColorAndOpacity(GetMetaStoryDiffMessageColor(DiffEntry));
}

void FDiffControl::OnSelectDiffEntry(const FSingleDiffEntry StateDiff)
{
	OnDiffEntryFocused.ExecuteIfBound();
	OnStateDiffEntryFocused.Broadcast(StateDiff);
}

void FDiffControl::GenerateTreeEntries(TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutDifferences)
{
	TArray<FSingleDiffEntry> DifferingProperties;

	for (int32 LeftIndex = 0; LeftIndex < DisplayedAssets.Num() - 1; ++LeftIndex)
	{
		const UMetaStory* LeftMetaStory = DisplayedAssets[LeftIndex].Get();
		if (!ensure(LeftMetaStory))
		{
			continue;
		}
		const TSharedPtr<FAsyncDiff> Diff = MetaStoryDifferences[LeftMetaStory].Right;
		Diff->FlushQueue(); // make sure differences are fully up-to-date
		Diff->GetStateTreeDifferences(DifferingProperties);
	}

	BindingDiffs.Empty();

	TSet<FString> ExistingEntryPaths;

	for (const FSingleDiffEntry& Difference : DifferingProperties)
	{
		FString DiffPath = Difference.Identifier.ToDisplayName();
		bool bGenerateNewEntry = true;
		if (IsBindingDiff(Difference.DiffType))
		{
			BindingDiffs.Add(Difference);
			bGenerateNewEntry = !ExistingEntryPaths.Contains(DiffPath);
		}

		if (bGenerateNewEntry)
		{
			TSharedPtr<FBlueprintDifferenceTreeEntry> Entry = MakeShared<FBlueprintDifferenceTreeEntry>(
				FOnDiffEntryFocused::CreateSP(AsShared(), &FDiffControl::OnSelectDiffEntry, Difference),
				FGenerateDiffEntryWidget::CreateStatic(&GenerateSingleEntryWidget, Difference, RightRevision));
			OutDifferences.Push(Entry);
			ExistingEntryPaths.Add(Difference.Identifier.ToDisplayName());
		}
	}
}

FDiffControl::FDiffControl(
	const UMetaStory* InOldObject,
	const UMetaStory* InNewObject,
	const FOnDiffEntryFocused& InSelectionCallback)
	: OnDiffEntryFocused(InSelectionCallback)
{
	if (InOldObject)
	{
		InsertObject(InOldObject);
	}

	if (InNewObject)
	{
		InsertObject(InNewObject);
	}
}

FDiffControl::~FDiffControl()
{
}

TSharedRef<SMetaStoryView> FDiffControl::InsertObject(TNotNull<const UMetaStory*> MetaStory)
{
	const FDiffWidgets DiffWidgets(MetaStory);
	TSharedRef<SMetaStoryView> TreeView = DiffWidgets.GetStateTreeWidget();

	const int32 Index = DisplayedAssets.Num();
	DisplayedAssets.Insert(TStrongObjectPtr<const UMetaStory>(MetaStory), Index);

	MetaStoryDifferences.Add(MetaStory, {});
	MetaStoryDiffWidgets.Add(MetaStory, DiffWidgets);

	// set up interaction with left panel
	if (DisplayedAssets.IsValidIndex(Index - 1))
	{
		const UMetaStory* OtherStateTree = DisplayedAssets[Index - 1].Get();
		const FDiffWidgets& OtherStateTreeDiff = MetaStoryDiffWidgets[OtherStateTree];
		const TSharedRef<SMetaStoryView> OtherTreeView = OtherStateTreeDiff.GetStateTreeWidget();

		MetaStoryDifferences[OtherStateTree].Right = MakeShared<FAsyncDiff>(OtherTreeView, TreeView);
		MetaStoryDifferences[MetaStory].Left = MetaStoryDifferences[OtherStateTree].Right;
	}
	// Set up interaction with right panel
	if (DisplayedAssets.IsValidIndex(Index + 1))
	{
		const UMetaStory* OtherStateTree = DisplayedAssets[Index + 1].Get();
		const FDiffWidgets& OtherStateTreeDiff = MetaStoryDiffWidgets[OtherStateTree];
		const TSharedRef<SMetaStoryView> OtherTreeView = OtherStateTreeDiff.GetStateTreeWidget();

		MetaStoryDifferences[OtherStateTree].Left = MakeShared<FAsyncDiff>(TreeView, OtherTreeView);
		MetaStoryDifferences[MetaStory].Right = MetaStoryDifferences[OtherStateTree].Left;
	}

	return TreeView;
}

TSharedRef<SMetaStoryView> FDiffControl::GetDetailsWidget(const UMetaStory* Object) const
{
	return MetaStoryDiffWidgets[Object].GetStateTreeWidget();
}

FDiffWidgets::FDiffWidgets(const UMetaStory* InMetaStory)
{
	UMetaStoryEditorData* EditorData = Cast<UMetaStoryEditorData>(InMetaStory->EditorData);
	MetaStoryViewModel = MakeShareable(new FMetaStoryViewModel());
	MetaStoryViewModel->Init(EditorData);
	SAssignNew(MetaStoryTreeView, SMetaStoryView, MetaStoryViewModel.ToSharedRef(), MakeShared<FUICommandList>());
}

TSharedRef<SMetaStoryView> FDiffWidgets::GetStateTreeWidget() const
{
	return MetaStoryTreeView.ToSharedRef();
}

} // UE::MetaStory::Diff
#undef LOCTEXT_NAMESPACE
