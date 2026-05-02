// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyFunctions/MetaStoryIntPropertyFunctions.h"

#include "MetaStoryExecutionContext.h"
#include "MetaStoryNodeDescriptionHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryIntPropertyFunctions)

#define LOCTEXT_NAMESPACE "MetaStory"

void FMetaStoryAddIntPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = InstanceData.Left + InstanceData.Right;
}

void FMetaStorySubtractIntPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = InstanceData.Left - InstanceData.Right;
}

void FMetaStoryMultiplyIntPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = InstanceData.Left * InstanceData.Right;
}

void FMetaStoryDivideIntPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
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

void FMetaStoryInvertIntPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = -InstanceData.Input;
}

void FMetaStoryAbsoluteIntPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Result = FMath::Abs(InstanceData.Input);
}

#if WITH_EDITOR
FText FMetaStoryAddIntPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForMathOperation<FInstanceDataType>(LOCTEXT("IntAdd", "+"), ID, InstanceDataView, BindingLookup, Formatting);
}

FText FMetaStorySubtractIntPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForMathOperation<FInstanceDataType>(LOCTEXT("IntSubtract", "-"), ID, InstanceDataView, BindingLookup, Formatting);
}

FText FMetaStoryMultiplyIntPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForMathOperation<FInstanceDataType>(LOCTEXT("IntMultiply", "*"), ID, InstanceDataView, BindingLookup, Formatting);
}

FText FMetaStoryDivideIntPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForMathOperation<FInstanceDataType>(LOCTEXT("TreeIntDivide", "/"), ID, InstanceDataView, BindingLookup, Formatting);
}

FText FMetaStoryInvertIntPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForSingleParameterFunc<FInstanceDataType>(LOCTEXT("IntInvert", "-"), ID, InstanceDataView, BindingLookup, Formatting);
}

FText FMetaStoryAbsoluteIntPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::DescHelpers::GetDescriptionForSingleParameterFunc<FInstanceDataType>(LOCTEXT("IntAbsolute", "Abs"), ID, InstanceDataView, BindingLookup, Formatting);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
