// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_METASTORY_TRACE_DEBUGGER

#include "Debugger/MetaStoryDebugger.h"
#include "Debugger/IMetaStoryTraceProvider.h"
#include "Debugger/MetaStoryTraceProvider.h"
#include "Debugger/MetaStoryTraceTypes.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryModule.h"
#include "Trace/Analysis.h"
#include "Trace/Analyzer.h"
#include "Trace/StoreClient.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Diagnostics.h"
#include "TraceServices/Model/Frames.h"

#define LOCTEXT_NAMESPACE "MetaStoryDebugger"

//----------------------------------------------------------------//
// UE::MetaStoryDebugger
//----------------------------------------------------------------//
namespace UE::MetaStoryDebugger
{
	struct FDiagnosticsSessionAnalyzer : UE::Trace::IAnalyzer
	{
		virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override
		{
			auto& Builder = Context.InterfaceBuilder;
			Builder.RouteEvent(RouteId_Session2, "Diagnostics", "Session2");
		}

		virtual bool OnEvent(const uint16 RouteId, EStyle, const FOnEventContext& Context) override
		{
			const FEventData& EventData = Context.EventData;

			switch (RouteId)
			{
			case RouteId_Session2:
				{
					EventData.GetString("Platform", SessionInfo.Platform);
					EventData.GetString("AppName", SessionInfo.AppName);
					EventData.GetString("CommandLine", SessionInfo.CommandLine);
					EventData.GetString("Branch", SessionInfo.Branch);
					EventData.GetString("BuildVersion", SessionInfo.BuildVersion);
					SessionInfo.Changelist = EventData.GetValue<uint32>("Changelist", 0);
					SessionInfo.ConfigurationType = static_cast<EBuildConfiguration>(EventData.GetValue<uint8>("ConfigurationType"));
					SessionInfo.TargetType = static_cast<EBuildTargetType>(EventData.GetValue<uint8>("TargetType"));

					return false;
				}
			default: ;
			}

			return true;
		}

		enum : uint16
		{
			RouteId_Session2,
		};

		TraceServices::FSessionInfo SessionInfo;
	};

} // UE::MetaStoryDebugger


//----------------------------------------------------------------//
// FMetaStoryDebugger
//----------------------------------------------------------------//
FMetaStoryDebugger::FMetaStoryDebugger()
	: MetaStoryModule(FModuleManager::GetModuleChecked<IMetaStoryModule>("MetaStoryModule"))
{
	TracingStateChangedHandle = UE::MetaStory::Delegates::OnTracingStateChanged.AddLambda([this](const EMetaStoryTraceStatus TraceStatus)
		{
			// MetaStory traces got enabled in the current process so let's analyse it if not already analysing something.
			if (TraceStatus == EMetaStoryTraceStatus::TracesStarted && !IsAnalysisSessionActive())
			{
				RequestAnalysisOfLatestTrace();
			}
		});

	TraceAnalysisStateChangedHandle = UE::MetaStory::Delegates::OnTraceAnalysisStateChanged.AddLambda([this](const EMetaStoryTraceAnalysisStatus TraceAnalysisStatus)
		{
			if (TraceAnalysisStatus == EMetaStoryTraceAnalysisStatus::Cleared
				|| TraceAnalysisStatus == EMetaStoryTraceAnalysisStatus::Stopped)
			{
				StopSessionAnalysis();
			}
		});

	TracingTimelineScrubbedHandle = UE::MetaStory::Delegates::OnTracingTimelineScrubbed.AddLambda([this](const double InScrubTime)
		{
			SetScrubTime(InScrubTime);
		});
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FMetaStoryDebugger::~FMetaStoryDebugger()
{
	UE::MetaStory::Delegates::OnTracingStateChanged.Remove(TracingStateChangedHandle);
	UE::MetaStory::Delegates::OnTraceAnalysisStateChanged.Remove(TraceAnalysisStateChangedHandle);
	UE::MetaStory::Delegates::OnTracingTimelineScrubbed.Remove(TracingTimelineScrubbedHandle);

	StopSessionAnalysis();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FMetaStoryDebugger::Tick(const float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMetaStoryDebugger::Tick);

	if (RetryLoadNextLiveSessionTimer > 0.0f)
	{
		// We are still not connected to the last live session.
		// Update polling timer and retry with remaining time; 0 or less will stop retries.
		if (TryStartNewLiveSessionAnalysis(RetryLoadNextLiveSessionTimer - DeltaTime))
		{
			RetryLoadNextLiveSessionTimer = 0.0f;
			LastLiveSessionId = INDEX_NONE;
		}
	}

	if (MetaStoryAsset.IsValid()
		&& IsAnalysisSessionActive()
		&& !IsAnalysisSessionPaused())
	{
		SyncToCurrentSessionDuration();
	}
}

void FMetaStoryDebugger::StopSessionAnalysis()
{
	// HitBreakpoint is normally reset when resuming the session analysis, but it is also
	// possible to stop the session analysis while it is paused from a breakpoint.
	// In this case we make sure to reset it before forcing the last update since the breakpoint
	// should no longer be in effect.
	HitBreakpoint.Reset();

	if (IsAnalysisSessionActive())
	{
		// Force one last update to process events emitted while closing the game session (e.g., EndPlay in PIE)
		// Note that we can only perform this if the associated asset is still loaded which might not be the case
		// during the standalone game shutdown.
		if (MetaStoryAsset.IsValid())
		{
			SyncToCurrentSessionDuration();
		}
		AnalysisSession->Stop(/*WaitOnAnalysis*/true);
	}

	bSessionAnalysisActive = false;
	bSessionAnalysisPaused = false;
	LastProcessedRecordedWorldTime = 0;
}

void FMetaStoryDebugger::SyncToCurrentSessionDuration()
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);
			AnalysisDuration = Session->GetDurationSeconds();
		}
		ReadTrace(AnalysisDuration);
	}
}

