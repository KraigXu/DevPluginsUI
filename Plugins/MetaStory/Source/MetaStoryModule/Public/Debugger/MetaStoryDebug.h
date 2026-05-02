// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryTypes.h"

class UMetaStory;
struct FMetaStoryExecutionContext;
struct FMetaStoryMinimalExecutionContext;
struct FMetaStorySharedEvent;
struct FMetaStoryTransitionSource;
enum class EMetaStoryUpdatePhase : uint8;
enum class EMetaStoryTraceEventType : uint8;

#if WITH_METASTORY_DEBUG

#define UE_STATETREE_DEBUG_CONDITION_ENTER_STATE(InCtx, InStateTree, InNodeIndex) ::UE::MetaStory::Debug::ConditionEnterState(*(InCtx), ::UE::MetaStory::Debug::FNodeReference((InStateTree), FMetaStoryIndex16(InNodeIndex)))
#define UE_STATETREE_DEBUG_CONDITION_TEST_CONDITION(InCtx, InStateTree, InNodeIndex) ::UE::MetaStory::Debug::ConditionTest(*(InCtx), ::UE::MetaStory::Debug::FNodeReference((InStateTree), FMetaStoryIndex16(InNodeIndex)))
#define UE_STATETREE_DEBUG_CONDITION_EXIT_STATE(InCtx, InStateTree, InNodeIndex) ::UE::MetaStory::Debug::ConditionExitState(*(InCtx), ::UE::MetaStory::Debug::FNodeReference((InStateTree), FMetaStoryIndex16(InNodeIndex)))

#define UE_STATETREE_DEBUG_EVALUATOR_ENTER_TREE(InCtx, InStateTree, InNodeIndex) ::UE::MetaStory::Debug::EvaluatorEnterTree(*(InCtx), ::UE::MetaStory::Debug::FNodeReference((InStateTree), FMetaStoryIndex16(InNodeIndex)))
#define UE_STATETREE_DEBUG_EVALUATOR_TICK(InCtx, InStateTree, InNodeIndex) ::UE::MetaStory::Debug::EvaluatorTick(*(InCtx), ::UE::MetaStory::Debug::FNodeReference((InStateTree), FMetaStoryIndex16(InNodeIndex)))
#define UE_STATETREE_DEBUG_EVALUATOR_EXIT_TREE(InCtx, InStateTree, InNodeIndex) ::UE::MetaStory::Debug::EvaluatorExitTree(*(InCtx), ::UE::MetaStory::Debug::FNodeReference((InStateTree), FMetaStoryIndex16(InNodeIndex)))

#define UE_STATETREE_DEBUG_TASK_ENTER_STATE(InCtx, InStateTree, InNodeIndex) ::UE::MetaStory::Debug::TaskEnterState(*(InCtx), ::UE::MetaStory::Debug::FNodeReference((InStateTree), FMetaStoryIndex16(InNodeIndex)))
#define UE_STATETREE_DEBUG_TASK_TICK(InCtx, InStateTree, InNodeIndex) ::UE::MetaStory::Debug::TaskTick(*(InCtx), ::UE::MetaStory::Debug::FNodeReference((InStateTree), FMetaStoryIndex16(InNodeIndex)))
#define UE_STATETREE_DEBUG_TASK_EXIT_STATE(InCtx, InStateTree, InNodeIndex) ::UE::MetaStory::Debug::TaskExitState(*(InCtx), ::UE::MetaStory::Debug::FNodeReference((InStateTree), FMetaStoryIndex16(InNodeIndex)))

#define UE_STATETREE_DEBUG_SEND_EVENT(InCtx, InStateTree, InTag, InPayload, InOrigin) ::UE::MetaStory::Debug::EventSent(*(InCtx), InStateTree, InTag, InPayload, InOrigin)
#define UE_STATETREE_DEBUG_EVENT_CONSUMED(InExecutionContextPtr, Event) ::UE::MetaStory::Debug::EventConsumed(*(InExecutionContextPtr), Event)

#define UE_STATETREE_DEBUG_ENTER_PHASE(InCtx, InPhase) \
		::UE::MetaStory::Debug::EnterPhase(*(InCtx), (InPhase), FMetaStoryStateHandle::Invalid); \
		TRACE_STATETREE_PHASE_EVENT((InCtx), (InPhase), EMetaStoryTraceEventType::Push, FMetaStoryStateHandle::Invalid)

