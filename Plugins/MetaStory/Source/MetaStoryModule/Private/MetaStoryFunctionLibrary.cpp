// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryFunctionLibrary.h"

#include "Blueprint/BlueprintExceptionInfo.h"
#include "MetaStory.h"
#include "MetaStoryReference.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/Script.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryFunctionLibrary)

#define LOCTEXT_NAMESPACE "MetaStoryFunctionLibrary"

void UMetaStoryFunctionLibrary::SetMetaStory(FMetaStoryReference& Reference, UMetaStory* NewMetaStory)
{
	Reference.SetMetaStory(NewMetaStory);
}

FMetaStoryReference UMetaStoryFunctionLibrary::MakeMetaStoryReference(UMetaStory* NewMetaStory)
{
	FMetaStoryReference Result;
	Result.SetMetaStory(NewMetaStory);
	return Result;
}

void UMetaStoryFunctionLibrary::K2_SetParametersProperty(FMetaStoryReference&, FGuid, const int32&)
{
	checkNoEntry();
}

void UMetaStoryFunctionLibrary::K2_GetParametersProperty(const FMetaStoryReference&, FGuid, int32&)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UMetaStoryFunctionLibrary::execK2_SetParametersProperty)
{
	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;

	P_GET_STRUCT_REF(FMetaStoryReference, MetaStoryReference);
	P_GET_STRUCT(FGuid, PropertyID);

	Stack.StepCompiledIn<FProperty>(nullptr);
	const FProperty* SourceProperty = Stack.MostRecentProperty;
	const uint8* SourcePtr = Stack.MostRecentPropertyAddress;
	P_FINISH;

	if (SourceProperty == nullptr|| SourcePtr == nullptr)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("SetParametersProperty_InvalidValueWarning", "Failed to resolve the Value for SetParametersProperty")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;
		FInstancedPropertyBag& InstancedPropertyBag = MetaStoryReference.GetMutableParameters();
		FStructView PropertyBagView = InstancedPropertyBag.GetMutableValue();
		const UPropertyBag* PropertyBag = InstancedPropertyBag.GetPropertyBagStruct();
		if (PropertyBagView.IsValid() && PropertyBag)
		{
			if (const FPropertyBagPropertyDesc* PropertyBagDesc = PropertyBag->FindPropertyDescByID(PropertyID))
			{
				if (const FProperty* TargetProperty = PropertyBag->FindPropertyByName(PropertyBagDesc->Name))
				{
					if (SourceProperty->SameType(TargetProperty))
					{
						void* TargetPtr = TargetProperty->ContainerPtrToValuePtr<void>(PropertyBagView.GetMemory());
						TargetProperty->CopyCompleteValue(TargetPtr, SourcePtr);
						MetaStoryReference.SetPropertyOverridden(PropertyID, true);
					}
				}
			}
		}
		P_NATIVE_END;
	}
}

DEFINE_FUNCTION(UMetaStoryFunctionLibrary::execK2_GetParametersProperty)
{
	P_GET_STRUCT_REF(FMetaStoryReference, MetaStoryReference);
	P_GET_STRUCT(FGuid, PropertyID);

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);

	const FProperty* TargetProperty = Stack.MostRecentProperty;
	void* TargetPtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	if (TargetProperty == nullptr || TargetPtr == nullptr)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("GetParametersProperty_InvalidValueWarning", "Failed to resolve the Value for GetParametersProperty")
		);
		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;
		const FInstancedPropertyBag& InstancedPropertyBag = MetaStoryReference.GetParameters();
		const FConstStructView PropertyBagView = InstancedPropertyBag.GetValue();
		const UPropertyBag* PropertyBag = InstancedPropertyBag.GetPropertyBagStruct();
		if (PropertyBagView.IsValid() && PropertyBag)
		{
			if (const FPropertyBagPropertyDesc* PropertyBagDesc = PropertyBag->FindPropertyDescByID(PropertyID))
			{
				if (const FProperty* SourceProperty = PropertyBag->FindPropertyByName(PropertyBagDesc->Name))
				{
					if (SourceProperty->SameType(TargetProperty))
					{
						const void* SourcePtr = SourceProperty->ContainerPtrToValuePtr<void>(PropertyBagView.GetMemory());
						TargetProperty->CopyCompleteValue(TargetPtr, SourcePtr);
					}
				}
			}
		}
		P_NATIVE_END;
	}
}

#undef LOCTEXT_NAMESPACE