TSharedPtr<const UE::MetaStoryDebugger::FInstanceDescriptor> FMetaStoryDebugger::GetDescriptor(const FMetaStoryInstanceDebugId InstanceId) const
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

		if (const IMetaStoryTraceProvider* Provider = Session->ReadProvider<IMetaStoryTraceProvider>(FMetaStoryTraceProvider::ProviderName))
		{
			return Provider->GetInstanceDescriptor(InstanceId);
		}
	}

	return nullptr;
}

FText FMetaStoryDebugger::GetInstanceName(const FMetaStoryInstanceDebugId InstanceId) const
{
	const TSharedPtr<const UE::MetaStoryDebugger::FInstanceDescriptor> FoundDescriptor = GetDescriptor(InstanceId);
	return (FoundDescriptor != nullptr) ? FText::FromString(FoundDescriptor->Name) : LOCTEXT("InstanceNotFound","Instance not found");
}

FText FMetaStoryDebugger::GetInstanceDescription(const FMetaStoryInstanceDebugId InstanceId) const
{
	const TSharedPtr<const UE::MetaStoryDebugger::FInstanceDescriptor> FoundDescriptor = GetDescriptor(InstanceId);
	return (FoundDescriptor != nullptr) ? DescribeInstance(*FoundDescriptor) : LOCTEXT("InstanceNotFound","Instance not found");
}

void FMetaStoryDebugger::SelectInstance(const FMetaStoryInstanceDebugId InstanceId)
{
	if (SelectedInstanceId != InstanceId)
	{
		SelectedInstanceId = InstanceId;

		// Update event collection for newly debugged instance
		SetScrubStateCollection(GetMutableEventCollection(InstanceId));
	}
}

// Deprecated
void FMetaStoryDebugger::GetSessionInstances(TArray<UE::MetaStoryDebugger::FInstanceDescriptor>& OutInstances) const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

		if (const IMetaStoryTraceProvider* Provider = Session->ReadProvider<IMetaStoryTraceProvider>(FMetaStoryTraceProvider::ProviderName))
		{
			Provider->GetInstances(OutInstances);
		}
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FMetaStoryDebugger::GetSessionInstanceDescriptors(TArray<const TSharedRef<const UE::MetaStoryDebugger::FInstanceDescriptor>>& OutInstances) const
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

		if (const IMetaStoryTraceProvider* Provider = Session->ReadProvider<IMetaStoryTraceProvider>(FMetaStoryTraceProvider::ProviderName))
		{
			Provider->GetInstances(OutInstances);
		}
	}
}

bool FMetaStoryDebugger::RequestAnalysisOfEditorSession()
{
	// Get snapshot of current trace to help identify the next live one
	TArray<FTraceDescriptor> TraceDescriptors;
	GetLiveTraces(TraceDescriptors);
	LastLiveSessionId = TraceDescriptors.Num() ? TraceDescriptors.Last().TraceId : INDEX_NONE;

	// 0 is the invalid value used for Trace Id
	constexpr int32 InvalidTraceId = 0;
	int32 ActiveTraceId = InvalidTraceId;

	// StartTraces returns true if a new connection was created.
	// In this case we will receive OnTracingStateChanged and try to start an analysis on that new connection as soon as possible.
	// Otherwise, it might have been able to use an active connection in which case it was returned in the output parameter.
	if (MetaStoryModule.StartTraces(ActiveTraceId))
	{
		return true;
	}

	// Otherwise we start analysis of the already active trace, if any.
	if (ActiveTraceId != InvalidTraceId)
	{
		if (const FTraceDescriptor* Descriptor = TraceDescriptors.FindByPredicate([ActiveTraceId](const FTraceDescriptor& Descriptor)
			{
				return Descriptor.TraceId == ActiveTraceId;
			}))
		{
			return RequestSessionAnalysis(*Descriptor);
		}
	}

	return false;
}

void FMetaStoryDebugger::RequestAnalysisOfLatestTrace()
{
	// Invalidate our current active session
	ActiveSessionTraceDescriptor = FTraceDescriptor();

	// Invalidate current selected instance so breakpoint can be hit by any instances in the next analysis
	ClearSelection();

	// Stop current analysis if any
	StopSessionAnalysis();

	// This might not succeed immediately but will schedule next retry if necessary
	TryStartNewLiveSessionAnalysis(1.0f);
}


