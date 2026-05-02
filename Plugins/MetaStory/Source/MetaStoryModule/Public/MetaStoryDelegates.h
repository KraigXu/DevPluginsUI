// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

#define UE_API METASTORYMODULE_API

class UMetaStory;
enum class EMetaStoryTraceStatus : uint8;
enum class EMetaStoryTraceAnalysisStatus : uint8;

namespace UE::MetaStory::Delegates
{

#if WITH_EDITOR

/** Called when linkable name in a MetaStory has changed. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnIdentifierChanged, const UMetaStory& /*MetaStory*/);
extern UE_API FOnIdentifierChanged OnIdentifierChanged;

/**
 * Called when schema of the MetaStory EditorData has changed.
 * This is used to refresh the asset editor.
 * Note that this is NOT called when updating the MetaStory schema from the EditorData on successful compilation.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSchemaChanged, const UMetaStory& /*MetaStory*/);
extern UE_API FOnSchemaChanged OnSchemaChanged;

/**
 * Called when parameters of the MetaStory EditorData changed.
 * This should mainly be used by the asset editor to maintain consistency in the UI for manipulations on the EditorData
 * until the tree gets compiled.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnParametersChanged, const UMetaStory& /*MetaStory*/);
extern UE_API FOnParametersChanged OnParametersChanged;

/**
 * Called when parameters of a MetaStory State changed.
 * This should mainly be used by the asset editor to maintain consistency in the UI for manipulations.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStateParametersChanged, const UMetaStory& /*MetaStory*/, const FGuid /*StateID*/);
extern UE_API FOnStateParametersChanged OnStateParametersChanged;

/**
 * Called when Global Tasks or Evaluators of the MetaStory EditorData changed.
 * This should mainly be used by the asset editor to maintain consistency in the UI for manipulations on the EditorData.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGlobalDataChanged, const UMetaStory& /*MetaStory*/);
extern UE_API FOnGlobalDataChanged OnGlobalDataChanged;

/**
 * Called when the theme colors change.
 * This should mainly be used by the asset editor to maintain consistency in the UI for manipulations on the EditorData.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnVisualThemeChanged, const UMetaStory& /*MetaStory*/);
extern UE_API FOnVisualThemeChanged OnVisualThemeChanged;

/**
 * Called when breakpoints of the MetaStory EditorData changed.
 * This should mainly be used by the asset editor to update the debugger.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBreakpointsChanged, const UMetaStory& /*MetaStory*/);
extern UE_API FOnBreakpointsChanged OnBreakpointsChanged;

/** Called when the MetaStory compiles. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostCompile, const UMetaStory& /*MetaStory*/);
extern UE_API FOnPostCompile OnPostCompile;

/** Request MetaStory compilation. */
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnRequestCompile, UMetaStory& /*MetaStoryToCompile*/);
extern UE_API FOnRequestCompile OnRequestCompile;

/** Request the editor hash of the given MetaStory. */
DECLARE_DELEGATE_RetVal_OneParam(uint32, FOnRequestEditorHash, const UMetaStory& /*MetaStoryToHash*/);
extern UE_API FOnRequestEditorHash OnRequestEditorHash;

#endif // WITH_EDITOR

#if WITH_METASTORY_TRACE

/** Called by the MetaStory module when MetaStory traces are enabled/disabled. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTracingStateChanged, EMetaStoryTraceStatus);
extern UE_API FOnTracingStateChanged OnTracingStateChanged;

#endif // WITH_METASTORY_TRACE

#if WITH_METASTORY_TRACE_DEBUGGER

/** Called by the MetaStory module when MetaStory traces analysis is started/stopped. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTraceAnalysisStateChanged, EMetaStoryTraceAnalysisStatus);
extern UE_API FOnTraceAnalysisStateChanged OnTraceAnalysisStateChanged;

/** Called by the MetaStory module whenever tracing timeline is scrubbed on Rewind Debugger */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTracingTimelineScrubbed, const double /* InScrubTime */);
extern UE_API FOnTracingTimelineScrubbed OnTracingTimelineScrubbed;

#endif // WITH_METASTORY_TRACE_DEBUGGER

}; // UE::MetaStory::Delegates

#undef UE_API
