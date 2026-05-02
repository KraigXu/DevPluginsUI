// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "MetaStorySettings.generated.h"

/**
 * Default MetaStory settings
 */
UCLASS(MinimalAPI, config = MetaStory, defaultconfig, DisplayName = "MetaStory")
class UMetaStorySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static UMetaStorySettings& Get() { return *CastChecked<UMetaStorySettings>(StaticClass()->GetDefaultObject()); }

	/**
	 * Editor targets relies on PIE and MetaStoryEditor to start/stop traces.
	 * This is to start traces automatically when launching Standalone, Client or Server builds. 
	 * It's also possible to do it manually using 'metastory.startdebuggertraces' and 'metastory.stopdebuggertraces' in the console.
	 */
	UPROPERTY(EditDefaultsOnly, Category = MetaStory, config)
	bool bAutoStartDebuggerTracesOnNonEditorTargets = false;
};
