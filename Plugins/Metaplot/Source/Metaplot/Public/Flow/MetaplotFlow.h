#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "../Scenario/MetaplotEditorTaskNode.h"
#include "MetaplotFlow.generated.h"

class AActor;
class UMetaplotStoryTask;

UENUM(BlueprintType)
enum class EMetaplotNodeType : uint8
{
	Start = 0 UMETA(DisplayName = "Start"),
	Normal UMETA(DisplayName = "Normal"),
	Conditional UMETA(DisplayName = "Conditional"),
	Parallel UMETA(DisplayName = "Parallel"),
	Terminal UMETA(DisplayName = "Terminal")
};

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
enum class EMetaplotNodeResult : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	Succeeded UMETA(DisplayName = "Succeeded"),
	Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EMetaplotNodeCompletionPolicy : uint8
{
	AllTasksFinished = 0 UMETA(DisplayName = "All Tasks Finished")
};

UENUM(BlueprintType)
enum class EMetaplotNodeResultPolicy : uint8
{
	AllSucceeded = 0 UMETA(DisplayName = "All Succeeded"),
	AnyFailedIsFailed UMETA(DisplayName = "Any Failed Is Failed")
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

USTRUCT(BlueprintType)
struct FMetaplotNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FGuid NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FText NodeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	EMetaplotNodeType NodeType = EMetaplotNodeType::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node", meta = (ClampMin = "0"))
	int32 StageIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node", meta = (ClampMin = "0"))
	int32 LayerIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	EMetaplotNodeCompletionPolicy CompletionPolicy = EMetaplotNodeCompletionPolicy::AllTasksFinished;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	EMetaplotNodeResultPolicy ResultPolicy = EMetaplotNodeResultPolicy::AnyFailedIsFailed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	EMetaplotNodeResult RuntimeResult = EMetaplotNodeResult::None;
};

USTRUCT(BlueprintType)
struct FMetaplotStoryTaskSpec
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMetaplotStoryTask> Task = nullptr;

	UPROPERTY()
	TSoftClassPtr<UMetaplotStoryTask> TaskClass;

	UPROPERTY()
	bool bRequired = true;
};

USTRUCT(BlueprintType)
struct FMetaplotNodeStoryTasks
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid NodeId;

	UPROPERTY()
	TArray<FMetaplotStoryTaskSpec> StoryTasks;
};

/**
 * StateTree-style editor state model for a Metaplot node.
 * This is introduced to align future Details customization with state semantics.
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

	UPROPERTY()
	TArray<FMetaplotNodeStoryTasks> NodeTaskSets;

	UPROPERTY()
	TArray<FMetaplotNodeEditorTasks> NodeEditorTaskSets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flow")
	TArray<FMetaplotNodeState> NodeStates;

	UPROPERTY()
	FGuid StartNodeId;

	bool MigrateStoryTaskSpecsToEditorTaskNodes();
	bool NormalizeEditorTaskNodes();
	bool SyncNodeEditorTaskSetsWithNodes();
};
