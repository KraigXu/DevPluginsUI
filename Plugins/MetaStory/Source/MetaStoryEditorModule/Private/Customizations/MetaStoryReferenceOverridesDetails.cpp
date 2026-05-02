// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryReferenceOverridesDetails.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "MetaStoryReference.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

TSharedRef<IPropertyTypeCustomization> FMetaStoryReferenceOverridesDetails::MakeInstance()
{
	return MakeShareable(new FMetaStoryReferenceOverridesDetails);
}

void FMetaStoryReferenceOverridesDetails::CustomizeHeader(const TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;
	PropUtils = InCustomizationUtils.GetPropertyUtilities();

	OverrideItemsHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryReferenceOverrides, OverrideItems));
	check(OverrideItemsHandle);
	
	InHeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		OverrideItemsHandle->CreatePropertyValueWidget()
	]
	.ShouldAutoExpand(true);
}

void FMetaStoryReferenceOverridesDetails::CustomizeChildren(const TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InChildrenBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	check(OverrideItemsHandle);
	
	const TSharedRef<FDetailArrayBuilder> NestedTreeOverridesBuilder = MakeShareable(new FDetailArrayBuilder(OverrideItemsHandle.ToSharedRef(), /*InGenerateHeader*/false, /*InDisplayResetToDefault*/ true, /*InDisplayElementNum*/ false));
	NestedTreeOverridesBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda(
		[](TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
		{

			TSharedPtr<IPropertyHandle> StateTagHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryReferenceOverrideItem, StateTag));
			check(StateTagHandle);
			TSharedPtr<IPropertyHandle> MetaStoryReferenceHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryReferenceOverrideItem, MetaStoryReference));
			check(MetaStoryReferenceHandle);

			IDetailPropertyRow& PropertyRow = ChildrenBuilder.AddProperty(MetaStoryReferenceHandle.ToSharedRef());

			TSharedPtr<SWidget> NameWidget;
			TSharedPtr<SWidget> ValueWidget;
			PropertyRow.GetDefaultWidgets(NameWidget, ValueWidget, /*bAddWidgetDecoration*/true);

			PropertyRow.CustomWidget(/*bShowChildren*/true)
			.NameContent()
			[
				StateTagHandle->CreatePropertyValueWidgetWithCustomization(nullptr)
			]
			.ValueContent()
			[
				ValueWidget.ToSharedRef()
			];
		}));
	InChildrenBuilder.AddCustomBuilder(NestedTreeOverridesBuilder);}

#undef LOCTEXT_NAMESPACE
