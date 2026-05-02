// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaStoryCompactTreeEditorView.h"

#include "MetaStoryDragDrop.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorStyle.h"
#include "MetaStoryViewModel.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SMetaStoryCompactTreeEditorView)

#define LOCTEXT_NAMESPACE "OutlinerStateTreeView"

namespace UE::MetaStory
{

FSlateColor UE::MetaStory::CompactTreeView::FMetaStoryStateItemLinkData::GetBorderColor() const
{
	if (LinkState == LinkState_None)
	{
		return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	}

	static const FName NAME_LinkingIn = "Colors.StateLinkingIn";
	static const FName NAME_StateLinkedOut = "Colors.StateLinkedOut";
	const FName ColorName = (LinkState & LinkState_LinkingIn) != 0 ? NAME_LinkingIn : NAME_StateLinkedOut;
	return FMetaStoryEditorStyle::Get().GetColor(ColorName);
}

void SCompactTreeEditorView::Construct(const FArguments& InArgs, const TSharedPtr<FMetaStoryViewModel>& InViewModel)
{
	MetaStoryViewModel = InViewModel;
	WeakStateTreeEditorData = InArgs._MetaStoryEditorData;
	bSubtreesOnly = InArgs._SubtreesOnly;
	bSelectableStatesOnly = InArgs._SelectableStatesOnly;
	bShowLinkedStates = InArgs._ShowLinkedStates;

	const UMetaStory* MetaStoryAsset = InViewModel
		? InViewModel->GetStateTree()
		: InArgs._MetaStoryEditorData
			? InArgs._MetaStoryEditorData->GetTypedOuter<UMetaStory>()
			: nullptr;

	if (ensureMsgf(MetaStoryAsset != nullptr
		, TEXT("Expecting either a valid MetaStoryViewModel or MetaStoryEditorData to construct the view")))
	{
		SCompactTreeView::Construct(
			SCompactTreeView::FArguments()
			.SelectionMode(InArgs._SelectionMode)
			.OnSelectionChanged(InArgs._OnSelectionChanged)
			.OnContextMenuOpening(InArgs._OnContextMenuOpening)
			, MetaStoryAsset);
	}
}

void SCompactTreeEditorView::CacheStatesInternal()
{
	if (const UMetaStoryEditorData* MetaStoryEditorData = WeakStateTreeEditorData.Get())
	{
		for (const UMetaStoryState* SubTree : MetaStoryEditorData->SubTrees)
		{
			CacheState(RootItem, SubTree);
		}
	}
}

void SCompactTreeEditorView::CacheState(TSharedPtr<FStateItem> ParentNode, const UMetaStoryState* State)
{
	if (!State)
	{
		return;
	}
	const UMetaStoryEditorData* MetaStoryEditorData = WeakStateTreeEditorData.Get();
	if (!MetaStoryEditorData)
	{
		return;
	}

	bool bShouldAdd = true;
	if (bSubtreesOnly
		&& State->Type != EMetaStoryStateType::Subtree)
	{
		bShouldAdd = false;
	}

	if (bSelectableStatesOnly
		&& State->SelectionBehavior == EMetaStoryStateSelectionBehavior::None)
	{
		bShouldAdd = false;
	}

	if (bShouldAdd)
	{
		const TSharedRef<FStateItem> StateItem = CreateStateItemInternal();
		UE::MetaStory::CompactTreeView::FMetaStoryStateItemLinkData& CustomData = StateItem->CustomData.GetMutable<UE::MetaStory::CompactTreeView::FMetaStoryStateItemLinkData>();
		StateItem->Desc = FText::FromName(State->Name);
		StateItem->TooltipText = FText::FromString(State->Description);
		StateItem->StateID = State->ID;
		StateItem->bIsEnabled = State->bEnabled;
		CustomData.bIsSubTree = State->Type == EMetaStoryStateType::Subtree;
		StateItem->Color = FMetaStoryStyle::Get().GetSlateColor("MetaStory.CompactView.State");
		if (const FMetaStoryEditorColor* FoundColor = MetaStoryEditorData->FindColor(State->ColorRef))
		{
			StateItem->Color = FoundColor->Color;
		}

		StateItem->Icon = FMetaStoryEditorStyle::GetBrushForSelectionBehaviorType(State->SelectionBehavior, !State->Children.IsEmpty(), State->Type);

		// Linked states
		if (State->Type == EMetaStoryStateType::Linked)
		{
			CustomData.bIsLinked = true;
			CustomData.LinkedDesc = FText::FromName(State->LinkedSubtree.Name);
		}
		else if (State->Type == EMetaStoryStateType::LinkedAsset)
		{
			CustomData.bIsLinked = true;
			CustomData.LinkedDesc = FText::FromString(GetNameSafe(State->LinkedAsset.Get()));
		}

		ParentNode->Children.Add(StateItem);

		ParentNode = StateItem;
	}

	for (const UMetaStoryState* ChildState : State->Children)
	{
		CacheState(ParentNode, ChildState);
	}
}

TSharedRef<SCompactTreeView::FStateItem> SCompactTreeEditorView::CreateStateItemInternal() const
{
	const TSharedRef<FStateItem> StateItem = MakeShared<FStateItem>();
	StateItem->CustomData = TInstancedStruct<UE::MetaStory::CompactTreeView::FMetaStoryStateItemLinkData>::Make({});
	return StateItem;
}

TSharedRef<STableRow<TSharedPtr<SCompactTreeView::FStateItem>>> SCompactTreeEditorView::GenerateStateItemRowInternal(
	TSharedPtr<FStateItem> Item
	, const TSharedRef<STableViewBase>& OwnerTable
	, TSharedRef<SHorizontalBox> Container)
{
	using namespace UE::MetaStory::CompactTreeView;
	const FMetaStoryStateItemLinkData& LinkData = Item->CustomData.Get<FMetaStoryStateItemLinkData>();

	// Link
	if (LinkData.bIsLinked)
	{
		// Link icon
		Container->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			.AutoWidth()
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.StateLinked"))
		];

		// Linked name
		Container->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(LinkData.LinkedDesc)
			];
	}

	return SNew(STableRow<TSharedPtr<FStateItem>>, OwnerTable)
		.OnDragDetected(this, &SCompactTreeEditorView::HandleDragDetected)
		.OnDragLeave(this, &SCompactTreeEditorView::HandleDragLeave)
		.OnCanAcceptDrop(this, &SCompactTreeEditorView::HandleCanAcceptDrop)
		.OnAcceptDrop(this, &SCompactTreeEditorView::HandleAcceptDrop)
		[
			SNew(SBorder)
				.BorderBackgroundColor_Lambda([Item]
					{
						if (const FStateItem* StateItem = Item.Get())
						{
							return StateItem->CustomData.Get<FMetaStoryStateItemLinkData>().GetBorderColor();
						}
						return FSlateColor(FLinearColor::Transparent);
					})
				[
					Container
				]
		];
}

