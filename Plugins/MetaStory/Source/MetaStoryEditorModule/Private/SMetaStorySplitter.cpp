// Copyright Epic Games, Inc. All Rights Reserved.
#include "SMetaStorySplitter.h"
#include "SMetaStoryView.h"
#include "MetaStory.h"
#include "MetaStoryDiffHelper.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryViewModel.h"

namespace UE::MetaStory::Diff
{
void SDiffSplitter::Construct(const FArguments& InArgs)
{
	Splitter = SNew(SSplitter).PhysicalSplitterHandleSize(5.f).Orientation(EOrientation::Orient_Horizontal);

	for (const FSlot::FSlotArguments& SlotArgs : InArgs._Slots)
	{
		AddSlot(SlotArgs);
	}

	ChildSlot
		[
			Splitter.ToSharedRef()
		];
}

void SDiffSplitter::AddSlot(const FSlot::FSlotArguments& SlotArgs, int32 Index)
{
	if (Index == INDEX_NONE)
	{
		Index = Panels.Num();
	}

	Splitter->AddSlot(Index)
		.Value(SlotArgs._Value)
		[
			SNew(SBox).Padding(15.f, 0.f, 15.f, 0.f)
			[
				SlotArgs._MetaStoryView.ToSharedRef()
			]
		];

	Panels.Insert(
		{
			SlotArgs._MetaStoryView,
			SlotArgs._MetaStory,
			SlotArgs._IsReadonly,
			SlotArgs._DifferencesWithRightPanel,
		},
		Index);
	if (SlotArgs._MetaStoryView)
	{
		SlotArgs._MetaStoryView->GetViewModel()->GetOnSelectionChanged().AddSP(this, &SDiffSplitter::HandleSelectionChanged);
	}
}

void SDiffSplitter::HandleSelectionChanged(const FStateSoftPath& StatePath, const FStateSoftPath& SecondaryStatePath)
{
	if (StatePath != SelectedState)
	{
		SelectedState = StatePath;

		for (const FPanel& Panel : Panels)
		{
			const FMetaStoryViewModel* ViewModel = Panel.MetaStoryView->GetViewModel().Get();
			const UMetaStoryEditorData* EditorData = Cast<UMetaStoryEditorData>(Panel.MetaStory->EditorData);
			if (EditorData != nullptr && ViewModel != nullptr)
			{
				UMetaStoryState* PanelState = SelectedState.ResolvePath(EditorData);
				if (!PanelState)
				{
					PanelState = SecondaryStatePath.ResolvePath(EditorData);
				}

				TArray<UMetaStoryState*> CurSelectedStates;
				ViewModel->GetSelectedStates(CurSelectedStates);
				if (CurSelectedStates.Num() != 1 || CurSelectedStates[0] != PanelState)
				{
					Panel.MetaStoryView->SetSelection({PanelState});
				}
			}
		}
	}
}

void SDiffSplitter::HandleSelectionChanged(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates)
{
	if (const UMetaStoryState* State = SelectedStates.Num() == 1 ? SelectedStates[0].Get() : nullptr)
	{
		const FStateSoftPath StatePath = FStateSoftPath(State);
		HandleSelectionChanged(StatePath, FStateSoftPath());
	}
}

SDiffSplitter::FSlot::FSlotArguments SDiffSplitter::Slot()
{
	return FSlot::FSlotArguments(MakeUnique<FSlot>());
}

} // UE::MetaStory::Diff