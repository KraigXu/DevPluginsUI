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

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class DEVPLUGINSUI_API UMetaplotSampleSetStringTask : public UMetaplotStoryTask
{
	GENERATED_BODY()

public:
	virtual EMetaplotTaskRunState TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FString Value;
};

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class DEVPLUGINSUI_API UMetaplotSampleIncIntTask : public UMetaplotStoryTask
{
	GENERATED_BODY()

public:
	virtual EMetaplotTaskRunState TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FName Key = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	int32 Delta = 1;
};
