#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Flow/MetaplotFlow.h"
#include "MetaplotTransitionDetailsProxy.generated.h"

class UMetaplotFlow;
class UMetaplotDetailsContext;

UCLASS(Transient)
class METAPLOTEDITOR_API UMetaplotTransitionDetailsProxy : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UMetaplotFlow* InFlowAsset, const FGuid& InSourceNodeId, const FGuid& InTargetNodeId);
	void SetDetailsContext(UMetaplotDetailsContext* InDetailsContext);
	UMetaplotDetailsContext* GetDetailsContext() const { return DetailsContext; }

	UPROPERTY(VisibleAnywhere, Category = "Metaplot|Transition")
	FGuid SourceNodeId;

	UPROPERTY(VisibleAnywhere, Category = "Metaplot|Transition")
	FGuid TargetNodeId;

	UPROPERTY(EditAnywhere, Category = "Metaplot|Transition")
	TArray<FMetaplotCondition> Conditions;

protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
	FMetaplotTransition* FindTransitionMutable() const;
	const FMetaplotTransition* FindTransition() const;
	void PullFromFlow();
	void PushToFlow();

	UPROPERTY(Transient)
	TObjectPtr<UMetaplotFlow> FlowAsset;

	UPROPERTY(Transient)
	TObjectPtr<UMetaplotDetailsContext> DetailsContext;
};