#define UE_STATETREE_DEBUG_EXIT_PHASE(InCtx, InPhase) \
		::UE::MetaStory::Debug::ExitPhase(*(InCtx), (InPhase), FMetaStoryStateHandle::Invalid); \
		TRACE_STATETREE_PHASE_EVENT((InCtx), (InPhase), EMetaStoryTraceEventType::Pop, FMetaStoryStateHandle::Invalid)

#define UE_STATETREE_DEBUG_STATE_EVENT(InCtx, InStateHandle, InEventType) \
		::UE::MetaStory::Debug::StateEvent(*(InCtx), (InStateHandle), (InEventType)); \
		TRACE_STATETREE_STATE_EVENT((InCtx), (InStateHandle), (InEventType))

#define UE_STATETREE_DEBUG_TRANSITION_EVENT(InCtx, InTransitionSource, InEventType) \
		::UE::MetaStory::Debug::TransitionEvent(*(InCtx), (InTransitionSource), (InEventType)); \
		TRACE_STATETREE_TRANSITION_EVENT((InCtx), (InTransitionSource), (InEventType))

#define UE_STATETREE_DEBUG_LOG_EVENT(InCtx, InLogVerbosity, InFormat, ...) TRACE_STATETREE_LOG_EVENT((InCtx), InLogVerbosity, InFormat, ##__VA_ARGS__)
#define UE_STATETREE_DEBUG_CONDITION_EVENT(InCtx, InIndex, InDataView, InEventType) TRACE_STATETREE_CONDITION_EVENT((InCtx), FMetaStoryIndex16(InIndex), (InDataView), (InEventType));
#define UE_STATETREE_DEBUG_INSTANCE_EVENT(InCtx, InEventType) TRACE_STATETREE_INSTANCE_EVENT((InCtx), (InEventType));
#define UE_STATETREE_DEBUG_INSTANCE_FRAME_EVENT(InCtx, InFrame) TRACE_STATETREE_INSTANCE_FRAME_EVENT((InCtx), (InFrame));
#define UE_STATETREE_DEBUG_ACTIVE_STATES_EVENT(InCtx, InActiveFrames) TRACE_STATETREE_ACTIVE_STATES_EVENT((InCtx), InActiveFrames);
#define UE_STATETREE_DEBUG_TASK_EVENT(InCtx, InIndex, InDataView, InEventType, InStatus) TRACE_STATETREE_TASK_EVENT((InCtx), FMetaStoryIndex16(InIndex), (InDataView), (InEventType), (InStatus));
#define UE_STATETREE_DEBUG_EVALUATOR_EVENT(InCtx, InIndex, InDataView, InEventType) TRACE_STATETREE_EVALUATOR_EVENT((InCtx), FMetaStoryIndex16(InIndex), (InDataView), (InEventType));

#define UE_STATETREE_ID_NAME PREPROCESSOR_JOIN(InstanceId,__LINE__) \

/** Scope based macros captures the instance ID since it might not be accessible when exiting the scope */
#define UE_STATETREE_DEBUG_SCOPED_PHASE(InCtx, InPhase) \
		UE_STATETREE_DEBUG_ENTER_PHASE((InCtx), (InPhase)); \
		ON_SCOPE_EXIT \
		{ \
			::UE::MetaStory::Debug::ExitPhase(*(InCtx), (InPhase), FMetaStoryStateHandle::Invalid); \
			TRACE_STATETREE_PHASE_EVENT((InCtx), (InPhase), EMetaStoryTraceEventType::Pop, FMetaStoryStateHandle::Invalid); \
		}

/** Scope based macros captures the instance ID since it might not be accessible when exiting the scope */
#define UE_STATETREE_DEBUG_SCOPED_STATE(InCtx, InStateHandle) \
		UE_STATETREE_DEBUG_STATE_EVENT((InCtx), (InStateHandle), EMetaStoryTraceEventType::Push); \
		ON_SCOPE_EXIT \
		{ \
			::UE::MetaStory::Debug::StateEvent(*(InCtx), (InStateHandle), EMetaStoryTraceEventType::Pop); \
			TRACE_STATETREE_STATE_EVENT((InCtx), (InStateHandle), EMetaStoryTraceEventType::Pop); \
		}

