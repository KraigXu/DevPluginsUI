// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryInstanceContainer.h"
#include "MetaStoryTypes.h"
#include "UObject/ObjectKey.h"
#include "MetaStoryExecutionRuntimeDataTypes.generated.h"

#define UE_API METASTORYMODULE_API

class UMetaStory;

namespace UE::MetaStory::InstanceData
{
	/** Helper structure that holds the execution runtime instances. */
	USTRUCT()
	struct FMetaStoryExecutionRuntimeData
	{
		GENERATED_BODY()

		/** The state tree of the instances. */
		TObjectKey<UMetaStory> MetaStory;

		UPROPERTY()
		UE::MetaStory::InstanceData::FMetaStoryInstanceContainer Instances;
	};
}

#undef UE_API
