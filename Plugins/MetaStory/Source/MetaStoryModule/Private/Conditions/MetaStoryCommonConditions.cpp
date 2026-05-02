// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/MetaStoryCommonConditions.h"
#include "MetaStoryExecutionContext.h"
#include "UObject/EnumProperty.h"
#include "MetaStoryNodeDescriptionHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryCommonConditions)

#define LOCTEXT_NAMESPACE "MetaStory"

namespace UE::MetaStory::Conditions
{

template<typename T>
bool CompareNumbers(const T Left, const T Right, const EGenericAICheck Operator)
{
	switch (Operator)
	{
	case EGenericAICheck::Equal:
		return Left == Right;
		break;
	case EGenericAICheck::NotEqual:
		return Left != Right;
		break;
	case EGenericAICheck::Less:
		return Left < Right;
		break;
	case EGenericAICheck::LessOrEqual:
		return Left <= Right;
		break;
	case EGenericAICheck::Greater:
		return Left > Right;
		break;
	case EGenericAICheck::GreaterOrEqual:
		return Left >= Right;
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled operator %d"), Operator);
		return false;
		break;
	}
}

} // UE::MetaStory::Conditions


//----------------------------------------------------------------------//
//  FMetaStoryCompareIntCondition
//----------------------------------------------------------------------//

bool FMetaStoryCompareIntCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const bool bResult = UE::MetaStory::Conditions::CompareNumbers<int32>(InstanceData.Left, InstanceData.Right, Operator);

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s%s %s %s")
		, *UE::MetaStory::DescHelpers::GetInvertText(bInvert, EMetaStoryNodeFormatting::Text).ToString()
		, *LexToString(InstanceData.Left)
		, *UE::MetaStory::DescHelpers::GetOperatorText(Operator, EMetaStoryNodeFormatting::Text).ToString()
		, *LexToString(InstanceData.Right));

	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryCompareIntCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText LeftValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Left)), Formatting);
	if (LeftValue.IsEmpty())
	{
		LeftValue = FText::AsNumber(InstanceData->Left);
	}

	FText RightValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Right)), Formatting);
	if (RightValue.IsEmpty())
	{
		RightValue = FText::AsNumber(InstanceData->Right);
	}

	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);
	const FText OperatorText = UE::MetaStory::DescHelpers::GetOperatorText(Operator, Formatting);

	return FText::FormatNamed(LOCTEXT("CompareInt", "{EmptyOrNot}{Left} {Op} {Right}"),
		TEXT("EmptyOrNot"), InvertText,
		TEXT("Left"),LeftValue,
		TEXT("Op"), OperatorText,
		TEXT("Right"),RightValue);
}
#endif

//----------------------------------------------------------------------//
//  FMetaStoryCompareFloatCondition
//----------------------------------------------------------------------//

bool FMetaStoryCompareFloatCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const bool bResult = UE::MetaStory::Conditions::CompareNumbers<double>(InstanceData.Left, InstanceData.Right, Operator);

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s%s %s %s")
		, *UE::MetaStory::DescHelpers::GetInvertText(bInvert, EMetaStoryNodeFormatting::Text).ToString()
		, *LexToString(InstanceData.Left)
		, *UE::MetaStory::DescHelpers::GetOperatorText(Operator, EMetaStoryNodeFormatting::Text).ToString()
		, *LexToString(InstanceData.Right));

	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryCompareFloatCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 1;
	Options.MaximumFractionalDigits = 3;

	FText LeftValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Left)), Formatting);
	if (LeftValue.IsEmpty())
	{
		LeftValue = FText::AsNumber(InstanceData->Left, &Options);
	}

	FText RightValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Right)), Formatting);
	if (RightValue.IsEmpty())
	{
		RightValue = FText::AsNumber(InstanceData->Right, &Options);
	}

	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);
	const FText OperatorText = UE::MetaStory::DescHelpers::GetOperatorText(Operator, Formatting);

	return FText::FormatNamed(LOCTEXT("CompareFloat", "{EmptyOrNot}{Left} {Op} {Right}"),
		TEXT("EmptyOrNot"), InvertText,
		TEXT("Left"),LeftValue,
		TEXT("Op"), OperatorText,
		TEXT("Right"),RightValue);
}
#endif

//----------------------------------------------------------------------//
//  FMetaStoryCompareBoolCondition
//----------------------------------------------------------------------//

bool FMetaStoryCompareBoolCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s%s is %s")
		, *UE::MetaStory::DescHelpers::GetInvertText(bInvert, EMetaStoryNodeFormatting::Text).ToString()
		, *UE::MetaStory::DescHelpers::GetBoolText(InstanceData.bLeft, EMetaStoryNodeFormatting::Text).ToString()
		, *UE::MetaStory::DescHelpers::GetBoolText(InstanceData.bRight, EMetaStoryNodeFormatting::Text).ToString());

	return (InstanceData.bLeft == InstanceData.bRight) ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryCompareBoolCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText LeftValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, bLeft)), Formatting);
	if (LeftValue.IsEmpty())
	{
		LeftValue = UE::MetaStory::DescHelpers::GetBoolText(InstanceData->bLeft, Formatting);
	}

	FText RightValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, bRight)), Formatting);
	if (RightValue.IsEmpty())
	{
		RightValue = UE::MetaStory::DescHelpers::GetBoolText(InstanceData->bRight, Formatting);
	}

	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);

	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("CompareBoolRich", "{EmptyOrNot}{Left} <s>is</> {Right}")
		: LOCTEXT("CompareBool", "{EmptyOrNot}{Left} is {Right}");

	return FText::FormatNamed(Format,
		TEXT("EmptyOrNot"), InvertText,
		TEXT("Left"),LeftValue,
		TEXT("Right"),RightValue);
}
#endif

//----------------------------------------------------------------------//
//  FMetaStoryCompareEnumCondition
//----------------------------------------------------------------------//

bool FMetaStoryCompareEnumCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s%s is %s")
		, *UE::MetaStory::DescHelpers::GetInvertText(bInvert, EMetaStoryNodeFormatting::Text).ToString()
		, *InstanceData.Left.Enum->GetNameStringByValue(InstanceData.Left.Value)
		, *InstanceData.Right.Enum->GetNameStringByValue(InstanceData.Right.Value));

	return (InstanceData.Left == InstanceData.Right) ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryCompareEnumCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText LeftValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Left)), Formatting);
	if (LeftValue.IsEmpty())
	{
		if (InstanceData->Left.Enum)
		{
			LeftValue = InstanceData->Left.Enum->GetDisplayNameTextByValue(InstanceData->Left.Value);
		}
		else
		{
			LeftValue = LOCTEXT("None", "None");
		}
	}

	FText RightValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Right)), Formatting);
	if (RightValue.IsEmpty())
	{
		if (InstanceData->Left.Enum)
		{
			RightValue = InstanceData->Right.Enum->GetDisplayNameTextByValue(InstanceData->Right.Value);
		}
		else
		{
			RightValue = LOCTEXT("None", "None");
		}
	}

	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);

	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("CompareEnumRich", "{EmptyOrNot}{Left} <s>is</> {Right}")
		: LOCTEXT("CompareEnum", "{EmptyOrNot}{Left} is {Right}");

	return FText::FormatNamed(Format,
		TEXT("EmptyOrNot"), InvertText,
		TEXT("Left"),LeftValue,
		TEXT("Right"),RightValue);
}

void FMetaStoryCompareEnumCondition::OnBindingChanged(const FGuid& ID, FMetaStoryDataView InstanceData, const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath, const IMetaStoryBindingLookup& BindingLookup)
{
	if (!TargetPath.GetStructID().IsValid())
	{
		return;
	}

	FInstanceDataType& Instance = InstanceData.GetMutable<FInstanceDataType>();

	// Left has changed, update enums from the leaf property.
	if (!TargetPath.IsPathEmpty()
		&& TargetPath.GetSegments().Last().GetName() == GET_MEMBER_NAME_CHECKED(FInstanceDataType, Left))
	{
		if (const FProperty* LeafProperty = BindingLookup.GetPropertyPathLeafProperty(SourcePath))
		{
			// Handle both old stype namespace enums and new class enum properties.
			UEnum* NewEnum = nullptr;
			if (const FByteProperty* ByteProperty = CastField<FByteProperty>(LeafProperty))
			{
				NewEnum = ByteProperty->GetIntPropertyEnum();
			}
			else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(LeafProperty))
			{
				NewEnum = EnumProperty->GetEnum();
			}

			if (Instance.Left.Enum != NewEnum)
			{
				Instance.Left.Initialize(NewEnum);
			}
		}
		else
		{
			Instance.Left.Initialize(nullptr);
		}

		if (Instance.Right.Enum != Instance.Left.Enum)
		{
			Instance.Right.Initialize(Instance.Left.Enum);
		}
	}
}

#endif


//----------------------------------------------------------------------//
//  FMetaStoryCompareDistanceCondition
//----------------------------------------------------------------------//

bool FMetaStoryCompareDistanceCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FVector::FReal Left = FVector::DistSquared(InstanceData.Source, InstanceData.Target);
	const FVector::FReal Right = FMath::Square(InstanceData.Distance);
	const bool bResult = UE::MetaStory::Conditions::CompareNumbers<FVector::FReal>(Left, Right, Operator);

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%sDistance %s %s %s (from [%s] to [%s])")
		, *UE::MetaStory::DescHelpers::GetInvertText(bInvert, EMetaStoryNodeFormatting::Text).ToString()
		, *LexToString(FMath::Sqrt(Left))
		, *UE::MetaStory::DescHelpers::GetOperatorText(Operator, EMetaStoryNodeFormatting::Text).ToString()
		, *LexToString(InstanceData.Distance)
		, *InstanceData.Source.ToString()
		, *InstanceData.Target.ToString());

	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryCompareDistanceCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 1;
	Options.MaximumFractionalDigits = 3;

	FText SourceValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Source)), Formatting);
	if (SourceValue.IsEmpty())
	{
		SourceValue = InstanceData->Source.ToText();
	}

	FText TargetValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Target)), Formatting);
	if (TargetValue.IsEmpty())
	{
		TargetValue = InstanceData->Target.ToText();
	}

	FText DistanceValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Distance)), Formatting);
	if (DistanceValue.IsEmpty())
	{
		DistanceValue = FText::AsNumber(InstanceData->Distance, &Options);
	}

	const FText OperatorText = UE::MetaStory::DescHelpers::GetOperatorText(Operator, Formatting);
	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);

	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("CompareDistanceRich", "{EmptyOrNot}<s>Distance from</> {Source} <s>to</> {Target} {Op} {Distance}")
		: LOCTEXT("CompareDistance", "{EmptyOrNot}Distance from {Source} to {Target} {Op} {Distance}");

	return FText::FormatNamed(Format,
		TEXT("EmptyOrNot"), InvertText,
		TEXT("Source"), SourceValue,
		TEXT("Target"), TargetValue,
		TEXT("Op"), OperatorText,
		TEXT("Distance"), DistanceValue);
}
#endif

//----------------------------------------------------------------------//
//  FMetaStoryCompareNameCondition
//----------------------------------------------------------------------//

bool FMetaStoryCompareNameCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("%s%s is %s")
							   , *UE::MetaStory::DescHelpers::GetInvertText(bInvert, EMetaStoryNodeFormatting::Text).ToString()
							   , *InstanceData.Left.ToString()
							   , *InstanceData.Right.ToString());

	return InstanceData.Left.IsEqual(InstanceData.Right) ^ bInvert;
}

#if WITH_EDITOR
FText FMetaStoryCompareNameCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText LeftValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Left)), Formatting);
	if (LeftValue.IsEmpty())
	{
		LeftValue = FText::FromName(InstanceData->Left);
	}

	FText RightValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Right)), Formatting);
	if (RightValue.IsEmpty())
	{
		RightValue = FText::FromName(InstanceData->Right);
	}

	const FText InvertText = UE::MetaStory::DescHelpers::GetInvertText(bInvert, Formatting);

	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("CompareNameRich", "{EmptyOrNot}{Left} <s>is</> {Right}")
	: LOCTEXT("CompareName", "{EmptyOrNot}{Left} is {Right}");

	return FText::FormatNamed(Format,
							  TEXT("EmptyOrNot"), InvertText,
							  TEXT("Left"),LeftValue,
							  TEXT("Right"),RightValue);
}
#endif

//----------------------------------------------------------------------//
//  FMetaStoryRandomCondition
//----------------------------------------------------------------------//

bool FMetaStoryRandomCondition::TestCondition(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const float RandomValue = FMath::FRandRange(0.0f, 1.0f);

	SET_NODE_CUSTOM_TRACE_TEXT(Context, Override, TEXT("Random value %f < %s")
		, RandomValue
		, *LexToString(InstanceData.Threshold));

	return RandomValue < InstanceData.Threshold;
}

#if WITH_EDITOR
FText FMetaStoryRandomCondition::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 1;
	Options.MaximumFractionalDigits = 3;

	FText ThresholdValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Threshold)), Formatting);
	if (ThresholdValue.IsEmpty())
	{
		ThresholdValue = FText::AsNumber(InstanceData->Threshold, &Options);
	}

	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("RandomRich", "<s>Random [0..1] &lt;</> {Threshold}")
		: LOCTEXT("Random", "Random [0..1] &lt; {Threshold}");

	return FText::FormatNamed(Format,
		TEXT("Threshold"), ThresholdValue);
}
#endif

#undef LOCTEXT_NAMESPACE

