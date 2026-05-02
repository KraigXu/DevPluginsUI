// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryNodeBase.h"
#include "MetaStoryPropertyFunctionBase.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;

/**
 * Base struct for all property functions.
 * PropertyFunction is a node which is executed just before evaluating owner's bindings.
 *
 * The property function's instance data is expected to have one property marked as output.
 * This property is used to find which properties the function can be used for,
 * and that property is hidden in the UI. It is expected that there's just one output property.
 *
 * Example:
 *
 *	USTRUCT()
 *	struct FMetaStoryBooleanOperationPropertyFunctionInstanceData
 *	{
 *		GENERATED_BODY()
 *
 *		UPROPERTY(EditAnywhere, Category = Param)
 *		bool bLeft = false;
 *
 *		UPROPERTY(EditAnywhere, Category = Param)
 *		bool bRight = false;
 *
 *      // This property is used to find which properties the function can be used for.
 *		UPROPERTY(EditAnywhere, Category = Output)
 *		bool bResult = false;
 *	};
 *
 */
USTRUCT(meta = (Hidden))
struct FMetaStoryPropertyFunctionBase : public FMetaStoryNodeBase
{
	GENERATED_BODY()

	/**
	 * Called right before evaluating bindings for the owning node.
	 * @param Context Reference to current execution context.
	 */
	virtual void Execute(FMetaStoryExecutionContext& Context) const {}

#if WITH_EDITOR
	virtual FName GetIconName() const override
	{
		return FName("MetaStoryEditorStyle|Node.Function");
	}

	UE_API virtual FColor GetIconColor() const override;
#endif
};

USTRUCT(meta = (Hidden))
struct FMetaStoryPropertyFunctionCommonBase : public FMetaStoryPropertyFunctionBase
{
	GENERATED_BODY()
};

#undef UE_API
