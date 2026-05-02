// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyFunctions/MetaStoryFloatPropertyFunctions.h"

#include "MetaStoryExecutionContext.h"
#include "MetaStoryNodeDescriptionHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryFloatPropertyFunctions)

#define LOCTEXT_NAMESPACE "MetaStory"

void FMetaStoryAddFloatPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = InstanceData.Left + InstanceData.Right;
}

void FMetaStorySubtractFloatPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = InstanceData.Left - InstanceData.Right;
}

void FMetaStoryMultiplyFloatPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = InstanceData.Left * InstanceData.Right;
}

void FMetaStoryDivideFloatPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.Right != 0)
	{
		InstanceData.Result = InstanceData.Left / InstanceData.Right;
	}
	else
	{
		InstanceData.Result = 0;
	}
}

void FMetaStoryInvertFloatPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = -InstanceData.Input;
}

void FMetaStoryAbsoluteFloatPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = FMath::Abs(InstanceData.Input);
}

#if WITH_EDITOR
FText FMetaStoryAddFloatPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForMathOperation<FInstanceDataType>(LOCTEXT("FloatAdd", "+"), ID, InstanceDataView, BindingLookup, Formatting);
}

FText FMetaStorySubtractFloatPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForMathOperation<FInstanceDataType>(LOCTEXT("FloatSubtract", "-"), ID, InstanceDataView, BindingLookup, Formatting);
}

FText FMetaStoryMultiplyFloatPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForMathOperation<FInstanceDataType>(LOCTEXT("FloatMultiply", "*"), ID, InstanceDataView, BindingLookup, Formatting);
}

FText FMetaStoryDivideFloatPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForMathOperation<FInstanceDataType>(LOCTEXT("TreeFloatDivide", "/"), ID, InstanceDataView, BindingLookup, Formatting);
}

FText FMetaStoryInvertFloatPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForSingleParameterFunc<FInstanceDataType>(LOCTEXT("FloatInvert", "-"), ID, InstanceDataView, BindingLookup, Formatting);
}

FText FMetaStoryAbsoluteFloatPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForSingleParameterFunc<FInstanceDataType>(LOCTEXT("FloatAbsolute", "Abs"), ID, InstanceDataView, BindingLookup, Formatting);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
