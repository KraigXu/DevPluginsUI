// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"

class FMetaStoryViewModel;

class FMetaStorySelectedDragDrop : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FActionTreeViewDragDrop, FDecoratedDragDropOp);

	static TSharedRef<FMetaStorySelectedDragDrop> New(TSharedPtr<FMetaStoryViewModel> InViewModel)
	{
		TSharedRef<FMetaStorySelectedDragDrop> Operation = MakeShared<FMetaStorySelectedDragDrop>();
		Operation->ViewModel = InViewModel;
		Operation->Construct();

		return Operation;
	}

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

	void SetCanDrop(const bool bState)
	{
		bCanDrop = bState;
	}

	TSharedPtr<FMetaStoryViewModel> ViewModel;
	bool bCanDrop = false;
};