// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/SoftObjectPtr.h"
#include "MetaStoryFlowEditorTaskNode.generated.h"

USTRUCT(BlueprintType)
struct METASTORYMODULE_API FMetaStoryFlowEditorTaskNodeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	TSoftClassPtr<UObject> TaskClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bConsideredForCompletion = true;
};

USTRUCT(BlueprintType)
struct METASTORYMODULE_API FMetaStoryFlowEditorTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Task", meta = (ShowOnlyInnerProperties))
	TObjectPtr<UObject> InstanceObject = nullptr;
};

USTRUCT(BlueprintType)
struct METASTORYMODULE_API FMetaStoryFlowEditorTaskNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FGuid ID;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Task", meta = (ShowOnlyInnerProperties))
	TObjectPtr<UObject> InstanceObject = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	bool bConsideredForCompletion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FInstancedStruct NodeData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Task")
	FInstancedStruct InstanceData;
};
