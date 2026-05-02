// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/TVariant.h"
#include "MetaStoryIndexTypes.h"
#include "MetaStoryExecutionTypes.h"
#include "MetaStoryTraceTypes.generated.h"

class UMetaStory;
struct FMetaStoryStateHandle;
enum class EMetaStoryStateSelectionBehavior : uint8;
enum class EMetaStoryRunStatus : uint8;

UENUM()
enum class EMetaStoryTraceStatus : uint8
{
	TracesStarted,
	StoppingTrace,
	TracesStopped,
	Cleared
};

UENUM()
enum class EMetaStoryTraceAnalysisStatus : uint8
{
	Started,
	Stopped,
	Cleared
};

UENUM()
enum class EMetaStoryTraceEventType : uint8
{
	Unset,
	OnEntering				UMETA(DisplayName = "Entering"),
	OnEntered				UMETA(DisplayName = "Entered"),
	OnExiting				UMETA(DisplayName = "Exiting"),
	OnExited				UMETA(DisplayName = "Exited"),
	Push					UMETA(DisplayName = "Push"),
	Pop						UMETA(DisplayName = "Pop"),
	OnStateSelected			UMETA(DisplayName = "Selected"),
	OnStateCompleted		UMETA(DisplayName = "Completed"),
	OnTicking				UMETA(DisplayName = "Tick"),
	OnTaskCompleted			UMETA(DisplayName = "Completed"),
	OnTicked				UMETA(DisplayName = "Ticked"),
	Passed					UMETA(DisplayName = "Passed"),
	Failed					UMETA(DisplayName = "Failed"),
	ForcedSuccess			UMETA(DisplayName = "Forced Success"),
	ForcedFailure			UMETA(DisplayName = "Forced Failure"),
	InternalForcedFailure	UMETA(DisplayName = "Internal Forced Failure"),
	OnRequesting			UMETA(DisplayName = "Requesting"),
	OnEvaluating			UMETA(DisplayName = "Evaluating"),
	OnTransition			UMETA(DisplayName = "Applying Transition"),
	OnTreeStarted			UMETA(DisplayName = "Tree Started"),
	OnTreeStopped			UMETA(DisplayName = "Tree Stopped")
};

#if WITH_METASTORY_TRACE_DEBUGGER

struct FMetaStoryTraceBaseEvent
{
	explicit FMetaStoryTraceBaseEvent(const double RecordingWorldTime, const EMetaStoryTraceEventType EventType)
		: RecordingWorldTime(RecordingWorldTime)
		, EventType(EventType)
	{
	}

	static FString GetDataTypePath()
	{
		return TEXT("");
	}

	static FString GetDataAsText()
	{
		return TEXT("");
	}

	static FString GetDebugText()
	{
		return TEXT("");
	}

	double RecordingWorldTime = 0;
	EMetaStoryTraceEventType EventType;
};

struct FMetaStoryTracePhaseEvent : FMetaStoryTraceBaseEvent
{
	explicit FMetaStoryTracePhaseEvent(const double RecordingWorldTime, const EMetaStoryUpdatePhase Phase, const EMetaStoryTraceEventType EventType, const FMetaStoryStateHandle StateHandle)
		: FMetaStoryTraceBaseEvent(RecordingWorldTime, EventType)
		, Phase(Phase)
		, StateHandle(StateHandle)
	{
	}

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;

	EMetaStoryUpdatePhase Phase;
	FMetaStoryStateHandle StateHandle;
};

struct FMetaStoryTraceLogEvent : FMetaStoryTraceBaseEvent
{
	explicit FMetaStoryTraceLogEvent(const double RecordingWorldTime, const ELogVerbosity::Type Verbosity, const FString& Message)
		: FMetaStoryTraceBaseEvent(RecordingWorldTime, EMetaStoryTraceEventType::Unset)
		, Verbosity(Verbosity)
		, Message(Message)
	{
	}

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;

	ELogVerbosity::Type Verbosity;
	FString Message;
};

struct FMetaStoryTracePropertyEvent : FMetaStoryTraceLogEvent
{
	explicit FMetaStoryTracePropertyEvent(const double RecordingWorldTime, const FString& Message)
		: FMetaStoryTraceLogEvent(RecordingWorldTime, ELogVerbosity::Verbose, Message)
	{
	}

	explicit FMetaStoryTracePropertyEvent(const double RecordingWorldTime, const ELogVerbosity::Type Verbosity, const FString& Message)
		: FMetaStoryTraceLogEvent(RecordingWorldTime, Verbosity, Message)
	{
	}

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;
};

struct FMetaStoryTraceTransitionEvent : FMetaStoryTraceBaseEvent
{
	explicit FMetaStoryTraceTransitionEvent(const double RecordingWorldTime, const FMetaStoryTransitionSource TransitionSource, const EMetaStoryTraceEventType EventType)
		: FMetaStoryTraceBaseEvent(RecordingWorldTime, EventType)
		, TransitionSource(TransitionSource)
	{
	}

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;

	FMetaStoryTransitionSource TransitionSource;
};

struct FMetaStoryTraceNodeEvent : FMetaStoryTraceBaseEvent
{
	explicit FMetaStoryTraceNodeEvent(const double RecordingWorldTime, const FMetaStoryIndex16 Index, const EMetaStoryTraceEventType EventType)
		: FMetaStoryTraceBaseEvent(RecordingWorldTime, EventType)
		, Index(Index)
	{
	}

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;
	
