// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AITypes.h"
#include "Internationalization/Text.h"
#include "MetaStoryTypes.h"
#include "MetaStoryPropertyBindings.h"

enum class EMetaStoryNodeFormatting : uint8;
struct FGameplayTagContainer;
struct FGameplayTagQuery;

namespace UE::MetaStory::DescHelpers
{
#if WITH_EDITOR || WITH_METASTORY_TRACE

/** @return description for a EGenericAICheck. */
extern METASTORYMODULE_API FText GetOperatorText(const EGenericAICheck Operator, EMetaStoryNodeFormatting Formatting);

/** @return description for condition inversion (returns "Not" plus a space). */
extern METASTORYMODULE_API FText GetInvertText(bool bInvert, EMetaStoryNodeFormatting Formatting);

/** @return description of a boolean value. */
extern METASTORYMODULE_API FText GetBoolText(bool bValue, EMetaStoryNodeFormatting Formatting);

/** @return description of a float interval. */
extern METASTORYMODULE_API FText GetIntervalText(const FFloatInterval& Interval, EMetaStoryNodeFormatting Formatting);

/** @return description of a float interval. */
extern METASTORYMODULE_API FText GetIntervalText(float Min, float Max, EMetaStoryNodeFormatting Formatting);

/** @return description of a float interval. */
extern METASTORYMODULE_API FText GetIntervalText(const FText& MinValueText, const FText& MaxValueText, EMetaStoryNodeFormatting Formatting);

/** @return description for a Gameplay Tag Container. If the length of container description is longer than ApproxMaxLength, the it truncated and ... as added to the end. */
extern METASTORYMODULE_API FText GetGameplayTagContainerAsText(const FGameplayTagContainer& TagContainer, const int ApproxMaxLength = 60);

/** @return description for a Gameplay Tag Query. If the query description is longer than ApproxMaxLength, the it truncated and ... as added to the end. */
extern METASTORYMODULE_API FText GetGameplayTagQueryAsText(const FGameplayTagQuery& TagQuery, const int ApproxMaxLength = 120);

/** @return description for exact match, used for Gameplay Tag matching functions (returns "Exactly" plus space). */
extern METASTORYMODULE_API FText GetExactMatchText(bool bExactMatch, EMetaStoryNodeFormatting Formatting);

/** @return description of a vector value. */
extern METASTORYMODULE_API FText GetText(const FVector& Value, EMetaStoryNodeFormatting Formatting);

/** @return description of a float value. */
extern METASTORYMODULE_API FText GetText(float Value, EMetaStoryNodeFormatting Formatting);

/** @return description of an int value. */
extern METASTORYMODULE_API FText GetText(int32 Value, EMetaStoryNodeFormatting Formatting);

/** @return description of a UObject value. */
extern METASTORYMODULE_API FText GetText(const UObject* Value, EMetaStoryNodeFormatting Formatting);

extern METASTORYMODULE_API FText GetMathOperationText(const FText& OperationText, const FText& LeftText, const FText& RightText, EMetaStoryNodeFormatting Formatting);

extern METASTORYMODULE_API FText GetSingleParamFunctionText(const FText& FunctionText, const FText& ParamText, EMetaStoryNodeFormatting Formatting);

#endif // WITH_EDITOR || WITH_METASTORY_TRACE

#if WITH_EDITOR

/** @return description in the form of (Left OperationText Right).
*	Expect TInstanceDataType to have a member Left and Right whose types have an overloaded UE::MetaStory::DescHelpers::GetText function.
*/
template <typename TInstanceDataType>
FText GetDescriptionForMathOperation(FText OperationText, const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting)
{
	const TInstanceDataType& InstanceData = InstanceDataView.Get<TInstanceDataType>();

	FText LeftValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(TInstanceDataType, Left)), Formatting);
	if (LeftValue.IsEmpty())
	{
		LeftValue = UE::MetaStory::DescHelpers::GetText(InstanceData.Left, Formatting);
	}

	FText RightValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(TInstanceDataType, Right)), Formatting);
	if (RightValue.IsEmpty())
	{
		RightValue = UE::MetaStory::DescHelpers::GetText(InstanceData.Right, Formatting);
	}

	return GetMathOperationText(OperationText, LeftValue, RightValue, Formatting);
}

/** @return description in the form of OperationText(Input).
 *	Expect TInstanceDataType to have a member input whose type has an overloaded UE::MetaStory::DescHelpers::GetText function.
 */
template <typename TInstanceDataType>
FText GetDescriptionForSingleParameterFunc(FText OperationText, const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting)
{
	const TInstanceDataType& InstanceData = InstanceDataView.Get<TInstanceDataType>();

	FText InputValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(TInstanceDataType, Input)), Formatting);
	if (InputValue.IsEmpty())
	{
		InputValue = UE::MetaStory::DescHelpers::GetText(InstanceData.Input, Formatting);
	}

	return GetSingleParamFunctionText(OperationText, InputValue, Formatting);
}
#endif // WITH_EDITOR
} // namespace UE::MetaStory::DescHelpers