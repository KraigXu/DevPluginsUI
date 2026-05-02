// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryAnyEnumDetails.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "MetaStoryPropertyHelpers.h"
#include "MetaStoryAnyEnum.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

TSharedRef<IPropertyTypeCustomization> FMetaStoryAnyEnumDetails::MakeInstance()
{
	return MakeShareable(new FMetaStoryAnyEnumDetails);
}

void FMetaStoryAnyEnumDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

	ValueProperty = StructProperty->GetChildHandle(TEXT("Value"));
	EnumProperty = StructProperty->GetChildHandle(TEXT("Enum"));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.f)
	.VAlign(VAlign_Center)
	[
		SNew(SComboButton)
		.OnGetMenuContent(this, &FMetaStoryAnyEnumDetails::OnGetComboContent)
		.ContentPadding(FMargin(2.0f, 0.0f))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FMetaStoryAnyEnumDetails::GetDescription)
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
	];
}

void FMetaStoryAnyEnumDetails::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
}

FText FMetaStoryAnyEnumDetails::GetDescription() const
{
	check(StructProperty);

	FMetaStoryAnyEnum MetaStoryEnum;
	FPropertyAccess::Result Result = UE::MetaStory::PropertyHelpers::GetStructValue<FMetaStoryAnyEnum>(StructProperty, MetaStoryEnum);
	if (Result == FPropertyAccess::Success)
	{
		if (MetaStoryEnum.Enum)
		{
			return MetaStoryEnum.Enum->GetDisplayNameTextByValue(int64(MetaStoryEnum.Value));
		}
	}
	else if (Result == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleSelected", "Multiple Selected");
	}

	return LOCTEXT("None", "None");
}

TSharedRef<SWidget> FMetaStoryAnyEnumDetails::OnGetComboContent() const
{
	check(StructProperty);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/true, /*InCommandList*/nullptr);

	bool bSuccess = false;
	FMetaStoryAnyEnum MetaStoryEnum;
	if (UE::MetaStory::PropertyHelpers::GetStructValue<FMetaStoryAnyEnum>(StructProperty, MetaStoryEnum) == FPropertyAccess::Success)
	{
		if (MetaStoryEnum.Enum)
		{
			// This is the number of entry in the enum, - 1, because the last item in an enum is the _MAX item
			for (int32 i = 0; i < MetaStoryEnum.Enum->NumEnums() - 1; i++)
			{
				const int64 Value = MetaStoryEnum.Enum->GetValueByIndex(i);
				MenuBuilder.AddMenuEntry(MetaStoryEnum.Enum->GetDisplayNameTextByIndex(i), TAttribute<FText>(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([this, Value]() {
						if (ValueProperty)
						{
							ValueProperty->SetValue(uint32(Value));
						}
					}))
				);
			}
			bSuccess = true;
		}
	}

	if (!bSuccess)
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("None", "None"), TAttribute<FText>(), FSlateIcon(), FUIAction());
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
