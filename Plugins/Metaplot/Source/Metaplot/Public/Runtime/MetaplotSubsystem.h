#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MetaplotSubsystem.generated.h"

class UMetaplotFlow;
class UMetaplotInstance;

UCLASS()
class METAPLOT_API UMetaplotSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Runtime")
	UMetaplotInstance* StartMetaplotInstance(UMetaplotFlow* FlowAsset);

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Runtime")
	void TickAll(float DeltaTime);

	const TArray<TObjectPtr<UMetaplotInstance>>& GetInstances() const { return Instances; }

private:
	UPROPERTY()
	TArray<TObjectPtr<UMetaplotInstance>> Instances;
};
