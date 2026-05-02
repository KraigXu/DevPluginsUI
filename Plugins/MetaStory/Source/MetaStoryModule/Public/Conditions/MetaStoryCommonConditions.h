// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AITypes.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryAnyEnum.h"
#include "MetaStoryCommonConditions.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryDataView;

USTRUCT()
struct FMetaStoryCompareIntConditionInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Input")
	int32 Left = 0;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 Right = 0;
};
METASTORY_POD_INSTANCEDATA(FMetaStoryCompareIntConditionInstanceData);

/**
 * Condition comparing two integers.
 */
USTRUCT(DisplayName = "Integer Compare")
struct FMetaStoryCompareIntCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryCompareIntConditionInstanceData;

	FMetaStoryCompareIntCondition() = default;
	explicit FMetaStoryCompareIntCondition(const EGenericAICheck InOperator, const EMetaStoryCompare InInverts = EMetaStoryCompare::Default)
		: bInvert(InInverts == EMetaStoryCompare::Invert)
		, Operator(InOperator)
	{}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
#endif
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (InvalidEnumValues = "IsTrue"))
	EGenericAICheck Operator = EGenericAICheck::Equal;
};


USTRUCT()
struct FMetaStoryCompareFloatConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	double Left = 0.0;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	double Right = 0.0;
};
METASTORY_POD_INSTANCEDATA(FMetaStoryCompareFloatConditionInstanceData);

/**
 * Condition comparing two floats.
 */
USTRUCT(DisplayName = "Float Compare")
struct FMetaStoryCompareFloatCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryCompareFloatConditionInstanceData;

	FMetaStoryCompareFloatCondition() = default;
	explicit FMetaStoryCompareFloatCondition(const EGenericAICheck InOperator, const EMetaStoryCompare InInverts = EMetaStoryCompare::Default)
		: bInvert(InInverts == EMetaStoryCompare::Invert)
		, Operator(InOperator)
	{}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
#endif

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (InvalidEnumValues = "IsTrue"))
	EGenericAICheck Operator = EGenericAICheck::Equal;
};


USTRUCT()
struct FMetaStoryCompareBoolConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	bool bLeft = false;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bRight = false;
};
METASTORY_POD_INSTANCEDATA(FMetaStoryCompareBoolConditionInstanceData);

/**
 * Condition comparing two booleans.
 */
USTRUCT(DisplayName = "Bool Compare")
struct FMetaStoryCompareBoolCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryCompareBoolConditionInstanceData;

	FMetaStoryCompareBoolCondition() = default;
	explicit FMetaStoryCompareBoolCondition(const EMetaStoryCompare InInverts)
		: bInvert(InInverts == EMetaStoryCompare::Invert)
	{}

	FMetaStoryCompareBoolCondition(const bool bInInverts)
		: bInvert(bInInverts)
	{}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
#endif

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};


USTRUCT()
struct FMetaStoryCompareEnumConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta=(AllowAnyBinding))
	FMetaStoryAnyEnum Left;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FMetaStoryAnyEnum Right;
};
METASTORY_POD_INSTANCEDATA(FMetaStoryCompareEnumConditionInstanceData);

/**
 * Condition comparing two enums.
 */
USTRUCT(DisplayName = "Enum Compare")
struct FMetaStoryCompareEnumCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryCompareEnumConditionInstanceData;

	FMetaStoryCompareEnumCondition() = default;
	explicit FMetaStoryCompareEnumCondition(const EMetaStoryCompare InInverts)
		: bInvert(InInverts == EMetaStoryCompare::Invert)
	{}

	FMetaStoryCompareEnumCondition(const bool bInInverts)
		: bInvert(bInInverts)
	{}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
	UE_API virtual void OnBindingChanged(const FGuid& ID, FMetaStoryDataView InstanceData, const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath, const IMetaStoryBindingLookup& BindingLookup) override;
#endif

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};


USTRUCT()
struct FMetaStoryCompareDistanceConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector Source = FVector(EForceInit::ForceInitToZero);

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector Target = FVector(EForceInit::ForceInitToZero);

	UPROPERTY(EditAnywhere, Category = "Parameter")
	double Distance = 0.0;
};
METASTORY_POD_INSTANCEDATA(FMetaStoryCompareDistanceConditionInstanceData);

/**
 * Condition comparing distance between two vectors.
 */
USTRUCT(DisplayName = "Distance Compare")
struct FMetaStoryCompareDistanceCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryCompareDistanceConditionInstanceData;

	FMetaStoryCompareDistanceCondition() = default;
	explicit FMetaStoryCompareDistanceCondition(const EGenericAICheck InOperator, const EMetaStoryCompare InInverts = EMetaStoryCompare::Default)
		: bInvert(InInverts == EMetaStoryCompare::Invert)
		, Operator(InOperator)
	{}
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
#endif

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (InvalidEnumValues = "IsTrue"))
	EGenericAICheck Operator = EGenericAICheck::Equal;
};

USTRUCT()
struct FMetaStoryCompareNameConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FName Left = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName Right = NAME_None;
};
METASTORY_POD_INSTANCEDATA(FMetaStoryCompareNameConditionInstanceData);

/**
 * Condition comparing two FNames.
 */
USTRUCT(DisplayName = "Name Compare")
struct FMetaStoryCompareNameCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryCompareNameConditionInstanceData;

	FMetaStoryCompareNameCondition() = default;
	explicit FMetaStoryCompareNameCondition(const EMetaStoryCompare InInverts)
	: bInvert(InInverts == EMetaStoryCompare::Invert)
	{}

	FMetaStoryCompareNameCondition(const bool bInInverts)
	: bInvert(bInInverts)
	{}

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
#endif

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};


USTRUCT()
struct FMetaStoryRandomConditionInstanceData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Threshold = 0.5f;
};
METASTORY_POD_INSTANCEDATA(FMetaStoryRandomConditionInstanceData);

/**
 * Random condition
 */
USTRUCT(DisplayName = "Random")
struct FMetaStoryRandomCondition : public FMetaStoryConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryRandomConditionInstanceData;

	FMetaStoryRandomCondition() = default;
	
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
#endif
};

#undef UE_API
