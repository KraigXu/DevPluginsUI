#pragma once

#include "CoreMinimal.h"
#include "Runtime/MetaplotStoryTask.h"
#include "MetaplotSampleStoryTasks.generated.h"

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class DEVPLUGINSUI_API UMetaplotSampleDelayTask : public UMetaplotStoryTask
{
	GENERATED_BODY()

public:
	virtual void EnterTask_Implementation(UMetaplotInstance* Instance, FGuid NodeId) override;
	virtual EMetaplotTaskRunState TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task", meta = (ClampMin = "0.0"))
	float DurationSeconds = 1.0f;

private:
	float RemainingSeconds = 0.0f;
};
