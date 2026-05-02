// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryExecutionExtension.h"
#include "MetaStoryExecutionTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryExecutionExtension)


FMetaStoryExecutionExtension::FNextTickArguments::FNextTickArguments()
	: Reason(UE::MetaStory::EMetaStoryTickReason::None)
{
}

FMetaStoryExecutionExtension::FNextTickArguments::FNextTickArguments(UE::MetaStory::EMetaStoryTickReason InReason)
	: Reason(InReason)
{ }

