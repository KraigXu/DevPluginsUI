// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "IMetaStoryEditorHost.generated.h"

namespace UE::MetaStoryEditor
{
	class FWorkspaceTabHost;
}

class UMetaStory;
class IMessageLogListing;
class IDetailsView;

// Interface required for re-using MetaStoryEditor mode across different AssetEditors
class IMetaStoryEditorHost : public TSharedFromThis<IMetaStoryEditorHost>
{
public:
	IMetaStoryEditorHost() = default;
	virtual ~IMetaStoryEditorHost() = default;

	virtual FName GetCompilerLogName() const = 0;
	virtual FName GetCompilerTabName() const = 0;
	virtual bool ShouldShowCompileButton() const = 0;
	virtual bool CanToolkitSpawnWorkspaceTab() const = 0;

	virtual UMetaStory* GetMetaStory() const = 0;
	virtual FSimpleMulticastDelegate& OnMetaStoryChanged() = 0;

	virtual TSharedPtr<IDetailsView> GetAssetDetailsView() = 0;
	virtual TSharedPtr<IDetailsView> GetDetailsView() = 0;
	virtual TSharedPtr<UE::MetaStoryEditor::FWorkspaceTabHost> GetTabHost() const = 0;
};

UCLASS(MinimalAPI)
class UMetaStoryEditorContext : public UObject
{
	GENERATED_BODY()
public:
	TSharedPtr<IMetaStoryEditorHost> EditorHostInterface;
};

