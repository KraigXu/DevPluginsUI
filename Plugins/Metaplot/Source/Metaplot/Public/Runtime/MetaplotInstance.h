#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Flow/MetaplotFlow.h"
#include "MetaplotInstance.generated.h"

class UMetaplotStoryTask;

USTRUCT()
struct FMetaplotRuntimeTaskState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMetaplotStoryTask> TaskInstance = nullptr;

	UPROPERTY()
	bool bRequired = true;

	UPROPERTY()
	EMetaplotTaskRunState RunState = EMetaplotTaskRunState::Running;
};

USTRUCT()
struct FMetaplotRuntimeNodeState
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid NodeId;

	UPROPERTY()
	TArray<FMetaplotRuntimeTaskState> Tasks;

	UPROPERTY()
	EMetaplotNodeResult Result = EMetaplotNodeResult::None;
};

UCLASS(BlueprintType)
class METAPLOT_API UMetaplotInstance : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Runtime")
	bool Initialize(UMetaplotFlow* InFlow);

	UFUNCTION(BlueprintCallable, Category = "Runtime")
	bool Start();

	UFUNCTION(BlueprintCallable, Category = "Runtime")
	void TickInstance(float DeltaTime);

	UFUNCTION(BlueprintPure, Category = "Runtime")
	bool IsRunning() const { return bIsRunning; }

	UFUNCTION(BlueprintPure, Category = "Runtime")
	FGuid GetCurrentNodeId() const { return CurrentNodeId; }

protected:
	bool ActivateNode(const FGuid& NodeId);
	void BuildNodeTasks(const FGuid& NodeId, FMetaplotRuntimeNodeState& OutNodeState);
	void EvaluateActiveNode(float DeltaTime);
	EMetaplotNodeResult ComputeNodeResult(const FMetaplotNode& Node, const FMetaplotRuntimeNodeState& NodeState) const;
	bool EvaluateTransitionConditions(const FMetaplotTransition& Transition) const;
	const FMetaplotNode* FindNode(const FGuid& NodeId) const;
	const FMetaplotNodeState* FindNodeState(const FGuid& NodeId) const;

protected:
	UPROPERTY()
	TObjectPtr<UMetaplotFlow> FlowAsset = nullptr;

	UPROPERTY()
	FMetaplotRuntimeNodeState ActiveNodeState;

	UPROPERTY()
	FGuid CurrentNodeId;

	UPROPERTY()
	TSet<FGuid> CompletedNodes;

	UPROPERTY()
	bool bIsRunning = false;
};
