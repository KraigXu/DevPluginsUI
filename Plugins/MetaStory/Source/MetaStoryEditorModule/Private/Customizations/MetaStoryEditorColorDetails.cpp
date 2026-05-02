// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorColorDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/Engine.h"
#include "ScopedTransaction.h"
#include "MetaStoryEditorData.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SMetaStoryEditorColorComboBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

//----------------------------------------------------------------//
// FMetaStoryEditorColorRefDetails
//----------------------------------------------------------------//

TSharedRef<IPropertyTypeCustomization> FMetaStoryEditorColorRefDetails::MakeInstance()
{
	return MakeShared<FMetaStoryEditorColorRefDetails>();
}

void FMetaStoryEditorColorRefDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	UMetaStoryEditorData* EditorData = GetEditorData(InStructPropertyHandle);
	if (!EditorData)
	{
		return;
	}

	HeaderRow
		.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			SNew(SMetaStoryEditorColorComboBox, InStructPropertyHandle, EditorData)
		];
}

UMetaStoryEditorData* FMetaStoryEditorColorRefDetails::GetEditorData(const TSharedRef<IPropertyHandle>& PropertyHandle) const
{
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);

	for (UObject* Outer : Objects)
	{
		if (UMetaStoryEditorData* EditorData = Cast<UMetaStoryEditorData>(Outer))
		{
			return EditorData;
		}

		if (UMetaStoryEditorData* EditorData = Outer->GetTypedOuter<UMetaStoryEditorData>())
		{
			return EditorData;
		}
	}

	return nullptr;
}

//----------------------------------------------------------------//
// FMetaStoryEditorColorDetails
//----------------------------------------------------------------//

TSharedRef<IPropertyTypeCustomization> FMetaStoryEditorColorDetails::MakeInstance()
{
	return MakeShared<FMetaStoryEditorColorDetails>();
}

void FMetaStoryEditorColorDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	ColorPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorColor, Color));

	TSharedPtr<IPropertyHandle> NamePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorColor, DisplayName));

	HeaderRow
		.WholeRowContent()
		.HAlign(HAlign_Fill)
		.MinDesiredWidth(200.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.MaxWidth(25)
			.VAlign(VAlign_Center)
			[
				SAssignNew(ColorButtonWidget, SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &FMetaStoryEditorColorDetails::OnColorButtonClicked)
				.ContentPadding(2.f)
				[
					SNew(SColorBlock)
					.Color(this, &FMetaStoryEditorColorDetails::GetColor)
					.Size(FVector2D(16.0))
				]
			]
			+ SHorizontalBox::Slot()
			.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
			.VAlign(VAlign_Center)
			[
				NamePropertyHandle->CreatePropertyValueWidget(/*bDisplayDefaultPropertyButtons*/false)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				StructPropertyHandle->CreateDefaultPropertyButtonWidgets()
			]
		];
}

FLinearColor* FMetaStoryEditorColorDetails::GetColorPtr() const
{
	void* ValueData = nullptr;
	if (ColorPropertyHandle->GetValueData(ValueData) == FPropertyAccess::Result::Success)
	{
		return static_cast<FLinearColor*>(ValueData);
	}
	return nullptr;
}

FLinearColor FMetaStoryEditorColorDetails::GetColor() const
{
	if (FLinearColor* ColorPtr = GetColorPtr())
	{
		return *ColorPtr;
	}
	return FLinearColor(ForceInitToZero);
}

void FMetaStoryEditorColorDetails::SetColor(FLinearColor Color)
{
	if (FLinearColor* ColorPtr = GetColorPtr())
	{
		ColorPropertyHandle->NotifyPreChange();
		*ColorPtr = Color;
		ColorPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

void FMetaStoryEditorColorDetails::OnColorCommitted(FLinearColor Color)
{
	SetColor(Color);

	// End Transaction
	ColorPickerTransaction.Reset();
}

void FMetaStoryEditorColorDetails::OnColorCancelled(FLinearColor Color)
{
	SetColor(Color);

	// Cancel Transaction
	if (ColorPickerTransaction.IsValid())
	{
		ColorPickerTransaction->Cancel();
		ColorPickerTransaction.Reset();
	}
}

FReply FMetaStoryEditorColorDetails::OnColorButtonClicked()
{
	CreateColorPickerWindow();
	return FReply::Handled();
}

void FMetaStoryEditorColorDetails::CreateColorPickerWindow()
{
	FColorPickerArgs PickerArgs;

	// Begin Transaction
	ColorPickerTransaction = MakeShared<FScopedTransaction>(LOCTEXT("SetMetaStoryColorProperty", "Set Color Property"));

	PickerArgs.bOnlyRefreshOnMouseUp = false;
	PickerArgs.ParentWidget = ColorButtonWidget;
	PickerArgs.bUseAlpha = false;
	PickerArgs.bOnlyRefreshOnOk = false;
	PickerArgs.bClampValue = false; // Linear Color
	PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &FMetaStoryEditorColorDetails::OnColorCommitted);
	PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &FMetaStoryEditorColorDetails::OnColorCancelled);
	PickerArgs.InitialColor = GetColor();

	OpenColorPicker(PickerArgs);
}

#undef LOCTEXT_NAMESPACE
