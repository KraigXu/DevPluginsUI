// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryFactory.h"
#include "MetaStoryMetaplotGraphFactory.generated.h"

#define UE_API METASTORYEDITORMODULE_API

/**
 * Creates a UMetaStory whose topology is driven by an embedded UMetaStoryFlow.
 */
UCLASS(DisplayName = "MetaStory (Flow Graph)")
class UMetaStoryMetaplotGraphFactory : public UMetaStoryFactory
{
	GENERATED_BODY()

public:
	UE_API UMetaStoryMetaplotGraphFactory(const FObjectInitializer& ObjectInitializer);

	UE_API virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};

#undef UE_API
