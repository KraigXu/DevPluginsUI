// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryModule.h"
#include "Debugger/MetaStoryTraceModule.h"
#include "HAL/IConsoleManager.h"
#include "MetaStoryTypes.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace TraceServices { class IAnalysisService; }
namespace TraceServices { class IModuleService; }
namespace UE::Trace { class FStoreClient; }
class UUserDefinedStruct;

class FMetaStoryModule : public IMetaStoryModule
{
public:
	FMetaStoryModule();

	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	virtual bool StartTraces(int32& OutTraceId) override;
	virtual bool IsTracing() const override;
	virtual void StopTraces() override;

#if WITH_METASTORY_DEBUG
	static FTSSimpleMulticastDelegate OnPreRuntimeValidationInstanceData;
	static FTSSimpleMulticastDelegate OnPostRuntimeValidationInstanceData;
#endif

private:

#if WITH_METASTORY_DEBUG
	void HandlePreGC();
	void HandlePostGC();
#endif

#if WITH_METASTORY_TRACE_DEBUGGER
	/**
	 * Gets the store client.
	 */
	virtual UE::Trace::FStoreClient* GetStoreClient() override;

	TSharedPtr<TraceServices::IAnalysisService> TraceAnalysisService;
	TSharedPtr<TraceServices::IModuleService> TraceModuleService;

	/** The client used to connect to the trace store. */
	TUniquePtr<UE::Trace::FStoreClient> StoreClient;

	FMetaStoryTraceModule MetaStoryTraceModule;
#endif // WITH_METASTORY_TRACE_DEBUGGER

#if WITH_METASTORY_TRACE
	TArray<const FString> ChannelsToRestore;

	FAutoConsoleCommand StartDebuggerTracesCommand;
	FAutoConsoleCommand StopDebuggerTracesCommand;
#endif // WITH_METASTORY_TRACE

#if WITH_METASTORY_DEBUG
	FDelegateHandle PreGCHandle;
	FDelegateHandle PostGCHandle;
#endif
};
