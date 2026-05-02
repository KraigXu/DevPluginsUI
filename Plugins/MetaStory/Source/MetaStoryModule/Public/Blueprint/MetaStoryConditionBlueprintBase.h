// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryNodeBlueprintBase.h"
#include "MetaStoryConditionBlueprintBase.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;

/*
 * Base class for Blueprint based Conditions. 
 */
UCLASS(MinimalAPI, Abstract, Blueprintable)
class UMetaStoryConditionBlueprintBase : public UMetaStoryNodeBlueprintBase
{
	GENERATED_BODY()
public:
	UE_API UMetaStoryConditionBlueprintBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintImplementableEvent)
	UE_API bool ReceiveTestCondition() const;

protected:
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const;

	friend struct FMetaStoryBlueprintConditionWrapper;

	uint8 bHasTestCondition : 1;
};

/**
 * Wrapper for Blueprint based Conditions.
 */
USTRUCT()
struct FMetaStoryBlueprintConditionWrapper : public FMetaStoryConditionBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override { return ConditionClass; };
	UE_API virtual bool TestCondition(FMetaStoryExecutionContext& Context) const override;
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
	UE_API virtual FName GetIconName() const override;
	UE_API virtual FColor GetIconColor() const override;
#endif
	
	UPROPERTY()
	TSubclassOf<UMetaStoryConditionBlueprintBase> ConditionClass;
};

#undef UE_API
