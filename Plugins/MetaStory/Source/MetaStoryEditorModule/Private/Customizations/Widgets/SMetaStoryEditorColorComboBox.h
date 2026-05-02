// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryEditorTypes.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class IPropertyHandle;
class SBorder;
class UMetaStoryEditorData;
struct FMetaStoryEditorColor;
struct FMetaStoryEditorColorRef;
template<typename OptionType> class SComboBox;

class SMetaStoryEditorColorComboBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaStoryEditorColorComboBox){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<IPropertyHandle> InColorRefHandle, UMetaStoryEditorData* InEditorData);

private:
	const FMetaStoryEditorColor* FindColorEntry(const FMetaStoryEditorColorRef& ColorRef) const;

	FText GetDisplayName(FMetaStoryEditorColorRef ColorRef) const;
	FLinearColor GetColor(FMetaStoryEditorColorRef ColorRef) const;

	TSharedRef<SWidget> GenerateColorOptionWidget(TSharedPtr<FMetaStoryEditorColorRef> ColorRef);
	TSharedRef<SWidget> GenerateColorWidget(const FMetaStoryEditorColorRef& ColorRef);

	void RefreshColorOptions();

	void OnSelectionChanged(TSharedPtr<FMetaStoryEditorColorRef> SelectedColorRefOption, ESelectInfo::Type SelectInfo);

	void UpdatedSelectedColorWidget();

	TWeakObjectPtr<UMetaStoryEditorData> WeakEditorData;

	TSharedPtr<IPropertyHandle> ColorRefHandle;
	TSharedPtr<IPropertyHandle> ColorIDHandle;

	TSharedPtr<SBorder> SelectedColorBorder;
	TSharedPtr<SComboBox<TSharedPtr<FMetaStoryEditorColorRef>>> ColorComboBox;

	TArray<TSharedPtr<FMetaStoryEditorColorRef>> ColorRefOptions;

	FMetaStoryEditorColorRef SelectedColorRef;
};
