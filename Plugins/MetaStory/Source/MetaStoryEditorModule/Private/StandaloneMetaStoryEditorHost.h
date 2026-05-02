// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMetaStoryEditorHost.h"

class FMetaStoryEditor;

class FStandaloneMetaStoryEditorHost : public IMetaStoryEditorHost
{	
public:
	void Init(const TWeakPtr<FMetaStoryEditor>& InWeakMetaStoryEditor);
	
	//~ IMetaStoryEditorHost overrides
	virtual UMetaStory* GetMetaStory() const override;
	virtual FSimpleMulticastDelegate& OnMetaStoryChanged() override;
	virtual TSharedPtr<IDetailsView> GetAssetDetailsView() override;
	virtual TSharedPtr<IDetailsView> GetDetailsView() override;
	virtual TSharedPtr<UE::MetaStoryEditor::FWorkspaceTabHost> GetTabHost() const override;
	virtual bool ShouldShowCompileButton() const override;
	virtual bool CanToolkitSpawnWorkspaceTab() const override;
	virtual FName GetCompilerLogName() const override;
	virtual FName GetCompilerTabName() const override;

protected:
	TWeakPtr<FMetaStoryEditor> WeakMetaStoryEditor;
	FSimpleMulticastDelegate OnMetaStoryChangedDelegate;
	TSharedPtr<UE::MetaStoryEditor::FWorkspaceTabHost> TabHost;
};