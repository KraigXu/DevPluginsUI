// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryDragDrop.h"

#include "MetaStoryState.h"
#include "MetaStoryViewModel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

TSharedPtr<SWidget> FMetaStorySelectedDragDrop::GetDefaultDecorator() const
{
	// Display all dragged nodes, enable the ones that can be moved into the current target node. 
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	if (ViewModel)
	{
		TArray<UMetaStoryState*> SelectedStates;
		ViewModel->GetSelectedStates(SelectedStates);

		for (UMetaStoryState* State : SelectedStates)
		{
			Box->AddSlot()
			.Padding(FMargin(4, 2))
			[
				SNew(STextBlock)
				.Text(FText::FromName(State->Name))
				.IsEnabled_Lambda([this]()
				{
					return bCanDrop;
				})
			];
		}
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			Box
		];
}
