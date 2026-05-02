// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryDelegates.h"

namespace UE::MetaStory::Delegates
{

#if WITH_EDITOR
FOnIdentifierChanged OnIdentifierChanged;
FOnSchemaChanged OnSchemaChanged;
FOnParametersChanged OnParametersChanged;
FOnGlobalDataChanged OnGlobalDataChanged;
FOnVisualThemeChanged OnVisualThemeChanged;
FOnStateParametersChanged OnStateParametersChanged;
FOnBreakpointsChanged OnBreakpointsChanged;
FOnPostCompile OnPostCompile;
FOnRequestCompile OnRequestCompile;
FOnRequestEditorHash OnRequestEditorHash;
#endif // WITH_EDITOR

#if WITH_METASTORY_TRACE
FOnTracingStateChanged OnTracingStateChanged;
#endif // WITH_METASTORY_TRACE

#if WITH_METASTORY_TRACE_DEBUGGER
FOnTraceAnalysisStateChanged OnTraceAnalysisStateChanged;
FOnTracingTimelineScrubbed OnTracingTimelineScrubbed;
#endif // WITH_METASTORY_TRACE_DEBUGGER

}; // UE::MetaStory::Delegates
