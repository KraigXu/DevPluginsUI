// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "MetaStoryCompileAllCommandlet.generated.h"

class ISourceControlProvider;

class UMetaStory;

/**
 * Commandlet to recompile all MetaStory assets in the project
 */
UCLASS()
class UMetaStoryCompileAllCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UMetaStoryCompileAllCommandlet(const FObjectInitializer& ObjectInitializer);

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

protected:
	bool CompileAndSaveMetaStory(TNonNullPtr<UMetaStory> MetaStory) const;

	ISourceControlProvider* SourceControlProvider = nullptr;
};


