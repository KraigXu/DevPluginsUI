// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryTaskBase.h"
#include "MetaStoryDelayTask.generated.h"

#define UE_API METASTORYMODULE_API

enum class EMetaStoryRunStatus : uint8;
struct FMetaStoryTransitionResult;

USTRUCT()
struct FMetaStoryDelayTaskInstanceData
{
	GENERATED_BODY()
	
	/** Delay before the task ends. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "!bRunForever", ClampMin="0.0"))
	float Duration = 1.f;
	
	/** Adds random range to the Duration. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "!bRunForever", ClampMin="0.0"))
	float RandomDeviation = 0.f;
	
	/** If true the task will run forever until a transition stops it. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bRunForever = false;

	/** Internal countdown in seconds. */
	float RemainingTime = 0.f;

	/** The handle of the scheduled tick request. */
	UE::MetaStory::FScheduledTickHandle ScheduledTickHandle;
};

/**
 * Simple task to wait indefinitely or for a given time (in seconds) before succeeding.
 */
USTRUCT(meta = (DisplayName = "Delay Task"))
struct FMetaStoryDelayTask : public FMetaStoryTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryDelayTaskInstanceData;
	
	UE_API FMetaStoryDelayTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override;
	UE_API virtual EMetaStoryRunStatus Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override;
	UE_API virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
	virtual FName GetIconName() const override
	{
		return FName("MetaStoryEditorStyle|Node.Time");
	}
	virtual FColor GetIconColor() const override
	{
		return UE::MetaStory::Colors::Grey;
	}
#endif
};

#undef UE_API
