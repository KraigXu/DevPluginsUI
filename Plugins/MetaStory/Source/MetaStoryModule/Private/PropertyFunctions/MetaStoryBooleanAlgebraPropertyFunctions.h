// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryPropertyFunctionBase.h"
#include "MetaStoryBooleanAlgebraPropertyFunctions.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;

USTRUCT()
struct FMetaStoryBooleanOperationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Param)
	bool bLeft = false;

	UPROPERTY(EditAnywhere, Category = Param)
	bool bRight = false;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bResult = false;
};

/**
 * Performs 'And' operation on two booleans.
 */
USTRUCT(meta=(DisplayName = "And", Category="Logic"))
struct FMetaStoryBooleanAndPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryBooleanOperationPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

/**
 * Performs 'Or' operation on two booleans.
 */
USTRUCT(meta=(DisplayName = "Or", Category="Logic"))
struct FMetaStoryBooleanOrPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryBooleanOperationPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

/**
 * Performs 'Exclusive Or' operation on two booleans.
 */
USTRUCT(meta=(DisplayName = "XOr", Category="Logic"))
struct FMetaStoryBooleanXOrPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryBooleanOperationPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

USTRUCT()
struct FMetaStoryBooleanNotOperationPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Param)
	bool bInput = false;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bResult = false;
};

/**
 * Performs 'Not' operation on a boolean.
 */
USTRUCT(meta=(DisplayName = "Not", Category="Logic"))
struct FMetaStoryBooleanNotPropertyFunction : public FMetaStoryPropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryBooleanNotOperationPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual void Execute(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;
#endif
};

#undef UE_API
