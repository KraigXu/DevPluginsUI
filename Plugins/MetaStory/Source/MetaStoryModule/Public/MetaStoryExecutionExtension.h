// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryExecutionExtension.generated.h"

#define UE_API METASTORYMODULE_API

class UObject;
class UMetaStory;
struct FMetaStoryInstanceStorage;
struct FMetaStoryTransitionDelayedState;
struct FMetaStoryReferenceOverrides;
struct FMetaStoryTransitionResult;

namespace UE::MetaStory
{
	enum class EMetaStoryTickReason : uint8;
}

/** Used by the execution context or a weak execution context to extend their functionalities. */
USTRUCT()
struct FMetaStoryExecutionExtension
{
	GENERATED_BODY()

	struct FContextParameters
	{
		FContextParameters(UObject& Owner, const UMetaStory& MetaStory, FMetaStoryInstanceStorage& InstanceData)
			: Owner(Owner)
			, MetaStory(MetaStory)
			, InstanceData(InstanceData)
		{}
		FContextParameters(TNotNull<UObject*> Owner, TNotNull<const UMetaStory*> MetaStory, FMetaStoryInstanceStorage& InstanceData)
			: Owner(*Owner)
			, MetaStory(*MetaStory)
			, InstanceData(InstanceData)
		{
		}
		UObject& Owner;
		const UMetaStory& MetaStory;
		FMetaStoryInstanceStorage& InstanceData;
	};

	virtual ~FMetaStoryExecutionExtension() = default;

	/** Prefix that will be used by METASTORY_LOG and METASTORY_CLOG, using Entity description. */
	virtual FString GetInstanceDescription(const FContextParameters& Context) const
	{
		return Context.Owner.GetName();
	}

	struct FNextTickArguments
	{
		UE_API FNextTickArguments();
		UE_API explicit FNextTickArguments(UE::MetaStory::EMetaStoryTickReason Reason);

		UE::MetaStory::EMetaStoryTickReason Reason;
	};

	/** Callback when the execution context request the tree to wakeup from a schedule tick sleep. */
	virtual void ScheduleNextTick(const FContextParameters& Context, const FNextTickArguments& Args)
	{
		
	}

	UE_DEPRECATED(5.7, "Use ScheduleNextTick with the FNextTickArguments parameter.")
	virtual void ScheduleNextTick(const FContextParameters& Context) final {}

	/** Callback when the overrides are set to the execution context . */
	virtual void OnLinkedMetaStoryOverridesSet(const FContextParameters& Context, const FMetaStoryReferenceOverrides& Overrides)
	{
		
	}

	/** Callback before the execution context applies a transition. */
	virtual void OnBeginApplyTransition(const FContextParameters& Context, const FMetaStoryTransitionResult& TransitionResult)
	{
		
	}
};

#undef UE_API
