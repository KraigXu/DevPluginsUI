// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorUILayer.h"
#include "MetaStoryEditor.h"
#include "MetaStoryEditorModule.h"
#include "Toolkits/IToolkit.h"
#include "ToolMenus.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEditorUILayer)

FMetaStoryEditorModeUILayer::FMetaStoryEditorModeUILayer(const IToolkitHost* InToolkitHost) : FAssetEditorModeUILayer(InToolkitHost)
{
}

void FMetaStoryEditorModeUILayer::OnToolkitHostingStarted(const TSharedRef<IToolkit>& Toolkit)
{
	if (!Toolkit->IsAssetEditor())
	{
		FAssetEditorModeUILayer::OnToolkitHostingStarted(Toolkit);
		HostedToolkit = Toolkit;
		Toolkit->SetModeUILayer(SharedThis(this));
		Toolkit->RegisterTabSpawners(ToolkitHost->GetTabManager().ToSharedRef());
		RegisterModeTabSpawners();	

		OnToolkitHostReadyForUI.Execute();

		// Set up an owner for the current scope so that we can cleanly clean up the toolbar extension on hosting finish
		UToolMenu* SecondaryModeToolbar = UToolMenus::Get()->ExtendMenu(GetSecondaryModeToolbarName());
		OnRegisterSecondaryModeToolbarExtension.ExecuteIfBound(SecondaryModeToolbar);
	}
}

void FMetaStoryEditorModeUILayer::OnToolkitHostingFinished(const TSharedRef<IToolkit>& Toolkit)
{	
	if (HostedToolkit.IsValid() && HostedToolkit.Pin() == Toolkit)
	{
		FAssetEditorModeUILayer::OnToolkitHostingFinished(Toolkit);
	}
}

void FMetaStoryEditorModeUILayer::SetModeMenuCategory(const TSharedPtr<FWorkspaceItem>& MenuCategoryIn)
{
	MenuCategory = MenuCategoryIn;
}

TSharedPtr<FWorkspaceItem> FMetaStoryEditorModeUILayer::GetModeMenuCategory() const
{
	check(MenuCategory);
	return MenuCategory;
}

void UMetaStoryEditorUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	FMetaStoryEditorModule& MetaStoryEditorModule = FModuleManager::LoadModuleChecked<FMetaStoryEditorModule>("MetaStoryEditorModule");
	MetaStoryEditorModule.OnRegisterLayoutExtensions().AddUObject(this, &UMetaStoryEditorUISubsystem::RegisterLayoutExtensions);
}

void UMetaStoryEditorUISubsystem::Deinitialize()
{
	FMetaStoryEditorModule& MetaStoryEditorModule = FModuleManager::LoadModuleChecked<FMetaStoryEditorModule>("MetaStoryEditorModule");
	MetaStoryEditorModule.OnRegisterLayoutExtensions().RemoveAll(this);
}

void UMetaStoryEditorUISubsystem::RegisterLayoutExtensions(FLayoutExtender& Extender)
{	
	Extender.ExtendStack(FMetaStoryEditor::LayoutLeftStackId, ELayoutExtensionPosition::After, FTabManager::FTab(UAssetEditorUISubsystem::TopLeftTabID, ETabState::ClosedTab));
	Extender.ExtendStack(FMetaStoryEditor::LayoutLeftStackId, ELayoutExtensionPosition::After, FTabManager::FTab(UAssetEditorUISubsystem::BottomRightTabID, ETabState::ClosedTab));

#if WITH_METASTORY_TRACE_DEBUGGER
	Extender.ExtendStack(FMetaStoryEditor::LayoutBottomMiddleStackId, ELayoutExtensionPosition::After, FTabManager::FTab(UAssetEditorUISubsystem::TopRightTabID, ETabState::ClosedTab));
#endif // WITH_METASTORY_TRACE_DEBUGGER
}
