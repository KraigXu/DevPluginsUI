#pragma once

#include "AssetTypeActions_Base.h"
#include "CoreMinimal.h"

class FAssetTypeActions_MetaplotScenarioAsset : public FAssetTypeActions_Base
{
public:
	explicit FAssetTypeActions_MetaplotScenarioAsset(EAssetTypeCategories::Type InAssetCategory);

	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor) override;

private:
	EAssetTypeCategories::Type AssetCategory;
};
