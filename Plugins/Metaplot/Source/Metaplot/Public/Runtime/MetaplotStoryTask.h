#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Flow/MetaplotFlow.h"
#include "MetaplotStoryTask.generated.h"

class UMetaplotInstance;

UENUM()
enum class EMetaplotStoryTaskCompletionType : uint8
{
	/** All tasks need to complete for the group to completes. */
	All,
	/** Any task completes the group. */
	Any,
};


UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class METAPLOT_API UMetaplotStoryTask : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Task")
	void EnterTask(UMetaplotInstance* Instance, FGuid NodeId);

	UFUNCTION(BlueprintNativeEvent, Category = "Task")
	EMetaplotTaskRunState TickTask(UMetaplotInstance* Instance, float DeltaTime);

	UFUNCTION(BlueprintNativeEvent, Category = "Task")
	void ExitTask(UMetaplotInstance* Instance, FGuid NodeId);
};