bool FMetaStoryDebugger::TryStartNewLiveSessionAnalysis(const float RetryPollingDuration)
{
	TArray<FTraceDescriptor> Traces;
	GetLiveTraces(Traces);

	if (Traces.Num() && Traces.Last().TraceId != LastLiveSessionId)
	{
		// Intentional call to StartSessionAnalysis instead of RequestSessionAnalysis since we want
		// to set 'bIsAnalyzingNextEditorSession' before calling OnNewSession delegate.
		const bool bStarted = StartSessionAnalysis(Traces.Last());
		if (bStarted)
		{
			UpdateAnalysisTransitionType(EAnalysisSourceType::EditorSession);

			SetScrubStateCollection(nullptr);
			OnNewSession.ExecuteIfBound();
		}

		return bStarted;
	}

	RetryLoadNextLiveSessionTimer = RetryPollingDuration;
	UE_CLOG(RetryLoadNextLiveSessionTimer > 0, LogMetaStory, Log, TEXT("Unable to start analysis for the most recent live session."));

	return false;
}

bool FMetaStoryDebugger::StartSessionAnalysis(const FTraceDescriptor& TraceDescriptor)
{
	if (ActiveSessionTraceDescriptor == TraceDescriptor)
	{
		return ActiveSessionTraceDescriptor.IsValid();
	}

	ActiveSessionTraceDescriptor = FTraceDescriptor();

	// Make sure any active analysis is stopped
	StopSessionAnalysis();

	UE::Trace::FStoreClient* StoreClient = GetStoreClient();
	if (StoreClient == nullptr)
	{
		return false;
	}

	// If new trace descriptor is not valid no need to continue
	if (TraceDescriptor.IsValid() == false)
	{
		return false;
	}

	AnalysisDuration = 0;
	LastTraceReadTime = 0;

	const uint32 TraceId = TraceDescriptor.TraceId;

	// Make sure it is still live
	const UE::Trace::FStoreClient::FSessionInfo* SessionInfo = StoreClient->GetSessionInfoByTraceId(TraceId);
	if (SessionInfo != nullptr)
	{
		UE::Trace::FStoreClient::FTraceData TraceData = StoreClient->ReadTrace(TraceId);
		if (!TraceData)
		{
			return false;
		}

		FString TraceName(StoreClient->GetStatus()->GetStoreDir());
		const UE::Trace::FStoreClient::FTraceInfo* TraceInfo = StoreClient->GetTraceInfoById(TraceId);
		if (TraceInfo != nullptr)
		{
			FString Name(TraceInfo->GetName());
			if (!Name.EndsWith(TEXT(".utrace")))
			{
				Name += TEXT(".utrace");
			}
			TraceName = FPaths::Combine(TraceName, Name);
			FPaths::NormalizeFilename(TraceName);
		}

		ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
		if (const TSharedPtr<TraceServices::IAnalysisService> TraceAnalysisService = TraceServicesModule.GetAnalysisService())
		{
			checkf(!IsAnalysisSessionActive(), TEXT("Must make sure that current session was properly stopped before starting a new one otherwise it can cause threading issues"));
			AnalysisSession = TraceAnalysisService->StartAnalysis(TraceId, *TraceName, MoveTemp(TraceData));
		}

		if (AnalysisSession.IsValid())
		{
			bSessionAnalysisActive = true;
			ActiveSessionTraceDescriptor = TraceDescriptor;
		}
	}

	return ActiveSessionTraceDescriptor.IsValid();
}

void FMetaStoryDebugger::SetScrubStateCollection(const UE::MetaStoryDebugger::FInstanceEventCollection* Collection)
{
	ScrubState.SetEventCollection(Collection);

	OnScrubStateChanged.ExecuteIfBound(ScrubState);

	RefreshActiveStates();
}

void FMetaStoryDebugger::GetLiveTraces(TArray<FTraceDescriptor>& OutTraceDescriptors) const
{
	UE::Trace::FStoreClient* StoreClient = GetStoreClient();
	if (StoreClient == nullptr)
	{
		return;
	}

	OutTraceDescriptors.Reset();

	const uint32 SessionCount = StoreClient->GetSessionCount();
	for (uint32 SessionIndex = 0; SessionIndex < SessionCount; ++SessionIndex)
	{
		const UE::Trace::FStoreClient::FSessionInfo* SessionInfo = StoreClient->GetSessionInfo(SessionIndex);
		if (SessionInfo != nullptr)
		{
			const uint32 TraceId = SessionInfo->GetTraceId();
			const UE::Trace::FStoreClient::FTraceInfo* TraceInfo = StoreClient->GetTraceInfoById(TraceId);
			if (TraceInfo != nullptr)
			{
				FTraceDescriptor& Trace = OutTraceDescriptors.AddDefaulted_GetRef();
				Trace.TraceId = TraceId;
				Trace.Name = FString(TraceInfo->GetName());
				UpdateMetadata(Trace);
			}
		}
	}
}

