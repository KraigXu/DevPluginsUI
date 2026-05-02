// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryModule.h"
#include "MetaStoryModuleImpl.h"

#include "MetaStoryTypes.h"
#include "CrashReporter/MetaStoryCrashReporterHandler.h"

#if WITH_METASTORY_TRACE
#include "Debugger/MetaStoryTrace.h"
#include "Debugger/MetaStoryTraceTypes.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "MetaStoryDelegates.h"
#include "MetaStorySettings.h"
#endif // WITH_METASTORY_TRACE

#if WITH_METASTORY_TRACE_DEBUGGER
#include "Debugger/MetaStoryDebuggerTypes.h"
#include "Debugger/MetaStoryTraceModule.h"
#include "Features/IModularFeatures.h"
#include "Trace/StoreClient.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ITraceServicesModule.h"
#endif // WITH_METASTORY_TRACE_DEBUGGER

#if WITH_EDITORONLY_DATA
#include "MetaStoryInstanceData.h"
#endif // WITH_EDITORONLY_DATA

#if WITH_METASTORY_DEBUG
#include "UObject/UObjectGlobals.h"
#endif

#define LOCTEXT_NAMESPACE "MetaStory"

#if WITH_METASTORY_DEBUG
FTSSimpleMulticastDelegate FMetaStoryModule::OnPreRuntimeValidationInstanceData;
FTSSimpleMulticastDelegate FMetaStoryModule::OnPostRuntimeValidationInstanceData;
#endif

#if WITH_METASTORY_DEBUG
namespace UE::MetaStory::Debug::Private
{
	bool bRuntimeValidationInstanceDataGC = false;
	static FAutoConsoleVariableRef CVarRuntimeValidationInstanceDataGC(
		TEXT("MetaStory.RuntimeValidation.InstanceDataGC"),
		bRuntimeValidationInstanceDataGC,
		TEXT("Test after each GC if nodes were properly GCed.")
	);
} // namespace UE::MetaStory::Debug::Private
#endif //WITH_METASTORY_DEBUG

#if WITH_METASTORY_TRACE_DEBUGGER
UE::Trace::FStoreClient* FMetaStoryModule::GetStoreClient()
{
	if (!StoreClient.IsValid())
	{
		StoreClient = TUniquePtr<UE::Trace::FStoreClient>(UE::Trace::FStoreClient::Connect(TEXT("localhost")));
	}
	return StoreClient.Get();
}
#endif // WITH_METASTORY_TRACE_DEBUGGER

FMetaStoryModule::FMetaStoryModule()
#if WITH_METASTORY_TRACE
	: StartDebuggerTracesCommand(FAutoConsoleCommand(
		TEXT("statetree.startdebuggertraces"),
		TEXT("Turns on MetaStory debugger traces if not already active."),
		FConsoleCommandDelegate::CreateLambda([]
			{
				int32 TraceId = 0;
				IMetaStoryModule::Get().StartTraces(TraceId);
			})))
	, StopDebuggerTracesCommand(FAutoConsoleCommand(
		TEXT("statetree.stopdebuggertraces"),
		TEXT("Turns off MetaStory debugger traces if active."),
		FConsoleCommandDelegate::CreateLambda([]
			{
				IMetaStoryModule::Get().StopTraces();
			})))
#endif // WITH_METASTORY_TRACE
{
}

void FMetaStoryModule::StartupModule()
{
#if UE_WITH_STATETREE_CRASHREPORTER
	UE::MetaStory::FCrashReporterHandler::Register();
#endif

#if WITH_METASTORY_TRACE_DEBUGGER
	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	TraceAnalysisService = TraceServicesModule.GetAnalysisService();
	TraceModuleService = TraceServicesModule.GetModuleService();

	IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, &MetaStoryTraceModule);
#endif // WITH_METASTORY_TRACE_DEBUGGER

#if WITH_METASTORY_TRACE
	UE::MetaStoryTrace::RegisterGlobalDelegates();

