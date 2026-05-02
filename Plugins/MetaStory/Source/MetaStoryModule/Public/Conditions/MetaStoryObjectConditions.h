// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryConditionBase.h"
#include "MetaStoryObjectConditions.generated.h"

#define UE_API METASTORYMODULE_API

USTRUCT()
struct FMetaStoryObjectIsValidConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UObject> Object = nullptr;
};

/**
 * Condition testing if specified object is valid.
 */
USTRUCT(DisplayName = "Object Is Valid", Category = "Object")
struct FMetaStoryObjectIsValidCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryObjectIsValidConditionInstanceData;

	FMetaStoryObjectIsValidCondition() = default;
	explicit FMetaStoryObjectIsValidCondition(const EMetaStoryCompare InInverts)
		: bInvert(InInverts == EMetaStoryCompare::Invert)
	{}

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
#endif

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};


USTRUCT()
struct FMetaStoryObjectEqualsConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UObject> Left = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UObject> Right = nullptr;
};

/**
 * Condition testing if two object pointers point to the same object.
 */
USTRUCT(DisplayName = "Object Equals", Category = "Object")
struct FMetaStoryObjectEqualsCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryObjectEqualsConditionInstanceData;

	FMetaStoryObjectEqualsCondition() = default;
	explicit FMetaStoryObjectEqualsCondition(const EMetaStoryCompare InInverts)
		: bInvert(InInverts == EMetaStoryCompare::Invert)
	{}

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
#endif

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};


USTRUCT()
struct FMetaStoryObjectIsChildOfClassConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UObject> Object = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UClass> Class = nullptr;
};

/**
 * Condition testing if object is child of specified class.
 */
USTRUCT(DisplayName = "Object Class Is", Category = "Object")
struct FMetaStoryObjectIsChildOfClassCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryObjectIsChildOfClassConditionInstanceData;

	FMetaStoryObjectIsChildOfClassCondition() = default;
	explicit FMetaStoryObjectIsChildOfClassCondition(const EMetaStoryCompare InInverts)
		: bInvert(InInverts == EMetaStoryCompare::Invert)
	{}

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;

#if WITH_EDITOR
		UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
#endif

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

#undef UE_API
