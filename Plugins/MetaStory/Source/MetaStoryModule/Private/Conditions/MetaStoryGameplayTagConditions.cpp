// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/MetaStoryGameplayTagConditions.h"
#include "MetaStoryExecutionContext.h"
#include "MetaStoryNodeDescriptionHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryGameplayTagConditions)

#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "MetaStoryEditor"

namespace UE::MetaStory::Conditions
{


}

#endif// WITH_EDITOR


//----------------------------------------------------------------------//
//  FMetaStoryGameplayTagMatchCondition
//----------------------------------------------------------------------//

bool FMetaStoryGameplayTagMatchCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s'%s' contains '%s%s'")
		, *UE::MetaStory::DescHelpers::GetInvertText(bInvert, EMetaStoryNodeFormatting::Text).ToString()
		, *UE::MetaStory::DescHelpers::GetGameplayTagContainerAsText(InstanceData.TagContainer).ToString()
		, *UE::MetaStory::DescHelpers::GetExactMatchText(bExactMatch, EMetaStoryNodeFormatting::Text).ToString()
		, *InstanceData.Tag.ToString());

	return (bExactMatch ? InstanceData.TagContainer.HasTagExact(InstanceData.Tag) : InstanceData.TagContainer.HasTag(InstanceData.Tag)) ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryGameplayTagMatchCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText ContainerValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, TagContainer)), Formatting);
	if (ContainerValue.IsEmpty())
	{
		ContainerValue = UE::MetaStory::DescHelpers::GetGameplayTagContainerAsText(InstanceData->TagContainer);
	}

	FText TagValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Tag)), Formatting);
	if (TagValue.IsEmpty())
	{
		TagValue = FText::FromString(InstanceData->Tag.ToString());
	}

	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);
	const FText ExactMatchText = UE::MetaStory::DescHelpers::GetExactMatchText(bExactMatch, Formatting);

	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("GameplayTagMatchRich", "{EmptyOrNot}{TagContainer} <s>contains</> {EmptyOrExactly}{Tag}")
		: LOCTEXT("GameplayTagMatch", "{EmptyOrNot}{TagContainer} contains {EmptyOrExactly}{Tag}");

	return FText::FormatNamed(Format,
		TEXT("EmptyOrNot"), InvertText,
		TEXT("TagContainer"), ContainerValue,
		TEXT("EmptyOrExactly"), ExactMatchText,
		TEXT("Tag"), TagValue);
}
#endif

//----------------------------------------------------------------------//
//  FMetaStoryGameplayTagContainerMatchCondition
//----------------------------------------------------------------------//

bool FMetaStoryGameplayTagContainerMatchCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	bool bResult = false;
	switch (MatchType)
	{
	case EGameplayContainerMatchType::Any:
		bResult = bExactMatch ? InstanceData.TagContainer.HasAnyExact(InstanceData.OtherContainer) : InstanceData.TagContainer.HasAny(InstanceData.OtherContainer);
		break;
	case EGameplayContainerMatchType::All:
		bResult = bExactMatch ? InstanceData.TagContainer.HasAllExact(InstanceData.OtherContainer) : InstanceData.TagContainer.HasAll(InstanceData.OtherContainer);
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled match type %s."), *UEnum::GetValueAsString(MatchType));
	}

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s'%s' contains '%s %s%s'")
		, *UE::MetaStory::DescHelpers::GetInvertText(bInvert, EMetaStoryNodeFormatting::Text).ToString()
		, *UE::MetaStory::DescHelpers::GetGameplayTagContainerAsText(InstanceData.TagContainer).ToString()
		, *UEnum::GetValueAsString(MatchType)
		, *UE::MetaStory::DescHelpers::GetExactMatchText(bExactMatch, EMetaStoryNodeFormatting::Text).ToString()
		, *UE::MetaStory::DescHelpers::GetGameplayTagContainerAsText(InstanceData.OtherContainer).ToString());

	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryGameplayTagContainerMatchCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText ContainerValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, TagContainer)), Formatting);
	if (ContainerValue.IsEmpty())
	{
		ContainerValue = UE::MetaStory::DescHelpers::GetGameplayTagContainerAsText(InstanceData->TagContainer);
	}

	FText OtherContainerValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, OtherContainer)), Formatting);
	if (OtherContainerValue.IsEmpty())
	{
		OtherContainerValue = UE::MetaStory::DescHelpers::GetGameplayTagContainerAsText(InstanceData->OtherContainer);
	}

	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);
	const FText ExactMatchText = UE::MetaStory::DescHelpers::GetExactMatchText(bExactMatch, Formatting);
	const FText MatchTypeText = UEnum::GetDisplayValueAsText(MatchType);

	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("GameplayTagContainerMatchRich", "{EmptyOrNot}{TagContainer} <s>contains {AnyOrAll}</> {EmptyOrExactly}{OtherTagContainer}")
		: LOCTEXT("GameplayTagContainerMatch", "{EmptyOrNot}{TagContainer} contains {AnyOrAll} {EmptyOrExactly}{OtherTagContainer}");

	return FText::FormatNamed(Format,
		TEXT("EmptyOrNot"), InvertText,
		TEXT("TagContainer"), ContainerValue,
		TEXT("AnyOrAll"), MatchTypeText,
		TEXT("EmptyOrExactly"), ExactMatchText,
		TEXT("OtherTagContainer"), OtherContainerValue);
}
#endif

//----------------------------------------------------------------------//
//  FMetaStoryGameplayTagQueryCondition
//----------------------------------------------------------------------//

bool FMetaStoryGameplayTagQueryCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s'%s' matches %s")
		, *UE::MetaStory::DescHelpers::GetInvertText(bInvert, EMetaStoryNodeFormatting::Text).ToString()
		, *UE::MetaStory::DescHelpers::GetGameplayTagContainerAsText(InstanceData.TagContainer).ToString()
		, *UE::MetaStory::DescHelpers::GetGameplayTagQueryAsText(TagQuery).ToString());

	return TagQuery.Matches(InstanceData.TagContainer) ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryGameplayTagQueryCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText ContainerValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, TagContainer)), Formatting);
	if (ContainerValue.IsEmpty())
	{
		ContainerValue = UE::MetaStory::DescHelpers::GetGameplayTagContainerAsText(InstanceData->TagContainer);
	}

	const FText QueryValue = UE::MetaStory::DescHelpers::GetGameplayTagQueryAsText(TagQuery);

	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);


	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("GameplayTagQueryRich", "{EmptyOrNot}{TagContainer} <s>matches</> {TagQuery}")
		: LOCTEXT("GameplayTagQuery", "{EmptyOrNot}{TagContainer} matches {TagQuery}");

	return FText::FormatNamed(Format,
		TEXT("EmptyOrNot"), InvertText,
		TEXT("TagContainer"), ContainerValue,
		TEXT("TagQuery"), QueryValue);
}
#endif

#if WITH_EDITOR
#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR

