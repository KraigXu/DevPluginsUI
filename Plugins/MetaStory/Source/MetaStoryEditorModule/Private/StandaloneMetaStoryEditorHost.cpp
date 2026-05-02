// Copyright Epic Games, Inc. All Rights Reserved.

#include "StandaloneMetaStoryEditorHost.h"
#include "MetaStoryEditor.h"
#include "MetaStoryEditorWorkspaceTabHost.h"

void FStandaloneMetaStoryEditorHost::Init(const TWeakPtr<FMetaStoryEditor>& InWeakMetaStoryEditor)
{
	WeakMetaStoryEditor = InWeakMetaStoryEditor;
	TabHost = MakeShared<UE::MetaStoryEditor::FWorkspaceTabHost>();
}

UMetaStory* FStandaloneMetaStoryEditorHost::GetMetaStory() const
{
	if (TSharedPtr<FMetaStoryEditor> SharedEditor = WeakMetaStoryEditor.Pin())
	{
		return SharedEditor->MetaStory;
	}
	return nullptr;
}

FName FStandaloneMetaStoryEditorHost::GetCompilerLogName() const
{
	return FMetaStoryEditor::CompilerLogListingName;
}

FName FStandaloneMetaStoryEditorHost::GetCompilerTabName() const
{
	return FMetaStoryEditor::CompilerResultsTabId;
}

bool FStandaloneMetaStoryEditorHost::ShouldShowCompileButton() const
{
	return true;
}

bool FStandaloneMetaStoryEditorHost::CanToolkitSpawnWorkspaceTab() const
{
	return false;
}

FSimpleMulticastDelegate& FStandaloneMetaStoryEditorHost::OnMetaStoryChanged()
{
	return OnMetaStoryChangedDelegate;
}

TSharedPtr<IDetailsView> FStandaloneMetaStoryEditorHost::GetAssetDetailsView()
{
	if (TSharedPtr<FMetaStoryEditor> SharedEditor = WeakMetaStoryEditor.Pin())
	{
		return SharedEditor->AssetDetailsView;
	}
	
	return nullptr;	
}

TSharedPtr<IDetailsView> FStandaloneMetaStoryEditorHost::GetDetailsView()
{
	if (TSharedPtr<FMetaStoryEditor> SharedEditor = WeakMetaStoryEditor.Pin())
	{
		return SharedEditor->SelectionDetailsView;
	}
	
	return nullptr;	
}

TSharedPtr<UE::MetaStoryEditor::FWorkspaceTabHost> FStandaloneMetaStoryEditorHost::GetTabHost() const
{
	return TabHost;
}