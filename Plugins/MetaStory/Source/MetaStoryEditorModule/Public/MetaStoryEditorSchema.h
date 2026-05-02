// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "MetaStoryEditorSchema.generated.h"

#define UE_API METASTORYEDITORMODULE_API

class UMetaStory;
namespace UE::MetaStory::Compiler
{
	struct FPostInternalContext;
}

/**
 * Schema describing how a MetaStory is edited.
 */
UCLASS(MinimalAPI)
class UMetaStoryEditorSchema : public UObject
{
	GENERATED_BODY()

public:
	/** @returns True if modifying extensions in the editor is allowed. An extension can be added by code. */
	virtual bool AllowExtensions() const
	{
		return true;
	}

public:
	/*
	 * Validates and applies the schema restrictions on the MetaStory.
	 * Also serves as the "safety net" of fixing up editor data following an editor operation.
	 */
	UE_API virtual void Validate(TNotNull<UMetaStory*> MetaStory);


	/**
	 * Handle compilation for the owning MetaStory asset.
	 * The MetaStory asset compiled successfully.
	 */
	virtual bool HandlePostInternalCompile(const UE::MetaStory::Compiler::FPostInternalContext& Context)
	{
		return true;
	}
};

#undef UE_API
