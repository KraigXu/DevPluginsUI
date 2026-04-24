#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MetaplotSubsystem.generated.h"

class UMetaplotFlow;
class UMetaplotInstance;

UCLASS()
class METAPLOT_API UMetaplotSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Metaplot|Runtime")
	UMetaplotInstance* StartMetaplotInstance(UMetaplotFlow* FlowAsset);

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Runtime")
	void TickAll(float DeltaTime);

	const TArray<TObjectPtr<UMetaplotInstance>>& GetInstances() const { return Instances; }

private:
	UPROPERTY()
	TArray<TObjectPtr<UMetaplotInstance>> Instances;
};
