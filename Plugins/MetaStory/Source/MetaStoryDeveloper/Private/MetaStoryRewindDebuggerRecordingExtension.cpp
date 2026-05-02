// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_METASTORY_TRACE
#include "MetaStoryRewindDebuggerRecordingExtension.h"
#include "Debugger/MetaStoryTraceTypes.h"
#include "MetaStoryDelegates.h"
#include "Debugger/MetaStoryTrace.h"

namespace UE::MetaStoryDebugger
{

//----------------------------------------------------------------------//
// FRewindDebuggerRecordingExtension
//----------------------------------------------------------------------//
void FRewindDebuggerRecordingExtension::RecordingStarted()
{
	if (!UE_TRACE_CHANNELEXPR_IS_ENABLED(MetaStoryDebugChannel))
	{
		Trace::ToggleChannel(TEXT("MetaStoryDebugChannel"), true);
		MetaStory::Delegates::OnTracingStateChanged.Broadcast(EMetaStoryTraceStatus::TracesStarted);
	}
}

void FRewindDebuggerRecordingExtension::RecordingStopped()
{
	// Shouldn't fire as channel will already be disabled when rewind debugger stops, but just as safeguard
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MetaStoryDebugChannel))
	{
		MetaStory::Delegates::OnTracingStateChanged.Broadcast(EMetaStoryTraceStatus::StoppingTrace);
		Trace::ToggleChannel(TEXT("MetaStoryDebugChannel"), false);
	}
}

void FRewindDebuggerRecordingExtension::Clear()
{
	MetaStory::Delegates::OnTracingStateChanged.Broadcast(EMetaStoryTraceStatus::Cleared);
}

} // UE::MetaStoryDebugger

#endif // WITH_METASTORY_TRACE