void FMetaStoryDebugger::UpdateMetadata(FTraceDescriptor& TraceDescriptor) const
{
	UE::Trace::FStoreClient* StoreClient = GetStoreClient();
	if (StoreClient == nullptr)
	{
		return;
	}

	const UE::Trace::FStoreClient::FTraceData TraceData = StoreClient->ReadTrace(TraceDescriptor.TraceId);
	if (!TraceData)
	{
		return;
	}

	// inspired from FStoreBrowser
	struct FDataStream : UE::Trace::IInDataStream
	{
		enum class EReadStatus
		{
			Ready = 0,
			StoppedByReadSizeLimit
		};

		virtual int32 Read(void* Data, const uint32 Size) override
		{
			if (BytesRead >= 1024 * 1024)
			{
				Status = EReadStatus::StoppedByReadSizeLimit;
				return 0;
			}
			const int32 InnerBytesRead = Inner->Read(Data, Size);
			BytesRead += InnerBytesRead;

			return InnerBytesRead;
		}

		virtual void Close() override
		{
			Inner->Close();
		}

		IInDataStream* Inner = nullptr;
		int32 BytesRead = 0;
		EReadStatus Status = EReadStatus::Ready;
	};

	FDataStream DataStream;
	DataStream.Inner = TraceData.Get();

	UE::MetaStoryDebugger::FDiagnosticsSessionAnalyzer Analyzer;
	UE::Trace::FAnalysisContext Context;
	Context.AddAnalyzer(Analyzer);
	Context.Process(DataStream).Wait();

	TraceDescriptor.SessionInfo = Analyzer.SessionInfo;
}

FText FMetaStoryDebugger::GetSelectedTraceDescription() const
{
	if (ActiveSessionTraceDescriptor.IsValid())
	{
		return DescribeTrace(ActiveSessionTraceDescriptor);
	}

	return LOCTEXT("NoSelectedTraceDescriptor", "No trace selected");
}

void FMetaStoryDebugger::SetScrubTime(const double ScrubTime)
{
	if (ScrubState.SetScrubTime(ScrubTime))
	{
		OnScrubStateChanged.ExecuteIfBound(ScrubState);

		RefreshActiveStates();
	}
}

bool FMetaStoryDebugger::IsActiveInstance(const double Time, const FMetaStoryInstanceDebugId InstanceId) const
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

		if (const IMetaStoryTraceProvider* Provider = Session->ReadProvider<IMetaStoryTraceProvider>(FMetaStoryTraceProvider::ProviderName))
		{
			const TSharedPtr<const UE::MetaStoryDebugger::FInstanceDescriptor> Descriptor = Provider->GetInstanceDescriptor(InstanceId);
			return Descriptor.IsValid() && Descriptor->Lifetime.Contains(Time);
		}
	}
	return false;
}

FText FMetaStoryDebugger::DescribeTrace(const FTraceDescriptor& TraceDescriptor)
{
	if (TraceDescriptor.IsValid())
	{
		const TraceServices::FSessionInfo& SessionInfo = TraceDescriptor.SessionInfo;

		return FText::FromString(FString::Printf(TEXT("%s-%s-%s-%s-%s"),
			*LexToString(TraceDescriptor.TraceId),
			*SessionInfo.Platform,
			*SessionInfo.AppName,
			LexToString(SessionInfo.ConfigurationType),
			LexToString(SessionInfo.TargetType)));
	}

	return LOCTEXT("InvalidTraceDescriptor", "Invalid");
}

FText FMetaStoryDebugger::DescribeInstance(const UE::MetaStoryDebugger::FInstanceDescriptor& InstanceDesc)
{
	if (InstanceDesc.IsValid() == false)
	{
		return LOCTEXT("NoSelectedInstanceDescriptor", "No instance selected");
	}
	return FText::FromString(LexToString(InstanceDesc));
}

void FMetaStoryDebugger::SetActiveStates(const FMetaStoryTraceActiveStates& NewActiveStates)
{
	ActiveStates = NewActiveStates;
	OnActiveStatesChanged.ExecuteIfBound(ActiveStates);
}

void FMetaStoryDebugger::RefreshActiveStates()
{
	if (ScrubState.IsPointingToValidActiveStates())
	{
		const UE::MetaStoryDebugger::FInstanceEventCollection& EventCollection = ScrubState.GetEventCollection();
		const int32 EventIndex = EventCollection.ActiveStatesChanges[ScrubState.GetActiveStatesIndex()].EventIndex;
		SetActiveStates(EventCollection.Events[EventIndex].Get<FMetaStoryTraceActiveStatesEvent>().ActiveStates);
	}
	else
	{
		SetActiveStates(FMetaStoryTraceActiveStates());
	}
}

bool FMetaStoryDebugger::CanStepBackToPreviousStateWithEvents() const
{
	return ScrubState.HasPreviousFrame();
}

void FMetaStoryDebugger::StepBackToPreviousStateWithEvents()
{
	ScrubState.GotoPreviousFrame();
	OnScrubStateChanged.Execute(ScrubState);

	RefreshActiveStates();
}

bool FMetaStoryDebugger::CanStepForwardToNextStateWithEvents() const
{
	return ScrubState.HasNextFrame();
}

void FMetaStoryDebugger::StepForwardToNextStateWithEvents()
{
	ScrubState.GotoNextFrame();
	OnScrubStateChanged.Execute(ScrubState);

	RefreshActiveStates();
}

bool FMetaStoryDebugger::CanStepBackToPreviousStateChange() const
{
	return ScrubState.HasPreviousActiveStates();
}

void FMetaStoryDebugger::StepBackToPreviousStateChange()
{
	ScrubState.GotoPreviousActiveStates();
	OnScrubStateChanged.Execute(ScrubState);

	RefreshActiveStates();
}

bool FMetaStoryDebugger::CanStepForwardToNextStateChange() const
{
	return ScrubState.HasNextActiveStates();
}

void FMetaStoryDebugger::StepForwardToNextStateChange()
{
	ScrubState.GotoNextActiveStates();
	OnScrubStateChanged.Execute(ScrubState);

	RefreshActiveStates();
}