/** Scope based macros captures the instance ID since it might not be accessible when exiting the scope */
#define UE_STATETREE_DEBUG_SCOPED_STATE_PHASE(InCtx, InStateHandle, InPhase) \
		::UE::MetaStory::Debug::EnterPhase(*(InCtx), (InPhase), (InStateHandle)); \
		TRACE_STATETREE_PHASE_EVENT((InCtx), (InPhase), EMetaStoryTraceEventType::Push, (InStateHandle)); \
		ON_SCOPE_EXIT \
		{ \
			::UE::MetaStory::Debug::ExitPhase(*(InCtx), (InPhase), (InStateHandle)); \
			TRACE_STATETREE_PHASE_EVENT((InCtx), (InPhase), EMetaStoryTraceEventType::Pop, (InStateHandle)); \
		}

namespace UE::MetaStory::Debug
{
	struct FNodeReference
	{
		explicit FNodeReference(TNotNull<const UMetaStory*> InStateTree, FMetaStoryIndex16 InNodeIndex);
		TNotNull<const UMetaStory*> MetaStory;
		FMetaStoryIndex16 Index;
	};

	struct FNodeDelegateArgs
	{
		FNodeReference Node;
		FGuid NodeId;
	};

	struct FEventSentDelegateArgs
	{
		TNotNull<const UMetaStory*> MetaStory;
		FGameplayTag Tag;
		FConstStructView Payload;
		FName Origin;
	};

	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FEventSentDelegate, const FMetaStoryMinimalExecutionContext& ExecutionContext, const FEventSentDelegateArgs& EventSentArgs);
	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FEventConsumedDelegate, const FMetaStoryExecutionContext& ExecutionContext, const FMetaStorySharedEvent& Event);
	DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FNodeDelegate, const FMetaStoryExecutionContext&, FNodeDelegateArgs);
	DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FPhaseDelegate, const FMetaStoryExecutionContext&, EMetaStoryUpdatePhase, FMetaStoryStateHandle);
	DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FStateDelegate, const FMetaStoryExecutionContext&, FMetaStoryStateHandle, EMetaStoryTraceEventType);
	DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FTransitionDelegate, const FMetaStoryExecutionContext&, const FMetaStoryTransitionSource&, EMetaStoryTraceEventType);

	void ConditionEnterState(const FMetaStoryExecutionContext& ExecutionContext, FNodeReference Node);
	void ConditionTest(const FMetaStoryExecutionContext& ExecutionContext, FNodeReference Node);
	void ConditionExitState(const FMetaStoryExecutionContext& ExecutionContext, FNodeReference Node);

	void EvaluatorEnterTree(const FMetaStoryExecutionContext& ExecutionContext, FNodeReference Node);
	void EvaluatorTick(const FMetaStoryExecutionContext& ExecutionContext, FNodeReference Node);
	void EvaluatorExitTree(const FMetaStoryExecutionContext& ExecutionContext, FNodeReference Node);

	void TaskEnterState(const FMetaStoryExecutionContext& ExecutionContext, FNodeReference Node);
	void TaskTick(const FMetaStoryExecutionContext& ExecutionContext, FNodeReference Node);
	void TaskExitState(const FMetaStoryExecutionContext& ExecutionContext, FNodeReference Node);

	void EventSent(const FMetaStoryMinimalExecutionContext& ExecutionContext, TNotNull<const UMetaStory*> MetaStory, FGameplayTag Tag, FConstStructView Payload, FName Origin);
	void EventConsumed(const FMetaStoryExecutionContext& ExecutionContext, const FMetaStorySharedEvent& Event);

	void EnterPhase(const FMetaStoryExecutionContext& ExecutionContext, EMetaStoryUpdatePhase Phase, FMetaStoryStateHandle StateHandle);
	void ExitPhase(const FMetaStoryExecutionContext& ExecutionContext, EMetaStoryUpdatePhase Phase, FMetaStoryStateHandle StateHandle);

	void StateEvent(const FMetaStoryExecutionContext& ExecutionContext, FMetaStoryStateHandle StateHandle, EMetaStoryTraceEventType EventType);

	void TransitionEvent(const FMetaStoryExecutionContext& ExecutionContext, const FMetaStoryTransitionSource& TransitionSource, EMetaStoryTraceEventType EventType);

	/**
	 * Debugging callback for when a condition activates.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FNodeDelegate OnConditionEnterState_AnyThread;

	/**
	 * Debugging callback for before a condition is tested.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FNodeDelegate OnTestCondition_AnyThread;

	/**
	 * Debugging callback for when a condition deactivates.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FNodeDelegate OnConditionExitState_AnyThread;

	/**
	 * Debugging callback for when a evaluator activates.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FNodeDelegate OnEvaluatorEnterTree_AnyThread;

	/**
	 * Debugging callback for before an evaluator ticks.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FNodeDelegate OnTickEvaluator_AnyThread;

	/**
	 * Debugging callback for when a evaluator deactivates.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FNodeDelegate OnEvaluatorExitTree_AnyThread;

	/**
	 * Debugging callback for when a task activates.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FNodeDelegate OnTaskEnterState_AnyThread;

	/**
	 * Debugging callback executed before a task ticks.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FNodeDelegate OnTickTask_AnyThread;

	/**
	 * Debugging callback for when a task deactivates.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FNodeDelegate OnTaskExitState_AnyThread;

	/**
	 * Debugging callback for when entering an update phase (global to the tree or specific to a state).
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FPhaseDelegate OnBeginUpdatePhase_AnyThread;

	/**
	 * Debugging callback for when exiting an update phase (global to the tree or specific to a state).
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FPhaseDelegate OnEndUpdatePhase_AnyThread;

	/**
	 * Debugging callback when an action related to a state is executing (e.g., entering, exiting, selecting, etc.).
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FStateDelegate OnStateEvent_AnyThread;

	/**
	 * Debugging callback when an action related to a transition is executing (e.g., requesting, evaluating, etc.).
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FTransitionDelegate OnTransitionEvent_AnyThread;

	/**
	 * Debugging callback for when an event is sent.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FEventSentDelegate OnEventSent_AnyThread;

	/**
	 * Debugging callback for when an event is consumed.
	 * @note The callback executes inside the MetaStory logic.
	 * @note The MetaStory can execute on any thread.
	 */
	METASTORYMODULE_API extern FEventConsumedDelegate OnEventConsumed_AnyThread;
} //UE::MetaStory::Debug

