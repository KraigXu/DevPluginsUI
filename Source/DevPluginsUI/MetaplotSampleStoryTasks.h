#pragma once

#include "CoreMinimal.h"
#include "Runtime/MetaplotStoryTask.h"
#include "MetaplotSampleStoryTasks.generated.h"

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class DEVPLUGINSUI_API UMetaplotSampleDelayTask : public UMetaplotStoryTask
{
	GENERATED_BODY()

public:
	virtual void EnterTask_Implementation(UMetaplotInstance* Instance, FGuid NodeId) override;
	virtual EMetaplotTaskRunState TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Sample", meta = (ClampMin = "0.0"))
	float DurationSeconds = 1.0f;

private:
	float RemainingSeconds = 0.0f;
};

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class DEVPLUGINSUI_API UMetaplotSampleSetStringTask : public UMetaplotStoryTask
{
	GENERATED_BODY()

public:
	virtual EMetaplotTaskRunState TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Sample")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Sample")
	FString Value;
};

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class DEVPLUGINSUI_API UMetaplotSampleIncIntTask : public UMetaplotStoryTask
{
	GENERATED_BODY()

public:
	virtual EMetaplotTaskRunState TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Sample")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Sample")
	int32 Delta = 1;
};
