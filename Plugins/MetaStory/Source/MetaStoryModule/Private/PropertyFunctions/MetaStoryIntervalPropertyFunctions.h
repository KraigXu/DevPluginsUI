// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryPropertyFunctionBase.h"
#include "MetaStoryIntervalPropertyFunctions.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;

USTRUCT()
struct FMetaStoryMakeIntervalPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	float Min = 0.f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float Max = 1.f;

	UPROPERTY(EditAnywhere, Category = Output)
	FFloatInterval Result = FFloatInterval(0.f, 1.f);
};

/**
 * Make an Interval from two floats.
 */
USTRUCT(DisplayName = "Make Interval")
struct FMetaStoryMakeIntervalPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryMakeIntervalPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

#undef UE_API
