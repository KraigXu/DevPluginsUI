// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMetaStoryEditorHost.h"

class FMetaStoryEditor;

class FStandaloneStateTreeEditorHost : public IMetaStoryEditorHost
{	
public:
	void Init(const TWeakPtr<FMetaStoryEditor>& InWeakStateTreeEditor);
	
	//~ IMetaStoryEditorHost overrides
	virtual UMetaStory* GetStateTree() const override;
	virtual FSimpleMulticastDelegate& OnStateTreeChanged() override;
	virtual TSharedPtr<IDetailsView> GetAssetDetailsView() override;
	virtual TSharedPtr<IDetailsView> GetDetailsView() override;
	virtual TSharedPtr<UE::MetaStoryEditor::FWorkspaceTabHost> GetTabHost() const override;
	virtual bool ShouldShowCompileButton() const override;
	virtual bool CanToolkitSpawnWorkspaceTab() const override;
	virtual FName GetCompilerLogName() const override;
	virtual FName GetCompilerTabName() const override;

protected:
	TWeakPtr<FMetaStoryEditor> WeakStateTreeEditor;
	FSimpleMulticastDelegate OnStateTreeChangedDelegate;
	TSharedPtr<UE::MetaStoryEditor::FWorkspaceTabHost> TabHost;
};