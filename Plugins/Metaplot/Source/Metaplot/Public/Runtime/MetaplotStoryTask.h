#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Flow/MetaplotFlow.h"
#include "MetaplotStoryTask.generated.h"

class UMetaplotInstance;

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class METAPLOT_API UMetaplotStoryTask : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Metaplot|Task")
	void EnterTask(UMetaplotInstance* Instance, FGuid NodeId);

	UFUNCTION(BlueprintNativeEvent, Category = "Metaplot|Task")
	EMetaplotTaskRunState TickTask(UMetaplotInstance* Instance, float DeltaTime);

	UFUNCTION(BlueprintNativeEvent, Category = "Metaplot|Task")
	void ExitTask(UMetaplotInstance* Instance, FGuid NodeId);
};