bool FMetaStoryDebugger::HasStateBreakpoint(const FMetaStoryStateHandle StateHandle, const EMetaStoryBreakpointType BreakpointType) const
{
	return Breakpoints.ContainsByPredicate([StateHandle, BreakpointType](const FMetaStoryDebuggerBreakpoint& Breakpoint)
		{
			if (Breakpoint.BreakpointType == BreakpointType)
			{
				const FMetaStoryStateHandle* BreakpointStateHandle = Breakpoint.ElementIdentifier.TryGet<FMetaStoryStateHandle>();
				return (BreakpointStateHandle != nullptr && *BreakpointStateHandle == StateHandle);
			}
			return false;
		});
}

bool FMetaStoryDebugger::HasTaskBreakpoint(const FMetaStoryIndex16 Index, const EMetaStoryBreakpointType BreakpointType) const
{
	return Breakpoints.ContainsByPredicate([Index, BreakpointType](const FMetaStoryDebuggerBreakpoint& Breakpoint)
	{
		if (Breakpoint.BreakpointType == BreakpointType)
		{
			const FMetaStoryDebuggerBreakpoint::FMetaStoryTaskIndex* BreakpointTaskIndex = Breakpoint.ElementIdentifier.TryGet<FMetaStoryDebuggerBreakpoint::FMetaStoryTaskIndex>();
			return (BreakpointTaskIndex != nullptr && BreakpointTaskIndex->Index == Index);
		}
		return false;
	});
}

bool FMetaStoryDebugger::HasTransitionBreakpoint(const FMetaStoryIndex16 Index, const EMetaStoryBreakpointType BreakpointType) const
{
	return Breakpoints.ContainsByPredicate([Index, BreakpointType](const FMetaStoryDebuggerBreakpoint& Breakpoint)
	{
		if (Breakpoint.BreakpointType == BreakpointType)
		{
			const FMetaStoryDebuggerBreakpoint::FMetaStoryTransitionIndex* BreakpointTransitionIndex = Breakpoint.ElementIdentifier.TryGet<FMetaStoryDebuggerBreakpoint::FMetaStoryTransitionIndex>();
			return (BreakpointTransitionIndex != nullptr && BreakpointTransitionIndex->Index == Index);
		}
		return false;
	});
}

void FMetaStoryDebugger::SetStateBreakpoint(const FMetaStoryStateHandle StateHandle, const EMetaStoryBreakpointType BreakpointType)
{
	Breakpoints.Emplace(StateHandle, BreakpointType);
}

void FMetaStoryDebugger::SetTransitionBreakpoint(const FMetaStoryIndex16 TransitionIndex, const EMetaStoryBreakpointType BreakpointType)
{
	Breakpoints.Emplace(FMetaStoryDebuggerBreakpoint::FMetaStoryTransitionIndex(TransitionIndex), BreakpointType);
}

void FMetaStoryDebugger::SetTaskBreakpoint(const FMetaStoryIndex16 NodeIndex, const EMetaStoryBreakpointType BreakpointType)
{
	Breakpoints.Emplace(FMetaStoryDebuggerBreakpoint::FMetaStoryTaskIndex(NodeIndex), BreakpointType);
}

void FMetaStoryDebugger::ClearBreakpoint(const FMetaStoryIndex16 NodeIndex, const EMetaStoryBreakpointType BreakpointType)
{
	const int32 Index = Breakpoints.IndexOfByPredicate([NodeIndex, BreakpointType](const FMetaStoryDebuggerBreakpoint& Breakpoint)
		{
			const FMetaStoryDebuggerBreakpoint::FMetaStoryTaskIndex* IndexPtr = Breakpoint.ElementIdentifier.TryGet<FMetaStoryDebuggerBreakpoint::FMetaStoryTaskIndex>();
			return (IndexPtr != nullptr && IndexPtr->Index == NodeIndex && Breakpoint.BreakpointType == BreakpointType);
		});

	if (Index != INDEX_NONE)
	{
		Breakpoints.RemoveAtSwap(Index);
	}
}

void FMetaStoryDebugger::ClearAllBreakpoints()
{
	Breakpoints.Empty();
}

const TraceServices::IAnalysisSession* FMetaStoryDebugger::GetAnalysisSession() const
{
	return AnalysisSession.Get();
}

bool FMetaStoryDebugger::RequestSessionAnalysis(const FTraceDescriptor& TraceDescriptor)
{
	if (StartSessionAnalysis(TraceDescriptor))
	{
		UpdateAnalysisTransitionType(EAnalysisSourceType::SelectedSession);

		SetScrubStateCollection(nullptr);
		OnNewSession.ExecuteIfBound();
		return true;
	}
	return false;
}

