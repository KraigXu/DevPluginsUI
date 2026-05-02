// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyFunctions/MetaStoryIntervalPropertyFunctions.h"

#include "MetaStoryExecutionContext.h"
#include "MetaStoryNodeDescriptionHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryIntervalPropertyFunctions)

#define LOCTEXT_NAMESPACE "MetaStory"

void FMetaStoryMakeIntervalPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = FFloatInterval(InstanceData.Min, InstanceData.Max);
}

#if WITH_EDITOR
FText FMetaStoryMakeIntervalPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType& InstanceData = InstanceDataView.Get<FInstanceDataType>();
	
	FText MinValueText = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Min)), Formatting);
	if (MinValueText.IsEmpty())
	{
		MinValueText = UE::MetaStory::DescHelpers::GetText(InstanceData.Min, Formatting);
	}

	FText MaxValueText = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Max)), Formatting);
	if (MaxValueText.IsEmpty())
	{
		MaxValueText = UE::MetaStory::DescHelpers::GetText(InstanceData.Max, Formatting);
	}

	return UE::MetaStory::DescHelpers::GetIntervalText(MinValueText, MaxValueText, Formatting);
}
#endif //WITH_EDITOR

#undef LOCTEXT_NAMESPACE