void SCompactTreeEditorView::OnSelectionChangedInternal(TConstArrayView<TSharedPtr<FStateItem>> SelectedStates)
{
	using namespace UE::MetaStory::CompactTreeView;

	if (bShowLinkedStates && MetaStoryViewModel)
	{
		ResetLinkedStates();

		// Find the Linked items
		TArray<FGuid> LinkingIn;
		TArray<FGuid> LinkedOut;
		for (const TSharedPtr<FStateItem>& StateItem : SelectedStates)
		{
			MetaStoryViewModel->GetLinkStates(StateItem->StateID, LinkingIn, LinkedOut);
		}

		// Set the outline
		{
			TArray<TSharedPtr<FStateItem>> FoundStates;
			FoundStates.Reserve(LinkingIn.Num());
			FindStatesByIDRecursive(FilteredRootItem, LinkingIn, FoundStates);

			for (const TSharedPtr<FStateItem>& Item : FoundStates)
			{
				PreviousLinkedStates.Add(Item);
				Item->CustomData.GetMutable<FMetaStoryStateItemLinkData>().LinkState |= FMetaStoryStateItemLinkData::LinkState_LinkingIn;
			}
		}
		{
			TArray<TSharedPtr<FStateItem>> FoundStates;
			FoundStates.Reserve(LinkedOut.Num());
			FindStatesByIDRecursive(FilteredRootItem, LinkedOut, FoundStates);

			for (const TSharedPtr<FStateItem>& Item : FoundStates)
			{
				PreviousLinkedStates.AddUnique(Item);
				Item->CustomData.GetMutable<FMetaStoryStateItemLinkData>().LinkState |= FMetaStoryStateItemLinkData::LinkState_LinkedOut;
			}
		}
	}
}

