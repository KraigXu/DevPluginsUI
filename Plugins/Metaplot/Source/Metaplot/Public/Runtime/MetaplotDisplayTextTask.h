#pragma once

#include "CoreMinimal.h"
#include "Runtime/MetaplotStoryTask.h"
#include "MetaplotDisplayTextTask.generated.h"

class AActor;
class UMetaplotInstance;

/**
 * Simple story task that exposes text-style parameters in details panel.
 * This is a baseline task model similar to StateTree-style configurable tasks.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories)
class METAPLOT_API UMetaplotDisplayTextTask : public UMetaplotStoryTask
{
	GENERATED_BODY()

public:
	virtual void EnterTask_Implementation(UMetaplotInstance* Instance, FGuid NodeId) override;
	virtual EMetaplotTaskRunState TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime) override;

	/** Text to display. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText Text;

	/** Bindable text input for external values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText BindableText;

	/** Display color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FLinearColor TextColor = FLinearColor::White;

	/** Font scale multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display", meta = (ClampMin = "0.1"))
	float FontScale = 1.0f;

	/** World offset used by consumers that render this task in scene. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FVector Offset = FVector::ZeroVector;

	/** Optional scene reference used by downstream display systems. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	TObjectPtr<AActor> ReferenceActor = nullptr;

	/** If false, task no-ops and completes immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	bool bEnabled = true;

private:
	bool bHasEntered = false;
};

