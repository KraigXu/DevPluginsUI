// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MetaplotFlowSchema.generated.h"

/**
 * Schema describing which Metaplot task types are allowed in an asset.
 */
UCLASS(Abstract)
class METAPLOT_API UMetaplotFlowSchema : public UObject
{
	GENERATED_BODY()

public:

	/** @return True if specified struct is supported */
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const
	{
		return false;
	}

	/** @return True if specified class is supported */
	virtual bool IsClassAllowed(const UClass* InScriptStruct) const
	{
		return false;
	}

	/** @return True if specified struct/class is supported as external data */
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const
	{
		return false;
	}

	/** @return True if the execution context can sleep or the next tick delayed. */
	virtual bool IsScheduledTickAllowed() const
	{
		return false;
	}
	
	/**
	 * Helper to check whether a class is one of the supported Metaplot Blueprint task bases.
	 */
	static bool IsChildOfBlueprintBase(const UClass* InClass);


#if WITH_EDITOR
	/** @return True if enter conditions are allowed. */
	virtual bool AllowEnterConditions() const
	{
		return true;
	}

	/** @return True if utility considerations are allowed. */
	virtual bool AllowUtilityConsiderations() const
	{
		return true;
	}

	/** @return True if evaluators are allowed. */
	virtual bool AllowEvaluators() const
	{
		return true;
	}

	/** @return True if multiple tasks are allowed. */
	virtual bool AllowMultipleTasks() const
	{
		return true;
	}
	
	/** @return True if modifying the tasks completion is allowed. If not allowed, "any" will be used.*/
	virtual bool AllowTasksCompletion() const
	{
		return true;
	}
#endif // WITH_EDITOR
};
