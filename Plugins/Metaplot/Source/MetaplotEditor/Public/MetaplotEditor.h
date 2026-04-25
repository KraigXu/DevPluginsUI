#pragma once

#include "Modules/ModuleManager.h"

class FAssetTypeActions_MetaplotScenarioAsset;

class FMetaplotEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterDetailsCustomizations();
	void UnregisterDetailsCustomizations();

	TSharedPtr<FAssetTypeActions_MetaplotScenarioAsset> ScenarioAssetTypeActions;
};
