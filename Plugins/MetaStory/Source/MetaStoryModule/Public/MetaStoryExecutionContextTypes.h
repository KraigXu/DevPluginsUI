// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryExecutionTypes.h"
#include "MetaStoryStatePath.h"
#include "Misc/NotNull.h"

#define UE_API METASTORYMODULE_API

class UMetaStory;

namespace UE::MetaStory::ExecutionContext
{
	/** Helper that combines the state handle and its tree asset. */
	struct FStateHandleContext
	{
		FStateHandleContext() = default;
		FStateHandleContext(TNotNull<const UMetaStory*> InMetaStory, FMetaStoryStateHandle InStateHandle)
			: MetaStory(InMetaStory)
			, StateHandle(InStateHandle)
		{
		}
		
		bool operator==(const FStateHandleContext&) const = default;
		
		const UMetaStory* MetaStory = nullptr;
		FMetaStoryStateHandle StateHandle;
	};

	/** Interface of structure that can store temporary frames and states. */
	struct ITemporaryStorage
	{
		virtual ~ITemporaryStorage() = default;

		struct FFrameAndParent
		{
			FMetaStoryExecutionFrame* Frame = nullptr;
			UE::MetaStory::FActiveFrameID ParentFrameID;
		};

		virtual FFrameAndParent GetExecutionFrame(UE::MetaStory::FActiveFrameID ID) = 0;
		virtual UE::MetaStory::FActiveState GetStateHandle(UE::MetaStory::FActiveStateID ID) const = 0;
	};

} // UE::MetaStory::ExecutionContext

#undef UE_API