	FMetaStoryIndex16 Index;
};

struct FMetaStoryTraceStateEvent : FMetaStoryTraceNodeEvent
{
	explicit FMetaStoryTraceStateEvent(const double RecordingWorldTime, const FMetaStoryIndex16 Index, const EMetaStoryTraceEventType EventType)
		: FMetaStoryTraceNodeEvent(RecordingWorldTime, Index, EventType)
	{
	}

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FMetaStoryStateHandle GetStateHandle() const;
};

struct FMetaStoryTraceTaskEvent : FMetaStoryTraceNodeEvent
{
	explicit FMetaStoryTraceTaskEvent(const double RecordingWorldTime
									, const FMetaStoryIndex16 Index
									, const EMetaStoryTraceEventType EventType
									, const EMetaStoryRunStatus Status
									, const FString& TypePath
									, const FString& InstanceDataAsText
									, const FString& DebugText)
		: FMetaStoryTraceNodeEvent(RecordingWorldTime, Index, EventType)
		, TypePath(TypePath)
		, InstanceDataAsText(InstanceDataAsText)
		, DebugText(DebugText)
		, Status(Status)
	{
	}

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;

	FString GetDataTypePath() const
	{
		return TypePath;
	}

	FString GetDataAsText() const
	{
		return InstanceDataAsText;
	}

	FString GetDebugText() const
	{
		return DebugText;
	}

	FString TypePath;
	FString InstanceDataAsText;
	FString DebugText;
	EMetaStoryRunStatus Status;
};

struct FMetaStoryTraceEvaluatorEvent : FMetaStoryTraceNodeEvent
{
	explicit FMetaStoryTraceEvaluatorEvent(const double RecordingWorldTime
										, const FMetaStoryIndex16 Index
										, const EMetaStoryTraceEventType EventType
										, const FString& TypePath
										, const FString& InstanceDataAsText
										, const FString& DebugText)
		: FMetaStoryTraceNodeEvent(RecordingWorldTime, Index, EventType)
		, TypePath(TypePath)
		, InstanceDataAsText(InstanceDataAsText)
		, DebugText(DebugText)
	{
	}

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;

	FString GetDataTypePath() const
	{
		return TypePath;
	}

	FString GetDataAsText() const
	{
		return InstanceDataAsText;
	}

	FString GetDebugText() const
	{
		return DebugText;
	}

	FString TypePath;
	FString InstanceDataAsText;
	FString DebugText;
};

struct FMetaStoryTraceConditionEvent : FMetaStoryTraceNodeEvent
{
	explicit FMetaStoryTraceConditionEvent(const double RecordingWorldTime
										, const FMetaStoryIndex16 Index
										, const EMetaStoryTraceEventType EventType
										, const FString& TypePath
										, const FString& InstanceDataAsText
										, const FString& DebugText)
		: FMetaStoryTraceNodeEvent(RecordingWorldTime, Index, EventType)
		, TypePath(TypePath)
		, InstanceDataAsText(InstanceDataAsText)
		, DebugText(DebugText)
	{
	}

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;

	FString GetDataTypePath() const
	{
		return TypePath;
	}

	FString GetDataAsText() const
	{
		return InstanceDataAsText;
	}

	FString GetDebugText() const
	{
		return DebugText;
	}

	FString TypePath;
	FString InstanceDataAsText;
	FString DebugText;
};

struct FMetaStoryTraceActiveStates
{
	struct FAssetActiveStates
	{
		bool operator==(const FAssetActiveStates& Other) const = default;

		TWeakObjectPtr<const UMetaStory> WeakMetaStory;
		TArray<FMetaStoryStateHandle> ActiveStates;
	};

	bool operator==(const FMetaStoryTraceActiveStates& Other) const = default;

	TArray<FAssetActiveStates> PerAssetStates;
};

struct FMetaStoryTraceActiveStatesEvent : FMetaStoryTraceBaseEvent
{
	// Intentionally implemented in source file to compile 'TArray<FMetaStoryStateHandle>' using only forward declaration.
	METASTORYMODULE_API explicit FMetaStoryTraceActiveStatesEvent(const double RecordingWorldTime);

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;

	FMetaStoryTraceActiveStates ActiveStates;
};

struct FMetaStoryTraceInstanceFrameEvent : FMetaStoryTraceBaseEvent
{
	explicit FMetaStoryTraceInstanceFrameEvent(const double RecordingWorldTime, const EMetaStoryTraceEventType EventType, const UMetaStory* MetaStory);

	METASTORYMODULE_API FString ToFullString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetValueString(const UMetaStory& MetaStory) const;
	METASTORYMODULE_API FString GetTypeString(const UMetaStory& MetaStory) const;

	TWeakObjectPtr<const UMetaStory> WeakMetaStory;
};

/** Type aliases for MetaStory trace events */
using FMetaStoryTraceEventVariantType = TVariant<FMetaStoryTracePhaseEvent,
												FMetaStoryTraceLogEvent,
												FMetaStoryTracePropertyEvent,
												FMetaStoryTraceNodeEvent,
												FMetaStoryTraceStateEvent,
												FMetaStoryTraceTaskEvent,
												FMetaStoryTraceEvaluatorEvent,
												FMetaStoryTraceTransitionEvent,
												FMetaStoryTraceConditionEvent,
												FMetaStoryTraceActiveStatesEvent,
												FMetaStoryTraceInstanceFrameEvent>;

#endif // WITH_METASTORY_TRACE_DEBUGGER
