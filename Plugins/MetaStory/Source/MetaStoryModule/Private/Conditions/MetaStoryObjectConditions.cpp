// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/MetaStoryObjectConditions.h"
#include "MetaStoryExecutionContext.h"
#include "MetaStoryNodeDescriptionHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryObjectConditions)

#define LOCTEXT_NAMESPACE "MetaStoryObjectCondition"


//----------------------------------------------------------------------//
//  FMetaStoryCondition_ObjectIsValid
//----------------------------------------------------------------------//

bool FMetaStoryObjectIsValidCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const bool bResult = IsValid(InstanceData.Object);
	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryObjectIsValidCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);
	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("ObjectIsValidConditionsRich", "{EmptyOrNot}<s>Is Object Valid</>")
		: LOCTEXT("ObjectIsValidCondition", "{EmptyOrNot}Is Object Valid");
	return FText::FormatNamed(Format,
		TEXT("EmptyOrNot"), InvertText);
}
#endif

//----------------------------------------------------------------------//
//  FMetaStoryCondition_ObjectEquals
//----------------------------------------------------------------------//

bool FMetaStoryObjectEqualsCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const bool bResult = InstanceData.Left == InstanceData.Right;
	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryObjectEqualsCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);
	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("ObjectEqualsConditionRich", "{EmptyOrNot}<s>Is Object Equals</>")
		: LOCTEXT("ObjectEqualsCondition", "{EmptyOrNot}Is Object Equals");
	return FText::FormatNamed(Format,
		TEXT("EmptyOrNot"), InvertText);
}
#endif

//----------------------------------------------------------------------//
//  FMetaStoryCondition_ObjectIsChildOfClass
//----------------------------------------------------------------------//

bool FMetaStoryObjectIsChildOfClassCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const bool bResult = InstanceData.Object && InstanceData.Class
						&& InstanceData.Object->GetClass()->IsChildOf(InstanceData.Class);

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s of type '%s' is%s child of '%s'")
		, *GetNameSafe(InstanceData.Object)
		, *GetNameSafe(InstanceData.Object ? InstanceData.Object->GetClass() : nullptr)
		, bResult ? TEXT("") : TEXT(" not")
		, *GetNameSafe(InstanceData.Class));

	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryObjectIsChildOfClassCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);
	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("ObjectIsChildOfConditionsRich", "{EmptyOrNot}<s>Is Child Of Class</>")
		: LOCTEXT("ObjectIsChildOfCondition", "{EmptyOrNot}Is Child Of Class");
	return FText::FormatNamed(Format,
		TEXT("EmptyOrNot"), InvertText);
}
#endif

#undef LOCTEXT_NAMESPACE