void FMetaStoryDebugger::UpdateAnalysisTransitionType(const EAnalysisSourceType SourceType)
{
	switch (AnalysisTransitionType)
	{
	case EAnalysisTransitionType::Unset:
		AnalysisTransitionType = (SourceType == EAnalysisSourceType::SelectedSession)
				? EAnalysisTransitionType::NoneToSelected
				: EAnalysisTransitionType::NoneToEditor;
		break;

	case EAnalysisTransitionType::NoneToSelected:
	case EAnalysisTransitionType::EditorToSelected:
	case EAnalysisTransitionType::SelectedToSelected:
		AnalysisTransitionType = (SourceType == EAnalysisSourceType::SelectedSession)
				? EAnalysisTransitionType::SelectedToSelected
				: EAnalysisTransitionType::SelectedToEditor;
		break;

	case EAnalysisTransitionType::NoneToEditor:
	case EAnalysisTransitionType::EditorToEditor:
	case EAnalysisTransitionType::SelectedToEditor:
		AnalysisTransitionType = (SourceType == EAnalysisSourceType::SelectedSession)
				? EAnalysisTransitionType::EditorToSelected
				: EAnalysisTransitionType::EditorToEditor;
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled transition type."));
	}
}

UE::Trace::FStoreClient* FMetaStoryDebugger::GetStoreClient() const
{
	return MetaStoryModule.GetStoreClient();
}

void FMetaStoryDebugger::ReadTrace(const uint64 FrameIndex)
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

		const TraceServices::IFrameProvider& FrameProvider = TraceServices::ReadFrameProvider(*Session);

		if (const TraceServices::FFrame* TargetFrame = FrameProvider.GetFrame(TraceFrameType_Game, FrameIndex))
		{
			UE::MetaStoryDebugger::ReadTrace(*Session
				, FrameProvider
				, *TargetFrame
				, /*ITraceReader*/this
				, UE::MetaStoryDebugger::FTraceFilter(MetaStoryAsset.Get())
				, LastTraceReadTime);
		}
	}

	// Notify outside session read scope
	SendNotifications();
}

void FMetaStoryDebugger::ReadTrace(const double ScrubTime)
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		UE::MetaStoryDebugger::ReadTrace(
			*Session
			, ScrubTime
			, this
			, UE::MetaStoryDebugger::FTraceFilter(MetaStoryAsset.Get())
			, LastTraceReadTime);

		SendNotifications();
	}
}

void FMetaStoryDebugger::SendNotifications()
{
	if (NewInstances.Num() > 0)
	{
		for (const FMetaStoryInstanceDebugId NewInstanceId : NewInstances)
		{
			OnNewInstance.ExecuteIfBound(NewInstanceId);
		}
		NewInstances.Reset();
	}

	if (HitBreakpoint.IsSet())
	{
		check(HitBreakpoint.InstanceId.IsValid());
		check(Breakpoints.IsValidIndex(HitBreakpoint.Index));

		// Force scrub time to latest simulation time to reflect most recent events.
		// This will notify scrub position changed and active states
		SetScrubTime(HitBreakpoint.Time);

		// Make sure the instance is selected in case the breakpoint was set for any instances
		if (SelectedInstanceId != HitBreakpoint.InstanceId)
		{
			SelectInstance(HitBreakpoint.InstanceId);
		}

		OnBreakpointHit.ExecuteIfBound(HitBreakpoint.InstanceId, Breakpoints[HitBreakpoint.Index]);

		PauseSessionAnalysis();
	}
}

bool FMetaStoryDebugger::EvaluateBreakpoints(const FMetaStoryInstanceDebugId InstanceId, const FMetaStoryTraceEventVariantType& Event)
{
	if (MetaStoryAsset == nullptr // asset is required to properly match state handles
		|| HitBreakpoint.IsSet() // Only consider first hit breakpoint in the frame
		|| Breakpoints.IsEmpty()
		|| (SelectedInstanceId.IsValid() && InstanceId != SelectedInstanceId)) // ignore events not for the selected instances
	{
		return false;
	}

	for (int BreakpointIndex = 0; BreakpointIndex < Breakpoints.Num(); ++BreakpointIndex)
	{
		const FMetaStoryDebuggerBreakpoint Breakpoint = Breakpoints[BreakpointIndex];
		if (Breakpoint.IsMatchingEvent(Event))
		{
			HitBreakpoint.Index = BreakpointIndex;
			HitBreakpoint.InstanceId = InstanceId;
			HitBreakpoint.Time = LastProcessedRecordedWorldTime;
		}
	}

	return HitBreakpoint.IsSet();
}

UE::MetaStoryDebugger::FInstanceEventCollection* FMetaStoryDebugger::GetOrCreateEventCollection(FMetaStoryInstanceDebugId InstanceId)
{
	UE::MetaStoryDebugger::FInstanceEventCollection* ExistingCollection = GetMutableEventCollection(InstanceId);

	// Create missing EventCollection if necessary
	if (ExistingCollection == nullptr)
	{
		// Push deferred notification for new instance Id
		NewInstances.Push(InstanceId);

		ExistingCollection = new UE::MetaStoryDebugger::FInstanceEventCollection(InstanceId);
		EventCollections.Add(ExistingCollection);

		// Update the active event collection when it's newly created for the currently debugged instance.
		// Otherwise (i.e. EventCollection already exists) it is updated when switching instance (i.e. SelectInstance)
		if (SelectedInstanceId == InstanceId && ScrubState.GetEventCollection().IsInvalid())
		{
			SetScrubStateCollection(ExistingCollection);
		}
	}
	return ExistingCollection;
}

void FMetaStoryDebugger::OnTraceEventProcessed(const FMetaStoryInstanceDebugId InstanceId, const FMetaStoryTraceEventVariantType& Event)
{
	Visit([&WorldTime = LastProcessedRecordedWorldTime](auto& TypedEvent)
		{
			WorldTime = TypedEvent.RecordingWorldTime;
		}, Event);

	EvaluateBreakpoints(InstanceId, Event);
}