#if !WITH_EDITOR
	// We don't automatically start traces for Editor targets since we rely on the debugger
	// to start recording either on user action or on PIE session start.
	if (UMetaStorySettings::Get().bAutoStartDebuggerTracesOnNonEditorTargets)
	{
		int32 TraceId = INDEX_NONE;
		StartTraces(TraceId);
	}
#endif // !WITH_EDITOR

#endif // WITH_METASTORY_TRACE

#if WITH_EDITORONLY_DATA
	UE::MetaStory::RegisterInstanceDataForLocalization();
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	UE::PropertyBinding::PropertyBindingIndex16ConversionFuncList.Add([](const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, TNotNull<FPropertyBindingIndex16*> Index)
		{
			static FName FMetaStoryIndex16TypeName = FMetaStoryIndex16::StaticStruct()->GetFName();
			const FName StructFName = Tag.GetType().GetParameter(0).GetName();
			if (FMetaStoryIndex16TypeName == StructFName)
			{
				FMetaStoryIndex16 MetaStoryIndex16;
				FMetaStoryIndex16::StaticStruct()->SerializeItem(Slot, &MetaStoryIndex16, /*Defaults*/nullptr);
				*Index = MetaStoryIndex16;
				return true;
			}
			return false;
		});
#endif //WITH_EDITOR

#if WITH_METASTORY_DEBUG
	PreGCHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FMetaStoryModule::HandlePreGC);
	PostGCHandle = FCoreUObjectDelegates::GetPostPurgeGarbageDelegate().AddRaw(this, &FMetaStoryModule::HandlePostGC);
#endif
}

void FMetaStoryModule::ShutdownModule()
{
#if WITH_METASTORY_DEBUG
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(PreGCHandle);
	FCoreUObjectDelegates::GetPostPurgeGarbageDelegate().Remove(PostGCHandle);
#endif

#if WITH_METASTORY_TRACE
	StopTraces();

	UE::MetaStoryTrace::UnregisterGlobalDelegates();
#endif // WITH_METASTORY_TRACE

#if WITH_METASTORY_TRACE_DEBUGGER
	if (StoreClient.IsValid())
	{
		StoreClient.Reset();
	}

	IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, &MetaStoryTraceModule);
#endif // WITH_METASTORY_TRACE_DEBUGGER

#if UE_WITH_STATETREE_CRASHREPORTER
	UE::MetaStory::FCrashReporterHandler::Unregister();
#endif

}

bool FMetaStoryModule::StartTraces(int32& OutTraceId)
{
	OutTraceId = INDEX_NONE;

#if WITH_METASTORY_TRACE
	if (IsRunningCommandlet() || IsTracing())
	{
		return false;
	}

	FGuid SessionGuid, TraceGuid;
	const bool bAlreadyConnected = FTraceAuxiliary::IsConnected(SessionGuid, TraceGuid);

#if WITH_METASTORY_TRACE_DEBUGGER
	if (const UE::Trace::FStoreClient* Client = GetStoreClient())
	{
		const UE::Trace::FStoreClient::FSessionInfo* SessionInfo = Client->GetSessionInfoByGuid(TraceGuid);
		// Note that 0 is returned instead of INDEX_NONE to match default invalid value for GetTraceId
		OutTraceId = SessionInfo != nullptr ? SessionInfo->GetTraceId() : 0;
	}
#endif // WITH_METASTORY_TRACE_DEBUGGER

	// If trace is already connected let's keep track of enabled channels to restore them when we stop recording
	if (bAlreadyConnected)
	{
		UE::Trace::EnumerateChannels([](const ANSICHAR* Name, const bool bIsEnabled, void* Channels)
		{
			TArray<FString>* EnabledChannels = static_cast<TArray<FString>*>(Channels);
			if (bIsEnabled)
			{
				EnabledChannels->Emplace(ANSI_TO_TCHAR(Name));
			}
		}, &ChannelsToRestore);
	}
	else
	{
		// Disable all channels and then enable only those we need to minimize trace file size.
		UE::Trace::EnumerateChannels([](const ANSICHAR* ChannelName, const bool bEnabled, void*)
			{
				if (bEnabled)
				{
					FString ChannelNameFString(ChannelName);
					UE::Trace::ToggleChannel(ChannelNameFString.GetCharArray().GetData(), false);
				}
			}
		, nullptr);
	}

	UE::Trace::ToggleChannel(TEXT("MetaStoryDebugChannel"), true);
	UE::Trace::ToggleChannel(TEXT("FrameChannel"), true);

	bool bAreTracesStarted = false;
	if (bAlreadyConnected == false)
	{
		FTraceAuxiliary::FOptions Options;
		Options.bExcludeTail = true;
		bAreTracesStarted = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::Network, TEXT("localhost"), TEXT(""), &Options, LogMetaStory);
	}

	if (UE::MetaStory::Delegates::OnTracingStateChanged.IsBound())
	{
		UE_LOG(LogMetaStory, Log, TEXT("MetaStory traces enabled"));
		UE::MetaStory::Delegates::OnTracingStateChanged.Broadcast(EMetaStoryTraceStatus::TracesStarted);
	}

	return bAreTracesStarted;
