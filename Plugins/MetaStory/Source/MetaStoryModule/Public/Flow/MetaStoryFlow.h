// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Flow/MetaStoryFlowNode.h"
#include "Flow/MetaStoryFlowEditorTaskNode.h"
#include "UObject/Object.h"
#include "MetaStoryFlow.generated.h"

UENUM(BlueprintType)
enum class EMetaStoryFlowConditionType : uint8
{
	RequiredNodeCompleted = 0 UMETA(DisplayName = "Required Node Completed"),
	RandomProbability UMETA(DisplayName = "Random Probability"),
	CustomBehavior UMETA(DisplayName = "Custom Behavior")
};

UENUM(BlueprintType)
enum class EMetaStoryFlowTaskRunState : uint8
{
	Running = 0 UMETA(DisplayName = "Running"),
	Succeeded UMETA(DisplayName = "Succeeded"),
	Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EMetaStoryFlowTaskCompletionType : uint8
{
	Any = 0 UMETA(DisplayName = "Any"),
	All UMETA(DisplayName = "All")
};

USTRUCT(BlueprintType)
struct FMetaStoryFlowCondition
{
	GENERATED_BODY()

	UPROPERTY()
	EMetaStoryFlowConditionType Type = EMetaStoryFlowConditionType::RequiredNodeCompleted;

	UPROPERTY()
	FGuid RequiredNodeId;

	UPROPERTY()
	float Probability = 1.0f;
};

USTRUCT(BlueprintType)
struct FMetaStoryFlowTransition
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid SourceNodeId;

	UPROPERTY()
	FGuid TargetNodeId;

	UPROPERTY()
	TArray<FMetaStoryFlowCondition> Conditions;
};

USTRUCT(BlueprintType)
struct FMetaStoryFlowNodeState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGuid ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	EMetaStoryFlowNodeType Type = EMetaStoryFlowNodeType::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	EMetaStoryFlowTaskCompletionType TasksCompletion = EMetaStoryFlowTaskCompletionType::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tasks")
	TArray<FMetaStoryFlowEditorTaskNode> Tasks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions")
	TArray<FMetaStoryFlowTransition> Transitions;
};

UCLASS(BlueprintType)
class METASTORYMODULE_API UMetaStoryFlow : public UObject
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	UPROPERTY()
	TArray<FMetaStoryFlowNode> Nodes;

	UPROPERTY()
	TArray<FMetaStoryFlowTransition> Transitions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow")
	TArray<FMetaStoryFlowNodeState> NodeStates;

	UPROPERTY()
	FGuid StartNodeId;

	bool NormalizeEditorTaskNodes();
	bool SyncNodeStatesWithNodes();
};
