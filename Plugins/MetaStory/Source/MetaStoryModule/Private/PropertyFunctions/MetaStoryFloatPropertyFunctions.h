// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryPropertyFunctionBase.h"
#include "MetaStoryFloatPropertyFunctions.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;

USTRUCT()
struct FMetaStoryFloatCombinaisonPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	float Left = 0.f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float Right = 0.f;

	UPROPERTY(EditAnywhere, Category = Output)
	float Result = 0.f;
};

/**
 * Add two floats.
 */
USTRUCT(meta=(DisplayName = "Add", Category = "Math|Float"))
struct FMetaStoryAddFloatPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryFloatCombinaisonPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

/**
 * Subtract right float from left float.
 */
USTRUCT(meta=(DisplayName = "Subtract", Category = "Math|Float"))
struct FMetaStorySubtractFloatPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryFloatCombinaisonPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

/**
 * Multiply the two given float.
 */
USTRUCT(meta=(DisplayName = "Multiply", Category = "Math|Float"))
struct FMetaStoryMultiplyFloatPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryFloatCombinaisonPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

/**
 * Divide left float by right float.
 */
USTRUCT(meta=(DisplayName = "Divide", Category = "Math|Float"))
struct FMetaStoryDivideFloatPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryFloatCombinaisonPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

USTRUCT()
struct FMetaStorySingleFloatPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	float Input = 0.f;

	UPROPERTY(EditAnywhere, Category = Output)
	float Result = 0.f;
};

/**
 * Invert the given float.
 */
USTRUCT(meta=(DisplayName = "Invert", Category = "Math|Float"))
struct FMetaStoryInvertFloatPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStorySingleFloatPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

/**
 * Gives the absolute value of the given float.
 */
USTRUCT(meta=(DisplayName = "Absolute", Category = "Math|Float"))
struct FMetaStoryAbsoluteFloatPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStorySingleFloatPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

#undef UE_API
