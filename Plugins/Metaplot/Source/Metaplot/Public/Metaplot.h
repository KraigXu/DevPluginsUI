#pragma once

#include "Modules/ModuleManager.h"

class FMetaplotModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
