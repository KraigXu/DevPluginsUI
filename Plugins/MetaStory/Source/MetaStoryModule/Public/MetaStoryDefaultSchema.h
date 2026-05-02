// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStorySchema.h"
#include "MetaStoryDefaultSchema.generated.h"

#define UE_API METASTORYMODULE_API

/**
 * Default shipped schema for MetaStory assets: allows built-in node structs (conditions, tasks, etc.),
 * Blueprint-based MetaStory nodes, and arbitrary UObject external context data.
 */
UCLASS(MinimalAPI, BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "MetaStory Default", CommonSchema))
class UMetaStoryDefaultSchema : public UMetaStorySchema
{
	GENERATED_BODY()

public:
	UE_API UMetaStoryDefaultSchema();

protected:
	UE_API virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	UE_API virtual bool IsClassAllowed(const UClass* InClass) const override;
	UE_API virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;
	UE_API virtual bool IsScheduledTickAllowed() const override;
};

#undef UE_API
