#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
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
	BlackboardCompare UMETA(DisplayName = "Blackboard Compare"),
	RandomProbability UMETA(DisplayName = "Random Probability"),
	CustomBehavior UMETA(DisplayName = "Custom Behavior")
};

UENUM(BlueprintType)
enum class EMetaplotComparisonOp : uint8
{
	Equal = 0 UMETA(DisplayName = "=="),
	NotEqual UMETA(DisplayName = "!="),
	Greater UMETA(DisplayName = ">"),
	Less UMETA(DisplayName = "<"),
	GreaterOrEqual UMETA(DisplayName = ">="),
	LessOrEqual UMETA(DisplayName = "<=")
};

UENUM(BlueprintType)
enum class EMetaplotBlackboardType : uint8
{
	Bool = 0 UMETA(DisplayName = "Bool"),
	Int UMETA(DisplayName = "Int"),
	Float UMETA(DisplayName = "Float"),
	String UMETA(DisplayName = "String"),
	Object UMETA(DisplayName = "Object")
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

USTRUCT(BlueprintType)
struct FMetaplotCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Condition")
	EMetaplotConditionType Type = EMetaplotConditionType::RequiredNodeCompleted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Condition")
	FGuid RequiredNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Condition")
	FName BlackboardKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Condition")
	EMetaplotComparisonOp ComparisonOp = EMetaplotComparisonOp::Equal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Condition")
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Condition")
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Condition")
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Condition")
	FString StringValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Condition")
	TSoftObjectPtr<UObject> ObjectValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Condition", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Probability = 1.0f;
};

USTRUCT(BlueprintType)
struct FMetaplotTransition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Transition")
	FGuid SourceNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Transition")
	FGuid TargetNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Transition")
	TArray<FMetaplotCondition> Conditions;
};

USTRUCT(BlueprintType)
struct FMetaplotNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node")
	FGuid NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node")
	FText NodeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node")
	EMetaplotNodeType NodeType = EMetaplotNodeType::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node", meta = (ClampMin = "0"))
	int32 StageIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node", meta = (ClampMin = "0"))
	int32 LayerIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node")
	EMetaplotNodeCompletionPolicy CompletionPolicy = EMetaplotNodeCompletionPolicy::AllTasksFinished;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node")
	EMetaplotNodeResultPolicy ResultPolicy = EMetaplotNodeResultPolicy::AnyFailedIsFailed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node")
	EMetaplotNodeResult RuntimeResult = EMetaplotNodeResult::None;
};

USTRUCT(BlueprintType)
struct FMetaplotStoryTaskSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Task")
	TSoftClassPtr<UMetaplotStoryTask> TaskClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Task")
	bool bRequired = true;
};

USTRUCT(BlueprintType)
struct FMetaplotNodeStoryTasks
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node")
	FGuid NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Node")
	TArray<FMetaplotStoryTaskSpec> StoryTasks;
};

USTRUCT(BlueprintType)
struct FMetaplotBlackboardEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Blackboard")
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Blackboard")
	EMetaplotBlackboardType Type = EMetaplotBlackboardType::Bool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Blackboard")
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Blackboard")
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Blackboard")
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Blackboard")
	FString StringValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Blackboard")
	TSoftObjectPtr<UObject> ObjectValue;
};

UCLASS(BlueprintType)
class METAPLOT_API UMetaplotFlow : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Flow")
	TArray<FMetaplotNode> Nodes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Flow")
	TArray<FMetaplotTransition> Transitions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Flow")
	TArray<FMetaplotNodeStoryTasks> NodeTaskSets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Flow")
	TArray<FMetaplotBlackboardEntry> DefaultBlackboard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metaplot|Flow")
	FGuid StartNodeId;
};
