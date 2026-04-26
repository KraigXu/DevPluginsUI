#include "MetaplotEditor.h"
#include "MetaplotEditorStyle.h"

#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Scenario/AssetTypeActions_MetaplotScenarioAsset.h"
#include "Scenario/MetaplotDetailsCustomization.h"

IMPLEMENT_MODULE(FMetaplotEditorModule, MetaplotEditor)

void FMetaplotEditorModule::StartupModule()
{
	FMetaplotEditorStyle::Register();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const EAssetTypeCategories::Type MetaplotCategory = AssetTools.RegisterAdvancedAssetCategory(
		FName(TEXT("Metaplot")),
		FText::FromString(TEXT("Metaplot")));

	ScenarioAssetTypeActions = MakeShared<FAssetTypeActions_MetaplotScenarioAsset>(MetaplotCategory);
	AssetTools.RegisterAssetTypeActions(ScenarioAssetTypeActions.ToSharedRef());
	RegisterDetailsCustomizations();
}

void FMetaplotEditorModule::ShutdownModule()
{
	UnregisterDetailsCustomizations();

	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")) && ScenarioAssetTypeActions.IsValid())
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(ScenarioAssetTypeActions.ToSharedRef());
	}

	FMetaplotEditorStyle::Unregister();
}

void FMetaplotEditorModule::RegisterDetailsCustomizations()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.RegisterCustomClassLayout(
		TEXT("MetaplotNodeDetailsProxy"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FMetaplotNodeDetailsProxyCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(
		TEXT("MetaplotTransitionDetailsProxy"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FMetaplotTransitionDetailsProxyCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(
		TEXT("MetaplotCondition"),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaplotConditionCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FMetaplotEditorModule::UnregisterDetailsCustomizations()
{
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		return;
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyModule.UnregisterCustomClassLayout(TEXT("MetaplotNodeDetailsProxy"));
	PropertyModule.UnregisterCustomClassLayout(TEXT("MetaplotTransitionDetailsProxy"));
	PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("MetaplotCondition"));
	PropertyModule.NotifyCustomizationModuleChanged();
}
