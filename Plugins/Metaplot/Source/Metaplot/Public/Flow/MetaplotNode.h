// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "MetaplotNode.generated.h"



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
enum class EMetaplotNodeResult : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	Succeeded UMETA(DisplayName = "Succeeded"),
	Failed UMETA(DisplayName = "Failed")
};


UENUM(BlueprintType)
enum class EMetaplotNodeResultPolicy : uint8
{
	AllSucceeded = 0 UMETA(DisplayName = "All Succeeded"),
	AnyFailedIsFailed UMETA(DisplayName = "Any Failed Is Failed")
};

UENUM(BlueprintType)
enum class EMetaplotNodeCompletionPolicy : uint8
{
	AllTasksFinished = 0 UMETA(DisplayName = "All Tasks Finished")
};



/**
 * Base struct for Metaplot editor/runtime node data.
 * It intentionally keeps only lightweight, framework-agnostic metadata.
 */
USTRUCT(BlueprintType)
struct METAPLOT_API FMetaplotNode
{
	GENERATED_BODY()
	
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaplotNode() = default;
	FMetaplotNode(const FMetaplotNode&) = default;
	FMetaplotNode(FMetaplotNode&&) = default;
	FMetaplotNode& operator=(const FMetaplotNode&) = default;
	FMetaplotNode& operator=(FMetaplotNode&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	struct FNoInstanceDataType
	{ };

	/**
	 * The instance data type. The implementation node should set its type.
	 * The type should match GetInstanceDataType().
	 */
	using FInstanceDataType = FNoInstanceDataType;

	/**
	 * The execution runtime data type.
	 * Execution runtime is optional. If needed, the implementation node should set its type.
	 * The type should match GetExecutionRuntimeDataType().
	 */
	using FExecutionRuntimeDataType = FNoInstanceDataType;

	virtual ~FMetaplotNode() {}

	/** @return Struct that represents the runtime data of the node. */
	virtual const UStruct* GetInstanceDataType() const
	{
		return nullptr;
	}

	/** @return Struct that represents optional persistent execution runtime data. */
	virtual const UStruct* GetExecutionRuntimeDataType() const
	{
		return nullptr;
	}

#if WITH_EDITOR
	/**
	 * Returns description for the node, use in the UI.
	 * The UI description is selected as follows:
	 * - Node Name, if not empty
	 * - Description if not empty
	 * - Display name of the node struct
	 */
	virtual FText GetDescription() const
	{
		return FText::GetEmpty();
	}

	/**
	 * @returns name of the icon in format:
	 *		StyleSetName | StyleName [ | SmallStyleName | StatusOverlayStyleName]
	 *		SmallStyleName and StatusOverlayStyleName are optional.
	 *		Example: "MetaplotEditorStyle|Node.Task"
	 */
	virtual FName GetIconName() const
	{
		return FName();
	}

	/** @return the color to be used with the icon. */
	virtual FColor GetIconColor() const
	{
		return FColor::FromHex(TEXT("#505050"));
	}
#endif

	/** Called after the owning asset containing this node is loaded from disk. */
	virtual void PostLoad() {}

	/** Name of the node. */
	UPROPERTY(EditDefaultsOnly, Category = "", meta=(EditCondition = "false", EditConditionHides))
	FName Name;

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
