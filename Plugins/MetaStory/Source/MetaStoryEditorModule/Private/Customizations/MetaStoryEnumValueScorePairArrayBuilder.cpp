// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEnumValueScorePairArrayBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "Considerations/MetaStoryCommonConsiderations.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MetaStoryPropertyHelpers.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

FMetaStoryEnumValueScorePairArrayBuilder::FMetaStoryEnumValueScorePairArrayBuilder(TSharedRef<IPropertyHandle> InBasePropertyHandle, const UEnum* InEnumType, bool InGenerateHeader, bool InDisplayResetToDefault, bool InDisplayElementNum)
	: FDetailArrayBuilder(InBasePropertyHandle, InGenerateHeader, InDisplayResetToDefault, InDisplayElementNum)
	, EnumType(InEnumType)
	, PairArrayProperty(InBasePropertyHandle->AsArray())
{
}

void FMetaStoryEnumValueScorePairArrayBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	uint32 NumChildren = 0;
	PairArrayProperty->GetNumElements(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> PairPropertyHandle = PairArrayProperty->GetElement(ChildIndex);

		CustomizePairRowWidget(PairPropertyHandle, ChildrenBuilder);
	}
}

void FMetaStoryEnumValueScorePairArrayBuilder::CustomizePairRowWidget(TSharedRef<IPropertyHandle> PairPropertyHandle, IDetailChildrenBuilder& ChildrenBuilder)
{
	TSharedPtr<IPropertyHandle> EnumValuePropertyHandle = PairPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEnumValueScorePair, EnumValue));
	TSharedPtr<IPropertyHandle> EnumNamePropertyHandle = PairPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEnumValueScorePair, EnumName));
	TSharedPtr<IPropertyHandle> ScorePropertyHandle = PairPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEnumValueScorePair, Score));

	IDetailPropertyRow& PropertyRow = ChildrenBuilder.AddProperty(PairPropertyHandle);

	PropertyRow.CustomWidget(false)
		.NameContent()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(SComboButton)
				.OnGetMenuContent(this, &FMetaStoryEnumValueScorePairArrayBuilder::GetEnumEntryComboContent, EnumValuePropertyHandle, EnumNamePropertyHandle)
				.ContentPadding(.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
						.Text(this, &FMetaStoryEnumValueScorePairArrayBuilder::GetEnumEntryDescription, PairPropertyHandle)
						.Font(IDetailLayoutBuilder::GetDetailFontBold())
				]
		]
		.ValueContent()
		[
			ScorePropertyHandle->CreatePropertyValueWidget()
		];
}

FText FMetaStoryEnumValueScorePairArrayBuilder::GetEnumEntryDescription(TSharedRef<IPropertyHandle> PairPropertyHandle) const
{
	FMetaStoryEnumValueScorePair EnumValueScorePair;
	FPropertyAccess::Result Result = UE::MetaStory::PropertyHelpers::GetStructValue<FMetaStoryEnumValueScorePair>(PairPropertyHandle, EnumValueScorePair);
	if (Result == FPropertyAccess::Success)
	{
		if (EnumType)
		{
			return EnumType->GetDisplayNameTextByValue(int64(EnumValueScorePair.EnumValue));
		}
	}
	else if (Result == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleSelected", "Multiple Selected");
	}

	return LOCTEXT("None", "None");
}

TSharedRef<SWidget> FMetaStoryEnumValueScorePairArrayBuilder::GetEnumEntryComboContent(TSharedPtr<IPropertyHandle> EnumValuePropertyHandle, TSharedPtr<IPropertyHandle> EnumNamePropertyHandle) const
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/true, /*InCommandList*/nullptr);

	if (EnumType)
	{
		const bool bHasMaxValue = EnumType->ContainsExistingMax();
		const int32 NumEnums = bHasMaxValue ? EnumType->NumEnums() - 1 : EnumType->NumEnums();

		for (int32 Index = 0; Index < NumEnums; Index++)
		{

#if WITH_METADATA
			if (EnumType->HasMetaData(TEXT("Hidden"), Index))
			{
				continue;
			}
#endif //WITH_METADATA

			const int64 Value = EnumType->GetValueByIndex(Index);
			MenuBuilder.AddMenuEntry(EnumType->GetDisplayNameTextByIndex(Index), TAttribute<FText>(), FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda(
					[=, this]() {
						if (EnumValuePropertyHandle)
						{
							EnumValuePropertyHandle->SetValue(Value);
							EnumNamePropertyHandle->SetValue(EnumType->GetNameByIndex(Index));
						}
					})
				));
		}
	}
	else
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("None", "None"), TAttribute<FText>(), FSlateIcon(), FUIAction());
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
