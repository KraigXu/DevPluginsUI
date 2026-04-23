#include "MetaplotEditor.h"

#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "Scenario/AssetTypeActions_MetaplotScenarioAsset.h"

IMPLEMENT_MODULE(FMetaplotEditorModule, MetaplotEditor)

void FMetaplotEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const EAssetTypeCategories::Type MetaplotCategory = AssetTools.RegisterAdvancedAssetCategory(
		FName(TEXT("Metaplot")),
		FText::FromString(TEXT("Metaplot")));

	ScenarioAssetTypeActions = MakeShared<FAssetTypeActions_MetaplotScenarioAsset>(MetaplotCategory);
	AssetTools.RegisterAssetTypeActions(ScenarioAssetTypeActions.ToSharedRef());
}

void FMetaplotEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")) && ScenarioAssetTypeActions.IsValid())
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(ScenarioAssetTypeActions.ToSharedRef());
	}
}
