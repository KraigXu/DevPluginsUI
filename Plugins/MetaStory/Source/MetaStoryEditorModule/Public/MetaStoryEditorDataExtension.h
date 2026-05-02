// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "MetaStoryEditorDataExtension.generated.h"

namespace UE::MetaStory::Compiler
{
	struct FPostInternalContext;
}

class IDetailLayoutBuilder;
class UMetaStoryEditorData;
class UMetaStoryState;

/**
 * Extension for the editor data of the MetaStory asset.
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, Within=MetaStoryEditorData, MinimalAPI)
class UMetaStoryEditorDataExtension : public UObject
{
	GENERATED_BODY()

public:
	virtual bool HandlePostInternalCompile(const UE::MetaStory::Compiler::FPostInternalContext& Context)
	{
		return true;
	}

	virtual void CustomizeDetails(TNonNullPtr<UMetaStoryState> State, IDetailLayoutBuilder& DetailBuilder)
	{
	}

protected:
	UMetaStoryEditorData* GetMetaStoryEditorData() const
	{
		return GetTypedOuter<UMetaStoryEditorData>();
	}
};
