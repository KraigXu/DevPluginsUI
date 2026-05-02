// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaStoryContextMenuButton.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "MetaStoryState.h"
#include "MetaStoryViewModel.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

void SMetaStoryContextMenuButton::Construct(const FArguments& InArgs, const TSharedRef<FMetaStoryViewModel>& InStateTreeViewModel, TWeakObjectPtr<UMetaStoryState> InOwnerState, const FGuid& InNodeID, bool InbIsTransition)
{
	MetaStoryViewModel = InStateTreeViewModel.ToSharedPtr();
	OwnerStateWeak = InOwnerState;
	NodeID = InNodeID;

	bIsStateTransition = false;
	bIsTransition = InbIsTransition;
	if (bIsTransition)
	{
		if (UMetaStoryState* OwnerState = OwnerStateWeak.Get())
		{
			for (const FMetaStoryTransition& Transition : OwnerState->Transitions)
			{
				if (NodeID == Transition.ID)
				{
					bIsStateTransition = true;
					break;
				}
			}
		}
	}

	SButton::FArguments ButtonArgs;

	ButtonArgs
	.OnClicked_Lambda([this]()
	{
		MetaStoryViewModel->BringNodeToFocus(OwnerStateWeak.Get(), NodeID);
		return FReply::Handled();
	})
	.ButtonStyle(InArgs._ButtonStyle)
	.ContentPadding(InArgs._ContentPadding)
	[
		SAssignNew(MenuAnchor, SMenuAnchor)
		.Placement(MenuPlacement_BelowAnchor)
		.OnGetMenuContent(FOnGetContent::CreateSP(this, &SMetaStoryContextMenuButton::MakeContextMenu))
		[
			InArgs._Content.Widget
		]
	];

	SButton::Construct(ButtonArgs);
}

FReply SMetaStoryContextMenuButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	MetaStoryViewModel->BringNodeToFocus(OwnerStateWeak.Get(), NodeID);

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (MenuAnchor.IsValid())
		{
			if (MenuAnchor->ShouldOpenDueToClick())
			{
				MenuAnchor->SetIsOpen(true);
			}
			else
			{
				MenuAnchor->SetIsOpen(false);
			}

			return FReply::Handled();
		}
	}

	return SButton::OnMouseButtonUp(MyGeometry, MouseEvent);
}

TSharedRef<SWidget> SMetaStoryContextMenuButton::MakeContextMenu() const
{
	FMenuBuilder MenuBuilder(/*ShouldCloseWindowAfterMenuSelection*/true, /*CommandList*/nullptr);

	if (MetaStoryViewModel.IsValid() && OwnerStateWeak.IsValid())
	{
		MenuBuilder.BeginSection(FName("Edit"), LOCTEXT("Edit", "Edit"));

		// Copy
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CopyItem", "Copy"),
			LOCTEXT("CopyItemTooltip", "Copy this item"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
			FUIAction(
			FExecuteAction::CreateSPLambda(this, [this]()
			{
				MetaStoryViewModel->CopyNode(OwnerStateWeak, NodeID);
			}),
			FCanExecuteAction::CreateSPLambda(this, [this]()
			{
				return !bIsTransition || bIsStateTransition;
			})
		));

		// Copy all
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CopyAllItems", "Copy all"),
			LOCTEXT("CopyAllItemsTooltip", "Copy all items"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
			FUIAction(
				FExecuteAction::CreateSPLambda(this, [this]()
					{
						MetaStoryViewModel->CopyAllNodes(OwnerStateWeak, NodeID);
					}),
				FCanExecuteAction::CreateSPLambda(this, [this]()
					{
						return !bIsTransition || bIsStateTransition;
					})
			));

		// Paste
		MenuBuilder.AddMenuEntry(
			LOCTEXT("PasteItem", "Paste"),
			LOCTEXT("PasteItemTooltip", "Paste into this item"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste"),
			FUIAction(
			FExecuteAction::CreateSPLambda(this, [this]()
			{
				MetaStoryViewModel->PasteNode(OwnerStateWeak, NodeID);
			}),
			FCanExecuteAction::CreateSPLambda(this, [this]()
			{
				return !bIsTransition || bIsStateTransition;
			})
		));

		// Duplicate
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DuplicateItem", "Duplicate"),
			LOCTEXT("DuplicateItemTooltip", "Duplicate this item"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Duplicate"),
			FUIAction(
			FExecuteAction::CreateSPLambda(this, [&]()
			{
				MetaStoryViewModel->DuplicateNode(OwnerStateWeak, NodeID);
			}),
			FCanExecuteAction::CreateSPLambda(this, [this]()
			{
				return !bIsTransition || bIsStateTransition;
			})
		));

		// Delete
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteItem", "Delete"),
			LOCTEXT("DeleteItemTooltip", "Delete this item"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
			FUIAction(
			FExecuteAction::CreateSPLambda(this, [&]()
			{
				MetaStoryViewModel->DeleteNode(OwnerStateWeak, NodeID);
			}),
			FCanExecuteAction::CreateSPLambda(this, [this]()
			{
				return !bIsTransition || bIsStateTransition;
			})
		));

		// Delete All
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteAllItems", "Delete all"),
			LOCTEXT("DeleteAllItemsTooltip", "Delete all items"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
			FUIAction(
			FExecuteAction::CreateSPLambda(this, [&]()
			{
				MetaStoryViewModel->DeleteAllNodes(OwnerStateWeak, NodeID);
			}),
			FCanExecuteAction::CreateSPLambda(this, [this]()
			{
				return !bIsTransition || bIsStateTransition;
			})
		));

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
