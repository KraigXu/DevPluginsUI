// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "MetaStoryExtension.generated.h"

class UMetaStory;
struct FMetaStoryLinker;

/**
 * Extension for the state tree asset.
 */
UCLASS(Abstract, DefaultToInstanced, Within=MetaStory, MinimalAPI)
class UMetaStoryExtension : public UObject
{
	GENERATED_BODY()

public:
	/** Resolves references to other MetaStory data. */
	virtual bool Link(FMetaStoryLinker& Linker)
	{
		return true;
	}

protected:
	UMetaStory* GetStateTree() const
	{
		return GetTypedOuter<UMetaStory>();
	}
};
