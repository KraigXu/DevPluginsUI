// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetaStory.h"
#include "MetaStoryExecutionTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "MetaStoryWorldSubsystem.generated.h"

struct FMetaStoryExecutionContext;
struct FMetaStoryExternalDataDesc;
struct FMetaStoryDataView;

/**
 * Per-UWorld single MetaStory runner: holds FMetaStoryInstanceData and drives FMetaStoryExecutionContext Start/Tick/Stop.
 * World-scoped tick driver for one active UMetaStory at a time.
 *
 * For assets that require context or external data, subclass and override NativePopulateContextData / NativeCollectExternalData.
 */
UCLASS()
class METASTORYMODULE_API UMetaStoryWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	/** Stops the current run (if any) then assigns the asset to use for subsequent StartMetaStory. */
	UFUNCTION(BlueprintCallable, Category = "MetaStory|Runtime")
	void SetActiveMetaStory(UMetaStory* InMetaStory);

	UFUNCTION(BlueprintPure, Category = "MetaStory|Runtime")
	UMetaStory* GetActiveMetaStory() const { return ActiveMetaStory; }

	/**
	 * Resets instance data from the active asset defaults and starts execution.
	 * @return false if the world is not a game world, the asset is missing/not ready, or context validation fails.
	 */
	UFUNCTION(BlueprintCallable, Category = "MetaStory|Runtime")
	bool StartMetaStory();

	UFUNCTION(BlueprintCallable, Category = "MetaStory|Runtime")
	void StopMetaStory(EMetaStoryRunStatus CompletionStatus = EMetaStoryRunStatus::Stopped);

	UFUNCTION(BlueprintPure, Category = "MetaStory|Runtime")
	bool IsMetaStoryRunning() const { return bStoryRunActive; }

	UFUNCTION(BlueprintPure, Category = "MetaStory|Runtime")
	EMetaStoryRunStatus GetLastRunStatus() const { return LastRunStatus; }

	const FMetaStoryInstanceData& GetInstanceData() const { return InstanceData; }
	FMetaStoryInstanceData& GetMutableInstanceData() { return InstanceData; }

protected:
	/** Override to bind schema context data (e.g. Context.SetContextDataByName). Default only checks AreContextDataViewsValid(). */
	virtual bool NativePopulateContextData(FMetaStoryExecutionContext& Context);

	/**
	 * Override to fill OutDataViews for each entry in ExternalDataDescs.
	 * Default returns true when the asset declares no external data; otherwise logs and returns false.
	 */
	virtual bool NativeCollectExternalData(
		const FMetaStoryExecutionContext& Context,
		const UMetaStory* MetaStory,
		TArrayView<const FMetaStoryExternalDataDesc> ExternalDataDescs,
		TArrayView<FMetaStoryDataView> OutDataViews);

private:
	bool HandleCollectExternalData(
		const FMetaStoryExecutionContext& Context,
		const UMetaStory* MetaStory,
		TArrayView<const FMetaStoryExternalDataDesc> ExternalDataDescs,
		TArrayView<FMetaStoryDataView> OutDataViews);

	UPROPERTY()
	TObjectPtr<UMetaStory> ActiveMetaStory = nullptr;

	UPROPERTY()
	FMetaStoryInstanceData InstanceData;

	bool bStoryRunActive = false;
	EMetaStoryRunStatus LastRunStatus = EMetaStoryRunStatus::Unset;
};