void SCompactTreeEditorView::OnUpdatingFilteredRootInternal()
{
	ResetLinkedStates();
}

void SCompactTreeEditorView::Refresh(const UMetaStoryEditorData* NewStateTreeEditorData)
{
	if (NewStateTreeEditorData)
	{
		WeakStateTreeEditorData = NewStateTreeEditorData;
	}

	SCompactTreeView::Refresh();
}

void SCompactTreeEditorView::ResetLinkedStates()
{
	using namespace UE::MetaStory::CompactTreeView;

	for (TWeakPtr<FStateItem>& PreviousLinkedState : PreviousLinkedStates)
	{
		if (const TSharedPtr<FStateItem> Pin = PreviousLinkedState.Pin())
		{
			Pin->CustomData.GetMutable<FMetaStoryStateItemLinkData>().LinkState = FMetaStoryStateItemLinkData::LinkState_None;
		}
	}
	PreviousLinkedStates.Reset();
}

FReply SCompactTreeEditorView::HandleDragDetected(const FGeometry&, const FPointerEvent&) const
{
	return FReply::Handled().BeginDragDrop(FMetaStorySelectedDragDrop::New(MetaStoryViewModel));
}

void SCompactTreeEditorView::HandleDragLeave(const FDragDropEvent& DragDropEvent) const
{
	const TSharedPtr<FMetaStorySelectedDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FMetaStorySelectedDragDrop>();
	if (DragDropOperation.IsValid())
	{
		DragDropOperation->SetCanDrop(false);
	}
}

TOptional<EItemDropZone> SCompactTreeEditorView::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FStateItem> TargetState) const
{
	if (MetaStoryViewModel)
	{
		const TSharedPtr<FMetaStorySelectedDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FMetaStorySelectedDragDrop>();
		if (DragDropOperation.IsValid())
		{
			DragDropOperation->SetCanDrop(true);

			// Cannot drop on selection or child of selection.
			if (MetaStoryViewModel && MetaStoryViewModel->IsChildOfSelection(MetaStoryViewModel->GetMutableStateByID(TargetState->StateID)))
			{
				DragDropOperation->SetCanDrop(false);
				return TOptional<EItemDropZone>();
			}

			return DropZone;
		}
	}

	return TOptional<EItemDropZone>();
}

FReply SCompactTreeEditorView::HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FStateItem> TargetState) const
{
	if (MetaStoryViewModel)
	{
		const TSharedPtr<FMetaStorySelectedDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FMetaStorySelectedDragDrop>();
		if (DragDropOperation.IsValid())
		{
			if (MetaStoryViewModel)
			{
				if (DropZone == EItemDropZone::AboveItem)
				{
					MetaStoryViewModel->MoveSelectedStatesBefore(MetaStoryViewModel->GetMutableStateByID(TargetState->StateID));
				}
				else if (DropZone == EItemDropZone::BelowItem)
				{
					MetaStoryViewModel->MoveSelectedStatesAfter(MetaStoryViewModel->GetMutableStateByID(TargetState->StateID));
				}
				else
				{
					MetaStoryViewModel->MoveSelectedStatesInto(MetaStoryViewModel->GetMutableStateByID(TargetState->StateID));
				}

				return FReply::Handled();
			}
		}
	}

	return FReply::Unhandled();
}

} // UE::MetaStory

#undef LOCTEXT_NAMESPACE
