#include "Scenario/AssetTypeActions_MetaplotScenarioAsset.h"

#include "Flow/MetaplotFlow.h"
#include "Scenario/MetaplotScenarioAssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_MetaplotScenarioAsset"

FAssetTypeActions_MetaplotScenarioAsset::FAssetTypeActions_MetaplotScenarioAsset(EAssetTypeCategories::Type InAssetCategory)
	: AssetCategory(InAssetCategory)
{
}

FText FAssetTypeActions_MetaplotScenarioAsset::GetName() const
{
	return LOCTEXT("MetaplotFlowAssetName", "Metaplot Flow");
}

FColor FAssetTypeActions_MetaplotScenarioAsset::GetTypeColor() const
{
	return FColor(95, 175, 255);
}

UClass* FAssetTypeActions_MetaplotScenarioAsset::GetSupportedClass() const
{
	return UMetaplotFlow::StaticClass();
}

uint32 FAssetTypeActions_MetaplotScenarioAsset::GetCategories()
{
	return AssetCategory;
}

void FAssetTypeActions_MetaplotScenarioAsset::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if (UMetaplotFlow* FlowAsset = Cast<UMetaplotFlow>(Object))
		{
			TSharedRef<FMetaplotScenarioAssetEditorToolkit> EditorToolkit = MakeShared<FMetaplotScenarioAssetEditorToolkit>();
			EditorToolkit->InitMetaplotScenarioAssetEditor(Mode, EditWithinLevelEditor, FlowAsset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
