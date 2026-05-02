// Copyright Epic Games, Inc. All Rights Reserved.

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"
#include "MetaStoryRewindDebuggerRecordingExtension.h"
#include "MetaStoryStyle.h"

class FMetaStoryDeveloperModule final : public IModuleInterface
{
	//~ Begin IModuleInterface
	virtual void StartupModule() override
	{
		FMetaStoryStyle::Register();

#if WITH_METASTORY_TRACE
		RewindDebuggerRecordingExtension = MakeUnique<UE::MetaStoryDebugger::FRewindDebuggerRecordingExtension>();
		IModularFeatures::Get().RegisterModularFeature(IRewindDebuggerRuntimeExtension::ModularFeatureName, RewindDebuggerRecordingExtension.Get());
#endif // WITH_METASTORY_TRACE
	}

	virtual void ShutdownModule() override
	{
		FMetaStoryStyle::Unregister();

#if WITH_METASTORY_TRACE
		IModularFeatures::Get().UnregisterModularFeature(IRewindDebuggerRuntimeExtension::ModularFeatureName, RewindDebuggerRecordingExtension.Get());
#endif // WITH_METASTORY_TRACE
	}
	//~ End IModuleInterface

#if WITH_METASTORY_TRACE
	TUniquePtr<UE::MetaStoryDebugger::FRewindDebuggerRecordingExtension> RewindDebuggerRecordingExtension;
#endif // WITH_METASTORY_TRACE
};

IMPLEMENT_MODULE(FMetaStoryDeveloperModule, MetaStoryDeveloper)