#else

#define UE_STATETREE_DEBUG_CONDITION_ENTER_STATE(...)
#define UE_STATETREE_DEBUG_CONDITION_TEST_CONDITION(...)
#define UE_STATETREE_DEBUG_CONDITION_EXIT_STATE(...)

#define UE_STATETREE_DEBUG_EVALUATOR_ENTER_TREE(...)
#define UE_STATETREE_DEBUG_EVALUATOR_TICK(...)
#define UE_STATETREE_DEBUG_EVALUATOR_EXIT_TREE(...)

#define UE_STATETREE_DEBUG_TASK_ENTER_STATE(...)
#define UE_STATETREE_DEBUG_TASK_TICK(...)
#define UE_STATETREE_DEBUG_TASK_EXIT_STATE(...)

#define UE_STATETREE_DEBUG_SEND_EVENT(...)
#define UE_STATETREE_DEBUG_EVENT_CONSUMED(...)

#define UE_STATETREE_DEBUG_ENTER_PHASE(...)
#define UE_STATETREE_DEBUG_EXIT_PHASE(...)
#define UE_STATETREE_DEBUG_STATE_EVENT(...)
#define UE_STATETREE_DEBUG_TRANSITION_EVENT(...)
#define UE_STATETREE_DEBUG_LOG_EVENT(...)
#define UE_STATETREE_DEBUG_CONDITION_EVENT(...)
#define UE_STATETREE_DEBUG_INSTANCE_EVENT(...)
#define UE_STATETREE_DEBUG_INSTANCE_FRAME_EVENT(...)
#define UE_STATETREE_DEBUG_ACTIVE_STATES_EVENT(...)
#define UE_STATETREE_DEBUG_TASK_EVENT(...)
#define UE_STATETREE_DEBUG_EVALUATOR_EVENT(...)
#define UE_STATETREE_DEBUG_CONDITION_EVENT(...)
#define UE_STATETREE_DEBUG_SCOPED_PHASE(...)
#define UE_STATETREE_DEBUG_SCOPED_STATE(...)
#define UE_STATETREE_DEBUG_SCOPED_STATE_PHASE(...)

#endif // WITH_METASTORY_DEBUG
