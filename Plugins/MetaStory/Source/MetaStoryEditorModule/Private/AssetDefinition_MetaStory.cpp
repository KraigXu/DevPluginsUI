// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_MetaStory.h"
#include "Modules/ModuleManager.h"
#include "SMetaStoryDiff.h"
#include "MetaStory.h"
#include "MetaStoryEditorModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssetDefinition_MetaStory)

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText UAssetDefinition_MetaStory::GetAssetDisplayName() const
{
	return LOCTEXT("FAssetTypeActions_MetaStory", "MetaStory");
}

FLinearColor UAssetDefinition_MetaStory::GetAssetColor() const
{
	return FColor(201, 185, 29);
}

TSoftClassPtr<> UAssetDefinition_MetaStory::GetAssetClass() const
{
	return UMetaStory::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaStory::GetAssetCategories() const
{
	static FAssetCategoryPath Categories[] =
		{
		EAssetCategoryPaths::AI,
		EAssetCategoryPaths::Gameplay
		};
	return Categories;
}

EAssetCommandResult UAssetDefinition_MetaStory::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	FMetaStoryEditorModule& EditorModule = FModuleManager::LoadModuleChecked<FMetaStoryEditorModule>("MetaStoryEditorModule");
	for (UMetaStory* MetaStory : OpenArgs.LoadObjects<UMetaStory>())
	{
		EditorModule.CreateMetaStoryEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, MetaStory);
	}
	return EAssetCommandResult::Handled;
}

EAssetCommandResult UAssetDefinition_MetaStory::PerformAssetDiff(const FAssetDiffArgs& DiffArgs) const
{
	const UMetaStory* OldMetaStory = Cast<UMetaStory>(DiffArgs.OldAsset);
	const UMetaStory* NewMetaStory = Cast<UMetaStory>(DiffArgs.NewAsset);

	if (OldMetaStory == nullptr || NewMetaStory == nullptr)
	{
		return EAssetCommandResult::Unhandled;
	}

	UE::MetaStory::Diff::SDiffWidget::CreateDiffWindow(OldMetaStory, NewMetaStory, DiffArgs.OldRevision, DiffArgs.NewRevision, UMetaStory::StaticClass());
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
