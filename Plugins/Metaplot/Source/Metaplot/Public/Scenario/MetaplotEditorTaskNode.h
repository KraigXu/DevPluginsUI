#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/SoftObjectPtr.h"
#include "MetaplotEditorTaskNode.generated.h"

class UMetaplotStoryTask;

USTRUCT(BlueprintType)
struct METAPLOT_API FMetaplotEditorTaskNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	TSoftClassPtr<UMetaplotStoryTask> TaskClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bConsideredForCompletion = true;
};

USTRUCT(BlueprintType)
struct METAPLOT_API FMetaplotEditorTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Task", meta = (ShowOnlyInnerProperties))
	TObjectPtr<UMetaplotStoryTask> InstanceObject = nullptr;
};

USTRUCT(BlueprintType)
struct METAPLOT_API FMetaplotEditorTaskNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FGuid ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	TSoftClassPtr<UMetaplotStoryTask> TaskClass;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Task", meta = (ShowOnlyInnerProperties))
	TObjectPtr<UMetaplotStoryTask> InstanceObject = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bConsideredForCompletion = true;

	// Phase A bridge: keep an InstancedStruct-backed representation close to StateTree's editor-node style.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FInstancedStruct NodeData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FInstancedStruct InstanceData;
};

USTRUCT(BlueprintType)
struct METAPLOT_API FMetaplotNodeEditorTasks
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FGuid NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	TArray<FMetaplotEditorTaskNode> Tasks;
};
