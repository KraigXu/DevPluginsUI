// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_METASTORY_TRACE

#include "Trace/Trace.h"
#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"
#include "Containers/StringView.h"
#include "Misc/NotNull.h"

class UMetaStory;
struct FMetaStoryActiveStates;
struct FMetaStoryDataView;
struct FMetaStoryExecutionFrame;
struct FMetaStoryIndex16;
struct FMetaStoryInstanceDebugId;
struct FMetaStoryReadOnlyExecutionContext;
struct FMetaStoryStateHandle;
struct FMetaStoryTransitionSource;
enum class EMetaStoryStateSelectionBehavior : uint8;
enum class EMetaStoryRunStatus : uint8;
enum class EMetaStoryTraceEventType : uint8;
enum class EMetaStoryUpdatePhase : uint8;
namespace ELogVerbosity { enum Type : uint8; }

UE_TRACE_CHANNEL_EXTERN(MetaStoryDebugChannel, METASTORYMODULE_API)

#define UE_API METASTORYMODULE_API

namespace UE::MetaStoryTrace
{

/** Struct allowing MetaStory nodes to add (or replace) a custom string to the trace event */
struct FNodeCustomDebugData
{
	enum class EMergePolicy: uint8
	{
		Unset,
		Append,
		Override
	};

	FNodeCustomDebugData() = default;

	FNodeCustomDebugData(FStringView TraceDebuggerString, const EMergePolicy MergePolicy): TraceDebuggerString(TraceDebuggerString)
	, MergePolicy(MergePolicy)
	{
	}

	FNodeCustomDebugData(FNodeCustomDebugData&& Other)
		: TraceDebuggerString(MoveTemp(Other.TraceDebuggerString))
		, MergePolicy(Other.MergePolicy)
	{
		Other.Reset();
	}

	FNodeCustomDebugData& operator=(FNodeCustomDebugData&& Other)
	{
		if (this == &Other)
		{
			return *this;
		}
		TraceDebuggerString = MoveTemp(Other.TraceDebuggerString);
		MergePolicy = Other.MergePolicy;
		Other.Reset();
		return *this;
	}

	bool IsSet() const
	{
		return MergePolicy != EMergePolicy::Unset && !TraceDebuggerString.IsEmpty();
	}

	void Reset()
	{
		TraceDebuggerString.Reset();
		MergePolicy = EMergePolicy::Unset;
	}

	bool ShouldOverrideDataView() const
	{
		return MergePolicy == EMergePolicy::Override;
	}
	bool ShouldAppendToDataView() const
	{
		return MergePolicy == EMergePolicy::Append;
	}

	FStringView GetTraceDebuggerString() const
	{
		return TraceDebuggerString;
	}

private:
	FString TraceDebuggerString;
	EMergePolicy MergePolicy = EMergePolicy::Unset;
};

void RegisterGlobalDelegates();
void UnregisterGlobalDelegates();
void OutputAssetDebugIdEvent(const UMetaStory* MetaStory, FMetaStoryIndex16 AssetDebugId);
void OutputPhaseScopeEvent(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, EMetaStoryUpdatePhase Phase, EMetaStoryTraceEventType EventType, FMetaStoryStateHandle StateHandle);
void OutputInstanceLifetimeEvent(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, EMetaStoryTraceEventType EventType);
void OutputInstanceAssetEvent(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, const UMetaStory* MetaStory);
void OutputInstanceFrameEvent(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, const FMetaStoryExecutionFrame* Frame);
void OutputLogEventTrace(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, ELogVerbosity::Type Verbosity, const TCHAR* Fmt, ...);
void OutputStateEventTrace(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, FMetaStoryStateHandle StateHandle, EMetaStoryTraceEventType EventType);
void OutputTaskEventTrace(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, FMetaStoryIndex16 TaskIdx, FMetaStoryDataView DataView, EMetaStoryTraceEventType EventType, EMetaStoryRunStatus Status);
void OutputEvaluatorEventTrace(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, FMetaStoryIndex16 EvaluatorIdx, FMetaStoryDataView DataView, EMetaStoryTraceEventType EventType);
void OutputConditionEventTrace(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, FMetaStoryIndex16 ConditionIdx, FMetaStoryDataView DataView, EMetaStoryTraceEventType EventType);
void OutputTransitionEventTrace(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, FMetaStoryTransitionSource TransitionSource, EMetaStoryTraceEventType EventType);
void OutputActiveStatesEventTrace(TNotNull<const FMetaStoryReadOnlyExecutionContext*> Context, TConstArrayView<FMetaStoryExecutionFrame> ActiveFrames);

UE_DEPRECATED(5.6, "This method will no longer be exposed publicly.")
FMetaStoryIndex16 FindOrAddDebugIdForAsset(const UMetaStory* MetaStory);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputPhaseScopeEvent(FMetaStoryInstanceDebugId InstanceId, EMetaStoryUpdatePhase Phase, EMetaStoryTraceEventType EventType, FMetaStoryStateHandle StateHandle);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputInstanceLifetimeEvent(FMetaStoryInstanceDebugId InstanceId, const UMetaStory* MetaStory, const TCHAR* InstanceName, EMetaStoryTraceEventType EventType);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputInstanceAssetEvent(FMetaStoryInstanceDebugId InstanceId, const UMetaStory* MetaStory);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputInstanceFrameEvent(FMetaStoryInstanceDebugId InstanceId, const FMetaStoryExecutionFrame* Frame);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputLogEventTrace(FMetaStoryInstanceDebugId InstanceId, ELogVerbosity::Type Verbosity, const TCHAR* Fmt, ...);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputStateEventTrace(FMetaStoryInstanceDebugId InstanceId, FMetaStoryStateHandle StateHandle, EMetaStoryTraceEventType EventType);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputTaskEventTrace(FMetaStoryInstanceDebugId InstanceId, FNodeCustomDebugData&& DebugData, FMetaStoryIndex16 TaskIdx, FMetaStoryDataView DataView, EMetaStoryTraceEventType EventType, EMetaStoryRunStatus Status);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputEvaluatorEventTrace(FMetaStoryInstanceDebugId InstanceId, FNodeCustomDebugData&& DebugData, FMetaStoryIndex16 EvaluatorIdx, FMetaStoryDataView DataView, EMetaStoryTraceEventType EventType);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputConditionEventTrace(FMetaStoryInstanceDebugId InstanceId, FNodeCustomDebugData&& DebugData, FMetaStoryIndex16 ConditionIdx, FMetaStoryDataView DataView, EMetaStoryTraceEventType EventType);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputTransitionEventTrace(FMetaStoryInstanceDebugId InstanceId, FMetaStoryTransitionSource TransitionSource, EMetaStoryTraceEventType EventType);
UE_DEPRECATED(5.7, "Use the overload taking a pointer to the execution context instead.")
void OutputActiveStatesEventTrace(FMetaStoryInstanceDebugId InstanceId, TConstArrayView<FMetaStoryExecutionFrame> ActiveFrames);

} // UE::MetaStoryTrace

