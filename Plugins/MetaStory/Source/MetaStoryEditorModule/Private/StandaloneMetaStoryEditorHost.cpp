// Copyright Epic Games, Inc. All Rights Reserved.

#include "StandaloneMetaStoryEditorHost.h"
#include "MetaStoryEditor.h"
#include "MetaStoryEditorWorkspaceTabHost.h"

void FStandaloneStateTreeEditorHost::Init(const TWeakPtr<FMetaStoryEditor>& InWeakStateTreeEditor)
{
	WeakStateTreeEditor = InWeakStateTreeEditor;
	TabHost = MakeShared<UE::MetaStoryEditor::FWorkspaceTabHost>();
}

UMetaStory* FStandaloneStateTreeEditorHost::GetStateTree() const
{
	if (TSharedPtr<FMetaStoryEditor> SharedEditor = WeakStateTreeEditor.Pin())
	{
		return SharedEditor->MetaStory;
	}
	return nullptr;
}

FName FStandaloneStateTreeEditorHost::GetCompilerLogName() const
{
	return FMetaStoryEditor::CompilerLogListingName;
}

FName FStandaloneStateTreeEditorHost::GetCompilerTabName() const
{
	return FMetaStoryEditor::CompilerResultsTabId;
}

bool FStandaloneStateTreeEditorHost::ShouldShowCompileButton() const
{
	return true;
}

bool FStandaloneStateTreeEditorHost::CanToolkitSpawnWorkspaceTab() const
{
	return false;
}

FSimpleMulticastDelegate& FStandaloneStateTreeEditorHost::OnStateTreeChanged()
{
	return OnStateTreeChangedDelegate;
}

TSharedPtr<IDetailsView> FStandaloneStateTreeEditorHost::GetAssetDetailsView()
{
	if (TSharedPtr<FMetaStoryEditor> SharedEditor = WeakStateTreeEditor.Pin())
	{
		return SharedEditor->AssetDetailsView;
	}
	
	return nullptr;	
}

TSharedPtr<IDetailsView> FStandaloneStateTreeEditorHost::GetDetailsView()
{
	if (TSharedPtr<FMetaStoryEditor> SharedEditor = WeakStateTreeEditor.Pin())
	{
		return SharedEditor->SelectionDetailsView;
	}
	
	return nullptr;	
}

TSharedPtr<UE::MetaStoryEditor::FWorkspaceTabHost> FStandaloneStateTreeEditorHost::GetTabHost() const
{
	return TabHost;
}