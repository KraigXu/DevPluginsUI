// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "Widgets/Input/SButton.h"

class SMenuAnchor;
class UMetaStoryEditorData;
struct FGeometry;
struct FPointerEvent;
class FMetaStoryViewModel;
class UMetaStoryState;

class SMetaStoryContextMenuButton : public SButton
{
public:
	SLATE_BEGIN_ARGS(SMetaStoryContextMenuButton)
		: _Content()
		, _ButtonStyle(&FCoreStyle::Get().GetWidgetStyle< FButtonStyle >("Button"))
		, _ContentPadding(FMargin(4.0, 2.0))
		{
		}

		/** Slot for this button's content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)

		/** The visual style of the button */
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)

		/** Spacing between button's border and the content. */
		SLATE_ATTRIBUTE(FMargin, ContentPadding)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FMetaStoryViewModel>& InStateTreeViewModel, const TWeakObjectPtr<UMetaStoryState> InOwnerState, const FGuid& InSourceID, bool InbIsTransition = false);

	//~ SWidget interface
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ SWidget overrides

	TSharedRef<SWidget> MakeContextMenu() const;

	TSharedPtr<FMetaStoryViewModel> MetaStoryViewModel;
	TWeakObjectPtr<UMetaStoryState> OwnerStateWeak;
	FGuid NodeID;

	uint8 bIsTransition : 1;
	// We have State Transition, Task Transition and Default Transition back to root
	uint8 bIsStateTransition : 1;

private:
	TSharedPtr<SMenuAnchor> MenuAnchor;
};
