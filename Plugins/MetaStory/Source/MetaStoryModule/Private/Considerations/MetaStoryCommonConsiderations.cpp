// Copyright Epic Games, Inc. All Rights Reserved.

#include "Considerations/MetaStoryCommonConsiderations.h"
#include "Algo/Sort.h"
#include "MetaStoryExecutionContext.h"
#include "MetaStoryNodeDescriptionHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryCommonConsiderations)

#define LOCTEXT_NAMESPACE "MetaStory"

#if WITH_EDITOR
FText FMetaStoryFloatInputConsideration::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting /*= EMetaStoryNodeFormatting::Text*/) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText InputText = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Input)), Formatting);
	if (InputText.IsEmpty())
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 2;

		InputText = FText::AsNumber(InstanceData->Input, &Options);
	}
	
	const FFloatInterval& Interval = InstanceData->Interval;
	FText IntervalText = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Interval)), Formatting);
	if (IntervalText.IsEmpty())
	{
		IntervalText = UE::MetaStory::DescHelpers::GetIntervalText(Interval, Formatting);
	}
	
	if (Formatting == EMetaStoryNodeFormatting::RichText)
	{
		return FText::FormatNamed(LOCTEXT("InputInIntervalRich", "{Input} <s>in</> {Interval}"),
			TEXT("Input"), InputText,
			TEXT("Interval"), IntervalText
		);
	}
	else //EMetaStoryNodeFormatting::Text
	{
		return FText::FormatNamed(LOCTEXT("InputInInterval", "{Input} in {Interval}"),
			TEXT("Input"), InputText,
			TEXT("Interval"), IntervalText
		);
	}
}
#endif //WITH_EDITOR

float FMetaStoryFloatInputConsideration::GetScore(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const float Input = InstanceData.Input;
	const FFloatInterval& Interval = InstanceData.Interval;
	const float NormalizedInput = FMath::Clamp(Interval.GetRangePct(Input), 0.f, 1.f);

	return ResponseCurve.Evaluate(NormalizedInput);
}

#if WITH_EDITOR
FText FMetaStoryConstantConsideration::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting /*= EMetaStoryNodeFormatting::Text*/) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText ConstantText = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Constant)), Formatting);
	if (ConstantText.IsEmpty())
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 2;

		ConstantText = FText::AsNumber(InstanceData->Constant, &Options);
	}

	return FText::FormatNamed(LOCTEXT("Constant", "{ConstantValue}"), 
		TEXT("ConstantValue"), ConstantText);
}
#endif //WITH_EDITOR

float FMetaStoryConstantConsideration::GetScore(FMetaStoryExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	return InstanceData.Constant;
}

float FMetaStoryEnumInputConsideration::GetScore(FMetaStoryExecutionContext& Context) const
{
	const FMetaStoryAnyEnum& Input = Context.GetInstanceData(*this).Input;
	const uint32 EnumValue = Input.Value;
	const TArray<FMetaStoryEnumValueScorePair>& Data = EnumValueScorePairs.Data;

	for (int32 Index = 0; Index < Data.Num(); ++Index)
	{
		const FMetaStoryEnumValueScorePair& Pair = Data[Index];
		if (EnumValue == Pair.EnumValue)
		{
			return Pair.Score;
		}
	}

	return 0.f;
}

#if WITH_EDITOR
EDataValidationResult FMetaStoryEnumInputConsideration::Compile(UE::MetaStory::ICompileNodeContext& Context)
{
	const FInstanceDataType& InstanceData = Context.GetInstanceDataView().Get<FInstanceDataType>();
	check(InstanceData.Input.Enum == EnumValueScorePairs.Enum);
	const int32 NumPairs = EnumValueScorePairs.Data.Num();

	//Validate uniqueness of keys
	const auto SortedPairs = EnumValueScorePairs.Data;
	Algo::Sort(SortedPairs, 
		[](const FMetaStoryEnumValueScorePair& A, const FMetaStoryEnumValueScorePair& B) {
		return A.EnumValue < B.EnumValue;
		});
	for (int32 Idx = 1; Idx < NumPairs; ++Idx)
	{
		if (SortedPairs[Idx].EnumValue == SortedPairs[Idx - 1].EnumValue)
		{
			Context.AddValidationError(LOCTEXT("DuplicateEnumValues", "Duplicate Enum Values found."));

			return EDataValidationResult::Invalid;
		}
	}

	return EDataValidationResult::Valid;
}

void FMetaStoryEnumInputConsideration::OnBindingChanged(const FGuid& ID, FMetaStoryDataView InstanceData, const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath, const IMetaStoryBindingLookup& BindingLookup)
{
	if (!TargetPath.GetStructID().IsValid())
	{
		return;
	}

	FInstanceDataType& Instance = InstanceData.GetMutable<FInstanceDataType>();

	// Left has changed, update enums from the leaf property.
	if (!TargetPath.IsPathEmpty()
		&& TargetPath.GetSegments().Last().GetName() == GET_MEMBER_NAME_CHECKED(FInstanceDataType, Input))
	{
		if (const FProperty* LeafProperty = BindingLookup.GetPropertyPathLeafProperty(SourcePath))
		{
			// Handle both old type namespace enums and new class enum properties.
			UEnum* NewEnum = nullptr;
			if (const FByteProperty* ByteProperty = CastField<FByteProperty>(LeafProperty))
			{
				NewEnum = ByteProperty->GetIntPropertyEnum();
			}
			else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(LeafProperty))
			{
				NewEnum = EnumProperty->GetEnum();
			}

			if (Instance.Input.Enum != NewEnum)
			{
				Instance.Input.Initialize(NewEnum);
			}
		}
		else
		{
			Instance.Input.Initialize(nullptr);
		}

		if (EnumValueScorePairs.Enum != Instance.Input.Enum)
		{
			EnumValueScorePairs.Initialize(Instance.Input.Enum);
		}
	}
}

FText FMetaStoryEnumInputConsideration::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting /*= EMetaStoryNodeFormatting::Text*/) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText InputText = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Input)), Formatting);
	
	return FText::FormatNamed(LOCTEXT("EnumInput", "{Input}"),
		TEXT("Input"), InputText);
}

#endif //WITH_EDITOR

#undef LOCTEXT_NAMESPACE
