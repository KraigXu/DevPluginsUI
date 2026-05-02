// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryNodeBase.h"
#include "MetaStoryEvaluatorBase.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;
struct FMetaStoryReadOnlyExecutionContext;

/**
 * Base struct of MetaStory Evaluators.
 * Evaluators calculate and expose data to be used for decision making in a MetaStory.
 */
USTRUCT(meta = (Hidden))
struct FMetaStoryEvaluatorBase : public FMetaStoryNodeBase
{
	GENERATED_BODY()
	
	/**
	 * Called when MetaStory is started.
	 * @param Context Reference to current execution context.
	 */
	virtual void TreeStart(FMetaStoryExecutionContext& Context) const {}

	/**
	 * Called when MetaStory is stopped.
	 * @param Context Reference to current execution context.
	 */
	virtual void TreeStop(FMetaStoryExecutionContext& Context) const {}

	/**
	 * Called each frame to update the evaluator.
	 * @param Context Reference to current execution context.
	 * @param DeltaTime Time since last MetaStory tick, or 0 if called during preselection.
	 */
	virtual void Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const {}

#if WITH_GAMEPLAY_DEBUGGER
	UE_API virtual FString GetDebugInfo(const FMetaStoryReadOnlyExecutionContext& Context) const;

	UE_DEPRECATED(5.6, "Use the version with the FMetaStoryReadOnlyExecutionContext.")
	UE_API virtual void AppendDebugInfoString(FString& DebugString, const FMetaStoryExecutionContext& Context) const;
#endif // WITH_GAMEPLAY_DEBUGGER
};

/**
* Base class (namespace) for all common Evaluators that are generally applicable.
* This allows schemas to safely include all Evaluators child of this struct. 
*/
USTRUCT(Meta=(Hidden))
struct FMetaStoryEvaluatorCommonBase : public FMetaStoryEvaluatorBase
{
	GENERATED_BODY()
};

#undef UE_API
