// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetaStoryFlowNode.generated.h"

UENUM(BlueprintType)
enum class EMetaStoryFlowNodeType : uint8
{
	Start = 0 UMETA(DisplayName = "Start"),
	Normal UMETA(DisplayName = "Normal"),
	Conditional UMETA(DisplayName = "Conditional"),
	Parallel UMETA(DisplayName = "Parallel"),
	Terminal UMETA(DisplayName = "Terminal")
};

UENUM(BlueprintType)
enum class EMetaStoryFlowNodeResult : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	Succeeded UMETA(DisplayName = "Succeeded"),
	Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EMetaStoryFlowNodeResultPolicy : uint8
{
	AllSucceeded = 0 UMETA(DisplayName = "All Succeeded"),
	AnyFailedIsFailed UMETA(DisplayName = "Any Failed Is Failed")
};

UENUM(BlueprintType)
enum class EMetaStoryFlowNodeCompletionPolicy : uint8
{
	AllTasksFinished = 0 UMETA(DisplayName = "All Tasks Finished")
};

/**
 * Grid node for MetaStory embedded flow topology (stage/layer layout).
 */
USTRUCT(BlueprintType)
struct METASTORYMODULE_API FMetaStoryFlowNode
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "", meta = (EditCondition = "false", EditConditionHides))
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FGuid NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FText NodeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	EMetaStoryFlowNodeType NodeType = EMetaStoryFlowNodeType::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node", meta = (ClampMin = "0"))
	int32 StageIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node", meta = (ClampMin = "0"))
	int32 LayerIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	EMetaStoryFlowNodeCompletionPolicy CompletionPolicy = EMetaStoryFlowNodeCompletionPolicy::AllTasksFinished;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	EMetaStoryFlowNodeResultPolicy ResultPolicy = EMetaStoryFlowNodeResultPolicy::AnyFailedIsFailed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	EMetaStoryFlowNodeResult RuntimeResult = EMetaStoryFlowNodeResult::None;
};
