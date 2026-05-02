// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"
#include "MetaStoryEvaluatorBase.h"
#include "MetaStoryNodeBlueprintBase.h"
#include "MetaStoryEvaluatorBlueprintBase.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;

/*
 * Base class for Blueprint based evaluators. 
 */
UCLASS(MinimalAPI, Abstract, Blueprintable)
class UMetaStoryEvaluatorBlueprintBase : public UMetaStoryNodeBlueprintBase
{
	GENERATED_BODY()
public:
	UE_API UMetaStoryEvaluatorBlueprintBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "TreeStart"))
	UE_API void ReceiveTreeStart();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "TreeStop"))
	UE_API void ReceiveTreeStop();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Tick"))
	UE_API void ReceiveTick(const float DeltaTime);

protected:
	UE_API virtual void TreeStart(FMetaStoryExecutionContext& Context);
	UE_API virtual void TreeStop(FMetaStoryExecutionContext& Context);
	UE_API virtual void Tick(FMetaStoryExecutionContext& Context, const float DeltaTime);

	uint8 bHasTreeStart : 1;
	uint8 bHasTreeStop : 1;
	uint8 bHasTick : 1;

	friend struct FMetaStoryBlueprintEvaluatorWrapper;
};

/**
 * Wrapper for Blueprint based Evaluators.
 */
USTRUCT()
struct FMetaStoryBlueprintEvaluatorWrapper : public FMetaStoryEvaluatorBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override { return EvaluatorClass; };
	
	UE_API virtual void TreeStart(FMetaStoryExecutionContext& Context) const override;
	UE_API virtual void TreeStop(FMetaStoryExecutionContext& Context) const override;
	UE_API virtual void Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override;
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
	UE_API virtual FName GetIconName() const override;
	UE_API virtual FColor GetIconColor() const override;
#endif
	
	UPROPERTY()
	TSubclassOf<UMetaStoryEvaluatorBlueprintBase> EvaluatorClass;
};

#undef UE_API