#define TRACE_METASTORY_INSTANCE_EVENT(Context, EventType) \
	UE::MetaStoryTrace::OutputInstanceLifetimeEvent(Context, EventType);

#define TRACE_METASTORY_INSTANCE_FRAME_EVENT(Context, Frame) \
	UE::MetaStoryTrace::OutputInstanceFrameEvent(Context, Frame);

#define TRACE_METASTORY_PHASE_EVENT(Context, Phase, EventType, StateHandle) \
	UE::MetaStoryTrace::OutputPhaseScopeEvent(Context, Phase, EventType, StateHandle); \

#define TRACE_METASTORY_LOG_EVENT(Context, TraceVerbosity, Format, ...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MetaStoryDebugChannel)) \
	{ \
		UE::MetaStoryTrace::OutputLogEventTrace(Context, ELogVerbosity::TraceVerbosity, Format, ##__VA_ARGS__); \
	}

#define TRACE_METASTORY_STATE_EVENT(Context, StateHandle, EventType) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MetaStoryDebugChannel)) \
	{ \
		UE::MetaStoryTrace::OutputStateEventTrace(Context, StateHandle, EventType); \
	}

#define TRACE_METASTORY_TASK_EVENT(Context, TaskIdx, DataView, EventType, Status) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MetaStoryDebugChannel)) \
	{ \
		UE::MetaStoryTrace::OutputTaskEventTrace(Context, TaskIdx, DataView, EventType, Status); \
	}

#define TRACE_METASTORY_EVALUATOR_EVENT(Context, EvaluatorIdx, DataView, EventType) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MetaStoryDebugChannel)) \
	{ \
		UE::MetaStoryTrace::OutputEvaluatorEventTrace(Context, EvaluatorIdx, DataView, EventType); \
	}

#define TRACE_METASTORY_CONDITION_EVENT(Context, ConditionIdx, DataView, EventType) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MetaStoryDebugChannel)) \
	{ \
		UE::MetaStoryTrace::OutputConditionEventTrace(Context, ConditionIdx, DataView, EventType); \
	}

#define TRACE_METASTORY_TRANSITION_EVENT(Context, TransitionIdx, EventType) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MetaStoryDebugChannel)) \
	{ \
		UE::MetaStoryTrace::OutputTransitionEventTrace(Context, TransitionIdx, EventType); \
	}

#define TRACE_METASTORY_ACTIVE_STATES_EVENT(Context, ActivateFrames) \
		UE::MetaStoryTrace::OutputActiveStatesEventTrace(Context, ActivateFrames);

#undef UE_API

#else //WITH_METASTORY_TRACE

#define TRACE_METASTORY_INSTANCE_EVENT(...)
#define TRACE_METASTORY_INSTANCE_FRAME_EVENT(...)
#define TRACE_METASTORY_PHASE_EVENT(...)
#define TRACE_METASTORY_LOG_EVENT(...)
#define TRACE_METASTORY_STATE_EVENT(...)
#define TRACE_METASTORY_TASK_EVENT(...)
#define TRACE_METASTORY_EVALUATOR_EVENT(...)
#define TRACE_METASTORY_CONDITION_EVENT(...)
#define TRACE_METASTORY_TRANSITION_EVENT(...)
#define TRACE_METASTORY_ACTIVE_STATES_EVENT(...)

#endif // WITH_METASTORY_TRACE
