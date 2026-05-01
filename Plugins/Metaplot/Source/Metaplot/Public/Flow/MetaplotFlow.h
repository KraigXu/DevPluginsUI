#pragma once

#include "CoreMinimal.h"
#include "MetaplotNode.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "../Scenario/MetaplotEditorTaskNode.h"
#include "MetaplotFlow.generated.h"

class AActor;
class UMetaplotStoryTask;

UENUM(BlueprintType)
enum class EMetaplotConditionType : uint8
{
	RequiredNodeCompleted = 0 UMETA(DisplayName = "Required Node Completed"),
	RandomProbability UMETA(DisplayName = "Random Probability"),
	CustomBehavior UMETA(DisplayName = "Custom Behavior")
};

UENUM(BlueprintType)
enum class EMetaplotTaskRunState : uint8
{
	Running = 0 UMETA(DisplayName = "Running"),
	Succeeded UMETA(DisplayName = "Succeeded"),
	Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EMetaplotTaskCompletionType : uint8
{
	Any = 0 UMETA(DisplayName = "Any"),
	All UMETA(DisplayName = "All")
};

USTRUCT(BlueprintType)
struct FMetaplotCondition
{
	GENERATED_BODY()

	UPROPERTY()
	EMetaplotConditionType Type = EMetaplotConditionType::RequiredNodeCompleted;

	UPROPERTY()
	FGuid RequiredNodeId;

	UPROPERTY()
	float Probability = 1.0f;
};

USTRUCT(BlueprintType)
struct FMetaplotTransition
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid SourceNodeId;

	UPROPERTY()
	FGuid TargetNodeId;

	UPROPERTY()
	TArray<FMetaplotCondition> Conditions;
};


/**
 * Editor/runtime state model for a Metaplot node.
 */
USTRUCT(BlueprintType)
struct FMetaplotNodeState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FGuid ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	EMetaplotNodeType Type = EMetaplotNodeType::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
	EMetaplotTaskCompletionType TasksCompletion = EMetaplotTaskCompletionType::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tasks")
	TArray<FMetaplotEditorTaskNode> Tasks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transitions")
	TArray<FMetaplotTransition> Transitions;
};

UCLASS(BlueprintType)
class METAPLOT_API UMetaplotFlow : public UObject
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	UPROPERTY()
	TArray<FMetaplotNode> Nodes;

	UPROPERTY()
	TArray<FMetaplotTransition> Transitions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow")
	TArray<FMetaplotNodeState> NodeStates;

	UPROPERTY()
	FGuid StartNodeId;

	bool NormalizeEditorTaskNodes();
	bool SyncNodeStatesWithNodes();
};
