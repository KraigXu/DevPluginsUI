// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryBooleanAlgebraPropertyFunctions.h"
#include "MetaStoryExecutionContext.h"
#include "MetaStoryNodeDescriptionHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryBooleanAlgebraPropertyFunctions)

#define LOCTEXT_NAMESPACE "MetaStory"

namespace UE::MetaStory::BooleanPropertyFunctions::Internal
{
#if WITH_EDITOR
	FText GetDescriptionForOperation(FText OperationText, const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting)
	{
		const FMetaStoryBooleanOperationPropertyFunctionInstanceData& InstanceData = InstanceDataView.Get<FMetaStoryBooleanOperationPropertyFunctionInstanceData>();

		FText LeftValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FMetaStoryBooleanOperationPropertyFunctionInstanceData, bLeft)), Formatting);
		if (LeftValue.IsEmpty())
		{
			LeftValue = UE::MetaStory::DescHelpers::GetBoolText(InstanceData.bLeft, Formatting);
		}

		FText RightValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FMetaStoryBooleanOperationPropertyFunctionInstanceData, bRight)), Formatting);
		if (RightValue.IsEmpty())
		{
			RightValue = UE::MetaStory::DescHelpers::GetBoolText(InstanceData.bRight, Formatting);
		}

		return UE::MetaStory::DescHelpers::GetMathOperationText(OperationText, LeftValue, RightValue, Formatting);
	}
#endif // WITH_EDITOR
} // namespace UE::MetaStory::BooleanPropertyFunctions::Internal

void FMetaStoryBooleanAndPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FMetaStoryBooleanOperationPropertyFunctionInstanceData& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bResult = InstanceData.bLeft && InstanceData.bRight;
}

#if WITH_EDITOR
FText FMetaStoryBooleanAndPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::BooleanPropertyFunctions::Internal::GetDescriptionForOperation(LOCTEXT("BoolAnd", "and"), ID, InstanceDataView, BindingLookup, Formatting);
}
#endif // WITH_EDITOR

void FMetaStoryBooleanOrPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FMetaStoryBooleanOperationPropertyFunctionInstanceData& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bResult = InstanceData.bLeft || InstanceData.bRight;
}

#if WITH_EDITOR
FText FMetaStoryBooleanOrPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::BooleanPropertyFunctions::Internal::GetDescriptionForOperation(LOCTEXT("BoolOr", "or"), ID, InstanceDataView, BindingLookup, Formatting);
}
#endif // WITH_EDITOR

void FMetaStoryBooleanXOrPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FMetaStoryBooleanOperationPropertyFunctionInstanceData& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bResult = InstanceData.bLeft ^ InstanceData.bRight;
}

#if WITH_EDITOR
FText FMetaStoryBooleanXOrPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	return UE::MetaStory::BooleanPropertyFunctions::Internal::GetDescriptionForOperation(LOCTEXT("BoolXOr", "xor"), ID, InstanceDataView, BindingLookup, Formatting);
}
#endif // WITH_EDITOR

void FMetaStoryBooleanNotPropertyFunction::Execute(FMetaStoryExecutionContext& Context) const
{
	FMetaStoryBooleanNotOperationPropertyFunctionInstanceData& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bResult = !InstanceData.bInput;
}

#if WITH_EDITOR
FText FMetaStoryBooleanNotPropertyFunction::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FMetaStoryBooleanNotOperationPropertyFunctionInstanceData& InstanceData = InstanceDataView.Get<FMetaStoryBooleanNotOperationPropertyFunctionInstanceData>();

	FText InputValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FMetaStoryBooleanNotOperationPropertyFunctionInstanceData, bInput)), Formatting);
	if (InputValue.IsEmpty())
	{
		InputValue = UE::MetaStory::DescHelpers::GetBoolText(InstanceData.bInput, Formatting);
	}

	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("BoolNotFuncRich", "(<s>Not</> {Input})")
		: LOCTEXT("BoolNotFunc", "(Not {Input})");

	return FText::FormatNamed(Format, TEXT("Input"), InputValue);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