#else
	return false;
#endif // WITH_METASTORY_TRACE
}

bool FMetaStoryModule::IsTracing() const
{
#if UE_TRACE_ENABLED
	// We are not relying on a dedicated flag since tracing can be started from many sources (e.g., RewindDebugger)
	if (!UE::Trace::IsTracing())
	{
		return false;
	}

	const UE::Trace::FChannel* Channel = UE::Trace::FindChannel(TEXT("MetaStoryDebugChannel"));
	return Channel != nullptr && Channel->IsEnabled();
#else
	return false;
#endif // UE_TRACE_ENABLED
}

void FMetaStoryModule::StopTraces()
{
#if WITH_METASTORY_TRACE
	if (IsTracing() == false)
	{
		return;
	}

	if (UE::MetaStory::Delegates::OnTracingStateChanged.IsBound())
	{
		UE_LOG(LogMetaStory, Log, TEXT("Stopping MetaStory traces..."));
		UE::MetaStory::Delegates::OnTracingStateChanged.Broadcast(EMetaStoryTraceStatus::StoppingTrace);
	}

	UE::Trace::ToggleChannel(TEXT("MetaStoryDebugChannel"), false);
	UE::Trace::ToggleChannel(TEXT("FrameChannel"), false);

	// When we have channels to restore it also indicates that the trace were active
	// so we only toggle the channels back (i.e. not calling FTraceAuxiliary::Stop)
	if (ChannelsToRestore.Num() > 0)
	{
		for (const FString& ChannelName : ChannelsToRestore)
		{
			UE::Trace::ToggleChannel(ChannelName.GetCharArray().GetData(), true);
		}
		ChannelsToRestore.Reset();
	}
	else
	{
		FTraceAuxiliary::Stop();
	}

	if (UE::MetaStory::Delegates::OnTracingStateChanged.IsBound())
	{
		UE_LOG(LogMetaStory, Log, TEXT("MetaStory traces stopped"));
		UE::MetaStory::Delegates::OnTracingStateChanged.Broadcast(EMetaStoryTraceStatus::TracesStopped);
	}
#endif // WITH_METASTORY_TRACE
}

#if WITH_METASTORY_DEBUG
void FMetaStoryModule::HandlePreGC()
{
	if (UE::MetaStory::Debug::Private::bRuntimeValidationInstanceDataGC)
	{
		OnPreRuntimeValidationInstanceData.Broadcast();
	}
}

void FMetaStoryModule::HandlePostGC()
{
	if (UE::MetaStory::Debug::Private::bRuntimeValidationInstanceDataGC)
	{
		OnPostRuntimeValidationInstanceData.Broadcast();
	}
}
#endif

IMPLEMENT_MODULE(FMetaStoryModule, MetaStoryModule)

#undef LOCTEXT_NAMESPACE
