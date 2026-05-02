// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryPropertyFunctionBase.h"
#include "MetaStoryIntPropertyFunctions.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;

USTRUCT()
struct FMetaStoryIntCombinaisonPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 Left = 0;

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 Right = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 Result = 0;
};

/**
 * Add two ints.
 */
USTRUCT(meta=(DisplayName = "Add", Category = "Math|Integer"))
struct FMetaStoryAddIntPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryIntCombinaisonPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

/**
 * Subtract right int from left int.
 */
USTRUCT(meta=(DisplayName = "Subtract", Category = "Math|Integer"))
struct FMetaStorySubtractIntPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryIntCombinaisonPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

/**
 * Multiply the two given ints.
 */
USTRUCT(meta=(DisplayName = "Multiply", Category = "Math|Integer"))
struct FMetaStoryMultiplyIntPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryIntCombinaisonPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

/**
 * Divide left int by right int.
 */
USTRUCT(meta=(DisplayName = "Divide", Category = "Math|Integer"))
struct FMetaStoryDivideIntPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryIntCombinaisonPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

USTRUCT()
struct FMetaStorySingleIntPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 Input = 0;

	UPROPERTY(EditAnywhere, Category = Output)
	int32 Result = 0;
};

/**
 * Invert the given int.
 */
USTRUCT(meta=(DisplayName = "Invert", Category = "Math|Integer"))
struct FMetaStoryInvertIntPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStorySingleIntPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

/**
 * Gives the absolute value of the given int.
 */
USTRUCT(meta=(DisplayName = "Absolute", Category = "Math|Integer"))
struct FMetaStoryAbsoluteIntPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStorySingleIntPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

#undef UE_API