const UE::MetaStoryDebugger::FInstanceEventCollection& FMetaStoryDebugger::GetEventCollection(const FMetaStoryInstanceDebugId InstanceId) const
{
	for (const UE::MetaStoryDebugger::FInstanceEventCollection& EventCollection : EventCollections)
	{
		if (EventCollection.InstanceId == InstanceId)
		{
			return EventCollection;
		}
	}

	return UE::MetaStoryDebugger::FInstanceEventCollection::Invalid;
}

UE::MetaStoryDebugger::FInstanceEventCollection* FMetaStoryDebugger::GetMutableEventCollection(const FMetaStoryInstanceDebugId InstanceId)
{
	for (UE::MetaStoryDebugger::FInstanceEventCollection& EventCollection : EventCollections)
	{
		if (EventCollection.InstanceId == InstanceId)
		{
			// Note that returning the pointer to the EventCollection element here is fine.
			// Using a TDereferencingIterator on a TIndirectArray returns us the actual collection instance.
			return &EventCollection;
		}
	}

	return nullptr;
}

void FMetaStoryDebugger::ResetEventCollections()
{
	EventCollections.Reset();
	SetScrubStateCollection(nullptr);
	LastProcessedRecordedWorldTime = 0;
}

//----------------------------------------------------------------//
// UE::MetaStoryDebugger
//----------------------------------------------------------------//
namespace UE::MetaStoryDebugger
{

bool ProcessEvent(
	FInstanceEventCollection& InEventCollection,
	const TraceServices::FFrame& InFrame,
	const FMetaStoryTraceEventVariantType& InEvent,
	TNotNull<ITraceReader*> TraceReader)
{
	TArray<FMetaStoryTraceEventVariantType>& Events = InEventCollection.Events;

	TraceServices::FFrame FrameToAddInSpans = InFrame;
	bool bShouldAddFrameToSpans = false;

	// Add new frame span if none added yet
	if (InEventCollection.FrameSpans.IsEmpty())
	{
		bShouldAddFrameToSpans = true;
	}
	else
	{
		const FFrameSpan& LastSpan = InEventCollection.FrameSpans.Last();
		const TraceServices::FFrame& LastFrame = LastSpan.Frame;
		const uint64 FrameIndexOffset = InEventCollection.ContiguousTracesData.IsEmpty()
			? 0
			: (InEventCollection.FrameSpans[InEventCollection.ContiguousTracesData.Last().LastSpanIndex].Frame.Index + 1);

		// Add new frame span for new larger frame index
		if (InFrame.Index + FrameIndexOffset > LastFrame.Index)
		{
			bShouldAddFrameToSpans = true;

			// Apply current offset to the frame index
			FrameToAddInSpans.Index += FrameIndexOffset;
		}
		else if (InFrame.Index < LastFrame.Index && InFrame.StartTime > LastFrame.StartTime)
		{
			// Frame index will restart at 0 if a new session is started,
			// in that case we offset the frame we store to append to existing data
			bShouldAddFrameToSpans = true;

			const FInstanceEventCollection::FContiguousTraceInfo& TraceInfo =
				InEventCollection.ContiguousTracesData.Emplace_GetRef(
					FInstanceEventCollection::FContiguousTraceInfo(InEventCollection.FrameSpans.Num() - 1));
			FrameToAddInSpans.Index += InEventCollection.FrameSpans[TraceInfo.LastSpanIndex].Frame.Index + 1;
		}
	}

	if (bShouldAddFrameToSpans)
	{
		double RecordingWorldTime = 0;
		Visit([&RecordingWorldTime](auto& TypedEvent)
			{
				RecordingWorldTime = TypedEvent.RecordingWorldTime;
			}, InEvent);

		InEventCollection.FrameSpans.Add(FFrameSpan(FrameToAddInSpans, RecordingWorldTime, Events.Num()));
	}

	// Add activate states change info
	if (InEvent.IsType<FMetaStoryTraceActiveStatesEvent>())
	{
		checkf(InEventCollection.FrameSpans.Num() > 0, TEXT("Expecting to always be in a frame span at this point."));
		const int32 FrameSpanIndex = InEventCollection.FrameSpans.Num() - 1;

		// Add new entry for the first event or if the last event is for a different frame
		if (InEventCollection.ActiveStatesChanges.IsEmpty()
			|| InEventCollection.ActiveStatesChanges.Last().SpanIndex != FrameSpanIndex)
		{
			InEventCollection.ActiveStatesChanges.Push({ FrameSpanIndex, Events.Num() });
		}
		else
		{
			// Multiple events for change of active states in the same frame, keep the last one until we implement scrubbing within a frame
			InEventCollection.ActiveStatesChanges.Last().EventIndex = Events.Num();
		}
	}

	// Store event in the collection
	Events.Emplace(InEvent);

	// Notify the reader
	TraceReader->OnTraceEventProcessed(InEventCollection.InstanceId, InEvent);

	return /*bKeepProcessing*/true;
}

void AddEvents(
	const double InStartTime,
	const double InEndTime,
	const TraceServices::IFrameProvider& InFrameProvider,
	const FMetaStoryInstanceDebugId InInstanceId,
	const IMetaStoryTraceProvider::FEventsTimeline& TimelineData,
	TNotNull<ITraceReader*> TraceReader)
{
	FInstanceEventCollection* EventCollection = TraceReader->GetOrCreateEventCollection(InInstanceId);
	if (EventCollection == nullptr)
	{
		return;
	}

	// Keep track of the frames containing events. Starting with an invalid frame.
	TraceServices::FFrame Frame;
	Frame.Index = INDEX_NONE;

	TimelineData.EnumerateEvents(InStartTime, InEndTime,
		[&InFrameProvider, &Frame, EventCollection, TraceReader](const double EventStartTime, const double EventEndTime, uint32 InDepth, const FMetaStoryTraceEventVariantType& Event)
		{
			bool bValidFrame = true;

			// Fetch frame when not set yet or if events no longer part of the current one
			if (Frame.Index == INDEX_NONE ||
				(EventEndTime < Frame.StartTime || Frame.EndTime < EventStartTime))
			{
				bValidFrame = InFrameProvider.GetFrameFromTime(TraceFrameType_Game, EventStartTime, Frame);

				if (bValidFrame == false)
				{
					// Edge case for events from a missing first complete frame.
					// (i.e. FrameProvider didn't get BeginFrame event but MetaStoryEvent were sent in that frame)
					// Doing this will merge our two first frames of state tree events using the same recording world time
					// but this should happen only for late start recording.
					const TraceServices::FFrame* FirstFrame = InFrameProvider.GetFrame(TraceFrameType_Game, 0);
					if (FirstFrame != nullptr && EventEndTime < FirstFrame->StartTime)
					{
						Frame = *FirstFrame;
						bValidFrame = true;
					}
				}
			}

			if (bValidFrame)
			{
				const bool bKeepProcessing = ProcessEvent(*EventCollection, Frame, Event, TraceReader);
				return bKeepProcessing ? TraceServices::EEventEnumerate::Continue : TraceServices::EEventEnumerate::Stop;
			}

			// Skip events outside of game frames
			return TraceServices::EEventEnumerate::Continue;
		});
}

void ReadTrace(
	const TraceServices::IAnalysisSession& Session,
	const double ScrubTime,
	const TNotNull<ITraceReader*> TraceReader,
	const FTraceFilter& Filter,
	double& OutLastTraceReadTime)
{
	TraceServices::FAnalysisSessionReadScope SessionReadScope(Session);

	const TraceServices::IFrameProvider& FrameProvider = TraceServices::ReadFrameProvider(Session);

	TraceServices::FFrame TargetFrame;
	if (FrameProvider.GetFrameFromTime(TraceFrameType_Game, ScrubTime, TargetFrame))
	{
		// Process only completed frames
		bool bValidFrame = true;
		if (TargetFrame.EndTime == std::numeric_limits<double>::infinity())
		{
			if (const TraceServices::FFrame* PreviousCompleteFrame = FrameProvider.GetFrame(TraceFrameType_Game, TargetFrame.Index - 1))
			{
				TargetFrame = *PreviousCompleteFrame;
			}
			else
			{
				bValidFrame = false;
			}
		}

		if (bValidFrame)
		{
			ReadTrace(Session, FrameProvider, TargetFrame, TraceReader, Filter, OutLastTraceReadTime);
		}
	}
}

void ReadTrace(
	const TraceServices::IAnalysisSession& Session,
	const TraceServices::IFrameProvider& FrameProvider,
	const TraceServices::FFrame& Frame,
	TNotNull<ITraceReader*> TraceReader,
	const FTraceFilter& Filter,
	double& OutLastTraceReadTime)
{
	TraceServices::FFrame LastReadFrame;
	const bool bValidLastReadFrame = FrameProvider.GetFrameFromTime(TraceFrameType_Game, OutLastTraceReadTime, LastReadFrame);
	if (OutLastTraceReadTime == 0 || (bValidLastReadFrame && Frame.Index > LastReadFrame.Index))
	{
		if (const IMetaStoryTraceProvider* Provider = Session.ReadProvider<IMetaStoryTraceProvider>(FMetaStoryTraceProvider::ProviderName))
		{
			if (Filter.Instance.IsValid())
			{
				Provider->ReadTimelines(Filter.Instance,
					[StartTime = OutLastTraceReadTime, EndTime = Frame.EndTime, &FrameProvider, TraceReader](const FMetaStoryInstanceDebugId InstanceId, const IMetaStoryTraceProvider::FEventsTimeline& TimelineData)
					{
						AddEvents(StartTime, EndTime, FrameProvider, InstanceId, TimelineData, TraceReader);
					});
			}
			else if (Filter.Asset)
			{
				Provider->ReadTimelines(*Filter.Asset,
					[StartTime = OutLastTraceReadTime, EndTime = Frame.EndTime, &FrameProvider, TraceReader](const FMetaStoryInstanceDebugId InstanceId, const IMetaStoryTraceProvider::FEventsTimeline& TimelineData)
					{
						AddEvents(StartTime, EndTime, FrameProvider, InstanceId, TimelineData, TraceReader);
					});
			}

			OutLastTraceReadTime = Frame.EndTime;
		}
	}
}

} // UE::MetaStoryDebugger

#undef LOCTEXT_NAMESPACE

#endif // WITH_METASTORY_TRACE_DEBUGGER
