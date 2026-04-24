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
	UFUNCTION(BlueprintCallable, Category = "Metaplot|Runtime")
	bool Initialize(UMetaplotFlow* InFlow);

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Runtime")
	bool Start();

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Runtime")
	void TickInstance(float DeltaTime);

	UFUNCTION(BlueprintPure, Category = "Metaplot|Runtime")
	bool IsRunning() const { return bIsRunning; }

	UFUNCTION(BlueprintPure, Category = "Metaplot|Runtime")
	FGuid GetCurrentNodeId() const { return CurrentNodeId; }

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Blackboard")
	bool SetBlackboardInt(FName Key, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Blackboard")
	bool GetBlackboardInt(FName Key, int32& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Blackboard")
	bool SetBlackboardBool(FName Key, bool Value);

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Blackboard")
	bool GetBlackboardBool(FName Key, bool& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Blackboard")
	bool SetBlackboardFloat(FName Key, float Value);

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Blackboard")
	bool GetBlackboardFloat(FName Key, float& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Blackboard")
	bool SetBlackboardString(FName Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Metaplot|Blackboard")
	bool GetBlackboardString(FName Key, FString& OutValue) const;

protected:
	bool ActivateNode(const FGuid& NodeId);
	void BuildNodeTasks(const FGuid& NodeId, FMetaplotRuntimeNodeState& OutNodeState);
	void EvaluateActiveNode(float DeltaTime);
	EMetaplotNodeResult ComputeNodeResult(const FMetaplotNode& Node, const FMetaplotRuntimeNodeState& NodeState) const;
	bool EvaluateTransitionConditions(const FMetaplotTransition& Transition) const;
	const FMetaplotNode* FindNode(const FGuid& NodeId) const;
	const TArray<FMetaplotStoryTaskSpec>* FindTaskSet(const FGuid& NodeId) const;
	int32 FindBlackboardEntryIndex(FName Key) const;
	bool CompareBlackboardInt(int32 CurrentValue, EMetaplotComparisonOp Op, int32 ExpectedValue) const;

protected:
	UPROPERTY()
	TObjectPtr<UMetaplotFlow> FlowAsset = nullptr;

	UPROPERTY()
	TArray<FMetaplotBlackboardEntry> RuntimeBlackboard;

	UPROPERTY()
	FMetaplotRuntimeNodeState ActiveNodeState;

	UPROPERTY()
	FGuid CurrentNodeId;

	UPROPERTY()
	TSet<FGuid> CompletedNodes;

	UPROPERTY()
	bool bIsRunning = false;
};
