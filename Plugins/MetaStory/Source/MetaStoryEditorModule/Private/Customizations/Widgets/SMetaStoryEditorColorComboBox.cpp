// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaStoryEditorColorComboBox.h"
#include "DetailLayoutBuilder.h"
#include "GuidStructCustomization.h"
#include "MetaStoryEditorData.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

void SMetaStoryEditorColorComboBox::Construct(const FArguments& InArgs, TSharedPtr<IPropertyHandle> InColorRefHandle, UMetaStoryEditorData* InEditorData)
{
	WeakEditorData = InEditorData;
	ColorRefHandle = InColorRefHandle;
	ColorIDHandle = InColorRefHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorColorRef, ID));

	ColorRefHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &SMetaStoryEditorColorComboBox::UpdatedSelectedColorWidget));

	ChildSlot
	[
		SAssignNew(ColorComboBox, SComboBox<TSharedPtr<FMetaStoryEditorColorRef>>)
		.OptionsSource(&ColorRefOptions)
		.OnComboBoxOpening(this, &SMetaStoryEditorColorComboBox::RefreshColorOptions)
		.OnGenerateWidget(this, &SMetaStoryEditorColorComboBox::GenerateColorOptionWidget)
		.OnSelectionChanged(this, &SMetaStoryEditorColorComboBox::OnSelectionChanged)
		[
			SAssignNew(SelectedColorBorder, SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
		]
	];

	UpdatedSelectedColorWidget();
}

const FMetaStoryEditorColor* SMetaStoryEditorColorComboBox::FindColorEntry(const FMetaStoryEditorColorRef& ColorRef) const
{
	if (UMetaStoryEditorData* EditorData = WeakEditorData.Get())
	{
		return EditorData->FindColor(FMetaStoryEditorColorRef(ColorRef));
	}
	return nullptr;
}

FText SMetaStoryEditorColorComboBox::GetDisplayName(FMetaStoryEditorColorRef ColorRef) const
{
	if (const FMetaStoryEditorColor* ColorEntry = FindColorEntry(ColorRef))
	{
		return FText::FromString(ColorEntry->DisplayName);
	}
	return FText::GetEmpty();
}

FLinearColor SMetaStoryEditorColorComboBox::GetColor(FMetaStoryEditorColorRef ColorRef) const
{
	if (const FMetaStoryEditorColor* ColorEntry = FindColorEntry(ColorRef))
	{
		return ColorEntry->Color;
	}
	return FLinearColor(ForceInitToZero);
}

TSharedRef<SWidget> SMetaStoryEditorColorComboBox::GenerateColorOptionWidget(TSharedPtr<FMetaStoryEditorColorRef> ColorRef)
{
	return SNew(SBox)
		.WidthOverride(120.f)
		.Padding(5.f, 1.f)
		[
			GenerateColorWidget(*ColorRef)
		];
}

TSharedRef<SWidget> SMetaStoryEditorColorComboBox::GenerateColorWidget(const FMetaStoryEditorColorRef& ColorRef)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth(25)
		.VAlign(VAlign_Center)
		[
			SNew(SColorBlock)
			.Color(this, &SMetaStoryEditorColorComboBox::GetColor, ColorRef)
			.Size(FVector2D(16.0))
		]
		+ SHorizontalBox::Slot()
		.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SMetaStoryEditorColorComboBox::GetDisplayName, ColorRef)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

void SMetaStoryEditorColorComboBox::RefreshColorOptions()
{
	ColorRefOptions.Reset();

	UMetaStoryEditorData* EditorData = WeakEditorData.Get();
	if (!EditorData)
	{
		return;
	}

	for (const FMetaStoryEditorColor& Color : EditorData->Colors)
	{
		TSharedRef<FMetaStoryEditorColorRef> ColorRefOption = MakeShared<FMetaStoryEditorColorRef>(Color.ColorRef);
		ColorRefOptions.Add(ColorRefOption);
		if (SelectedColorRef == Color.ColorRef)
		{
			ColorComboBox->SetSelectedItem(ColorRefOption);
		}
	}
}

void SMetaStoryEditorColorComboBox::OnSelectionChanged(TSharedPtr<FMetaStoryEditorColorRef> SelectedColorRefOption, ESelectInfo::Type SelectInfo)
{
	if (SelectedColorRefOption.IsValid())
	{
		WriteGuidToProperty(ColorIDHandle, SelectedColorRefOption->ID);
		UpdatedSelectedColorWidget();
	}
}

void SMetaStoryEditorColorComboBox::UpdatedSelectedColorWidget()
{
	check(SelectedColorBorder.IsValid());

	TArray<const void*> RawData;
	ColorRefHandle->AccessRawData(RawData);

	if (RawData.IsEmpty())
	{
		SelectedColorBorder->ClearContent();
		return;
	}

	SelectedColorRef = *static_cast<const FMetaStoryEditorColorRef*>(RawData.Pop(EAllowShrinking::No));

	// Make "Multiple Values" content if there's at least one Color ID that differs
	for (const void* ColorRefRaw : RawData)
	{
		if (SelectedColorRef != *static_cast<const FMetaStoryEditorColorRef*>(ColorRefRaw))
		{
			SelectedColorBorder->SetContent(SNew(STextBlock)
				.Text(LOCTEXT("MultipleValues", "Multiple Values"))
				.Font(IDetailLayoutBuilder::GetDetailFont()));
			return;
		}
	}

	SelectedColorBorder->SetContent(GenerateColorWidget(SelectedColorRef));
}

#undef LOCTEXT_NAMESPACE
