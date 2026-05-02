// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEnumValueScorePairsDetails.h"
#include "Considerations/MetaStoryCommonConsiderations.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "MetaStoryEnumValueScorePairArrayBuilder.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

TSharedRef<IPropertyTypeCustomization> FMetaStoryEnumValueScorePairsDetails::MakeInstance()
{
	return MakeShareable(new FMetaStoryEnumValueScorePairsDetails);
}

void FMetaStoryEnumValueScorePairsDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FMetaStoryEnumValueScorePairsDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	EnumProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEnumValueScorePairs, Enum));
	PairsProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEnumValueScorePairs, Data));

	UObject* Object = nullptr; 
	if (EnumProperty->GetValue(Object) == FPropertyAccess::Success)
	{
		const UEnum* EnumType = static_cast<UEnum*>(Object);
		TSharedRef<FMetaStoryEnumValueScorePairArrayBuilder> Builder = MakeShareable(
			new FMetaStoryEnumValueScorePairArrayBuilder(
				PairsProperty.ToSharedRef(), EnumType, /*InGenerateHeader*/ true, /*InDisplayResetToDefault*/ false, /*InDisplayElementNum*/ true));
			
		StructBuilder.AddCustomBuilder(Builder);
	}
}
#undef LOCTEXT_NAMESPACE
