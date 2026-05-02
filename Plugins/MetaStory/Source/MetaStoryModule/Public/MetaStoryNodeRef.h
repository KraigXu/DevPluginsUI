// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtr.h"

class UMetaStory;
struct FMetaStoryTaskBase;

/**
 * A reference to a task that can utilized in a async callback. Use FMetaStoryWeakTaskRef to store the reference and Pin it to get the strong version. Similar to StrongPtr and WeakPtr.
 */
struct UE_DEPRECATED(5.6, "FMetaStoryStrongTaskRef is deprecated. We now use TaskIndex in WeakExecutionContext.") FMetaStoryStrongTaskRef
{
	explicit FMetaStoryStrongTaskRef() = default;
#if WITH_METASTORY_DEBUG
	explicit FMetaStoryStrongTaskRef(TStrongObjectPtr<const UMetaStory> MetaStory, const FMetaStoryTaskBase* Task, FMetaStoryIndex16 NodeIndex, FGuid NodeId);
#else
	explicit FMetaStoryStrongTaskRef(TStrongObjectPtr<const UMetaStory> MetaStory, const FMetaStoryTaskBase* Task, FMetaStoryIndex16 NodeIndex);
#endif

	METASTORYMODULE_API const UMetaStory* GetMetaStory() const;
	METASTORYMODULE_API const FMetaStoryTaskBase* GetTask() const;
	FMetaStoryIndex16 GetTaskIndex() const
	{
		return NodeIndex;
	}

	operator bool() const
	{
		return IsValid();
	}

	METASTORYMODULE_API bool IsValid() const;

private:
	TStrongObjectPtr<const UMetaStory> MetaStory;
	const FMetaStoryTaskBase* Task = nullptr;
	FMetaStoryIndex16 NodeIndex = FMetaStoryIndex16::Invalid;
#if WITH_METASTORY_DEBUG
	FGuid NodeId;
#endif
};


/**
 * A reference to a task that can be retrieve. Similar to StrongPtr and WeakPtr.
 */
struct UE_DEPRECATED(5.6, "FMetaStoryWeakTaskRef is deprecated. We now use TaskIndex in WeakExecutionContext.") FMetaStoryWeakTaskRef
{
	explicit FMetaStoryWeakTaskRef() = default;
	METASTORYMODULE_API explicit FMetaStoryWeakTaskRef(TNotNull<const UMetaStory*> MetaStory, FMetaStoryIndex16 TaskIndex);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	METASTORYMODULE_API FMetaStoryStrongTaskRef Pin() const;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	void Release()
	{
		*this = FMetaStoryWeakTaskRef();
	}

private:
	TWeakObjectPtr<const UMetaStory> MetaStory;
	FMetaStoryIndex16 NodeIndex = FMetaStoryIndex16::Invalid;
#if WITH_METASTORY_DEBUG
	FGuid NodeId;
#endif
};
