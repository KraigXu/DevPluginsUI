// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryNodeBase.h"
#include "MetaStoryEditorNode.generated.h"

#define UE_API METASTORYEDITORMODULE_API

UENUM()
enum class EMetaStoryNodeType : uint8
{
	EnterCondition,
	Evaluator,
	Task,
	TransitionCondition,
	StateParameters,
	PropertyFunction,
};

/**
 * Base for Evaluator, Task and Condition nodes.
 */
USTRUCT()
struct FMetaStoryEditorNode
{
	GENERATED_BODY()

	void Reset()
	{
		Node.Reset();
		Instance.Reset();
		InstanceObject = nullptr;
		ExecutionRuntimeData.Reset();
		ExecutionRuntimeDataObject = nullptr;
		ID = FGuid();
	}

	/**
	 * This is used to name nodes for runtime, as well as for error reporting.
	 * If the node has a specified name, used that, or else of return the display name of the node.
	 * @return name of the node.
	 */
	UE_API FName GetName() const;

	/**
	 * Get DataView for the node template.
	 * @return DataView for the node template
	 */
	TStructView<FMetaStoryNodeBase> GetNode() const
	{
		return TStructView<FMetaStoryNodeBase>(Node.GetScriptStruct(), const_cast<uint8*>(Node.GetMemory()));
	}

	/**
	 * Get ID for the node template, which is different from the ID for the Instance.
	 * @return ID for the node template
	 */
	FGuid GetNodeID() const
	{
		return FGuid::Combine(ID, FGuid::NewDeterministicGuid(TEXT("Node Struct")));
	}

	FMetaStoryDataView GetInstance() const
	{
		return InstanceObject ? FMetaStoryDataView(InstanceObject) : FMetaStoryDataView(const_cast<FInstancedStruct&>(Instance));
	}

	FMetaStoryDataView GetInstance()
	{
		return InstanceObject ? FMetaStoryDataView(InstanceObject) : FMetaStoryDataView(Instance);
	}

	FMetaStoryDataView GetExecutionRuntimeData() const
	{
		return ExecutionRuntimeDataObject ? FMetaStoryDataView(ExecutionRuntimeDataObject) : FMetaStoryDataView(const_cast<FInstancedStruct&>(ExecutionRuntimeData));
	}

	FMetaStoryDataView GetExecutionRuntimeData()
	{
		return ExecutionRuntimeDataObject ? FMetaStoryDataView(ExecutionRuntimeDataObject) : FMetaStoryDataView(ExecutionRuntimeData);
	}

	UPROPERTY(EditDefaultsOnly, Category = Node)
	FInstancedStruct Node;

	UPROPERTY(EditDefaultsOnly, Category = Node)
	FInstancedStruct Instance;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = Node)
	TObjectPtr<UObject> InstanceObject = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = Node)
	FInstancedStruct ExecutionRuntimeData;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = Node)
	TObjectPtr<UObject> ExecutionRuntimeDataObject = nullptr;

	/** ID for the node instance. */
	UPROPERTY(EditDefaultsOnly, Category = Node)
	FGuid ID;

	UPROPERTY(EditDefaultsOnly, Category = Node)
	uint8 ExpressionIndent = 0;

	UPROPERTY(EditDefaultsOnly, Category = Node)
	EMetaStoryExpressionOperand ExpressionOperand = EMetaStoryExpressionOperand::And;
};

template <typename T>
struct TStateTreeEditorNode : public FMetaStoryEditorNode
{
	using NodeType = T;
	inline T& GetNode() { return Node.template GetMutable<T>(); }
	inline typename T::FInstanceDataType& GetInstanceData() { return Instance.template GetMutable<typename T::FInstanceDataType>(); }
	inline typename T::FExecutionRuntimeDataType& GetExecutionRuntimeData() { return ExecutionRuntimeData.template GetMutable<typename T::FExecutionRuntimeDataType>(); }
};

#undef UE_API
