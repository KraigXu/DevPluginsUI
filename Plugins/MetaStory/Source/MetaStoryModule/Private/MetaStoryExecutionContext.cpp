// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryExecutionContext.h"
#include "MetaStoryAsyncExecutionContext.h"
#include "MetaStoryTaskBase.h"
#include "MetaStoryEvaluatorBase.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryConsiderationBase.h"
#include "MetaStoryDelegate.h"
#include "MetaStoryInstanceDataHelpers.h"
#include "MetaStoryPropertyFunctionBase.h"
#include "MetaStoryReference.h"
#include "AutoRTFM.h"
#include "ObjectTrace.h"
#include "Containers/StaticArray.h"
#include "CrashReporter/MetaStoryCrashReporterHandler.h"
#include "Debugger/MetaStoryDebug.h"
#include "Debugger/MetaStoryTrace.h"
#include "Debugger/MetaStoryTraceTypes.h"
#include "Misc/ScopeExit.h"
#include "VisualLogger/VisualLogger.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Logging/LogScopedVerbosityOverride.h"

#define METASTORY_LOG(Verbosity, Format, ...) UE_VLOG_UELOG(GetOwner(), LogMetaStory, Verbosity, TEXT("%s: ") Format, *GetInstanceDescriptionInternal(), ##__VA_ARGS__)
#define METASTORY_CLOG(Condition, Verbosity, Format, ...) UE_CVLOG_UELOG((Condition), GetOwner(), LogMetaStory, Verbosity, TEXT("%s: ") Format, *GetInstanceDescriptionInternal(), ##__VA_ARGS__)

namespace UE::MetaStory::Debug
{
	// Debug printing indent for hierarchical data.
	constexpr int32 IndentSize = 2;
};

namespace UE::MetaStory::ExecutionContext::Private
{
	bool bCopyBoundPropertiesOnNonTickedTask = false;
	FAutoConsoleVariableRef CVarCopyBoundPropertiesOnNonTickedTask(
		TEXT("MetaStory.CopyBoundPropertiesOnNonTickedTask"),
		bCopyBoundPropertiesOnNonTickedTask,
		TEXT("When ticking the tasks, copy the bindings when the tasks are not ticked because it disabled ticking or completed. The bindings are not updated on failure.")
	);

	bool bTickGlobalNodesFollowingTreeHierarchy = true;
	FAutoConsoleVariableRef CVarTickGlobalNodesFollowingTreeHierarchy(
		TEXT("MetaStory.TickGlobalNodesFollowingTreeHierarchy"),
		bTickGlobalNodesFollowingTreeHierarchy,
		TEXT("If true, then the global evaluators and global tasks are ticked following the asset hierarchy.\n")
		TEXT("The order is (1)root evaluators, (2)root global tasks, (3)state tasks, (4)linked asset evaluators, (5)linked asset global tasks, (6)linked asset state tasks.\n")
		TEXT("If false (5.5 behavior), then all the global nodes, from all linked assets, are evaluated, then the state tasks are ticked.\n")
		TEXT("You should upgrade your asset. This option is to help migrate to the new behavior.")
	);

	bool bGlobalTasksCompleteOwningFrame = true;
	FAutoConsoleVariableRef CVarGlobalTasksCompleteFrame(
		TEXT("MetaStory.GlobalTasksCompleteOwningFrame"),
		bGlobalTasksCompleteOwningFrame,
		TEXT("If true, the global tasks complete the tree they are part of. The root or the linked state. 5.6 behavior.\n")
		TEXT("If false, the global tasks complete the root tree execution (even for linked assets). 5.5 behavior.")
		TEXT("You should upgrade your asset. This option is to help migrate to the new behavior.")
	);

	bool bSetDeprecatedTransitionResultProperties = false;
	FAutoConsoleVariableRef CVarSetDeprecatedTransitionResultProperties(
		TEXT("MetaStory.SetDeprecatedTransitionResultProperties"),
		bSetDeprecatedTransitionResultProperties,
		TEXT("If true, set the FMetaStoryTransitionResult deprecated properties when a transition occurs.\n")
		TEXT("The information is not relevant and not valid but can be needed for backward compatibility.")
	);

	bool bTargetStateRequiresTheSameEventForStateSelectionAsTheRequestedTransition = false;
	FAutoConsoleVariableRef CVarTargetStateRequiresTheSameEventForStateSelectionAsTheRequestedTransition(
		TEXT("MetaStory.TargetStateRequiresTheSameEventForStateSelectionAsTheRequestedTransition"),
		bTargetStateRequiresTheSameEventForStateSelectionAsTheRequestedTransition,
		TEXT("If true, when a transition is triggered by an event that occurs and the target state requires an event, \n")
		TEXT("then the state required event must match the transition required event.")
	);

	const FLazyName Name_Tick = "Tick";
	const FLazyName Name_Start = "Start";
	const FLazyName Name_Stop = "Stop";

	constexpr uint32 NumEMetaStoryRunStatus()
	{
		// Keep compile-time guard semantics without relying on generated enum helper macro names.
		return 5;
	}

	constexpr uint32 NumEMetaStoryFinishTaskType()
	{
		// Keep compile-time guard semantics without relying on generated enum helper macro names.
		return 2;
	}

	template<typename FrameType>
	FrameType* FindExecutionFrame(FActiveFrameID FrameID, const TArrayView<FrameType> Frames, const TArrayView<FrameType> TemporaryFrames)
	{
		FrameType* Frame = Frames.FindByPredicate(
			[FrameID](const FMetaStoryExecutionFrame& Frame)
			{
				return Frame.FrameID == FrameID;
			});
		if (Frame)
		{
			return Frame;
		}

		return TemporaryFrames.FindByPredicate(
			[FrameID](const FMetaStoryExecutionFrame& Frame)
			{
				return Frame.FrameID == FrameID;
			});
	}

	const FMetaStoryCompactFrame* FindMetaStoryFrame(const FMetaStoryExecutionFrameHandle& FrameHandle)
	{
		check(FrameHandle.IsValid());
		const FMetaStoryCompactFrame* FrameInfo = FrameHandle.GetMetaStory()->GetFrameFromHandle(FrameHandle.GetRootState());
		ensureAlwaysMsgf(FrameInfo, TEXT("The compiled data is invalid. It should contains the information for the new root frame."));
		return FrameInfo;
	}

	// From metric, paths are usually of length < 8. We put a large enough number to not have to worry about complex assets.
	static constexpr int32 ExpectedActiveStatePathLength = 24;
	using FActiveStateInlineArray = TArray<FActiveState, TInlineAllocator<ExpectedActiveStatePathLength, FNonconcurrentLinearArrayAllocator>>;
	void GetActiveStatePath(TArrayView<const FMetaStoryExecutionFrame> Frames, FActiveStateInlineArray& OutResult)
	{
		if (Frames.Num() == 0 || Frames[0].MetaStory == nullptr)
		{
			return;
		}

		for (const FMetaStoryExecutionFrame& Frame : Frames)
		{
			for (int32 StateIndex = 0; StateIndex < Frame.ActiveStates.Num(); ++StateIndex)
			{
				OutResult.Emplace(Frame.FrameID, Frame.ActiveStates.StateIDs[StateIndex], Frame.ActiveStates.States[StateIndex]);
			}
		}
	}

	using FStateHandleContextInlineArray = TArray<FStateHandleContext, TInlineAllocator<FMetaStoryActiveStates::MaxStates>>;
	bool GetStatesListToState(FStateHandleContext State, FStateHandleContextInlineArray& OutStates)
	{
		// Walk towards the root from the state.
		FStateHandleContext CurrentState = State;
		while (CurrentState.MetaStory && CurrentState.StateHandle.IsValid())
		{
			if (OutStates.Num() == FMetaStoryActiveStates::MaxStates)
			{
				return false;
			}
			// Store the states that are in between the 'NextState' and the common ancestor.
			OutStates.Add(CurrentState);
			CurrentState = FStateHandleContext(CurrentState.MetaStory, CurrentState.MetaStory->GetStates()[CurrentState.StateHandle.Index].Parent);
		}
		Algo::Reverse(OutStates);
		return true;
	}

	static constexpr int32 ExpectedAmountTransitionEvent = 8;
	using FSharedEventInlineArray = TArray<FMetaStorySharedEvent, TInlineAllocator<ExpectedAmountTransitionEvent, FNonconcurrentLinearArrayAllocator>>;
	void GetTriggerTransitionEvent(const FMetaStoryCompactStateTransition& Transition, FMetaStoryInstanceStorage& Storage, const FMetaStorySharedEvent& TransitionEvent, const TArrayView<const FMetaStorySharedEvent> EventsToProcess, FSharedEventInlineArray& OutTransitionEvents)
	{
		if (Transition.Trigger == EMetaStoryTransitionTrigger::OnEvent)
		{
			check(Transition.RequiredEvent.IsValid());

			if (TransitionEvent.IsValid())
			{
				OutTransitionEvents.Add(TransitionEvent);
			}
			else
			{
				for (const FMetaStorySharedEvent& Event : EventsToProcess)
				{
					check(Event.IsValid());
					if (Transition.RequiredEvent.DoesEventMatchDesc(*Event))
					{
						OutTransitionEvents.Emplace(Event);
					}
				}
			}
		}
		else if (EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnTick))
		{
			// Dummy event to make sure we iterate the transition loop once.
			OutTransitionEvents.Emplace(FMetaStorySharedEvent());
		}
		else if (EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnDelegate))
		{
			if (Storage.IsDelegateBroadcasted(Transition.RequiredDelegateDispatcher))
			{
				// Dummy event to make sure we iterate the transition loop once.
				OutTransitionEvents.Emplace(FMetaStorySharedEvent());
			}
		}
		else
		{
			ensureMsgf(false, TEXT("The trigger type is not supported."));
		}
	}

	FString GetStatePathAsString(TNotNull<const UMetaStory*> MetaStory, const TArrayView<const UE::MetaStory::FActiveState>& Path)
	{
		TValueOrError<FString, void> Result = UE::MetaStory::FActiveStatePath::Describe(MetaStory, Path);
		if (Result.HasValue())
		{
			return Result.StealValue();
		}
		return FString();
	}

	void CleanFrame(FMetaStoryExecutionState& Exec, FActiveFrameID FrameID)
	{
		Exec.DelegateActiveListeners.RemoveAll(FrameID);
	}

	void CleanState(FMetaStoryExecutionState& Exec, FActiveStateID StateID)
	{
		Exec.DelegateActiveListeners.RemoveAll(StateID);
		Exec.DelayedTransitions.RemoveAll([StateID](const FMetaStoryTransitionDelayedState& DelayedState)
			{
				return DelayedState.StateID == StateID;
			});
	}

	void InitEvaluationScopeInstanceData(UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer& Container, TNotNull<const UMetaStory*> MetaStory, const int32 NodesBegin, const int32 NodesEnd)
	{
		for (int32 NodeIndex = NodesBegin; NodeIndex != NodesEnd; NodeIndex++)
		{
			const FMetaStoryNodeBase& Node = MetaStory->GetNodes()[NodeIndex].Get<const FMetaStoryNodeBase>();
			if (Node.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceData
				|| Node.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject)
			{
				Container.Add(Node.InstanceDataHandle, MetaStory->GetDefaultEvaluationScopeInstanceData().GetStruct(Node.InstanceTemplateIndex.Get()));
			}
		}
	}
} // namespace UE::MetaStory::ExecutionContext::Private

namespace UE::MetaStory::ExecutionContext
{
	bool MarkDelegateAsBroadcasted(FMetaStoryDelegateDispatcher Dispatcher, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryInstanceStorage& Storage)
	{
		const UMetaStory* MetaStory = CurrentFrame.MetaStory;
		check(MetaStory);

		for (FMetaStoryStateHandle ActiveState : CurrentFrame.ActiveStates)
		{
			const FMetaStoryCompactState* State = MetaStory->GetStateFromHandle(ActiveState);
			check(State);

			if (!State->bHasDelegateTriggerTransitions)
			{
				continue;
			}

			const int32 TransitionEnd = State->TransitionsBegin + State->TransitionsNum;
			for (int32 TransitionIndex = State->TransitionsBegin; TransitionIndex < TransitionEnd; ++TransitionIndex)
			{
				const FMetaStoryCompactStateTransition* Transition = MetaStory->GetTransitionFromIndex(FMetaStoryIndex16(TransitionIndex));
				check(Transition);
				if (Transition->RequiredDelegateDispatcher == Dispatcher)
				{
					ensureMsgf(EnumHasAnyFlags(Transition->Trigger, EMetaStoryTransitionTrigger::OnDelegate), TEXT("The transition should have both (a valid dispatcher and the OnDelegate flag) or none."));
					Storage.MarkDelegateAsBroadcasted(Dispatcher);
					return true;
				}
			}
		}

		return false;
	}

	/** @return in order {Failed, Succeeded, Stopped, Running, Unset} */
	EMetaStoryRunStatus GetPriorityRunStatus(EMetaStoryRunStatus A, EMetaStoryRunStatus B)
	{
		static_assert((int32)EMetaStoryRunStatus::Running == 0);
		static_assert((int32)EMetaStoryRunStatus::Stopped == 1);
		static_assert((int32)EMetaStoryRunStatus::Succeeded == 2);
		static_assert((int32)EMetaStoryRunStatus::Failed == 3);
		static_assert((int32)EMetaStoryRunStatus::Unset == 4);
		static_assert(Private::NumEMetaStoryRunStatus() == 5, "The number of entries in EMetaStoryRunStatus changed. Update GetPriorityRunStatus.");

		static constexpr int32 PriorityMatrix[] = { 1, 2, 3, 4, 0 };
		return PriorityMatrix[(uint8)A] > PriorityMatrix[(uint8)B] ? A : B;
	}

	UE::MetaStory::ETaskCompletionStatus CastToTaskStatus(EMetaStoryFinishTaskType FinishTask)
	{
		static_assert(Private::NumEMetaStoryFinishTaskType() == 2, "The number of entries in EMetaStoryFinishTaskType changed. Update CastToTaskStatus.");

		return FinishTask == EMetaStoryFinishTaskType::Succeeded ? UE::MetaStory::ETaskCompletionStatus::Succeeded : UE::MetaStory::ETaskCompletionStatus::Failed;
	}

	EMetaStoryRunStatus CastToRunStatus(EMetaStoryFinishTaskType FinishTask)
	{
		static_assert(Private::NumEMetaStoryFinishTaskType() == 2, "The number of entries in EMetaStoryFinishTaskType changed. Update CastToRunStatus.");

		return FinishTask == EMetaStoryFinishTaskType::Succeeded ? EMetaStoryRunStatus::Succeeded : EMetaStoryRunStatus::Failed;
	}

	UE::MetaStory::ETaskCompletionStatus CastToTaskStatus(EMetaStoryRunStatus InStatus)
	{
		static_assert((int32)EMetaStoryRunStatus::Running == (int32)UE::MetaStory::ETaskCompletionStatus::Running);
		static_assert((int32)EMetaStoryRunStatus::Stopped == (int32)UE::MetaStory::ETaskCompletionStatus::Stopped);
		static_assert((int32)EMetaStoryRunStatus::Succeeded == (int32)UE::MetaStory::ETaskCompletionStatus::Succeeded);
		static_assert((int32)EMetaStoryRunStatus::Failed == (int32)UE::MetaStory::ETaskCompletionStatus::Failed);
		static_assert(Private::NumEMetaStoryRunStatus() == 5, "The number of entries in EMetaStoryRunStatus changed. Update CastToTaskStatus.");

		return InStatus != EMetaStoryRunStatus::Unset ? (UE::MetaStory::ETaskCompletionStatus)InStatus : UE::MetaStory::ETaskCompletionStatus::Running;
	}

	EMetaStoryRunStatus CastToRunStatus(UE::MetaStory::ETaskCompletionStatus InStatus)
	{
		static_assert((int32)EMetaStoryRunStatus::Running == (int32)UE::MetaStory::ETaskCompletionStatus::Running);
		static_assert((int32)EMetaStoryRunStatus::Stopped == (int32)UE::MetaStory::ETaskCompletionStatus::Stopped);
		static_assert((int32)EMetaStoryRunStatus::Succeeded == (int32)UE::MetaStory::ETaskCompletionStatus::Succeeded);
		static_assert((int32)EMetaStoryRunStatus::Failed == (int32)UE::MetaStory::ETaskCompletionStatus::Failed);
		static_assert(UE::MetaStory::NumberOfTaskStatus == 4, "The number of entries in EMetaStoryRunStatus changed. Update CastToRunStatus.");

		return (EMetaStoryRunStatus)InStatus;
	}
} // namespace UE::MetaStory::ExecutionContext

/**
 * FMetaStoryReadOnlyExecutionContext implementation
 */
FMetaStoryReadOnlyExecutionContext::FMetaStoryReadOnlyExecutionContext(TNotNull<UObject*> InOwner, TNotNull<const UMetaStory*> InMetaStory, FMetaStoryInstanceData& InInstanceData)
	: FMetaStoryReadOnlyExecutionContext(InOwner, InMetaStory, InInstanceData.GetMutableStorage())
{
}

FMetaStoryReadOnlyExecutionContext::FMetaStoryReadOnlyExecutionContext(TNotNull<UObject*> InOwner, TNotNull<const UMetaStory*> InMetaStory, FMetaStoryInstanceStorage& InStorage)
	: Owner(*InOwner)
	, RootMetaStory(*InMetaStory)
	, Storage(InStorage)
{
	Storage.AcquireReadAccess();

	if (IsValid())
	{
		constexpr bool bWriteAccessAcquired = false;
		Storage.GetRuntimeValidation().SetContext(&Owner, &RootMetaStory, bWriteAccessAcquired);
	}
}

FMetaStoryReadOnlyExecutionContext::~FMetaStoryReadOnlyExecutionContext()
{
	Storage.ReleaseReadAccess();
}

FMetaStoryScheduledTick FMetaStoryReadOnlyExecutionContext::GetNextScheduledTick() const
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return FMetaStoryScheduledTick::MakeSleep();
	}

	const FMetaStoryExecutionState& Exec = Storage.GetExecutionState();
	if (Exec.TreeRunStatus != EMetaStoryRunStatus::Running)
	{
		return FMetaStoryScheduledTick::MakeSleep();
	}

	// USchema::IsScheduleTickAllowed.
	//Used the MetaStory cached value to prevent runtime changes that could affect the behavior.
	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
	{
		const FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		if (!CurrentFrame.MetaStory->IsScheduledTickAllowed())
		{
			return FMetaStoryScheduledTick::MakeEveryFrames(UE::MetaStory::EMetaStoryTickReason::Forced);
		}
	}

	const FMetaStoryEventQueue& EventQueue = Storage.GetEventQueue();
	const bool bHasEvents = EventQueue.HasEvents();
	const bool bHasBroadcastedDelegates = Storage.HasBroadcastedDelegates();

	struct FCustomTickRate
	{
		FCustomTickRate(float InDeltaTime, UE::MetaStory::EMetaStoryTickReason InReason)
			: DeltaTime(InDeltaTime)
			, Reason(InReason)
		{}

		bool operator<(const FCustomTickRate& Other) const
		{
			return DeltaTime < Other.DeltaTime;
		}

		float DeltaTime = 0.0f;
		UE::MetaStory::EMetaStoryTickReason Reason = UE::MetaStory::EMetaStoryTickReason::None;
	};


	TOptional<FCustomTickRate> CustomTickRate;

	// We wish to return in order: EveryFrames, then NextFrame, then CustomTickRate, then Sleep.
	// Do we have a state that requires a tick or is waiting for an event.
	{
		bool bHasTaskWithEveryFramesTick = false;
		UE::MetaStory::EMetaStoryTickReason TaskTickingReason = UE::MetaStory::EMetaStoryTickReason::None;
		for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
		{
			const FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
			const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;

			// Test global tasks.
			if (CurrentMetaStory->DoesRequestTickGlobalTasks(bHasEvents))
			{
				bHasTaskWithEveryFramesTick = true;
			}

			// Test active states tasks.
			for (int32 StateIndex = 0; StateIndex < CurrentFrame.ActiveStates.Num(); ++StateIndex)
			{
				const FMetaStoryStateHandle CurrentHandle = CurrentFrame.ActiveStates[StateIndex];
				const FMetaStoryCompactState& State = CurrentMetaStory->States[CurrentHandle.Index];
				checkf(State.bEnabled, TEXT("Only enabled states are in ActiveStates."));
				if (State.bHasCustomTickRate)
				{
					if (!CustomTickRate.IsSet() || CustomTickRate.GetValue().DeltaTime > State.CustomTickRate)
					{
						CustomTickRate = {State.CustomTickRate, UE::MetaStory::EMetaStoryTickReason::StateCustomTickRate};
					}
				}
				else if (!CustomTickRate.IsSet())
				{
					if (State.DoesRequestTickTasks(bHasEvents))
					{
						bHasTaskWithEveryFramesTick = true;
						TaskTickingReason = UE::MetaStory::EMetaStoryTickReason::TaskTicking;
					}
					else if (State.ShouldTickTransitions(bHasEvents, bHasBroadcastedDelegates))
					{
						// todo: ShouldTickTransitions has onevent or ontick. both can be already triggered and we are waiting for the delay
						bHasTaskWithEveryFramesTick = true;
						TaskTickingReason = UE::MetaStory::EMetaStoryTickReason::TransitionTicking;
					}
				}
			}
		}

		if (!CustomTickRate.IsSet() && bHasTaskWithEveryFramesTick)
		{
			return FMetaStoryScheduledTick::MakeEveryFrames(TaskTickingReason);
		}

		// If one state has a custom tick rate, then it overrides the tick rate for all states.
		//Only return the CustomTickRate if it's > than NextFrame, the custom tick rate will be processed at the end.
		if (const FCustomTickRate* CustomTickRateValue = CustomTickRate.GetPtrOrNull())
		{
			if (CustomTickRateValue->DeltaTime <= 0.0f)
			{
				// A state might override the custom tick rate with > 0, then another state overrides it again with 0 to tick back every frame.
				return FMetaStoryScheduledTick::MakeEveryFrames(CustomTickRateValue->Reason);
			}
		}
	}

	// Requests
	if (Exec.HasScheduledTickRequests())
	{
		// The ScheduledTickRequests loop value is cached. Returns every frame or next frame. CustomTime needs to wait after the other tests.
		const FMetaStoryScheduledTick ScheduledTickRequest = Exec.GetScheduledTickRequest();
		if (ScheduledTickRequest.ShouldTickEveryFrames() || ScheduledTickRequest.ShouldTickOnceNextFrame())
		{
			return ScheduledTickRequest;
		}
		const FCustomTickRate CachedTickRate(ScheduledTickRequest.GetTickRate(), ScheduledTickRequest.GetReason());

		CustomTickRate = CustomTickRate.IsSet() ? FMath::Min(CustomTickRate.GetValue(), CachedTickRate) : CachedTickRate;
	}

	// Transitions
	if (Storage.GetTransitionRequests().Num() > 0)
	{
		return FMetaStoryScheduledTick::MakeNextFrame(UE::MetaStory::EMetaStoryTickReason::TransitionRequest);
	}

	// Events are cleared every tick.
	if (bHasEvents && Storage.IsOwningEventQueue())
	{
		return FMetaStoryScheduledTick::MakeNextFrame(UE::MetaStory::EMetaStoryTickReason::Event);
	}

	// Completed task. For EnterState or for user that only called TickTasks and not TickTransitions.
	if (Exec.bHasPendingCompletedState)
	{
		return FMetaStoryScheduledTick::MakeNextFrame(UE::MetaStory::EMetaStoryTickReason::CompletedState);
	}

	// Min of all delayed transitions.
	if (Exec.DelayedTransitions.Num() > 0)
	{
		for (const FMetaStoryTransitionDelayedState& Transition : Exec.DelayedTransitions)
		{
			const FCustomTickRate TransitionTickRate(Transition.TimeLeft, UE::MetaStory::EMetaStoryTickReason::DelayedTransition);
			CustomTickRate = CustomTickRate.IsSet() ? FMath::Min(CustomTickRate.GetValue(), TransitionTickRate) : TransitionTickRate;
		}
	}

	// Custom tick rate for tasks and transitions.
	if (const FCustomTickRate* CustomTickRateValue = CustomTickRate.GetPtrOrNull())
	{
		return FMetaStoryScheduledTick::MakeCustomTickRate(CustomTickRateValue->DeltaTime, CustomTickRateValue->Reason);
	}

	return FMetaStoryScheduledTick::MakeSleep();
}

EMetaStoryRunStatus FMetaStoryReadOnlyExecutionContext::GetMetaStoryRunStatus() const
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return EMetaStoryRunStatus::Failed;
	}

	return Storage.GetExecutionState().TreeRunStatus;
}

EMetaStoryRunStatus FMetaStoryReadOnlyExecutionContext::GetLastTickStatus() const
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return EMetaStoryRunStatus::Failed;
	}

	const FMetaStoryExecutionState& Exec = Storage.GetExecutionState();
	return Exec.LastTickStatus;
}

TConstArrayView<FMetaStoryExecutionFrame> FMetaStoryReadOnlyExecutionContext::GetActiveFrames() const
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return TConstArrayView<FMetaStoryExecutionFrame>();
	}

	const FMetaStoryExecutionState& Exec = Storage.GetExecutionState();
	return Exec.ActiveFrames;
}

FString FMetaStoryReadOnlyExecutionContext::GetActiveStateName() const
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return FString();
	}

	const FMetaStoryExecutionState& Exec = Storage.GetExecutionState();

	TStringBuilder<1024> FullStateName;

	const UMetaStory* LastMetaStory = &RootMetaStory;
	int32 Indent = 0;

	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); FrameIndex++)
	{
		const FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;

		// Append linked state marker at the end of the previous line.
		if (Indent > 0)
		{
			FullStateName << TEXT(" >");
		}
		// If tree has changed, append that too.
		if (CurrentFrame.MetaStory != LastMetaStory)
		{
			FullStateName << TEXT(" [");
			FullStateName << CurrentFrame.MetaStory.GetFName();
			FullStateName << TEXT(']');

			LastMetaStory = CurrentFrame.MetaStory;
		}

		for (int32 Index = 0; Index < CurrentFrame.ActiveStates.Num(); Index++)
		{
			const FMetaStoryStateHandle Handle = CurrentFrame.ActiveStates[Index];
			if (Handle.IsValid())
			{
				const FMetaStoryCompactState& State = CurrentMetaStory->States[Handle.Index];
				if (Indent > 0)
				{
					FullStateName += TEXT("\n");
				}
				FullStateName.Appendf(TEXT("%*s-"), Indent * 3, TEXT("")); // Indent
				FullStateName << State.Name;
				Indent++;
			}
		}
	}

	switch (Exec.TreeRunStatus)
	{
	case EMetaStoryRunStatus::Failed:
		FullStateName << TEXT(" FAILED\n");
		break;
	case EMetaStoryRunStatus::Succeeded:
		FullStateName << TEXT(" SUCCEEDED\n");
		break;
	case EMetaStoryRunStatus::Running:
		// Empty
		break;
	default:
		FullStateName << TEXT("--\n");
	}

	return FullStateName.ToString();
}

TArray<FName> FMetaStoryReadOnlyExecutionContext::GetActiveStateNames() const
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return TArray<FName>();
	}

	TArray<FName> Result;
	const FMetaStoryExecutionState& Exec = Storage.GetExecutionState();

	// Active States
	for (const FMetaStoryExecutionFrame& CurrentFrame : Exec.ActiveFrames)
	{
		const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;
		for (int32 Index = 0; Index < CurrentFrame.ActiveStates.Num(); Index++)
		{
			const FMetaStoryStateHandle Handle = CurrentFrame.ActiveStates[Index];
			if (Handle.IsValid())
			{
				const FMetaStoryCompactState& State = CurrentMetaStory->States[Handle.Index];
				Result.Add(State.Name);
			}
		}
	}

	return Result;
}

#if WITH_GAMEPLAY_DEBUGGER
FString FMetaStoryReadOnlyExecutionContext::GetDebugInfoString() const
{
	TStringBuilder<2048> DebugString;
	DebugString << TEXT("MetaStory (asset: '");
	RootMetaStory.GetFullName(DebugString);
	DebugString << TEXT("')");

	if (IsValid())
	{
		const FMetaStoryExecutionState& Exec = Storage.GetExecutionState();

		DebugString << TEXT("Status: ");
		DebugString << UEnum::GetDisplayValueAsText(Exec.TreeRunStatus).ToString();
		DebugString << TEXT("\n");

		// Active States
		DebugString << TEXT("Current State:\n");
		for (const FMetaStoryExecutionFrame& CurrentFrame : Exec.ActiveFrames)
		{
			const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;

			if (CurrentFrame.bIsGlobalFrame)
			{
				DebugString.Appendf(TEXT("\nEvaluators\n  [ %-30s | %8s | %8s | %15s ]\n"),
					TEXT("Name"), TEXT("Bindings"), TEXT("Output Bindings"), TEXT("Data Handle"));
				for (int32 EvalIndex = CurrentMetaStory->EvaluatorsBegin; EvalIndex < (CurrentMetaStory->EvaluatorsBegin + CurrentMetaStory->EvaluatorsNum); EvalIndex++)
				{
					const FMetaStoryEvaluatorBase& Eval = CurrentMetaStory->Nodes[EvalIndex].Get<const FMetaStoryEvaluatorBase>();
					DebugString.Appendf(TEXT("| %-30s | %8d | %8d | %15s |\n"),
						*Eval.Name.ToString(), Eval.BindingsBatch.Get(), Eval.OutputBindingsBatch.Get(), *Eval.InstanceDataHandle.Describe());
				}

				DebugString << TEXT("\nGlobal Tasks\n");
				for (int32 TaskIndex = CurrentMetaStory->GlobalTasksBegin; TaskIndex < (CurrentMetaStory->GlobalTasksBegin + CurrentMetaStory->GlobalTasksNum); TaskIndex++)
				{
					const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
					if (Task.bTaskEnabled)
					{
						DebugString << Task.GetDebugInfo(*this);

					}
				}
			}

			for (int32 Index = 0; Index < CurrentFrame.ActiveStates.Num(); Index++)
			{
				FMetaStoryStateHandle Handle = CurrentFrame.ActiveStates[Index];
				if (Handle.IsValid())
				{
					const FMetaStoryCompactState& State = CurrentMetaStory->States[Handle.Index];
					DebugString << TEXT('[');
					DebugString << State.Name;
					DebugString << TEXT("]\n");

					if (State.TasksNum > 0)
					{
						DebugString += TEXT("\nTasks:\n");
						for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
						{
							const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
							if (Task.bTaskEnabled)
							{
								DebugString << Task.GetDebugInfo(*this);
							}
						}
					}
				}
			}
		}
	}
	else
	{
		DebugString << TEXT("MetaStory context is not initialized properly.");
	}

	return DebugString.ToString();
}
#endif // WITH_GAMEPLAY_DEBUGGER

#if WITH_METASTORY_DEBUG
int32 FMetaStoryReadOnlyExecutionContext::GetStateChangeCount() const
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return 0;
	}

	const FMetaStoryExecutionState& Exec = Storage.GetExecutionState();
	return Exec.StateChangeCount;
}

void FMetaStoryReadOnlyExecutionContext::DebugPrintInternalLayout()
{
	LOG_SCOPE_VERBOSITY_OVERRIDE(LogMetaStory, ELogVerbosity::Log);
	UE_LOG(LogMetaStory, Log, TEXT("%s"), *RootMetaStory.DebugInternalLayoutAsString());
}
#endif // WITH_METASTORY_DEBUG

FString FMetaStoryReadOnlyExecutionContext::GetInstanceDescriptionInternal() const
{
	const TInstancedStruct<FMetaStoryExecutionExtension>& ExecutionExtension = Storage.GetExecutionState().ExecutionExtension;
	return ExecutionExtension.IsValid()
		? ExecutionExtension.Get().GetInstanceDescription(FMetaStoryExecutionExtension::FContextParameters(Owner, RootMetaStory, Storage))
		: Owner.GetName();
}

#if WITH_METASTORY_TRACE
FMetaStoryInstanceDebugId FMetaStoryReadOnlyExecutionContext::GetInstanceDebugId() const
{
	FMetaStoryInstanceDebugId& InstanceDebugId = Storage.GetMutableExecutionState().InstanceDebugId;
	if (!InstanceDebugId.IsValid())
	{
		// Using an Id from the object trace pool to allow each instance to be a unique debuggable object in the Traces and RewindDebugger
#if OBJECT_TRACE_ENABLED
		InstanceDebugId = FMetaStoryInstanceDebugId(FObjectTrace::AllocateInstanceId());
#else
		InstanceDebugId = FMetaStoryInstanceDebugId(0);
#endif
	}
	return InstanceDebugId;
}
#endif // WITH_METASTORY_TRACE

/**
 * FMetaStoryMinimalExecutionContext implementation
 */
 // Deprecated
FMetaStoryMinimalExecutionContext::FMetaStoryMinimalExecutionContext(UObject& InOwner, const UMetaStory& InMetaStory, FMetaStoryInstanceData& InInstanceData)
	: FMetaStoryMinimalExecutionContext(&InOwner, &InMetaStory, InInstanceData.GetMutableStorage())
{
}

// Deprecated
FMetaStoryMinimalExecutionContext::FMetaStoryMinimalExecutionContext(UObject& InOwner, const UMetaStory& InMetaStory, FMetaStoryInstanceStorage& InStorage)
	: FMetaStoryMinimalExecutionContext(&InOwner, &InMetaStory, InStorage)
{
}

FMetaStoryMinimalExecutionContext::FMetaStoryMinimalExecutionContext(TNotNull<UObject*> InOwner, TNotNull<const UMetaStory*> InMetaStory, FMetaStoryInstanceData& InInstanceData)
	: FMetaStoryMinimalExecutionContext(InOwner, InMetaStory, InInstanceData.GetMutableStorage())
{
}

FMetaStoryMinimalExecutionContext::FMetaStoryMinimalExecutionContext(TNotNull<UObject*> InOwner, TNotNull<const UMetaStory*> InMetaStory, FMetaStoryInstanceStorage& InStorage)
	: FMetaStoryReadOnlyExecutionContext(InOwner, InMetaStory, InStorage)
{
	Storage.AcquireWriteAccess();

	if (IsValid())
	{
		constexpr bool bWriteAccessAcquired = true;
		Storage.GetRuntimeValidation().SetContext(&Owner, &RootMetaStory, bWriteAccessAcquired);
	}
}

FMetaStoryMinimalExecutionContext::~FMetaStoryMinimalExecutionContext()
{
	Storage.ReleaseWriteAccess();
}

UE::MetaStory::FScheduledTickHandle FMetaStoryMinimalExecutionContext::AddScheduledTickRequest(FMetaStoryScheduledTick ScheduledTick)
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return UE::MetaStory::FScheduledTickHandle();
	}

	UE::MetaStory::FScheduledTickHandle Result = Storage.GetMutableExecutionState().AddScheduledTickRequest(ScheduledTick);
	ScheduleNextTick(UE::MetaStory::EMetaStoryTickReason::ScheduledTickRequest);
	return Result;
}

void FMetaStoryMinimalExecutionContext::UpdateScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle, FMetaStoryScheduledTick ScheduledTick)
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}

	if (Storage.GetMutableExecutionState().UpdateScheduledTickRequest(Handle, ScheduledTick))
	{
		ScheduleNextTick(UE::MetaStory::EMetaStoryTickReason::ScheduledTickRequest);
	}
}

void FMetaStoryMinimalExecutionContext::RemoveScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle)
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}

	if (Storage.GetMutableExecutionState().RemoveScheduledTickRequest(Handle))
	{
		ScheduleNextTick(UE::MetaStory::EMetaStoryTickReason::ScheduledTickRequest);
	}
}

void FMetaStoryMinimalExecutionContext::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_SendEvent);

	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}

	METASTORY_LOG(VeryVerbose, TEXT("Send Event '%s'"), *Tag.ToString());
	UE_METASTORY_DEBUG_LOG_EVENT(this, Log, TEXT("Send Event '%s'"), *Tag.ToString());

	FMetaStoryEventQueue& LocalEventQueue = Storage.GetMutableEventQueue();
	if (LocalEventQueue.SendEvent(&Owner, Tag, Payload, Origin))
	{
		ScheduleNextTick(UE::MetaStory::EMetaStoryTickReason::Event);
		UE_METASTORY_DEBUG_SEND_EVENT(this, &RootMetaStory, Tag, Payload, Origin);
	}
}

FMetaStoryExecutionExtension* FMetaStoryMinimalExecutionContext::GetMutableExecutionExtension() const
{
	return Storage.GetMutableExecutionState().ExecutionExtension.GetMutablePtr();
}

void FMetaStoryMinimalExecutionContext::ScheduleNextTick(UE::MetaStory::EMetaStoryTickReason Reason)
{
	if (bAllowedToScheduleNextTick && RootMetaStory.IsScheduledTickAllowed())
	{
		if (FMetaStoryExecutionExtension* ExecExtension = GetMutableExecutionExtension())
		{
			ExecExtension->ScheduleNextTick(FMetaStoryExecutionExtension::FContextParameters(Owner, RootMetaStory, Storage), FMetaStoryExecutionExtension::FNextTickArguments(Reason));
		}
	}
}

/**
 * FMetaStoryExecutionContext::FSelectStateResult implementation
 */
FMetaStoryExecutionFrame& FMetaStoryExecutionContext::FSelectStateResult::MakeAndAddTemporaryFrame(
	const UE::MetaStory::FActiveFrameID FrameID,
	const UE::MetaStory::FMetaStoryExecutionFrameHandle& FrameHandle,
	const bool bIsGlobalFrame)
{
	check(FrameHandle.IsValid());

	FMetaStoryExecutionFrame& ExecFrame = TemporaryFrames.AddDefaulted_GetRef();
	ExecFrame.FrameID = FrameID;
	ExecFrame.MetaStory = FrameHandle.GetMetaStory();
	ExecFrame.RootState = FrameHandle.GetRootState();
	ExecFrame.bIsGlobalFrame = bIsGlobalFrame;

	const FMetaStoryCompactFrame* TargetFrame = UE::MetaStory::ExecutionContext::Private::FindMetaStoryFrame(FrameHandle);
	ensureMsgf(TargetFrame, TEXT("The compiled data is invalid. It should contains the information for the new root frame."));
	ExecFrame.ActiveTasksStatus = TargetFrame ? FMetaStoryTasksCompletionStatus(*TargetFrame) : FMetaStoryTasksCompletionStatus();

	return ExecFrame;
}

FMetaStoryExecutionFrame& FMetaStoryExecutionContext::FSelectStateResult::MakeAndAddTemporaryFrameWithNewRoot(
	const UE::MetaStory::FActiveFrameID FrameID,
	const UE::MetaStory::FMetaStoryExecutionFrameHandle& FrameHandle,
	FMetaStoryExecutionFrame& OtherFrame)
{
	check(FrameHandle.IsValid());

	constexpr bool bIsGlobalFrame = true;
	FMetaStoryExecutionFrame& ExecFrame = MakeAndAddTemporaryFrame(FrameID, FrameHandle, bIsGlobalFrame);
	ExecFrame.ExternalDataBaseIndex = OtherFrame.ExternalDataBaseIndex;
	ExecFrame.GlobalParameterDataHandle = OtherFrame.GlobalParameterDataHandle;
	ExecFrame.GlobalInstanceIndexBase = OtherFrame.GlobalInstanceIndexBase;
	// Don't stop globals.
	ExecFrame.bHaveEntered = OtherFrame.bHaveEntered;
	OtherFrame.bHaveEntered = false;

	return ExecFrame;
}

FMetaStoryExecutionContext::FSelectStateResult::FFrameAndParent FMetaStoryExecutionContext::FSelectStateResult::GetExecutionFrame(UE::MetaStory::FActiveFrameID ID)
{
	const int32 FoundIndex = SelectedFrames.IndexOfByKey(ID);
	FMetaStoryExecutionFrame* Frame = FoundIndex != INDEX_NONE
		? TemporaryFrames.FindByPredicate([ID](const FMetaStoryExecutionFrame& Other){ return Other.FrameID == ID; })
		: nullptr;
	const UE::MetaStory::FActiveFrameID ParentFrameID = FoundIndex > 0 ? SelectedFrames[FoundIndex - 1] : UE::MetaStory::FActiveFrameID();
	return FFrameAndParent{ .Frame = Frame, .ParentFrameID = ParentFrameID };
}

UE::MetaStory::FActiveState FMetaStoryExecutionContext::FSelectStateResult::GetStateHandle(UE::MetaStory::FActiveStateID ID) const
{
	const int32 FoundIndex = UE::MetaStory::FActiveStatePath::IndexOf(SelectedStates, ID);
	if (FoundIndex != INDEX_NONE)
	{
		return SelectedStates[FoundIndex];
	}
	return {};
}

/**
 * FMetaStoryExecutionContext::FCurrentlyProcessedFrameScope implementation
 */
FMetaStoryExecutionContext::FCurrentlyProcessedFrameScope::FCurrentlyProcessedFrameScope(FMetaStoryExecutionContext& InContext, const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame) : Context(InContext)
{
	check(CurrentFrame.MetaStory);
	FMetaStoryInstanceStorage* SharedInstanceDataStorage = &CurrentFrame.MetaStory->GetSharedInstanceData()->GetMutableStorage();

	SavedFrame = Context.CurrentlyProcessedFrame;
	SavedParentFrame = Context.CurrentlyProcessedParentFrame;
	SavedSharedInstanceDataStorage = Context.CurrentlyProcessedSharedInstanceStorage;
	Context.CurrentlyProcessedFrame = &CurrentFrame;
	Context.CurrentlyProcessedParentFrame = CurrentParentFrame;
	Context.CurrentlyProcessedSharedInstanceStorage = SharedInstanceDataStorage;

	UE_METASTORY_DEBUG_INSTANCE_FRAME_EVENT(&Context, Context.CurrentlyProcessedFrame);
}

FMetaStoryExecutionContext::FCurrentlyProcessedFrameScope::~FCurrentlyProcessedFrameScope()
{
	Context.CurrentlyProcessedFrame = SavedFrame;
	Context.CurrentlyProcessedParentFrame = SavedParentFrame;
	Context.CurrentlyProcessedSharedInstanceStorage = SavedSharedInstanceDataStorage;

	if (Context.CurrentlyProcessedFrame)
	{
		UE_METASTORY_DEBUG_INSTANCE_FRAME_EVENT(&Context, Context.CurrentlyProcessedFrame);
	}
}

/**
 * FMetaStoryExecutionContext::FNodeInstanceDataScope implementation
 */
FMetaStoryExecutionContext::FNodeInstanceDataScope::FNodeInstanceDataScope(FMetaStoryExecutionContext& InContext, const FMetaStoryNodeBase* InNode, const int32 InNodeIndex, const FMetaStoryDataHandle InNodeDataHandle, const FMetaStoryDataView InNodeInstanceData)
	: Context(InContext)
{
	SavedNode = Context.CurrentNode;
	SavedNodeIndex = Context.CurrentNodeIndex;
	SavedNodeDataHandle = Context.CurrentNodeDataHandle;
	SavedNodeInstanceData = Context.CurrentNodeInstanceData;
	Context.CurrentNode = InNode;
	Context.CurrentNodeIndex = InNodeIndex;
	Context.CurrentNodeDataHandle = InNodeDataHandle;
	Context.CurrentNodeInstanceData = InNodeInstanceData;
}

FMetaStoryExecutionContext::FNodeInstanceDataScope::~FNodeInstanceDataScope()
{
	Context.CurrentNodeDataHandle = SavedNodeDataHandle;
	Context.CurrentNodeInstanceData = SavedNodeInstanceData;
	Context.CurrentNodeIndex = SavedNodeIndex;
	Context.CurrentNode = SavedNode;
}

/**
 * FMetaStoryExecutionContext implementation
 */
FMetaStoryExecutionContext::FMetaStoryExecutionContext(UObject& InOwner, const UMetaStory& InMetaStory, FMetaStoryInstanceData& InInstanceData, const FOnCollectMetaStoryExternalData& InCollectExternalDataDelegate, const EMetaStoryRecordTransitions RecordTransitions)
	: FMetaStoryExecutionContext(&InOwner, &InMetaStory, InInstanceData, InCollectExternalDataDelegate, RecordTransitions)
{
}

FMetaStoryExecutionContext::FMetaStoryExecutionContext(TNotNull<UObject*> InOwner, TNotNull<const UMetaStory*> InMetaStory, FMetaStoryInstanceData& InInstanceData, const FOnCollectMetaStoryExternalData& InCollectExternalDataDelegate, const EMetaStoryRecordTransitions RecordTransitions)
	: FMetaStoryMinimalExecutionContext(InOwner, InMetaStory, InInstanceData)
	, InstanceData(InInstanceData)
	, CollectExternalDataDelegate(InCollectExternalDataDelegate)
{
	if (IsValid())
	{
		// Initialize data views for all possible items.
		ContextAndExternalDataViews.SetNum(RootMetaStory.GetNumContextDataViews());
		EventQueue = InstanceData.GetSharedMutableEventQueue();
		bRecordTransitions = RecordTransitions == EMetaStoryRecordTransitions::Yes;
	}
	else
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory asset is not valid ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
	}
}

FMetaStoryExecutionContext::FMetaStoryExecutionContext(const FMetaStoryExecutionContext& InContextToCopy, const UMetaStory& InMetaStory, FMetaStoryInstanceData& InInstanceData)
	: FMetaStoryExecutionContext(InContextToCopy, &InMetaStory, InInstanceData)
{
}

FMetaStoryExecutionContext::FMetaStoryExecutionContext(const FMetaStoryExecutionContext& InContextToCopy, TNotNull<const UMetaStory*> InMetaStory, FMetaStoryInstanceData& InInstanceData)
	: FMetaStoryExecutionContext(&InContextToCopy.Owner, InMetaStory, InInstanceData, InContextToCopy.CollectExternalDataDelegate)
{
	SetLinkedMetaStoryOverrides(InContextToCopy.LinkedAssetMetaStoryOverrides);
	const bool bIsSameSchema = RootMetaStory.GetSchema()->GetClass() == InContextToCopy.GetMetaStory()->GetSchema()->GetClass();
	if (bIsSameSchema)
	{
		for (const FMetaStoryExternalDataDesc& TargetDataDesc : GetContextDataDescs())
		{
			const int32 TargetIndex = TargetDataDesc.Handle.DataHandle.GetIndex();
			ContextAndExternalDataViews[TargetIndex] = InContextToCopy.ContextAndExternalDataViews[TargetIndex];
		}
	}
	else
	{
		METASTORY_LOG(Error, TEXT("%hs: '%s' using MetaStory '%s' trying to run subtree '%s' but their schemas don't match"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(InContextToCopy.GetMetaStory()), *GetFullNameSafe(&RootMetaStory));
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FMetaStoryExecutionContext::~FMetaStoryExecutionContext()
{
	// Mark external data indices as invalid
	FMetaStoryExecutionState& Exec = InstanceData.GetMutableStorage().GetMutableExecutionState();
	for (FMetaStoryExecutionFrame& Frame : Exec.ActiveFrames)
	{
		Frame.ExternalDataBaseIndex = {};
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FMetaStoryExecutionContext::SetCollectExternalDataCallback(const FOnCollectMetaStoryExternalData& Callback)
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}

	FMetaStoryExecutionState& Exec = GetExecState();
	if (!ensureMsgf(Exec.CurrentPhase == EMetaStoryUpdatePhase::Unset, TEXT("%hs can't be called while already in %s ('%s' using MetaStory '%s')."),
		__FUNCTION__, *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString(), *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory)))
	{
		return;
	}

	CollectExternalDataDelegate = Callback;
}

void FMetaStoryExecutionContext::SetLinkedMetaStoryOverrides(const FMetaStoryReferenceOverrides* InLinkedMetaStoryOverrides)
{
	if (InLinkedMetaStoryOverrides)
	{
		SetLinkedMetaStoryOverrides(*InLinkedMetaStoryOverrides);
	}
	else
	{
		SetLinkedMetaStoryOverrides(FMetaStoryReferenceOverrides());
	}
}

void FMetaStoryExecutionContext::SetLinkedMetaStoryOverrides(FMetaStoryReferenceOverrides InLinkedMetaStoryOverrides)
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}

	FMetaStoryExecutionState& Exec = GetExecState();
	if (!ensureMsgf(Exec.CurrentPhase == EMetaStoryUpdatePhase::Unset, TEXT("%hs can't be called while already in %s ('%s' using MetaStory '%s')."),
		__FUNCTION__, *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString(), *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory)))
	{
		return;
	}

	bool bValid = true;

	// Confirms that the overrides schema matches.
	const TConstArrayView<FMetaStoryReferenceOverrideItem> InOverrideItems = InLinkedMetaStoryOverrides.GetOverrideItems();
	for (const FMetaStoryReferenceOverrideItem& Item : InOverrideItems)
	{
		if (const UMetaStory* ItemMetaStory = Item.GetMetaStoryReference().GetMetaStory())
		{
			if (!ItemMetaStory->IsReadyToRun())
			{
				METASTORY_LOG(Error, TEXT("%hs: '%s' using MetaStory '%s' trying to set override '%s' but the tree is not initialized properly."),
					__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(GetMetaStory()), *GetFullNameSafe(ItemMetaStory));
				bValid = false;
				break;
			}

			if (!RootMetaStory.HasCompatibleContextData(*ItemMetaStory))
			{
				METASTORY_LOG(Error, TEXT("%hs: '%s' using MetaStory '%s' trying to set override '%s' but the tree context data is not compatible."),
					__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(GetMetaStory()), *GetFullNameSafe(ItemMetaStory));
				bValid = false;
				break;
			}

			const UMetaStorySchema* OverrideSchema = ItemMetaStory->GetSchema();
			if (ItemMetaStory->GetSchema() == nullptr)
			{
				METASTORY_LOG(Error, TEXT("%hs: '%s' using MetaStory '%s' trying to set override '%s' but the tree does not have a schema."),
					__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(GetMetaStory()), *GetFullNameSafe(ItemMetaStory));
				bValid = false;
				break;
			}

			const bool bIsSameSchema = RootMetaStory.GetSchema()->GetClass() == OverrideSchema->GetClass();
			if (!bIsSameSchema)
			{
				METASTORY_LOG(Error, TEXT("%hs: '%s' using MetaStory '%s' trying to set override '%s' but their schemas don't match."),
					__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(GetMetaStory()), *GetFullNameSafe(Item.GetMetaStoryReference().GetMetaStory()));
				bValid = false;
				break;
			}
		}
	}

	bool bChanged = false;
	if (bValid)
	{
		LinkedAssetMetaStoryOverrides = MoveTemp(InLinkedMetaStoryOverrides);
		bChanged = LinkedAssetMetaStoryOverrides.GetOverrideItems().Num() > 0;
	}
	else if (LinkedAssetMetaStoryOverrides.GetOverrideItems().Num() > 0)
	{
		LinkedAssetMetaStoryOverrides.Reset();
		bChanged = true;
	}

	if (bChanged)
	{
		TInstancedStruct<FMetaStoryExecutionExtension>& ExecutionExtension = Storage.GetMutableExecutionState().ExecutionExtension;
		if (ExecutionExtension.IsValid())
		{
			ExecutionExtension.GetMutable().OnLinkedMetaStoryOverridesSet(FMetaStoryExecutionExtension::FContextParameters(Owner, RootMetaStory, Storage), LinkedAssetMetaStoryOverrides);
		}
	}
}

const FMetaStoryReference* FMetaStoryExecutionContext::GetLinkedMetaStoryOverrideForTag(const FGameplayTag StateTag) const
{
	for (const FMetaStoryReferenceOverrideItem& Item : LinkedAssetMetaStoryOverrides.GetOverrideItems())
	{
		if (StateTag.MatchesTag(Item.GetStateTag()))
		{
			return &Item.GetMetaStoryReference();
		}
	}

	return nullptr;
}

bool FMetaStoryExecutionContext::FExternalGlobalParameters::Add(const FPropertyBindingCopyInfo& Copy, uint8* InParameterMemory)
{
	const int32 TypeHash = HashCombine(GetTypeHash(Copy.SourceLeafProperty), GetTypeHash(Copy.SourceIndirection));
	const int32 NumMappings = Mappings.Num();
	Mappings.Add(TypeHash, InParameterMemory);
	return Mappings.Num() > NumMappings;
}

uint8* FMetaStoryExecutionContext::FExternalGlobalParameters::Find(const FPropertyBindingCopyInfo& Copy) const
{
	const int32 TypeHash = HashCombine(GetTypeHash(Copy.SourceLeafProperty), GetTypeHash(Copy.SourceIndirection));
	if (uint8* const* MappingPtr = Mappings.Find(TypeHash))
	{
		return *MappingPtr;
	}

	checkf(false, TEXT("Missing external parameter data"));
	return nullptr;
}

void FMetaStoryExecutionContext::FExternalGlobalParameters::Reset()
{
	Mappings.Reset();
}

void FMetaStoryExecutionContext::SetExternalGlobalParameters(const FExternalGlobalParameters* Parameters)
{
	ExternalGlobalParameters = Parameters;
}

bool FMetaStoryExecutionContext::AreContextDataViewsValid() const
{
	if (!IsValid())
	{
		return false;
	}

	bool bResult = true;

	for (const FMetaStoryExternalDataDesc& DataDesc : RootMetaStory.GetContextDataDescs())
	{
		const FMetaStoryDataView& DataView = ContextAndExternalDataViews[DataDesc.Handle.DataHandle.GetIndex()];

		// Required items must have valid pointer of the expected type.  
		if (DataDesc.Requirement == EMetaStoryExternalDataRequirement::Required)
		{
			if (!DataView.IsValid() || !DataDesc.IsCompatibleWith(DataView))
			{
				bResult = false;
				break;
			}
		}
		else // Optional items must have the expected type if they are set.
		{
			if (DataView.IsValid() && !DataDesc.IsCompatibleWith(DataView))
			{
				bResult = false;
				break;
			}
		}
	}
	return bResult;
}

bool FMetaStoryExecutionContext::SetContextDataByName(const FName Name, FMetaStoryDataView DataView)
{
	const FMetaStoryExternalDataDesc* Desc = RootMetaStory.GetContextDataDescs().FindByPredicate([&Name](const FMetaStoryExternalDataDesc& Desc)
		{
			return Desc.Name == Name;
		});
	if (Desc)
	{
		ContextAndExternalDataViews[Desc->Handle.DataHandle.GetIndex()] = DataView;
		return true;
	}
	return false;
}

FMetaStoryDataView FMetaStoryExecutionContext::GetContextDataByName(const FName Name) const
{
	const FMetaStoryExternalDataDesc* Desc = RootMetaStory.GetContextDataDescs().FindByPredicate([&Name](const FMetaStoryExternalDataDesc& Desc)
		{
			return Desc.Name == Name;
		});
	if (Desc)
	{
		return ContextAndExternalDataViews[Desc->Handle.DataHandle.GetIndex()];
	}
	return FMetaStoryDataView();
}

FMetaStoryWeakExecutionContext FMetaStoryExecutionContext::MakeWeakExecutionContext() const
{
	return FMetaStoryWeakExecutionContext(*this);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FMetaStoryWeakTaskRef FMetaStoryExecutionContext::MakeWeakTaskRef(const FMetaStoryTaskBase& Node) const
{
	// This function has been deprecated
	check(CurrentNode == &Node);
	return MakeWeakTaskRefInternal();
}

FMetaStoryWeakTaskRef FMetaStoryExecutionContext::MakeWeakTaskRefInternal() const
{
	// This function has been deprecated
	FMetaStoryWeakTaskRef Result;
	if (const FMetaStoryExecutionFrame* Frame = GetCurrentlyProcessedFrame())
	{
		if (Frame->MetaStory->Nodes.IsValidIndex(CurrentNodeIndex)
			&& Frame->MetaStory->Nodes[CurrentNodeIndex].GetPtr<const FMetaStoryTaskBase>() != nullptr)
		{
			Result = FMetaStoryWeakTaskRef(Frame->MetaStory, FMetaStoryIndex16(CurrentNodeIndex));
		}
	}
	return Result;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

EMetaStoryRunStatus FMetaStoryExecutionContext::Start(const FInstancedPropertyBag* InitialParameters, int32 RandomSeed)
{
	const TOptional<int32> ParamRandomSeed = RandomSeed == -1 ? TOptional<int32>() : RandomSeed;
	return Start(FStartParameters{ .GlobalParameters = InitialParameters, .RandomSeed = ParamRandomSeed });
}

void FMetaStoryExecutionContext::SetUpdatePhaseInExecutionState(FMetaStoryExecutionState& ExecutionState, const EMetaStoryUpdatePhase UpdatePhase) const
{
	if (ExecutionState.CurrentPhase == UpdatePhase)
	{
		return;
	}

	if (ExecutionState.CurrentPhase != EMetaStoryUpdatePhase::Unset)
	{
		UE_METASTORY_DEBUG_EXIT_PHASE(this, ExecutionState.CurrentPhase);
	}

	ExecutionState.CurrentPhase = UpdatePhase;

	if (ExecutionState.CurrentPhase != EMetaStoryUpdatePhase::Unset)
	{
		UE_METASTORY_DEBUG_ENTER_PHASE(this, ExecutionState.CurrentPhase);
	}
}

EMetaStoryRunStatus FMetaStoryExecutionContext::Start(FStartParameters Parameters)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_Start);
	UE_METASTORY_CRASH_REPORTER_SCOPE(&Owner, &RootMetaStory, UE::MetaStory::ExecutionContext::Private::Name_Start.Resolve());

	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return EMetaStoryRunStatus::Failed;
	}

	FMetaStoryExecutionState& Exec = GetExecState();
	if (!ensureMsgf(Exec.CurrentPhase == EMetaStoryUpdatePhase::Unset, TEXT("%hs can't be called while already in %s ('%s' using MetaStory '%s')."),
		__FUNCTION__, *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString(), *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory)))
	{
		return EMetaStoryRunStatus::Failed;
	}

	// Stop if still running previous state.
	if (Exec.TreeRunStatus == EMetaStoryRunStatus::Running)
	{
		Stop();
	}

	// Initialize instance data. No active states yet, so we'll initialize the evals and global tasks.
	InstanceData.Reset();

	constexpr bool bWriteAccessAcquired = true;
	Storage.GetRuntimeValidation().SetContext(&Owner, &RootMetaStory, bWriteAccessAcquired);
	Exec.ExecutionExtension = MoveTemp(Parameters.ExecutionExtension);
	if (Parameters.SharedEventQueue)
	{
		InstanceData.SetSharedEventQueue(Parameters.SharedEventQueue.ToSharedRef());
	}

#if WITH_METASTORY_TRACE
	// Make sure the debug id is valid. We want to construct it with the current GetInstanceDescriptionInternal
	GetInstanceDebugId();
#endif

	if (!Parameters.GlobalParameters || !SetGlobalParameters(*Parameters.GlobalParameters))
	{
		SetGlobalParameters(RootMetaStory.GetDefaultParameters());
	}

	Exec.RandomStream.Initialize(Parameters.RandomSeed.IsSet() ? Parameters.RandomSeed.GetValue() : FPlatformTime::Cycles());

	TGuardValue<bool> ScheduledNextTickScope(bAllowedToScheduleNextTick, false);
	ensure(Exec.ActiveFrames.Num() == 0);

	// Initialize for the init frame.
	TSharedRef<FSelectStateResult> SelectStateResult = MakeShared<FSelectStateResult>();
	check(CurrentlyProcessedTemporaryStorage == nullptr);
	TGuardValue<TSharedPtr<FSelectStateResult>> TemporaryStorageScope(CurrentlyProcessedTemporaryStorage, SelectStateResult);

	const FActiveFrameID InitFrameID = FActiveFrameID(Storage.GenerateUniqueId());
	const FMetaStoryExecutionFrameHandle InitFrameHandle = FMetaStoryExecutionFrameHandle(&RootMetaStory, FMetaStoryStateHandle::Root);
	constexpr bool bIsGlobalFrame = true;
	FMetaStoryExecutionFrame& InitFrame = SelectStateResult->MakeAndAddTemporaryFrame(InitFrameID, InitFrameHandle, bIsGlobalFrame);
	InitFrame.ExecutionRuntimeIndexBase = FMetaStoryIndex16(Storage.AddExecutionRuntimeData(GetOwner(), InitFrameHandle));
	InitFrame.GlobalParameterDataHandle = FMetaStoryDataHandle(EMetaStoryDataSourceType::GlobalParameterData);

	if (!CollectActiveExternalData(SelectStateResult->TemporaryFrames))
	{
		METASTORY_LOG(Warning, TEXT("%hs: Failed to collect external data ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return EMetaStoryRunStatus::Failed;
	}

	// Must sent instance creation event first 
	UE_METASTORY_DEBUG_INSTANCE_EVENT(this, EMetaStoryTraceEventType::Push);

	METASTORY_LOG(VeryVerbose, TEXT("%hs: Starting MetaStory %s on owner '%s'."),
		__FUNCTION__, *GetFullNameSafe(&RootMetaStory), *GetNameSafe(&Owner));

	// From this point any calls to Stop should be deferred.
	SetUpdatePhaseInExecutionState(Exec, EMetaStoryUpdatePhase::StartTree);

	// Start evaluators and global tasks. Fail the execution if any global task fails.
	constexpr FMetaStoryExecutionFrame* ParentInitFrame = nullptr;
	SelectStateResult->SelectedFrames.Add(InitFrame.FrameID);
	EMetaStoryRunStatus GlobalTasksRunStatus = StartTemporaryEvaluatorsAndGlobalTasks(ParentInitFrame, InitFrame);
	if (GlobalTasksRunStatus == EMetaStoryRunStatus::Running)
	{
		// Exception with Start. Tick the evaluators.
		constexpr float DeltaTime = 0.0f;
		TickGlobalEvaluatorsForFrameWithValidation(DeltaTime, ParentInitFrame, InitFrame);

		// Initialize to unset running state.
		Exec.TreeRunStatus = EMetaStoryRunStatus::Running;
		Exec.LastTickStatus = EMetaStoryRunStatus::Unset;

		FSelectStateArguments SelectStateArgs;
		SelectStateArgs.SourceState = FActiveState(InitFrameID, FActiveStateID(), InitFrameHandle.GetRootState());
		SelectStateArgs.TargetState = FStateHandleContext(InitFrameHandle.GetMetaStory(), InitFrameHandle.GetRootState());
		SelectStateArgs.Behavior = ESelectStateBehavior::StateTransition;
		SelectStateArgs.SelectionRules = InitFrameHandle.GetMetaStory()->StateSelectionRules;
		if (SelectState(SelectStateArgs, SelectStateResult))
		{
			check(SelectStateResult->SelectedStates.Num() > 0);
			const FMetaStoryStateHandle LastSelectedStateHandle = SelectStateResult->SelectedStates.Last().GetStateHandle();
			if (LastSelectedStateHandle.IsCompletionState())
			{
				// Transition to a terminal state (succeeded/failed).
				METASTORY_LOG(Warning, TEXT("%hs: Tree %s at MetaStory start on '%s' using MetaStory '%s'."),
					__FUNCTION__,
					LastSelectedStateHandle == FMetaStoryStateHandle::Succeeded ? TEXT("succeeded") : TEXT("failed"),
					*GetNameSafe(&Owner),
					*GetFullNameSafe(&RootMetaStory)
				);
				Exec.TreeRunStatus = LastSelectedStateHandle.ToCompletionStatus();
			}
			else
			{
				// Enter state tasks can fail/succeed, treat it same as tick.
				FMetaStoryTransitionResult Transition;
				Transition.TargetState = InitFrameHandle.GetRootState();
				Transition.CurrentRunStatus = Exec.LastTickStatus;
				const EMetaStoryRunStatus LastTickStatus = EnterState(SelectStateResult, Transition);

				Exec.LastTickStatus = LastTickStatus;

				// Report state completed immediately.
				if (Exec.LastTickStatus != EMetaStoryRunStatus::Running)
				{
					StateCompleted();
				}

				// Was not able to enter the root state. Fail selection.
				if (Exec.ActiveFrames.Num() == 0 || Exec.ActiveFrames[0].ActiveStates.Num() == 0)
				{
					GlobalTasksRunStatus = EMetaStoryRunStatus::Failed;
					Exec.TreeRunStatus = EMetaStoryRunStatus::Failed;
					METASTORY_LOG(Error, TEXT("%hs: Failed to enter the initial state on '%s' using MetaStory '%s'. Check that the MetaStory logic can always select a state at start."),
						__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
				}
			}
		}
		else
		{
			GlobalTasksRunStatus = EMetaStoryRunStatus::Failed;
			Exec.TreeRunStatus = EMetaStoryRunStatus::Failed;
			METASTORY_LOG(Error, TEXT("%hs: Failed to select initial state on '%s' using MetaStory '%s'. Check that the MetaStory logic can always select a state at start."),
				__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		}
	}
	else
	{
		Exec.TreeRunStatus = GlobalTasksRunStatus;
		METASTORY_LOG(VeryVerbose, TEXT("%hs: Start globals on '%s' using MetaStory '%s' completes."),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
	}

	if (Exec.TreeRunStatus != EMetaStoryRunStatus::Running)
	{
		StopTemporaryEvaluatorsAndGlobalTasks(ParentInitFrame, InitFrame, GlobalTasksRunStatus);

		METASTORY_LOG(VeryVerbose, TEXT("%hs: Global tasks completed the MetaStory %s on start in status '%s'."),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory), *UEnum::GetDisplayValueAsText(GlobalTasksRunStatus).ToString());

		// No active states or global tasks anymore, reset frames.
		Exec.ActiveFrames.Reset();
		RemoveAllDelegateListeners();

		SelectStateResult->SelectedFrames.Pop();

		// We are not considered as running yet so we only set the status without requiring a stop.
		Exec.TreeRunStatus = GlobalTasksRunStatus;
	}
	InstanceData.ResetTemporaryInstances();

	// Reset phase since we are now safe to stop and before potentially stopping the instance.
	SetUpdatePhaseInExecutionState(Exec, EMetaStoryUpdatePhase::Unset);

	// Use local for resulting run state since Stop will reset the instance data.
	EMetaStoryRunStatus Result = Exec.TreeRunStatus;

	if (Exec.RequestedStop != EMetaStoryRunStatus::Unset
		&& Exec.TreeRunStatus == EMetaStoryRunStatus::Running)
	{
		METASTORY_LOG(VeryVerbose, TEXT("Processing Deferred Stop"));
		UE_METASTORY_DEBUG_LOG_EVENT(this, Log, TEXT("Processing Deferred Stop"));
		Result = Stop(Exec.RequestedStop);
	}

	return Result;
}

EMetaStoryRunStatus FMetaStoryExecutionContext::Stop(EMetaStoryRunStatus CompletionStatus)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_Stop);
	UE_METASTORY_CRASH_REPORTER_SCOPE(&Owner, &RootMetaStory, UE::MetaStory::ExecutionContext::Private::Name_Stop.Resolve());

	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return EMetaStoryRunStatus::Failed;
	}

	if (!CollectActiveExternalData())
	{
		METASTORY_LOG(Warning, TEXT("%hs: Failed to collect external data ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return EMetaStoryRunStatus::Failed;
	}

	TGuardValue<bool> ScheduledNextTickScope(bAllowedToScheduleNextTick, false);

	// Make sure that we return a valid completion status (i.e. Succeeded, Failed or Stopped)
	if (CompletionStatus == EMetaStoryRunStatus::Unset
		|| CompletionStatus == EMetaStoryRunStatus::Running)
	{
		CompletionStatus = EMetaStoryRunStatus::Stopped;
	}

	FMetaStoryExecutionState& Exec = GetExecState();

	// A reentrant call to Stop or a call from Start or Tick must be deferred.
	if (Exec.CurrentPhase != EMetaStoryUpdatePhase::Unset)
	{
		METASTORY_LOG(VeryVerbose, TEXT("Deferring Stop at end of %s"), *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString());
		UE_METASTORY_DEBUG_LOG_EVENT(this, Log, TEXT("Deferring Stop at end of %s"), *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString());

		Exec.RequestedStop = CompletionStatus;
		return EMetaStoryRunStatus::Running;
	}

	// No need to clear on exit since we reset all the instance data before leaving the function.
	SetUpdatePhaseInExecutionState(Exec, EMetaStoryUpdatePhase::StopTree);

	EMetaStoryRunStatus Result = Exec.TreeRunStatus;

	// Exit states if still in some valid state.
	if (Exec.TreeRunStatus == EMetaStoryRunStatus::Running)
	{
		// Transition to Succeeded state.
		const TSharedPtr<FSelectStateResult> EmptySelectionResult;
		FMetaStoryTransitionResult Transition;
		Transition.TargetState = FMetaStoryStateHandle::FromCompletionStatus(CompletionStatus);
		Transition.CurrentRunStatus = CompletionStatus;
		ExitState(EmptySelectionResult, Transition);

		// No active states or global tasks anymore, reset frames.
		Exec.ActiveFrames.Reset();

		Result = CompletionStatus;
	}

	Exec.TreeRunStatus = CompletionStatus;

	// Trace before resetting the instance data since it is required to provide all the event information
	UE_METASTORY_DEBUG_ACTIVE_STATES_EVENT(this, {});
	UE_METASTORY_DEBUG_EXIT_PHASE(this, EMetaStoryUpdatePhase::StopTree);
	UE_METASTORY_DEBUG_INSTANCE_EVENT(this, EMetaStoryTraceEventType::Pop);

	// Destruct all allocated instance data (does not shrink the buffer). This will invalidate Exec too.
	InstanceData.Reset();

	// External data needs to be recollected if this exec context is reused.
	bActiveExternalDataCollected = false;

	return Result;
}

EMetaStoryRunStatus FMetaStoryExecutionContext::TickPrelude()
{
	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return EMetaStoryRunStatus::Failed;
	}

	if (!CollectActiveExternalData())
	{
		METASTORY_LOG(Warning, TEXT("%hs: Failed to collect external data ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return EMetaStoryRunStatus::Failed;
	}

	FMetaStoryExecutionState& Exec = GetExecState();

	// No ticking if the tree is done or stopped.
	if (Exec.TreeRunStatus != EMetaStoryRunStatus::Running)
	{
		return Exec.TreeRunStatus;
	}

	if (!ensureMsgf(Exec.CurrentPhase == EMetaStoryUpdatePhase::Unset, TEXT("%hs can't be called while already in %s ('%s' using MetaStory '%s')."),
		__FUNCTION__, *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString(), *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory)))
	{
		return EMetaStoryRunStatus::Failed;
	}

	// From this point any calls to Stop should be deferred.
	SetUpdatePhaseInExecutionState(Exec, EMetaStoryUpdatePhase::TickMetaStory);

	return EMetaStoryRunStatus::Running;
}


EMetaStoryRunStatus FMetaStoryExecutionContext::TickPostlude()
{
	FMetaStoryExecutionState& Exec = GetExecState();

	// Reset phase since we are now safe to stop.
	SetUpdatePhaseInExecutionState(Exec, EMetaStoryUpdatePhase::Unset);

	// Use local for resulting run state since Stop will reset the instance data.
	EMetaStoryRunStatus Result = Exec.TreeRunStatus;

	if (Exec.RequestedStop != EMetaStoryRunStatus::Unset)
	{
		METASTORY_LOG(VeryVerbose, TEXT("Processing Deferred Stop"));
		UE_METASTORY_DEBUG_LOG_EVENT(this, Log, TEXT("Processing Deferred Stop"));

		Result = Stop(Exec.RequestedStop);
	}

	return Result;
}

EMetaStoryRunStatus FMetaStoryExecutionContext::Tick(const float DeltaTime)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_Tick);
	UE_METASTORY_CRASH_REPORTER_SCOPE(&Owner, &RootMetaStory, UE::MetaStory::ExecutionContext::Private::Name_Tick.Resolve());

	TGuardValue<bool> ScheduledNextTickScope(bAllowedToScheduleNextTick, false);

	const EMetaStoryRunStatus PreludeResult = TickPrelude();
	if (PreludeResult != EMetaStoryRunStatus::Running)
	{
		return PreludeResult;
	}

	TickUpdateTasksInternal(DeltaTime);
	TickTriggerTransitionsInternal();

	return TickPostlude();
}

EMetaStoryRunStatus FMetaStoryExecutionContext::TickUpdateTasks(const float DeltaTime)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_Tick);
	UE_METASTORY_CRASH_REPORTER_SCOPE(&Owner, &RootMetaStory, UE::MetaStory::ExecutionContext::Private::Name_Tick.Resolve());

	TGuardValue<bool> ScheduledNextTickScope(bAllowedToScheduleNextTick, false);

	const EMetaStoryRunStatus PreludeResult = TickPrelude();
	if (PreludeResult != EMetaStoryRunStatus::Running)
	{
		return PreludeResult;
	}

	TickUpdateTasksInternal(DeltaTime);

	return TickPostlude();
}

EMetaStoryRunStatus FMetaStoryExecutionContext::TickTriggerTransitions()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_Tick);
	UE_METASTORY_CRASH_REPORTER_SCOPE(&Owner, &RootMetaStory, UE::MetaStory::ExecutionContext::Private::Name_Tick.Resolve());

	TGuardValue<bool> ScheduledNextTickScope(bAllowedToScheduleNextTick, false);

	const EMetaStoryRunStatus PreludeResult = TickPrelude();
	if (PreludeResult != EMetaStoryRunStatus::Running)
	{
		return PreludeResult;
	}

	TickTriggerTransitionsInternal();

	return TickPostlude();
}

void FMetaStoryExecutionContext::TickUpdateTasksInternal(float DeltaTime)
{
	FMetaStoryExecutionState& Exec = GetExecState();

	// If stop is requested, do not try to tick tasks.
	if (Exec.RequestedStop != EMetaStoryRunStatus::Unset)
	{
		return;
	}

	// Prevent wrong user input.
	DeltaTime = FMath::Max(0.f, DeltaTime);

	// Update the delayed transitions.
	for (FMetaStoryTransitionDelayedState& DelayedState : Exec.DelayedTransitions)
	{
		DelayedState.TimeLeft -= DeltaTime;
	}

	const EMetaStoryRunStatus PreviousTickStatus = Exec.LastTickStatus;
	auto LogRequestStop = [&Exec, this]()
		{
			if (Exec.RequestedStop != EMetaStoryRunStatus::Unset) // -V547
			{
				UE_METASTORY_DEBUG_LOG_EVENT(this, Log, TEXT("Global tasks completed (%s), stopping the tree"), *UEnum::GetDisplayValueAsText(Exec.RequestedStop).ToString());
				METASTORY_LOG(Log, TEXT("Global tasks completed (%s), stopping the tree"), *UEnum::GetDisplayValueAsText(Exec.RequestedStop).ToString());
			}
		};
	auto TickTaskLogic = [&Exec, &LogRequestStop, PreviousTickStatus, this](float DeltaTime)
		{
			// Tick tasks on active states.
			Exec.LastTickStatus = TickTasks(DeltaTime);
			// Report state completed immediately (and no global task completes)
			if (Exec.LastTickStatus != EMetaStoryRunStatus::Running && Exec.RequestedStop == EMetaStoryRunStatus::Unset && PreviousTickStatus == EMetaStoryRunStatus::Running)
			{
				StateCompleted();
			}

			LogRequestStop();
		};

	if (UE::MetaStory::ExecutionContext::Private::bTickGlobalNodesFollowingTreeHierarchy)
	{
		TickTaskLogic(DeltaTime);
	}
	else
	{
		// Tick global evaluators and tasks.
		const bool bTickGlobalTasks = true;
		const EMetaStoryRunStatus EvalAndGlobalTaskStatus = TickEvaluatorsAndGlobalTasks(DeltaTime, bTickGlobalTasks);
		if (EvalAndGlobalTaskStatus == EMetaStoryRunStatus::Running)
		{
			if (Exec.LastTickStatus == EMetaStoryRunStatus::Running)
			{
				TickTaskLogic(DeltaTime);
			}
		}
		else
		{
			using namespace UE::MetaStory;
			if (ExecutionContext::Private::bGlobalTasksCompleteOwningFrame)
			{
				// Only set RequestStop if it's the first frame (root)
				if (Exec.ActiveFrames.Num() > 0)
				{
					const UMetaStory* MetaStory = Exec.ActiveFrames[0].MetaStory;
					check(MetaStory == &RootMetaStory);
					const ETaskCompletionStatus GlobalTaskStatus = Exec.ActiveFrames[0].ActiveTasksStatus.GetStatus(MetaStory).GetCompletionStatus();
					const EMetaStoryRunStatus GlobalRunStatus = ExecutionContext::CastToRunStatus(GlobalTaskStatus);
					if (GlobalRunStatus != EMetaStoryRunStatus::Running)
					{
						Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, GlobalRunStatus);
						LogRequestStop();
					}
				}
				else
				{
					// Start failed and the user called Tick anyway.
					Exec.RequestedStop = EMetaStoryRunStatus::Failed;
					LogRequestStop();
				}
			}
			else
			{
				// Any completion stops the tree execution.
				Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, EvalAndGlobalTaskStatus);
				LogRequestStop();
			}
		}
	}
}

void FMetaStoryExecutionContext::TickTriggerTransitionsInternal()
{
	UE_METASTORY_DEBUG_SCOPED_PHASE(this, EMetaStoryUpdatePhase::TickTransitions);

	FMetaStoryExecutionState& Exec = GetExecState();

	// If stop is requested, do not try to trigger transitions.
	if (Exec.RequestedStop != EMetaStoryRunStatus::Unset)
	{
		return;
	}

	// Reset the completed subframe counter (for unit-test that do not recreate an execution context between each tick)
	TriggerTransitionsFromFrameIndex.Reset();

	// The state selection is repeated up to MaxIteration time. This allows failed EnterState() to potentially find a new state immediately.
	// This helps event driven MetaStorys to not require another event/tick to find a suitable state.
	static constexpr int32 MaxIterations = 5;
	for (int32 Iter = 0; Iter < MaxIterations; Iter++)
	{
		ON_SCOPE_EXIT{ InstanceData.ResetTemporaryInstances(); };

		if (TriggerTransitions())
		{
			check(RequestedTransition.IsValid());
			UE_METASTORY_DEBUG_SCOPED_PHASE(this, EMetaStoryUpdatePhase::ApplyTransitions);
			UE_METASTORY_DEBUG_TRANSITION_EVENT(this, RequestedTransition->Source, EMetaStoryTraceEventType::OnTransition);

			BeginApplyTransition(RequestedTransition->Transition);

			ExitState(RequestedTransition->Selection, RequestedTransition->Transition);

			// Tree succeeded or failed.
			if (RequestedTransition->Transition.TargetState.IsCompletionState())
			{
				// Transition to a terminal state (succeeded/failed), or default transition failed.
				Exec.TreeRunStatus = RequestedTransition->Transition.TargetState.ToCompletionStatus();

				// Stop evaluators and global tasks (handled in ExitState)
				if (!ensure(Exec.ActiveFrames.Num() == 0))
				{
					StopEvaluatorsAndGlobalTasks(Exec.TreeRunStatus);

					// No active states or global tasks anymore, reset frames.
					Exec.ActiveFrames.Reset();
					RemoveAllDelegateListeners();
				}

				break;
			}

			// Enter state tasks can fail/succeed, treat it same as tick.
			const EMetaStoryRunStatus LastTickStatus = EnterState(RequestedTransition->Selection, RequestedTransition->Transition);

			RequestedTransition.Reset();

			Exec.LastTickStatus = LastTickStatus;

			// Report state completed immediately.
			if (Exec.LastTickStatus != EMetaStoryRunStatus::Running)
			{
				StateCompleted();
			}
		}

		// Stop as soon as have found a running state.
		if (Exec.LastTickStatus == EMetaStoryRunStatus::Running)
		{
			break;
		}
	}
}

void FMetaStoryExecutionContext::BeginApplyTransition(const FMetaStoryTransitionResult& InTransitionResult)
{
	if (FMetaStoryExecutionExtension* ExecExtension = GetMutableExecutionExtension())
	{
		ExecExtension->OnBeginApplyTransition({ Owner, RootMetaStory, Storage }, InTransitionResult);
	}
}

void FMetaStoryExecutionContext::BroadcastDelegate(const FMetaStoryDelegateDispatcher& Dispatcher)
{
	if (!Dispatcher.IsValid())
	{
		return;
	}

	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}

	const FMetaStoryExecutionFrame* CurrentFrame = GetCurrentlyProcessedFrame();
	check(CurrentFrame);

	GetExecState().DelegateActiveListeners.BroadcastDelegate(Dispatcher, GetExecState());
	if (UE::MetaStory::ExecutionContext::MarkDelegateAsBroadcasted(Dispatcher, *CurrentFrame, GetMutableInstanceData()->GetMutableStorage()))
	{
		ScheduleNextTick(UE::MetaStory::EMetaStoryTickReason::Delegate);
	}
}

// Deprecated
bool FMetaStoryExecutionContext::AddDelegateListener(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate)
{
	BindDelegate(Listener, MoveTemp(Delegate));
	return true;
}

void FMetaStoryExecutionContext::BindDelegate(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate)
{
	if (!Listener.IsValid())
	{
		// The listener is not bound to a dispatcher. It will never trigger the delegate.
		return;
	}

	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}

	const FMetaStoryExecutionFrame* CurrentFrame = GetCurrentlyProcessedFrame();
	if (CurrentFrame == nullptr)
	{
		return;
	}

	const UE::MetaStory::FActiveStateID CurrentlyProcessedStateID = CurrentFrame->ActiveStates.FindStateID(CurrentlyProcessedState);
	GetExecState().DelegateActiveListeners.Add(Listener, MoveTemp(Delegate), CurrentFrame->FrameID, CurrentlyProcessedStateID, FMetaStoryIndex16(CurrentNodeIndex));
}

// Deprecated
void FMetaStoryExecutionContext::RemoveDelegateListener(const FMetaStoryDelegateListener& Listener)
{
	UnbindDelegate(Listener);
}

void FMetaStoryExecutionContext::UnbindDelegate(const FMetaStoryDelegateListener& Listener)
{
	if (!Listener.IsValid())
	{
		return;
	}

	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}

	GetExecState().DelegateActiveListeners.Remove(Listener);
}

void FMetaStoryExecutionContext::ConsumeEvent(const FMetaStorySharedEvent& Event)
{
	if (EventQueue && EventQueue->ConsumeEvent(Event))
	{
		UE_METASTORY_DEBUG_EVENT_CONSUMED(this, Event);
	}
}

void FMetaStoryExecutionContext::RequestTransition(const FMetaStoryTransitionRequest& Request)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_RequestTransition);

	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}

	FMetaStoryExecutionState& Exec = GetExecState();

	if (bAllowDirectTransitions)
	{
		checkf(CurrentlyProcessedFrame, TEXT("Expecting CurrentlyProcessedFrame to be valid when called during TriggerTransitions()."));

		METASTORY_LOG(Verbose, TEXT("Request transition to '%s' at priority %s"), *GetSafeStateName(*CurrentlyProcessedFrame, Request.TargetState), *UEnum::GetDisplayValueAsText(Request.Priority).ToString());

		const FActiveStateID CurrentlyProcessedStateID = CurrentlyProcessedFrame->ActiveStates.FindStateID(CurrentlyProcessedState);
		const FStateHandleContext Target = FStateHandleContext(CurrentlyProcessedFrame->MetaStory, Request.TargetState);
		const bool bRequested = RequestTransitionInternal(*CurrentlyProcessedFrame, CurrentlyProcessedStateID, Target, FTransitionArguments{ .Priority = Request.Priority, .Fallback = Request.Fallback });
		if (bRequested)
		{
			check(RequestedTransition.IsValid());
			RequestedTransition->Source = FMetaStoryTransitionSource(CurrentlyProcessedFrame->MetaStory, EMetaStoryTransitionSourceType::ExternalRequest, Request.TargetState, Request.Priority);
		}
	}
	else if (Exec.ActiveFrames.Num() > 0)
	{
		const FMetaStoryExecutionFrame* RootFrame = &Exec.ActiveFrames[0];
		if (CurrentlyProcessedFrame)
		{
			RootFrame = CurrentlyProcessedFrame;
		}

		if (RootFrame->ActiveStates.Num() == 0)
		{
			METASTORY_LOG(Warning, TEXT("%hs: RequestTransition called on %s using MetaStory %s without an active state. Start() must be called before requesting a transition."),
				__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
			return;
		}

		METASTORY_LOG(Verbose, TEXT("Request transition to '%s' at priority %s"), *GetSafeStateName(*RootFrame, Request.TargetState), *UEnum::GetDisplayValueAsText(Request.Priority).ToString());

		FMetaStoryTransitionRequest RequestWithSource = Request;
		RequestWithSource.SourceFrameID = RootFrame->FrameID;

		const int32 ActiveStateIndex = RootFrame->ActiveStates.IndexOfReverse(CurrentlyProcessedState);
		RequestWithSource.SourceStateID = ActiveStateIndex != INDEX_NONE ? RootFrame->ActiveStates.StateIDs[ActiveStateIndex] : RootFrame->ActiveStates.StateIDs[0];

		InstanceData.AddTransitionRequest(&Owner, RequestWithSource);
		ScheduleNextTick(UE::MetaStory::EMetaStoryTickReason::TransitionRequest);
	}
	else
	{
		METASTORY_LOG(Warning, TEXT("%hs: RequestTransition called on %s using MetaStory %s without an active frame. Start() must be called before requesting a transition."),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}
}

void FMetaStoryExecutionContext::RequestTransition(FMetaStoryStateHandle InTargetState, EMetaStoryTransitionPriority InPriority, EMetaStorySelectionFallback InFallback)
{
	RequestTransition(FMetaStoryTransitionRequest(InTargetState, InPriority, InFallback));
}

void FMetaStoryExecutionContext::FinishTask(const FMetaStoryTaskBase& Task, EMetaStoryFinishTaskType FinishType)
{
	using namespace UE::MetaStory;

	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return;
	}

	// Like GetInstanceData, only accept task if we are currently processing.
	if (!ensure(CurrentNode == &Task))
	{
		return;
	}
	check(CurrentlyProcessedFrame);
	check(CurrentNodeIndex >= 0);

	FMetaStoryExecutionState& Exec = GetExecState();

	const UMetaStory* CurrentMetaStory = CurrentlyProcessedFrame->MetaStory;
	const ETaskCompletionStatus TaskStatus = ExecutionContext::CastToTaskStatus(FinishType);

	if (CurrentlyProcessedState.IsValid())
	{
		check(CurrentMetaStory->States.IsValidIndex(CurrentlyProcessedState.Index));
		const FMetaStoryCompactState& State = CurrentMetaStory->States[CurrentlyProcessedState.Index];

		const int32 ActiveStateIndex = CurrentlyProcessedFrame->ActiveStates.IndexOfReverse(CurrentlyProcessedState);
		check(ActiveStateIndex != INDEX_NONE);

		const int32 StateTaskIndex = CurrentNodeIndex - State.TasksBegin;
		check(StateTaskIndex >= 0);

		FTasksCompletionStatus StateTasksStatus = const_cast<FMetaStoryExecutionFrame*>(CurrentlyProcessedFrame)->ActiveTasksStatus.GetStatus(State);
		StateTasksStatus.SetStatusWithPriority(StateTaskIndex, TaskStatus);
		Exec.bHasPendingCompletedState = Exec.bHasPendingCompletedState || StateTasksStatus.IsCompleted();
	}
	else
	{
		// global frame
		const int32 FrameTaskIndex = CurrentNodeIndex - CurrentMetaStory->GlobalTasksBegin;
		check(FrameTaskIndex >= 0);
		FTasksCompletionStatus GlobalTasksStatus = const_cast<FMetaStoryExecutionFrame*>(CurrentlyProcessedFrame)->ActiveTasksStatus.GetStatus(CurrentMetaStory);
		GlobalTasksStatus.SetStatusWithPriority(FrameTaskIndex, TaskStatus);
		Exec.bHasPendingCompletedState = Exec.bHasPendingCompletedState || GlobalTasksStatus.IsCompleted();
	}
}

// Deprecated
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FMetaStoryExecutionContext::FinishTask(const UE::MetaStory::FFinishedTask& Task, EMetaStoryFinishTaskType FinishType)
{
	FMetaStoryExecutionState& Exec = GetExecState();
	FMetaStoryExecutionFrame* Frame = Exec.FindActiveFrame(Task.FrameID);
	if (Frame == nullptr)
	{
		return;
	}

	using namespace UE::MetaStory;

	const UE::MetaStory::ETaskCompletionStatus Status = ExecutionContext::CastToTaskStatus(Task.RunStatus);
	if (Task.Reason == FFinishedTask::EReasonType::GlobalTask)
	{
		if (Frame->bIsGlobalFrame)
		{
			Frame->ActiveTasksStatus.GetStatus(Frame->MetaStory).SetStatusWithPriority(Task.TaskIndex.AsInt32(), Status);
		}
	}
	else
	{
		const int32 FoundIndex = Frame->ActiveStates.IndexOfReverse(Task.StateID);
		if (FoundIndex != INDEX_NONE)
		{
			const FMetaStoryStateHandle StateHandle = Frame->ActiveStates[FoundIndex];
			const FMetaStoryCompactState* State = Frame->MetaStory->GetStateFromHandle(StateHandle);
			if (State != nullptr)
			{
				if (Task.Reason == FFinishedTask::EReasonType::InternalTransition)
				{
					Frame->ActiveTasksStatus.GetStatus(*State).SetCompletionStatus(Status);
				}
				else
				{
					check(Task.Reason == FFinishedTask::EReasonType::StateTask);
					Frame->ActiveTasksStatus.GetStatus(*State).SetStatusWithPriority(Task.TaskIndex.AsInt32(), Status);
				}
			}
		}
	}
}

// Deprecated
bool FMetaStoryExecutionContext::IsFinishedTaskValid(const UE::MetaStory::FFinishedTask& Task) const
{
	return false;
}

// Deprecated
void FMetaStoryExecutionContext::UpdateCompletedStateList()
{
}

// Deprecated
void FMetaStoryExecutionContext::MarkStateCompleted(UE::MetaStory::FFinishedTask& NewFinishedTask)
{
}

// Deprecated
void FMetaStoryExecutionContext::UpdateInstanceData(TConstArrayView<FMetaStoryExecutionFrame> CurrentActiveFrames, TArrayView<FMetaStoryExecutionFrame> NextActiveFrames)
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FMetaStoryExecutionContext::UpdateInstanceData(const TSharedPtr<FSelectStateResult>& Args)
{
	// Go though all the state and frame (includes unchanged, sustained, changed).
	//Keep 2 buffers. InstanceStructs for unchanged and sustained. TempInstanceStructs for changed.
	//1. Set the frame GlobalParameterDataHandle, GlobalInstanceIndexBase and ActiveInstanceIndexBase
	//2. Add instances for global frame.
	// Note that the global parameters are in a different buffer or it's last state parameter or the previous frame (see Start, SelectStateInternal_Linked, SelectStateInternal_LinkedAsset).
	//3. Add the instances for state parameters (Set the state StateParameterDataHandle).
	//4. Add instances for state tasks.
	//5. Remove the previous instances (that are not needed) and copy/move the new TempInstanceStructs.

	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	FMetaStoryExecutionState& Exec = GetExecState();

	// Estimate how many new instance data items we might have.
	int32 EstimatedNumStructs = 0;
	if (Args)
	{
		FActiveFrameID CurrentFrameID;
		const UMetaStory* CurrentMetaStory = nullptr;
		for (const FActiveState& SelectedState : Args->SelectedStates)
		{
			// Global
			if (SelectedState.GetFrameID() != CurrentFrameID)
			{
				CurrentFrameID = SelectedState.GetFrameID();

				const FMetaStoryExecutionFrame* Frame = FindExecutionFrame(CurrentFrameID, MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(Args->TemporaryFrames));
				if (ensure(Frame) && Frame->bIsGlobalFrame)
				{
					check(Frame->MetaStory);

					CurrentMetaStory = Frame->MetaStory;
					EstimatedNumStructs += CurrentMetaStory->NumGlobalInstanceData;
				}
			}

			// State
			if (ensure(CurrentMetaStory))
			{
				const FMetaStoryCompactState* CurrentState = CurrentMetaStory->GetStateFromHandle(SelectedState.GetStateHandle());
				if (ensure(CurrentState))
				{
					EstimatedNumStructs += CurrentState->InstanceDataNum;
				}
			}
		}
	}

	TArray<FConstStructView, FNonconcurrentLinearArrayAllocator> InstanceStructs;
	InstanceStructs.Reserve(EstimatedNumStructs);

	TArray<FInstancedStruct*, FNonconcurrentLinearArrayAllocator> TempInstanceStructs;
	TempInstanceStructs.Reserve(EstimatedNumStructs);

	constexpr int32 ExpectedAmountOfFrames = 8;
	TArray<FMetaStoryCompactParameters, TInlineAllocator<ExpectedAmountOfFrames, FNonconcurrentLinearArrayAllocator>> TempParams;

	TArrayView<FMetaStoryTemporaryInstanceData> TempInstances = Storage.GetMutableTemporaryInstances();
	auto FindInstanceTempData = [&TempInstances](FActiveFrameID FrameID, FMetaStoryDataHandle DataHandle)
		{
			FMetaStoryTemporaryInstanceData* TempData = TempInstances.FindByPredicate([&FrameID, &DataHandle](const FMetaStoryTemporaryInstanceData& Data)
				{
					return Data.FrameID == FrameID && Data.DataHandle == DataHandle;
				});
			return TempData ? &TempData->Instance : nullptr;
		};

	// Find next instance data sources and find common/existing section of instance data at start.
	int32 CurrentGlobalInstanceIndexBase = 0;
	int32 NumCommonInstanceData = 0;

	const UStruct* NextStateParameterDataStruct = nullptr;
	FMetaStoryDataHandle NextStateParameterDataHandle = FMetaStoryDataHandle::Invalid;

	FMetaStoryDataHandle CurrentGlobalParameterDataHandle = FMetaStoryDataHandle(EMetaStoryDataSourceType::GlobalParameterData);
	if (Args)
	{
		FActiveFrameID CurrentFrameID;
		FMetaStoryExecutionFrame* CurrentFrame = nullptr;
		const UMetaStory* CurrentMetaStory = nullptr;
		int32 CurrentFrameBaseIndex = 0;
		for (int32 SelectedStateIndex = 0; SelectedStateIndex < Args->SelectedStates.Num(); ++SelectedStateIndex)
		{
			const FActiveState& SelectedState = Args->SelectedStates[SelectedStateIndex];

			// Global
			if (SelectedState.GetFrameID() != CurrentFrameID)
			{
				CurrentFrameID = SelectedState.GetFrameID();
				CurrentFrame = Exec.FindActiveFrame(CurrentFrameID);

				// The frame/globals are common (before the transition or sustained from the transition)
				const bool bIsFrameCommon = CurrentFrame != nullptr;

				if (CurrentFrame == nullptr)
				{
					CurrentFrame = Args->FindTemporaryFrame(SelectedState.GetFrameID());
				}
				check(CurrentFrame);
				CurrentMetaStory = CurrentFrame->MetaStory;

				// Global Nodes
				if (CurrentFrame->bIsGlobalFrame)
				{
					// Handle global tree parameters
					if (NextStateParameterDataHandle.IsValid())
					{
						// Point to the parameter block set by linked state.
						check(NextStateParameterDataStruct == CurrentMetaStory->GetDefaultParameters().GetPropertyBagStruct());
						CurrentGlobalParameterDataHandle = NextStateParameterDataHandle;
						NextStateParameterDataHandle = FMetaStoryDataHandle::Invalid; // Mark as used.
					}

					const int32 BaseIndex = InstanceStructs.Num();
					CurrentGlobalInstanceIndexBase = BaseIndex;

					InstanceStructs.AddDefaulted(CurrentMetaStory->NumGlobalInstanceData);
					TempInstanceStructs.AddZeroed(CurrentMetaStory->NumGlobalInstanceData);

					// Global Evals
					for (int32 EvalIndex = CurrentMetaStory->EvaluatorsBegin; EvalIndex < (CurrentMetaStory->EvaluatorsBegin + CurrentMetaStory->EvaluatorsNum); EvalIndex++)
					{
						const FMetaStoryEvaluatorBase& Eval = CurrentMetaStory->Nodes[EvalIndex].Get<const FMetaStoryEvaluatorBase>();
						const FConstStructView EvalInstanceData = CurrentMetaStory->DefaultInstanceData.GetStruct(Eval.InstanceTemplateIndex.Get());
						InstanceStructs[BaseIndex + Eval.InstanceDataHandle.GetIndex()] = EvalInstanceData;
						if (!bIsFrameCommon)
						{
							TempInstanceStructs[BaseIndex + Eval.InstanceDataHandle.GetIndex()] = FindInstanceTempData(CurrentFrameID, Eval.InstanceDataHandle);
						}
					}

					// Global tasks
					for (int32 TaskIndex = CurrentMetaStory->GlobalTasksBegin; TaskIndex < (CurrentMetaStory->GlobalTasksBegin + CurrentMetaStory->GlobalTasksNum); TaskIndex++)
					{
						const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
						const FConstStructView TaskInstanceData = CurrentMetaStory->DefaultInstanceData.GetStruct(Task.InstanceTemplateIndex.Get());
						InstanceStructs[BaseIndex + Task.InstanceDataHandle.GetIndex()] = TaskInstanceData;
						if (!bIsFrameCommon)
						{
							TempInstanceStructs[BaseIndex + Task.InstanceDataHandle.GetIndex()] = FindInstanceTempData(CurrentFrameID, Task.InstanceDataHandle);
						}
					}

					if (bIsFrameCommon)
					{
						NumCommonInstanceData = InstanceStructs.Num();
					}
				}

				CurrentFrameBaseIndex = InstanceStructs.Num();

				CurrentFrame->GlobalParameterDataHandle = CurrentGlobalParameterDataHandle;
				CurrentFrame->GlobalInstanceIndexBase = FMetaStoryIndex16(CurrentGlobalInstanceIndexBase);
				CurrentFrame->ActiveInstanceIndexBase = FMetaStoryIndex16(CurrentFrameBaseIndex);
			}

			// State Params and Nodes
			if (ensure(CurrentMetaStory))
			{
				const FMetaStoryStateHandle CurrentStateHandle = SelectedState.GetStateHandle();
				const FMetaStoryCompactState* CurrentState = CurrentMetaStory->GetStateFromHandle(CurrentStateHandle);
				if (ensure(CurrentState))
				{
					// Find if the state is common (before the transition or sustained from the transition).
					//The frame may contain valid states (sustained or not). SelectedState will be added later.
					const bool bIsStateCommon = CurrentFrame->ActiveStates.Contains(SelectedState.GetStateID());

					InstanceStructs.AddDefaulted(CurrentState->InstanceDataNum);
					TempInstanceStructs.AddZeroed(CurrentState->InstanceDataNum);

					bool bCanHaveTempData = false;
					if (CurrentState->Type == EMetaStoryStateType::Subtree)
					{
						check(CurrentState->ParameterDataHandle.IsValid());
						check(CurrentState->ParameterTemplateIndex.IsValid());
						const FConstStructView ParamsInstanceData = CurrentMetaStory->DefaultInstanceData.GetStruct(CurrentState->ParameterTemplateIndex.Get());
						if (!NextStateParameterDataHandle.IsValid())
						{
							// Parameters are not set by a linked state, create instance data.
							InstanceStructs[CurrentFrameBaseIndex + CurrentState->ParameterDataHandle.GetIndex()] = ParamsInstanceData;
							CurrentFrame->StateParameterDataHandle = CurrentState->ParameterDataHandle;
							bCanHaveTempData = true;
						}
						else
						{
							// Point to the parameter block set by linked state.
							const FMetaStoryCompactParameters* Params = ParamsInstanceData.GetPtr<const FMetaStoryCompactParameters>();
							const UStruct* StateParameterDataStruct = Params ? Params->Parameters.GetPropertyBagStruct() : nullptr;
							check(NextStateParameterDataStruct == StateParameterDataStruct);

							CurrentFrame->StateParameterDataHandle = NextStateParameterDataHandle;
							NextStateParameterDataHandle = FMetaStoryDataHandle::Invalid; // Mark as used.

							// This state will not instantiate parameter data, so we don't care about the temp data either.
							bCanHaveTempData = false;
						}
					}
					else
					{
						if (CurrentState->ParameterTemplateIndex.IsValid())
						{
							// Linked state's instance data is the parameters.
							check(CurrentState->ParameterDataHandle.IsValid());

							const FMetaStoryCompactParameters* Params = nullptr;
							if (FInstancedStruct* TempParamsInstanceData = FindInstanceTempData(CurrentFrameID, CurrentState->ParameterDataHandle))
							{
								// If we have temp data for the parameters, then setup the instance data with just a type, so that we can steal the temp data below (TempInstanceStructs).
								// We expect overridden linked assets to hit this code path. 
								InstanceStructs[CurrentFrameBaseIndex + CurrentState->ParameterDataHandle.GetIndex()] = FConstStructView(TempParamsInstanceData->GetScriptStruct());
								Params = TempParamsInstanceData->GetPtr<const FMetaStoryCompactParameters>();
								bCanHaveTempData = true;
							}
							else
							{
								// If not temp data, use the states or linked assets default values.
								FConstStructView ParamsInstanceData;
								if (CurrentState->Type == EMetaStoryStateType::LinkedAsset)
								{
									// This state is a container for the linked MetaStory.
									// Its instance data matches the linked MetaStory parameters.
									// The linked MetaStory asset is the next frame.
									const bool bIsLastFrame = SelectedStateIndex == Args->SelectedStates.Num() - 1;
									if (!bIsLastFrame)
									{
										const FActiveState& FollowingSelectedState = Args->SelectedStates[SelectedStateIndex + 1];
										const FMetaStoryExecutionFrame* FollowingNextFrame = FindExecutionFrame(FollowingSelectedState.GetFrameID(), MakeArrayView(Exec.ActiveFrames), MakeArrayView(Args->TemporaryFrames));
										if (ensure(FollowingNextFrame))
										{
											ParamsInstanceData = FConstStructView::Make(TempParams.Emplace_GetRef(FollowingNextFrame->MetaStory->GetDefaultParameters()));
										}
									}
								}
								if (!ParamsInstanceData.IsValid())
								{
									ParamsInstanceData = CurrentMetaStory->DefaultInstanceData.GetStruct(CurrentState->ParameterTemplateIndex.Get());
								}
								InstanceStructs[CurrentFrameBaseIndex + CurrentState->ParameterDataHandle.GetIndex()] = ParamsInstanceData;
								Params = ParamsInstanceData.GetPtr<const FMetaStoryCompactParameters>();
								bCanHaveTempData = true;
							}

							if (CurrentState->Type == EMetaStoryStateType::Linked
								|| CurrentState->Type == EMetaStoryStateType::LinkedAsset)
							{
								// Store the index of the parameter data, so that we can point the linked state to it.
								check(CurrentState->ParameterDataHandle.GetSource() == EMetaStoryDataSourceType::StateParameterData);
								checkf(!NextStateParameterDataHandle.IsValid(), TEXT("NextStateParameterDataIndex not should be set yet when we encounter a linked state."));
								NextStateParameterDataHandle = CurrentState->ParameterDataHandle;
								NextStateParameterDataStruct = Params ? Params->Parameters.GetPropertyBagStruct() : nullptr;
							}
						}
					}

					if (!bIsStateCommon && bCanHaveTempData)
					{
						TempInstanceStructs[CurrentFrameBaseIndex + CurrentState->ParameterDataHandle.GetIndex()] = FindInstanceTempData(CurrentFrameID, CurrentState->ParameterDataHandle);
					}

					if (CurrentState->EventDataIndex.IsValid())
					{
						InstanceStructs[CurrentFrameBaseIndex + CurrentState->EventDataIndex.Get()] = FConstStructView(FMetaStorySharedEvent::StaticStruct());
					}

					for (int32 TaskIndex = CurrentState->TasksBegin; TaskIndex < (CurrentState->TasksBegin + CurrentState->TasksNum); TaskIndex++)
					{
						const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
						const FConstStructView TaskInstanceData = CurrentMetaStory->DefaultInstanceData.GetStruct(Task.InstanceTemplateIndex.Get());
						InstanceStructs[CurrentFrameBaseIndex + Task.InstanceDataHandle.GetIndex()] = TaskInstanceData;
						if (!bIsStateCommon)
						{
							TempInstanceStructs[CurrentFrameBaseIndex + Task.InstanceDataHandle.GetIndex()] = FindInstanceTempData(CurrentFrameID, Task.InstanceDataHandle);
						}
					}

					if (bIsStateCommon)
					{
						NumCommonInstanceData = InstanceStructs.Num();
					}
				}
			}
		}
	}

	// Common section should match.
#if WITH_METASTORY_DEBUG
	for (int32 Index = 0; Index < NumCommonInstanceData; Index++)
	{
		check(Index < InstanceData.Num());

		FConstStructView ExistingInstanceDataView = InstanceData.GetStruct(Index);
		FConstStructView NewInstanceDataView = InstanceStructs[Index];

		check(NewInstanceDataView.GetScriptStruct() == ExistingInstanceDataView.GetScriptStruct());

		const FMetaStoryInstanceObjectWrapper* ExistingWrapper = ExistingInstanceDataView.GetPtr<const FMetaStoryInstanceObjectWrapper>();
		const FMetaStoryInstanceObjectWrapper* NewWrapper = ExistingInstanceDataView.GetPtr<const FMetaStoryInstanceObjectWrapper>();
		if (ExistingWrapper && NewWrapper)
		{
			check(ExistingWrapper->InstanceObject && NewWrapper->InstanceObject);
			check(ExistingWrapper->InstanceObject->GetClass() == NewWrapper->InstanceObject->GetClass());
		}
	}
#endif

	// Remove instance data that was not common.
	InstanceData.ShrinkTo(NumCommonInstanceData);

	// Add new instance data.
	InstanceData.Append(Owner,
		MakeArrayView(InstanceStructs.GetData() + NumCommonInstanceData, InstanceStructs.Num() - NumCommonInstanceData),
		MakeArrayView(TempInstanceStructs.GetData() + NumCommonInstanceData, TempInstanceStructs.Num() - NumCommonInstanceData));

	InstanceData.ResetTemporaryInstances();
}

FMetaStoryDataView FMetaStoryExecutionContext::GetDataView(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle)
{
	switch (Handle.GetSource())
	{
	case EMetaStoryDataSourceType::ContextData:
		check(!ContextAndExternalDataViews.IsEmpty());
		return ContextAndExternalDataViews[Handle.GetIndex()];

	case EMetaStoryDataSourceType::EvaluationScopeInstanceData:
	case EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject:
	{
		// The data can be accessed in any of the caches (depends on how the binding was constructed), but it is most likely on top of the stack.
		for (int32 Index = EvaluationScopeInstanceCaches.Num() - 1; Index >= 0; --Index)
		{
			if (EvaluationScopeInstanceCaches[Index].MetaStory == CurrentFrame.MetaStory)
			{
				if (FMetaStoryDataView* Result = EvaluationScopeInstanceCaches[Index].Container->GetDataViewPtr(Handle))
				{
					return *Result;
				}
			}
		}
		checkf(false, TEXT("The evaluation scope instance data needs to be constructed before you can access it."));
		return nullptr;
	}

	case EMetaStoryDataSourceType::ExternalData:
		check(!ContextAndExternalDataViews.IsEmpty());
		return ContextAndExternalDataViews[CurrentFrame.ExternalDataBaseIndex.Get() + Handle.GetIndex()];

	case EMetaStoryDataSourceType::TransitionEvent:
	{
		if (CurrentlyProcessedTransitionEvent)
		{
			// const_cast because events are read only, but we cannot express that in FMetaStoryDataView.
			return FMetaStoryDataView(FStructView::Make(*const_cast<FMetaStoryEvent*>(CurrentlyProcessedTransitionEvent)));
		}

		return nullptr;
	}

	case EMetaStoryDataSourceType::StateEvent:
	{
		// If state selection is going, return FMetaStoryEvent of the event currently captured by the state selection.
		if (CurrentlyProcessedStateSelectionResult)
		{
			FSelectionEventWithID* FoundEvent = CurrentlyProcessedStateSelectionResult->SelectionEvents.FindByPredicate(
				[FrameID = CurrentFrame.FrameID, StateHandle = Handle.GetState()]
				(const FSelectionEventWithID& Event)
				{
					return Event.State.GetStateHandle() == StateHandle && Event.State.GetFrameID() == FrameID;
				});
			if (FoundEvent)
			{
				// Events are read only, but we cannot express that in FMetaStoryDataView.
				return FMetaStoryDataView(FStructView::Make(*FoundEvent->Event.GetMutable()));
			}
		}

		if (UE::MetaStory::InstanceData::Private::IsActiveInstanceHandleSourceValid(Storage, CurrentFrame, Handle))
		{
			return UE::MetaStory::InstanceData::GetDataView(Storage, CurrentlyProcessedSharedInstanceStorage, ParentFrame, CurrentFrame, Handle);
		}
	}

	case EMetaStoryDataSourceType::ExternalGlobalParameterData:
		checkf(false, TEXT("External global parameter data currently not supported for linked state-trees"));
		return nullptr;

	default:
		return UE::MetaStory::InstanceData::GetDataView(Storage, CurrentlyProcessedSharedInstanceStorage, ParentFrame, CurrentFrame, Handle);
	}
}

FMetaStoryDataView FMetaStoryExecutionContext::GetDataView(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo)
{
	const FMetaStoryDataHandle Handle = CopyInfo.SourceDataHandle.Get<FMetaStoryDataHandle>();
	if (Handle.GetSource() == EMetaStoryDataSourceType::ExternalGlobalParameterData)
	{
		return GetDataViewOrTemporary(ParentFrame, CurrentFrame, CopyInfo);
	}

	return GetDataView(ParentFrame, CurrentFrame, Handle);
}

FMetaStoryDataView FMetaStoryExecutionContext::GetExecutionRuntimeDataView() const
{
	check(CurrentNode && CurrentlyProcessedFrame);
	check(CurrentlyProcessedFrame->ExecutionRuntimeIndexBase.IsValid());

	if (!ensureMsgf(CurrentNode->ExecutionRuntimeTemplateIndex.IsValid(), TEXT("The node doesn't support execution runtime data.")))
	{
		return FMetaStoryDataView();
	}

	const UE::MetaStory::InstanceData::FMetaStoryInstanceContainer& Container = Storage.GetExecutionRuntimeData();
	const int32 ActiveIndex = CurrentlyProcessedFrame->ExecutionRuntimeIndexBase.Get() + CurrentNode->ExecutionRuntimeTemplateIndex.Get();

	if (!ensureMsgf(Container.IsValidIndex(ActiveIndex), TEXT("Invalid execution runtime data index.")))
	{
		return FMetaStoryDataView();
	}

	if (Container.IsObject(ActiveIndex))
	{
		return Container.GetMutableObject(ActiveIndex);
	}
	else
	{
		return const_cast<UE::MetaStory::InstanceData::FMetaStoryInstanceContainer&>(Container).GetMutableStruct(ActiveIndex);
	}
}

EMetaStoryRunStatus FMetaStoryExecutionContext::ForceTransition(const FMetaStoryRecordedTransitionResult& RecordedTransition)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	TArray<FStateHandleContext, FNonconcurrentLinearArrayAllocator> StateContexts;
	StateContexts.Reserve(RecordedTransition.States.Num());
	for (const FMetaStoryRecordedActiveState& RecordedActiveState : RecordedTransition.States)
	{
		if (RecordedActiveState.MetaStory == nullptr)
		{
			return EMetaStoryRunStatus::Unset;
		}
		StateContexts.Add(FStateHandleContext(RecordedActiveState.MetaStory, RecordedActiveState.State));
	}

	TSharedRef<FSelectStateResult> SelectStateResult = MakeShared<FSelectStateResult>();
	TGuardValue<TSharedPtr<FSelectStateResult>> TemporaryStorageScope(CurrentlyProcessedTemporaryStorage, SelectStateResult);
	if (!ForceTransitionInternal(StateContexts, SelectStateResult))
	{
		UE_METASTORY_DEBUG_LOG_EVENT(this, Verbose, TEXT("The force transition failed."));
		return EMetaStoryRunStatus::Unset;
	}

	if (SelectStateResult->SelectedStates.Num() != StateContexts.Num())
	{
		UE_METASTORY_DEBUG_LOG_EVENT(this, Verbose, TEXT("The force transition failed to find the target."));
		return EMetaStoryRunStatus::Unset;
	}

	TArray<FMetaStorySharedEvent, FNonconcurrentLinearArrayAllocator> SharedEvents;
	SharedEvents.Reserve(RecordedTransition.Events.Num());
	for (const FMetaStoryEvent& SelectedEvent : RecordedTransition.Events)
	{
		SharedEvents.Add(FMetaStorySharedEvent(SelectedEvent));
	}
	check(RecordedTransition.Events.Num() == SharedEvents.Num());
	for (int32 RecordedStateIndex = 0; RecordedStateIndex < RecordedTransition.States.Num(); ++RecordedStateIndex)
	{
		const FMetaStoryRecordedActiveState& RecordedActiveState = RecordedTransition.States[RecordedStateIndex];
		if (SharedEvents.IsValidIndex(RecordedActiveState.EventIndex))
		{
			const FActiveState& SelectedState = SelectStateResult->SelectedStates[RecordedStateIndex];
			SelectStateResult->SelectionEvents.Add(FSelectionEventWithID{.State = SelectedState, .Event= SharedEvents[RecordedActiveState.EventIndex]});
		}
	}

	FMetaStoryTransitionResult TransitionResult;
	// The SourceFrameID and SourceStateID are left uninitialized on purpose.
	TransitionResult.TargetState = StateContexts.Last().StateHandle;
	TransitionResult.CurrentState = FMetaStoryStateHandle::Invalid;
	TransitionResult.CurrentRunStatus = GetExecState().LastTickStatus;
	TransitionResult.ChangeType = EMetaStoryStateChangeType::Changed;
	TransitionResult.Priority = RecordedTransition.Priority;

	BeginApplyTransition(TransitionResult);

	ExitState(SelectStateResult, TransitionResult);
	return EnterState(SelectStateResult, TransitionResult);
}

namespace UE::MetaStory::ExecutionContext::Private
{
	bool TestStateContextPath(TNotNull<const FMetaStoryExecutionContext*> ExecutionContext, const TArrayView<const FStateHandleContext> StateContexts)
	{
		const UMetaStory* PreviousMetaStory = ExecutionContext->GetMetaStory();
		bool bNewTree = true;
		bool bNewFrame = true;
		for (int32 Index = 0; Index < StateContexts.Num(); ++Index)
		{
			const FStateHandleContext& StateContext = StateContexts[Index];
			if (StateContext.MetaStory == nullptr || StateContext.MetaStory != PreviousMetaStory)
			{
				// Child state has to have the same MetaStory.
				return false;
			}

			const FMetaStoryCompactState* State = StateContext.MetaStory->GetStateFromHandle(StateContext.StateHandle);
			if (State == nullptr)
			{
				// The handle do not exist in the asset.
				return false;
			}
			if (bNewFrame || bNewTree)
			{
				if (StateContext.MetaStory->GetFrameFromHandle(StateContext.StateHandle) == nullptr)
				{
					// The state is a new frame (from a linked or linked asset) but is not compiled as a frame.
					return false;
				}
			}
			if (!bNewFrame)
			{
				if (State->Parent != StateContexts[Index - 1].StateHandle)
				{
					// Current state is not a child of previous state.
					return false;
				}
			}
			bNewFrame = false;
			bNewTree = false;

			if (State->Type == EMetaStoryStateType::LinkedAsset)
			{
				bNewTree = true;
				bNewFrame = true;
				if (Index + 1 >= StateContexts.Num())
				{
					// The path cannot end with a linked asset state.
					return false;
				}
				const FStateHandleContext& NextStateContext = StateContexts[Index + 1];
				if (NextStateContext.MetaStory == nullptr
					|| !NextStateContext.MetaStory->IsReadyToRun()
					|| !NextStateContext.MetaStory->HasCompatibleContextData(ExecutionContext->GetMetaStory())
					|| NextStateContext.MetaStory->GetSchema()->GetClass() != ExecutionContext->GetMetaStory()->GetSchema()->GetClass())
				{
					// The trees have to be compatible.
					return false;
				}
				PreviousMetaStory = NextStateContext.MetaStory;
			}
			if (State->Type == EMetaStoryStateType::Linked)
			{
				bNewFrame = true;
			}
		}
		if (bNewFrame || bNewTree)
		{
			// The path cannot end with a linked or linked asset state.
			return false;
		}

		return true;
	}
}

bool FMetaStoryExecutionContext::ForceTransitionInternal(const TArrayView<const UE::MetaStory::ExecutionContext::FStateHandleContext> StateContexts, const TSharedRef<FSelectStateResult>& OutSelectionResult)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	if (!IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: MetaStory context is not initialized properly ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return false;
	}

	if (StateContexts.IsEmpty())
	{
		UE_METASTORY_DEBUG_LOG_EVENT(this, Verbose, TEXT("There are no states in the transition."));
		return false;
	}

	if (!TestStateContextPath(this, StateContexts))
	{
		UE_METASTORY_DEBUG_LOG_EVENT(this, Verbose, TEXT("The StateContexts is invalid."));
		return false;
	}

	const FMetaStoryExecutionState& Exec = GetExecState();

	// A reentrant call to ForceTransition or a call from Start, Tick or Stop must be deferred.
	if (Exec.CurrentPhase != EMetaStoryUpdatePhase::Unset)
	{
		UE_METASTORY_DEBUG_LOG_EVENT(this, Warning, TEXT("Can't force a transition while %s"), *UEnum::GetDisplayValueAsText(Exec.CurrentPhase).ToString());
		return false;
	}

	if (!CollectActiveExternalData())
	{
		METASTORY_LOG(Warning, TEXT("%hs: Failed to collect external data ('%s' using MetaStory '%s')"),
			__FUNCTION__, *GetNameSafe(&Owner), *GetFullNameSafe(&RootMetaStory));
		return false;
	}

	FActiveStateInlineArray CurrentActiveStatePath;
	GetActiveStatePath(Exec.ActiveFrames, CurrentActiveStatePath);
	if (CurrentActiveStatePath.Num() == 0)
	{
		UE_METASTORY_DEBUG_LOG_EVENT(this, Verbose, TEXT("The tree needs active states to transition from."));
		return false;
	}

	FSelectStateArguments SelectStateArgs;
	SelectStateArgs.ActiveStates = MakeConstArrayView(CurrentActiveStatePath);
	SelectStateArgs.SourceState = SelectStateArgs.ActiveStates[0];
	SelectStateArgs.TargetState = StateContexts.Last();
	SelectStateArgs.Behavior = ESelectStateBehavior::Forced;
	SelectStateArgs.SelectionRules = RootMetaStory.StateSelectionRules;

	FSelectStateInternalArguments InternalArgs;
	InternalArgs.MissingActiveStates = MakeConstArrayView(CurrentActiveStatePath);
	InternalArgs.MissingSourceFrameID = Exec.ActiveFrames[0].FrameID;
	InternalArgs.MissingSourceStates = MakeConstArrayView(CurrentActiveStatePath).Left(1);
	InternalArgs.MissingStatesToReachTarget = MakeConstArrayView(StateContexts);

	FSelectStateResult SelectStateResult;
	if (!SelectStateInternal(SelectStateArgs, InternalArgs, OutSelectionResult))
	{
		UE_METASTORY_DEBUG_LOG_EVENT(this, Verbose, TEXT("The force selection to target failed."));
		return false;
	}
	return true;
}

const FMetaStoryExecutionFrame* FMetaStoryExecutionContext::FindFrame(const UMetaStory* MetaStory, FMetaStoryStateHandle RootState, TConstArrayView<FMetaStoryExecutionFrame> Frames, const FMetaStoryExecutionFrame*& OutParentFrame)
{
	const int32 FrameIndex = Frames.IndexOfByPredicate([MetaStory, RootState](const FMetaStoryExecutionFrame& Frame)
		{
			return Frame.HasRoot(MetaStory, RootState);
		});

	if (FrameIndex == INDEX_NONE)
	{
		OutParentFrame = nullptr;
		return nullptr;
	}

	if (FrameIndex > 0)
	{
		OutParentFrame = &Frames[FrameIndex - 1];
	}

	return &Frames[FrameIndex];
}

bool FMetaStoryExecutionContext::IsHandleSourceValid(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle) const
{
	switch (Handle.GetSource())
	{
	case EMetaStoryDataSourceType::None:
		return true;

	case EMetaStoryDataSourceType::ContextData:
		return true;

	case EMetaStoryDataSourceType::EvaluationScopeInstanceData:
	case EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject:
		// The data can be accessed in any of the caches (depends on how the binding was constructed), but it is most likely on top of the stack.
		for (int32 Index = EvaluationScopeInstanceCaches.Num() - 1; Index >= 0; --Index)
		{
			if (EvaluationScopeInstanceCaches[Index].MetaStory == CurrentFrame.MetaStory)
			{
				if (FMetaStoryDataView* Result = EvaluationScopeInstanceCaches[Index].Container->GetDataViewPtr(Handle))
				{
					return true;
				}
			}
		}
		return false;

	case EMetaStoryDataSourceType::ExternalData:
		return CurrentFrame.ExternalDataBaseIndex.IsValid()
			&& ContextAndExternalDataViews.IsValidIndex(CurrentFrame.ExternalDataBaseIndex.Get() + Handle.GetIndex());

	case EMetaStoryDataSourceType::TransitionEvent:
		return CurrentlyProcessedTransitionEvent != nullptr;

	case EMetaStoryDataSourceType::StateEvent:
		return CurrentlyProcessedStateSelectionResult != nullptr
			|| UE::MetaStory::InstanceData::Private::IsActiveInstanceHandleSourceValid(Storage, CurrentFrame, Handle);

	case EMetaStoryDataSourceType::ExternalGlobalParameterData:
	{
		checkf(false, TEXT("External global parameter data currently not supported for linked state-trees"));
		break;
	}

	default:
		return UE::MetaStory::InstanceData::Private::IsHandleSourceValid(Storage, ParentFrame, CurrentFrame, Handle);
	}

	return false;
}

bool FMetaStoryExecutionContext::IsHandleSourceValid(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo) const
{
	const FMetaStoryDataHandle Handle = CopyInfo.SourceDataHandle.Get<FMetaStoryDataHandle>();
	if (Handle.GetSource() == EMetaStoryDataSourceType::ExternalGlobalParameterData)
	{
		return ExternalGlobalParameters ? ExternalGlobalParameters->Find(CopyInfo) != nullptr : false;
	}

	return IsHandleSourceValid(ParentFrame, CurrentFrame, Handle);
}

FMetaStoryDataView FMetaStoryExecutionContext::GetDataViewOrTemporary(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle)
{
	if (IsHandleSourceValid(ParentFrame, CurrentFrame, Handle))
	{
		return GetDataView(ParentFrame, CurrentFrame, Handle);
	}

	return GetTemporaryDataView(ParentFrame, CurrentFrame, Handle);
}

FMetaStoryDataView FMetaStoryExecutionContext::GetDataViewOrTemporary(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo)
{
	const FMetaStoryDataHandle Handle = CopyInfo.SourceDataHandle.Get<FMetaStoryDataHandle>();
	if (Handle.GetSource() == EMetaStoryDataSourceType::ExternalGlobalParameterData)
	{
		uint8* MemoryPtr = ExternalGlobalParameters->Find(CopyInfo);
		return FMetaStoryDataView(CopyInfo.SourceStructType, MemoryPtr);
	}

	return GetDataViewOrTemporary(ParentFrame, CurrentFrame, Handle);
}

FMetaStoryDataView FMetaStoryExecutionContext::GetTemporaryDataView(const FMetaStoryExecutionFrame* ParentFrame,
	const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle)
{
	switch (Handle.GetSource())
	{
	case EMetaStoryDataSourceType::ExternalGlobalParameterData:
		checkf(false, TEXT("External global parameter data currently not supported for linked state-trees"));
		return {};
	case EMetaStoryDataSourceType::EvaluationScopeInstanceData:
	case EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject:
		ensureMsgf(false, TEXT("The evaluation scope instance data needs to be constructed before you can access it."));
		return {};

	default:
		return UE::MetaStory::InstanceData::Private::GetTemporaryDataView(Storage, ParentFrame, CurrentFrame, Handle);
	}
}

FMetaStoryDataView FMetaStoryExecutionContext::AddTemporaryInstance(const FMetaStoryExecutionFrame& Frame, const FMetaStoryIndex16 OwnerNodeIndex, const FMetaStoryDataHandle DataHandle, FConstStructView NewInstanceData)
{
	const FStructView NewInstance = Storage.AddTemporaryInstance(Owner, Frame, OwnerNodeIndex, DataHandle, NewInstanceData);
	if (FMetaStoryInstanceObjectWrapper* Wrapper = NewInstance.GetPtr<FMetaStoryInstanceObjectWrapper>())
	{
		return FMetaStoryDataView(Wrapper->InstanceObject);
	}
	return NewInstance;
}

void FMetaStoryExecutionContext::PushEvaluationScopeInstanceContainer(UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer& Container, const FMetaStoryExecutionFrame& Frame)
{
	EvaluationScopeInstanceCaches.Emplace(&Container, Frame.MetaStory);
}

void FMetaStoryExecutionContext::PopEvaluationScopeInstanceContainer(UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer& Container)
{
	if (ensure(EvaluationScopeInstanceCaches.Num() > 0 && EvaluationScopeInstanceCaches.Last().Container == &Container))
	{
		EvaluationScopeInstanceCaches.Pop();
	}
}

void FMetaStoryExecutionContext::CopyAllBindingsOnActiveInstances(const ECopyBindings CopyType)
{
	FMetaStoryExecutionState& Exec = GetExecState();
	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); FrameIndex++)
	{
		FMetaStoryExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;
		const int32 CurrentActiveNodeIndex = CurrentFrame.ActiveNodeIndex.AsInt32();

		FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);

		const bool bShouldCallOnEvaluatorsAndGlobalTasks = CurrentFrame.bIsGlobalFrame;
		if (bShouldCallOnEvaluatorsAndGlobalTasks)
		{
			const int32 EvaluatorEnd = CurrentMetaStory->EvaluatorsBegin + CurrentMetaStory->EvaluatorsNum;
			for (int32 EvalIndex = CurrentMetaStory->EvaluatorsBegin; EvalIndex < EvaluatorEnd; ++EvalIndex)
			{
				if (EvalIndex <= CurrentActiveNodeIndex)
				{
					const FMetaStoryEvaluatorBase& Eval = CurrentMetaStory->Nodes[EvalIndex].Get<const FMetaStoryEvaluatorBase>();
					if (Eval.BindingsBatch.IsValid())
					{
						const FMetaStoryDataView EvalInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Eval.InstanceDataHandle);
						FNodeInstanceDataScope DataScope(*this, &Eval, EvalIndex, Eval.InstanceDataHandle, EvalInstanceView);
						CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, EvalInstanceView, Eval.BindingsBatch);
					}
				}
			}

			const int32 GlobalTasksEnd = CurrentMetaStory->GlobalTasksBegin + CurrentMetaStory->GlobalTasksNum;
			for (int32 TaskIndex = CurrentMetaStory->GlobalTasksBegin; TaskIndex < GlobalTasksEnd; ++TaskIndex)
			{
				if (TaskIndex <= CurrentActiveNodeIndex)
				{
					const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
					const bool bTaskRequestCopy = (CopyType == ECopyBindings::ExitState && Task.bShouldCopyBoundPropertiesOnExitState)
						|| (CopyType == ECopyBindings::EnterState)
						|| (CopyType == ECopyBindings::Tick && Task.bShouldCopyBoundPropertiesOnTick);
					if (bTaskRequestCopy && Task.BindingsBatch.IsValid())
					{
						const FMetaStoryDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
						FNodeInstanceDataScope DataScope(*this, &Task, TaskIndex, Task.InstanceDataHandle, TaskInstanceView);
						CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskInstanceView, Task.BindingsBatch);
					}
				}
			}
		}

		for (int32 StateIndex = 0; StateIndex < CurrentFrame.ActiveStates.Num(); ++StateIndex)
		{
			const FMetaStoryStateHandle CurrentStateHandle = CurrentFrame.ActiveStates.States[StateIndex];
			const UE::MetaStory::FActiveStateID CurrentStateID = CurrentFrame.ActiveStates.StateIDs[StateIndex];
			const FMetaStoryCompactState& CurrentState = CurrentMetaStory->States[CurrentStateHandle.Index];

			FCurrentlyProcessedStateScope StateScope(*this, CurrentStateHandle);

			if (CurrentState.Type == EMetaStoryStateType::Linked
				|| CurrentState.Type == EMetaStoryStateType::LinkedAsset)
			{
				if (CurrentState.ParameterDataHandle.IsValid()
					&& CurrentState.ParameterBindingsBatch.IsValid())
				{
					const FMetaStoryDataView StateParamsDataView = GetDataView(CurrentParentFrame, CurrentFrame, CurrentState.ParameterDataHandle);
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, StateParamsDataView, CurrentState.ParameterBindingsBatch);
				}
			}

			const int32 TasksEnd = CurrentState.TasksBegin + CurrentState.TasksNum;
			for (int32 TaskIndex = CurrentState.TasksBegin; TaskIndex < TasksEnd; ++TaskIndex)
			{
				if (TaskIndex <= CurrentActiveNodeIndex)
				{
					const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();

					const bool bTaskRequestCopy = (CopyType == ECopyBindings::ExitState && Task.bShouldCopyBoundPropertiesOnExitState)
						|| (CopyType == ECopyBindings::EnterState)
						|| (CopyType == ECopyBindings::Tick && Task.bShouldCopyBoundPropertiesOnTick);
					if (bTaskRequestCopy && Task.BindingsBatch.IsValid())
					{
						const FMetaStoryDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
						CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskInstanceView, Task.BindingsBatch);
					}
				}
			}
		}
	}
}

bool FMetaStoryExecutionContext::CopyBatchOnActiveInstances(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataView TargetView, const FMetaStoryIndex16 BindingsBatch)
{
	constexpr bool bOnActiveInstances = true;
	return CopyBatchInternal<bOnActiveInstances>(ParentFrame, CurrentFrame, TargetView, BindingsBatch);
}

bool FMetaStoryExecutionContext::CopyBatchWithValidation(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataView TargetView, const FMetaStoryIndex16 BindingsBatch)
{
	constexpr bool bOnActiveInstances = false;
	return CopyBatchInternal<bOnActiveInstances>(ParentFrame, CurrentFrame, TargetView, BindingsBatch);
}

template<bool bOnActiveInstances>
bool FMetaStoryExecutionContext::CopyBatchInternal(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataView TargetView, const FMetaStoryIndex16 BindingsBatch)
{
	using namespace UE::MetaStory::InstanceData;
	using namespace UE::MetaStory::ExecutionContext::Private;

	const FPropertyBindingCopyInfoBatch& Batch = CurrentFrame.MetaStory->PropertyBindings.Super::GetBatch(BindingsBatch);
	check(TargetView.GetStruct() == Batch.TargetStruct.Get().Struct);

	FEvaluationScopeInstanceContainer EvaluationScopeContainer;
	bool bEvaluationScopeContainerPushed = false;
	if (Batch.PropertyFunctionsBegin != Batch.PropertyFunctionsEnd)
	{
		check(Batch.PropertyFunctionsBegin.IsValid() && Batch.PropertyFunctionsEnd.IsValid());

		const FEvaluationScopeInstanceContainer::FMemoryRequirement& MemoryRequirement = CurrentFrame.MetaStory->PropertyFunctionEvaluationScopeMemoryRequirements[BindingsBatch.Get()];
		if (MemoryRequirement.Size > 0)
		{
			void* FunctionBindingMemory = FMemory_Alloca_Aligned(MemoryRequirement.Size, MemoryRequirement.Alignment);
			EvaluationScopeContainer = FEvaluationScopeInstanceContainer(FunctionBindingMemory, MemoryRequirement);
			PushEvaluationScopeInstanceContainer(EvaluationScopeContainer, CurrentFrame);
			InitEvaluationScopeInstanceData(EvaluationScopeContainer, CurrentFrame.MetaStory, Batch.PropertyFunctionsBegin.Get(), Batch.PropertyFunctionsEnd.Get());
			bEvaluationScopeContainerPushed = true;
		}

		if constexpr (bOnActiveInstances)
		{
			EvaluatePropertyFunctionsOnActiveInstances(ParentFrame, CurrentFrame, FMetaStoryIndex16(Batch.PropertyFunctionsBegin), Batch.PropertyFunctionsEnd.Get() - Batch.PropertyFunctionsBegin.Get());
		}
		else
		{
			EvaluatePropertyFunctionsWithValidation(ParentFrame, CurrentFrame, FMetaStoryIndex16(Batch.PropertyFunctionsBegin), Batch.PropertyFunctionsEnd.Get() - Batch.PropertyFunctionsBegin.Get());
		}
	}

	bool bSucceed = true;
	for (const FPropertyBindingCopyInfo& Copy : CurrentFrame.MetaStory->PropertyBindings.Super::GetBatchCopies(Batch))
	{
		if constexpr (bOnActiveInstances)
		{
			const FMetaStoryDataView SourceView = GetDataView(ParentFrame, CurrentFrame, Copy);
			bSucceed &= CurrentFrame.MetaStory->PropertyBindings.Super::CopyProperty(Copy, SourceView, TargetView);
		}
		else
		{
			const FMetaStoryDataView SourceView = GetDataViewOrTemporary(ParentFrame, CurrentFrame, Copy);
			if (!SourceView.IsValid())
			{
				bSucceed = false;
				break;
			}

			bSucceed &= CurrentFrame.MetaStory->PropertyBindings.Super::CopyProperty(Copy, SourceView, TargetView);
		}
	}

	if (bEvaluationScopeContainerPushed)
	{
		PopEvaluationScopeInstanceContainer(EvaluationScopeContainer);
	}

	return bSucceed;
}

bool FMetaStoryExecutionContext::CollectActiveExternalData()
{
	return CollectActiveExternalData(GetExecState().ActiveFrames);
}

bool FMetaStoryExecutionContext::CollectActiveExternalData(const TArrayView<FMetaStoryExecutionFrame> Frames)
{
	if (bActiveExternalDataCollected)
	{
		return true;
	}

	bool bAllExternalDataValid = true;
	const FMetaStoryExecutionFrame* PrevFrame = nullptr;

	for (FMetaStoryExecutionFrame& Frame : Frames)
	{
		if (PrevFrame && PrevFrame->MetaStory == Frame.MetaStory)
		{
			Frame.ExternalDataBaseIndex = PrevFrame->ExternalDataBaseIndex;
		}
		else
		{
			Frame.ExternalDataBaseIndex = CollectExternalData(Frame.MetaStory);
		}

		if (!Frame.ExternalDataBaseIndex.IsValid())
		{
			bAllExternalDataValid = false;
		}

		PrevFrame = &Frame;
	}

	if (bAllExternalDataValid)
	{
		bActiveExternalDataCollected = true;
	}

	return bAllExternalDataValid;
}

FMetaStoryIndex16 FMetaStoryExecutionContext::CollectExternalData(const UMetaStory* MetaStory)
{
	if (!MetaStory)
	{
		return FMetaStoryIndex16::Invalid;
	}

	// If one of the active states share the same MetaStory, get the external data from there.
	for (const FCollectedExternalDataCache& Cache : CollectedExternalCache)
	{
		if (Cache.MetaStory == MetaStory)
		{
			return Cache.BaseIndex;
		}
	}

	const TConstArrayView<FMetaStoryExternalDataDesc> ExternalDataDescs = MetaStory->GetExternalDataDescs();
	const int32 BaseIndex = ContextAndExternalDataViews.Num();
	const int32 NumDescs = ExternalDataDescs.Num();
	FMetaStoryIndex16 Result(BaseIndex);

	if (NumDescs > 0)
	{
		ContextAndExternalDataViews.AddDefaulted(NumDescs);
		const TArrayView<FMetaStoryDataView> DataViews = MakeArrayView(ContextAndExternalDataViews.GetData() + BaseIndex, NumDescs);

		if (ensureMsgf(CollectExternalDataDelegate.IsBound(), TEXT("The MetaStory asset has external data, expecting CollectExternalData delegate to be provided.")))
		{
			if (!CollectExternalDataDelegate.Execute(*this, MetaStory, MetaStory->GetExternalDataDescs(), DataViews))
			{
				// The caller is responsible for error reporting. 
				return FMetaStoryIndex16::Invalid;
			}
		}

		// Check that the data is valid and present.
		for (int32 Index = 0; Index < NumDescs; Index++)
		{
			const FMetaStoryExternalDataDesc& DataDesc = ExternalDataDescs[Index];
			const FMetaStoryDataView& DataView = ContextAndExternalDataViews[BaseIndex + Index];

			if (DataDesc.Requirement == EMetaStoryExternalDataRequirement::Required)
			{
				// Required items must have valid pointer of the expected type.  
				if (!DataView.IsValid() || !DataDesc.IsCompatibleWith(DataView))
				{
					Result = FMetaStoryIndex16::Invalid;
					break;
				}
			}
			else
			{
				// Optional items must have same type if they are set.
				if (DataView.IsValid() && !DataDesc.IsCompatibleWith(DataView))
				{
					Result = FMetaStoryIndex16::Invalid;
					break;
				}
			}
		}
	}

	if (!Result.IsValid())
	{
		// Rollback
		ContextAndExternalDataViews.SetNum(BaseIndex);
	}

	// Cached both succeeded and failed attempts.
	CollectedExternalCache.Add({ MetaStory, Result });

	return FMetaStoryIndex16(Result);
}

bool FMetaStoryExecutionContext::SetGlobalParameters(const FInstancedPropertyBag& Parameters)
{
	if (ensureMsgf(RootMetaStory.GetDefaultParameters().GetPropertyBagStruct() == Parameters.GetPropertyBagStruct(),
		TEXT("Parameters must be of the same struct type. Make sure to migrate the provided parameters to the same type as the MetaStory default parameters.")))
	{
		Storage.SetGlobalParameters(Parameters);
		return true;
	}

	return false;
}

// Deprecated
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FMetaStoryExecutionContext::CaptureNewStateEvents(TConstArrayView<FMetaStoryExecutionFrame> PrevFrames, TConstArrayView<FMetaStoryExecutionFrame> NewFrames, TArrayView<FMetaStoryFrameStateSelectionEvents> FramesStateSelectionEvents)
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FMetaStoryExecutionContext::CaptureNewStateEvents(const TSharedRef<const FSelectStateResult>& Args)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext::Private;

	FMetaStoryExecutionState& Exec = GetExecState();

	// Mark the events from delayed transitions as in use, so that each State will receive unique copy of the event struct. 
	TArray<FMetaStorySharedEvent, TInlineAllocator<16>> EventsInUse;
	for (const FMetaStoryTransitionDelayedState& DelayedTransition : Exec.DelayedTransitions)
	{
		if (DelayedTransition.CapturedEvent.IsValid())
		{
			EventsInUse.Add(DelayedTransition.CapturedEvent);
		}
	}

	// For each state that are changed (not sustained)
	FActiveFrameID CurrentFrameID;
	const FMetaStoryExecutionFrame* CurrentFrame = nullptr;
	for (int32 SelectedStateIndex = 0; SelectedStateIndex < Args->SelectedStates.Num(); ++SelectedStateIndex)
	{
		const FActiveState& SelectedState = Args->SelectedStates[SelectedStateIndex];

		// Global
		if (SelectedState.GetFrameID() != CurrentFrameID)
		{
			CurrentFrameID = SelectedState.GetFrameID();
			CurrentFrame = FindExecutionFrame(SelectedState.GetFrameID(), MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(Args->TemporaryFrames));
		}

		check(CurrentFrame);
		const bool bIsStateCommon = CurrentFrame->ActiveStates.Contains(SelectedState.GetStateID());
		if (bIsStateCommon)
		{
			continue;
		}

		if (const FMetaStoryCompactState* State = CurrentFrame->MetaStory->GetStateFromHandle(SelectedState.GetStateHandle()))
		{
			if (State->EventDataIndex.IsValid())
			{
				FMetaStorySharedEvent& MetaStoryEvent = Storage.GetMutableStruct(CurrentFrame->ActiveInstanceIndexBase.Get() + State->EventDataIndex.Get()).Get<FMetaStorySharedEvent>();
				const FSelectionEventWithID* EventToCapturePtr = Args->SelectionEvents.FindByPredicate([SelectedState](const FSelectionEventWithID& EventID)
					{
						return EventID.State == SelectedState;
					});
				if (ensureAlways(EventToCapturePtr))
				{
					const FMetaStorySharedEvent& EventToCapture = EventToCapturePtr->Event;
					if (EventsInUse.Contains(EventToCapture))
					{
						// Event is already spoken for, make a copy.
						MetaStoryEvent = FMetaStorySharedEvent(*EventToCapture);
					}
					else
					{
						// Event not in use, steal it.
						MetaStoryEvent = EventToCapture;
						EventsInUse.Add(EventToCapture);
					}
				}
			}
		}
	}
}

// Deprecated
EMetaStoryRunStatus FMetaStoryExecutionContext::EnterState(FMetaStoryTransitionResult& Transition)
{
	return EMetaStoryRunStatus::Failed;
}

EMetaStoryRunStatus FMetaStoryExecutionContext::EnterState(const TSharedPtr<FSelectStateResult>& ArgsPtr, const FMetaStoryTransitionResult& Transition)
{
	// 1. Update the data instance data of all frames and selected states.
	// The data won't be available until the node is reached or the state is added to the FMetaStoryExecutionFrame::ActiveState.
	// 2. Capture StateEvents
	// 3. Call Enter state for each selected state from Target.
	//Note, no need to update the bindings for previously active state, the bindings are updated in ExitState or there was no binding before (See Start).

	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_EnterState);

	if (!ArgsPtr.IsValid())
	{
		TSharedPtr<FSelectStateResult> EmptySelectStateResult;
		UpdateInstanceData(EmptySelectStateResult);
		return EMetaStoryRunStatus::Failed;
	}

	FSelectStateResult& Args = *ArgsPtr;
	FMetaStoryExecutionState& Exec = GetExecState();

	if (bRecordTransitions)
	{
		RecordedTransitions.Add(MakeRecordedTransitionResult(ArgsPtr.ToSharedRef(), Transition));
	}

	UpdateInstanceData(ArgsPtr);
	if (Args.SelectedStates.IsEmpty())
	{
		return EMetaStoryRunStatus::Failed;
	}
	CaptureNewStateEvents(ArgsPtr.ToSharedRef());

	++Exec.StateChangeCount;

	FMetaStoryTransitionResult CurrentTransition = Transition;
	EMetaStoryRunStatus Result = EMetaStoryRunStatus::Running;

	METASTORY_LOG(Log, TEXT("Enter state '%s' (%d)")
		, *UE::MetaStory::ExecutionContext::Private::GetStatePathAsString(&RootMetaStory, Args.SelectedStates)
		, Exec.StateChangeCount
	);
	UE_METASTORY_DEBUG_ENTER_PHASE(this, EMetaStoryUpdatePhase::EnterStates);

	bool bTargetReached = false;
	FActiveFrameID CurrentFrameID;
	const TArrayView<const UE::MetaStory::FActiveState> SelectedStates = Args.SelectedStates;
	for (int32 SelectedStateIndex = 0; SelectedStateIndex < SelectedStates.Num(); ++SelectedStateIndex)
	{
		const UE::MetaStory::FActiveState& SelectedState = SelectedStates[SelectedStateIndex];

		const bool bIsCurrentFrameNew = CurrentFrameID != SelectedState.GetFrameID();
		CurrentFrameID = SelectedState.GetFrameID();

		bool bFrameAdded = false;
		int32 FrameIndex = Exec.IndexOfActiveFrame(SelectedState.GetFrameID());
		if (FrameIndex == INDEX_NONE)
		{
			check(bIsCurrentFrameNew);
			FMetaStoryExecutionFrame* FoundNewFrame = Args.FindTemporaryFrame(SelectedState.GetFrameID());
			if (!ensure(FoundNewFrame != nullptr))
			{
				return EMetaStoryRunStatus::Failed;
			}

			// Add it to the active list
			FrameIndex = Exec.ActiveFrames.Add(MoveTemp(*FoundNewFrame));
			bFrameAdded = true;
		}

		FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];

		int32 ActiveStateIndex = CurrentFrame.ActiveStates.IndexOfReverse(SelectedState.GetStateID());
		// The state is the target of current transition or is a sustained state
		const bool bAreCommon = ActiveStateIndex != INDEX_NONE;
		bTargetReached = bTargetReached || SelectedState == Args.TargetState;
		const bool bSustainedState = bAreCommon && bTargetReached;

		// States which were active before and will remain active, but are not on target branch will not get
		// EnterState called. That is, a transition is handled as "replan from this state".
		if (bAreCommon && !bSustainedState)
		{
			// Is already in the active and we do not need to call EnterState with Sustained
			continue;
		}

		FMetaStoryExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;

		// Create the status. We lost the previous tasks' completion status.
		if (!ensureMsgf(CurrentFrame.ActiveTasksStatus.IsValid(), TEXT("Frame is not formed correct.")))
		{
			const FMetaStoryCompactFrame* FrameInfo = CurrentFrame.MetaStory->GetFrameFromHandle(CurrentFrame.RootState);
			ensureAlwaysMsgf(FrameInfo, TEXT("The compiled data is invalid. It should contains the information for the root frame."));
			CurrentFrame.ActiveTasksStatus = FrameInfo ? FMetaStoryTasksCompletionStatus(*FrameInfo) : FMetaStoryTasksCompletionStatus();
		}

		if (bIsCurrentFrameNew && !bFrameAdded && !CurrentFrame.bHaveEntered)
		{
			// EnterState on Changed global was called during selection with StartTemporaryEvaluatorsAndGlobalTasks
			CurrentTransition.CurrentState = FMetaStoryStateHandle::Invalid;
			CurrentTransition.ChangeType = EMetaStoryStateChangeType::Sustained;

			EMetaStoryRunStatus GlobalTaskRunStatus = StartGlobalsForFrameOnActiveInstances(CurrentParentFrame, CurrentFrame, CurrentTransition);
			Result = GetPriorityRunStatus(Result, GlobalTaskRunStatus);
			if (Result == EMetaStoryRunStatus::Failed)
			{
				break;
			}
		}

		FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
		{
			const FMetaStoryStateHandle CurrentStateHandle = SelectedState.GetStateHandle();
			const FMetaStoryCompactState& CurrentState = CurrentMetaStory->States[CurrentStateHandle.Index];
			const UE::MetaStory::FActiveStateID CurrentStateID = SelectedState.GetStateID();

			checkf(CurrentState.bEnabled, TEXT("Only enabled states are in SelectedStates."));

			// New state. Add it
			if (!bSustainedState)
			{
				CurrentFrame.ActiveTasksStatus.Push(CurrentState);
				if (!CurrentFrame.ActiveStates.Push(CurrentStateHandle, CurrentStateID))
				{
					// The ActiveStates array supports a max of 8 states. The depth is verified at compilation.
					ensureMsgf(false, TEXT("Reached max execution depth when trying to enter state '%s'. '%s' using MetaStory '%s'."),
						*GetStateStatusString(Exec),
						*GetNameSafe(&Owner),
						*GetFullNameSafe(&RootMetaStory)
					);
					break;
				}
				ActiveStateIndex = CurrentFrame.ActiveStates.Num() - 1;
			}

			FCurrentlyProcessedStateScope StateScope(*this, CurrentStateHandle);

			if (CurrentState.Type == EMetaStoryStateType::Linked
				|| CurrentState.Type == EMetaStoryStateType::LinkedAsset)
			{
				if (CurrentState.ParameterDataHandle.IsValid()
					&& CurrentState.ParameterBindingsBatch.IsValid())
				{
					const FMetaStoryDataView StateParamsDataView = GetDataView(CurrentParentFrame, CurrentFrame, CurrentState.ParameterDataHandle);
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, StateParamsDataView, CurrentState.ParameterBindingsBatch);
				}
			}

			const EMetaStoryStateChangeType ChangeType = bSustainedState ? EMetaStoryStateChangeType::Sustained : EMetaStoryStateChangeType::Changed;
			CurrentTransition.CurrentState = CurrentStateHandle;
			CurrentTransition.ChangeType = ChangeType;

			UE_METASTORY_DEBUG_STATE_EVENT(this, CurrentStateHandle, EMetaStoryTraceEventType::OnEntering);
			METASTORY_LOG(Log, TEXT("%*sState '%s' (%s)"),
				(FrameIndex + ActiveStateIndex + 1) * UE::MetaStory::Debug::IndentSize, TEXT(""),
				*GetSafeStateName(CurrentMetaStory, CurrentStateHandle),
				*UEnum::GetDisplayValueAsText(CurrentTransition.ChangeType).ToString()
			);

			// @todo: this needs to support EvaluationData Scope 
			// Call state change events on conditions if needed.
			if (CurrentState.bHasStateChangeConditions)
			{
				const int32 EnterConditionsEnd = CurrentState.EnterConditionsBegin + CurrentState.EnterConditionsNum;
				for (int32 ConditionIndex = CurrentState.EnterConditionsBegin; ConditionIndex < EnterConditionsEnd; ++ConditionIndex)
				{
					const FMetaStoryConditionBase& Cond = CurrentMetaStory->Nodes[ConditionIndex].Get<const FMetaStoryConditionBase>();
					if (Cond.bHasShouldCallStateChangeEvents)
					{
						const bool bShouldCallEnterState = CurrentTransition.ChangeType == EMetaStoryStateChangeType::Changed
							|| (CurrentTransition.ChangeType == EMetaStoryStateChangeType::Sustained && Cond.bShouldStateChangeOnReselect);

						if (bShouldCallEnterState)
						{
							const FMetaStoryDataView ConditionInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Cond.InstanceDataHandle);
							FNodeInstanceDataScope DataScope(*this, &Cond, ConditionIndex, Cond.InstanceDataHandle, ConditionInstanceView);

							if (Cond.BindingsBatch.IsValid())
							{
								CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, ConditionInstanceView, Cond.BindingsBatch);
							}

							UE_METASTORY_DEBUG_CONDITION_ENTER_STATE(this, CurrentFrame.MetaStory, FMetaStoryIndex16(ConditionIndex));
							Cond.EnterState(*this, CurrentTransition);

							// Reset copied properties that might contain object references.
							if (Cond.BindingsBatch.IsValid())
							{
								CurrentFrame.MetaStory->PropertyBindings.Super::ResetObjects(Cond.BindingsBatch, ConditionInstanceView);
							}
						}
					}
				}
			}

			// Activate tasks on current state.
			UE::MetaStory::FTasksCompletionStatus CurrentStateTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(CurrentState);
			for (int32 StateTaskIndex = 0; StateTaskIndex < CurrentState.TasksNum; ++StateTaskIndex)
			{
				const int32 AssetTaskIndex = CurrentState.TasksBegin + StateTaskIndex;
				const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[AssetTaskIndex].Get<const FMetaStoryTaskBase>();
				const FMetaStoryDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);

				// Ignore disabled task
				if (Task.bTaskEnabled == false)
				{
					METASTORY_LOG(VeryVerbose, TEXT("%*sSkipped 'EnterState' for disabled Task: '%s'"), UE::MetaStory::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
					continue;
				}

				FNodeInstanceDataScope DataScope(*this, &Task, AssetTaskIndex, Task.InstanceDataHandle, TaskInstanceView);

				CurrentFrame.ActiveNodeIndex = FMetaStoryIndex16(AssetTaskIndex);

				// Copy bound properties.
				if (Task.BindingsBatch.IsValid())
				{
					CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskInstanceView, Task.BindingsBatch);
				}

				const bool bShouldCallEnterState = CurrentTransition.ChangeType == EMetaStoryStateChangeType::Changed
					|| (CurrentTransition.ChangeType == EMetaStoryStateChangeType::Sustained && Task.bShouldStateChangeOnReselect);

				if (bShouldCallEnterState)
				{
					METASTORY_LOG(Verbose, TEXT("%*sTask '%s'.EnterState()"), (FrameIndex + ActiveStateIndex + 1) * UE::MetaStory::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
					UE_METASTORY_DEBUG_TASK_ENTER_STATE(this, CurrentMetaStory, FMetaStoryIndex16(AssetTaskIndex));

					EMetaStoryRunStatus TaskRunStatus = EMetaStoryRunStatus::Unset;
					{
						QUICK_SCOPE_CYCLE_COUNTER(MetaStory_Task_EnterState);
						CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_Task_EnterState);

						TaskRunStatus = Task.EnterState(*this, CurrentTransition);
					}

					UE::MetaStory::ETaskCompletionStatus TaskStatus = CastToTaskStatus(TaskRunStatus);
					TaskStatus = CurrentStateTasksStatus.SetStatusWithPriority(StateTaskIndex, TaskStatus);

					TaskRunStatus = CastToRunStatus(TaskStatus);
					if (TaskRunStatus != EMetaStoryRunStatus::Failed && Task.OutputBindingsBatch.IsValid())
					{
						CopyBatchOnActiveInstances(CurrentlyProcessedParentFrame, *CurrentlyProcessedFrame, TaskInstanceView, Task.OutputBindingsBatch);
					}

					UE_METASTORY_DEBUG_TASK_EVENT(this, AssetTaskIndex, TaskInstanceView, EMetaStoryTraceEventType::OnEntered, TaskRunStatus);

					if (CurrentStateTasksStatus.IsConsideredForCompletion(StateTaskIndex))
					{
						Result = GetPriorityRunStatus(Result, TaskRunStatus);
						if (Result == EMetaStoryRunStatus::Failed)
						{
							break;
						}
					}
				}
			}
			UE_METASTORY_DEBUG_STATE_EVENT(this, CurrentStateHandle, EMetaStoryTraceEventType::OnEntered);
		}

		if (Result == EMetaStoryRunStatus::Failed)
		{
			break;
		}
	}

	UE_METASTORY_DEBUG_EXIT_PHASE(this, EMetaStoryUpdatePhase::EnterStates);
	UE_METASTORY_DEBUG_ACTIVE_STATES_EVENT(this, Exec.ActiveFrames);

	Exec.bHasPendingCompletedState = Result != EMetaStoryRunStatus::Running;
	return Result;
}

// Deprecated
void FMetaStoryExecutionContext::ExitState(const FMetaStoryTransitionResult& Transition)
{
}

void FMetaStoryExecutionContext::ExitState(const TSharedPtr<const FSelectStateResult>& Args, const FMetaStoryTransitionResult& Transition)
{
	// 1. Copy all bindings on all active nodes.
	// 2. Call ExitState on all active global tasks, global evaluators, state tasks and state condition that are affected by the transition.
	//  If the frame/state is before the target
	//    then it is not affected.
	//  Else
	//    Excluding the target, if the active state is not in the new selection, then call ExitState with EMetaStoryStateChangeType::Changed.
	//    Including the target, if the active state is in the new selection then call ExitState with EMetaStoryStateChangeType::Sustained.
	// 3. If the state/frame is not in the new selection, clean up memory, delegate...
	//  If the owning frame receive an ExitState, then frame globals will also receive an ExitState.
	//  It will be sustained if the owning frame is sustained.

	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_ExitState);

	FMetaStoryExecutionState& Exec = GetExecState();
	if (Exec.ActiveFrames.IsEmpty())
	{
		return;
	}

	CopyAllBindingsOnActiveInstances(ECopyBindings::ExitState);

	METASTORY_LOG(Log, TEXT("Exit state '%s' (%d)"), *DebugGetStatePath(Exec.ActiveFrames), Exec.StateChangeCount);
	UE_METASTORY_DEBUG_SCOPED_PHASE(this, EMetaStoryUpdatePhase::ExitStates);

	FActiveStateInlineArray ActiveStates;
	GetActiveStatePath(GetExecState().ActiveFrames, ActiveStates);

	const FActiveState ArgsTargetState = Args ? Args->TargetState : FActiveState();
	const TArrayView<const FActiveState> CommonStates = Args != nullptr
		? FActiveStatePath::Intersect(MakeConstArrayView(ActiveStates), MakeConstArrayView(Args->SelectedStates))
		: TArrayView<const FActiveState>();
	const TArrayView<const FActiveFrameID> ArgsSelectedFrames = Args != nullptr
		? MakeConstArrayView(Args->SelectedFrames)
		: TArrayView<const FActiveFrameID>();
	const TArrayView<const FActiveState> ChangedStates = MakeConstArrayView(ActiveStates).Mid(CommonStates.Num());
	const bool bHasSustained = CommonStates.Contains(ArgsTargetState);

	bool bContinue = true;
	FMetaStoryTransitionResult CurrentTransition = Transition;

	auto ExitPreviousFrame = [this, &Exec, &ArgsSelectedFrames, &CurrentTransition](const int32 FrameIndex, const bool bCallExit)
		{
			FMetaStoryExecutionFrame& Frame = Exec.ActiveFrames[FrameIndex];
			const bool bIsFrameCommon = ArgsSelectedFrames.Contains(Frame.FrameID);
			if (Frame.bIsGlobalFrame && (bCallExit || !bIsFrameCommon))
			{
				const bool bIsFrameSustained = bIsFrameCommon;
				FMetaStoryExecutionFrame* ParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
				CurrentTransition.CurrentState = FMetaStoryStateHandle::Invalid;
				CurrentTransition.ChangeType = bIsFrameSustained ? EMetaStoryStateChangeType::Sustained : EMetaStoryStateChangeType::Changed;
				StopGlobalsForFrameOnActiveInstances(ParentFrame, Frame, CurrentTransition);
			}

			if (!bIsFrameCommon)
			{
				checkf(Frame.ActiveStates.Num() == 0, TEXT("All states must received ExitState first."));
				CleanFrame(Exec, Frame.FrameID);
				Exec.ActiveFrames.RemoveAt(FrameIndex, EAllowShrinking::No);
			}
		};

	int32 FrameIndex = Exec.ActiveFrames.Num() - 1;
	for (; bContinue && FrameIndex >= 0; --FrameIndex)
	{
		FMetaStoryExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;

		FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
		for (int32 StateIndex = CurrentFrame.ActiveStates.Num() - 1; bContinue && StateIndex >= 0; --StateIndex)
		{
			const FMetaStoryStateHandle CurrentHandle = CurrentFrame.ActiveStates[StateIndex];
			const FActiveStateID CurrentStateID = CurrentFrame.ActiveStates.StateIDs[StateIndex];
			const FMetaStoryCompactState& CurrentState = CurrentMetaStory->States[CurrentHandle.Index];

			const FActiveState CurrentActiveState(CurrentFrame.FrameID, CurrentStateID, CurrentHandle);
			const bool bIsStateCommon = !ChangedStates.Contains(CurrentActiveState);

			// It is in the common list and there is no "target" (everything else is new, a new state was created).
			if (bIsStateCommon && !bHasSustained)
			{
				// It will also stop the frame loop.
				bContinue = false;
				break;
			}

			// It is the target.
			if (bIsStateCommon && bHasSustained && CurrentActiveState == ArgsTargetState)
			{
				// this is the last sustained
				bContinue = false;
			}

			const bool bIsStateSustained = bIsStateCommon;

			CurrentTransition.CurrentState = CurrentHandle;
			CurrentTransition.ChangeType = bIsStateSustained ? EMetaStoryStateChangeType::Sustained : EMetaStoryStateChangeType::Changed;

			METASTORY_LOG(Log, TEXT("%*sState '%s' (%s)"), (FrameIndex + StateIndex + 1) * UE::MetaStory::Debug::IndentSize, TEXT("")
				, *GetSafeStateName(CurrentFrame, CurrentHandle)
				, *UEnum::GetDisplayValueAsText(CurrentTransition.ChangeType).ToString());

			FCurrentlyProcessedStateScope StateScope(*this, CurrentHandle);
			UE_METASTORY_DEBUG_STATE_EVENT(this, CurrentHandle, EMetaStoryTraceEventType::OnExiting);

			for (int32 TaskIndex = (CurrentState.TasksBegin + CurrentState.TasksNum) - 1; TaskIndex >= CurrentState.TasksBegin; --TaskIndex)
			{
				const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();

				// Ignore disabled task
				if (Task.bTaskEnabled)
				{
					// Call task completed only if EnterState() was called.
					// The task order in the tree (BF) allows us to use the comparison.
					if (TaskIndex <= CurrentFrame.ActiveNodeIndex.AsInt32())
					{
						const FMetaStoryDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
						FNodeInstanceDataScope DataScope(*this, &Task, TaskIndex, Task.InstanceDataHandle, TaskInstanceView);

						const bool bShouldCallStateChange = CurrentTransition.ChangeType == EMetaStoryStateChangeType::Changed
							|| (CurrentTransition.ChangeType == EMetaStoryStateChangeType::Sustained && Task.bShouldStateChangeOnReselect);

						if (bShouldCallStateChange)
						{
							METASTORY_LOG(Verbose, TEXT("%*sTask '%s'.ExitState()"), (FrameIndex + StateIndex + 1) * UE::MetaStory::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
							UE_METASTORY_DEBUG_TASK_EXIT_STATE(this, CurrentMetaStory, FMetaStoryIndex16(TaskIndex));
							{
								QUICK_SCOPE_CYCLE_COUNTER(MetaStory_Task_ExitState);
								CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_Task_ExitState);
								Task.ExitState(*this, CurrentTransition);
							}

							if (Task.OutputBindingsBatch.IsValid())
							{
								CopyBatchOnActiveInstances(CurrentlyProcessedParentFrame, CurrentFrame, TaskInstanceView, Task.OutputBindingsBatch);
							}

							UE_METASTORY_DEBUG_TASK_EVENT(this, TaskIndex, TaskInstanceView, EMetaStoryTraceEventType::OnExited, Transition.CurrentRunStatus);
						}
					}
				}
				else
				{
					METASTORY_LOG(VeryVerbose, TEXT("%*sSkipped 'ExitState' for disabled Task: '%s'"), UE::MetaStory::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
				}
				CurrentFrame.ActiveNodeIndex = FMetaStoryIndex16(TaskIndex - 1);
			}

			// @todo: this needs to support EvaluationScoped Data
			// Call state change events on conditions if needed.
			if (CurrentState.bHasStateChangeConditions)
			{
				for (int32 ConditionIndex = (CurrentState.EnterConditionsBegin + CurrentState.EnterConditionsNum) - 1; ConditionIndex >= CurrentState.EnterConditionsBegin; --ConditionIndex)
				{
					const FMetaStoryConditionBase& Cond = CurrentFrame.MetaStory->Nodes[ConditionIndex].Get<const FMetaStoryConditionBase>();
					if (Cond.bHasShouldCallStateChangeEvents)
					{
						const bool bShouldCallStateChange = CurrentTransition.ChangeType == EMetaStoryStateChangeType::Changed
							|| (CurrentTransition.ChangeType == EMetaStoryStateChangeType::Sustained && Cond.bShouldStateChangeOnReselect);

						if (bShouldCallStateChange)
						{
							const FMetaStoryDataView ConditionInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Cond.InstanceDataHandle);
							FNodeInstanceDataScope DataScope(*this, &Cond, ConditionIndex, Cond.InstanceDataHandle, ConditionInstanceView);

							if (Cond.BindingsBatch.IsValid())
							{
								CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, ConditionInstanceView, Cond.BindingsBatch);
							}

							UE_METASTORY_DEBUG_CONDITION_EXIT_STATE(this, CurrentFrame.MetaStory, FMetaStoryIndex16(ConditionIndex));
							Cond.ExitState(*this, CurrentTransition);

							// Reset copied properties that might contain object references.
							if (Cond.BindingsBatch.IsValid())
							{
								CurrentFrame.MetaStory->PropertyBindings.Super::ResetObjects(Cond.BindingsBatch, ConditionInstanceView);
							}
						}
					}
				}
			}

			// Reset the completed state. This is to keep the wrong UE5.6 behavior.
			if (!EnumHasAnyFlags(CurrentFrame.MetaStory->GetStateSelectionRules(), EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates)
				&& bIsStateSustained)
			{
				CurrentFrame.ActiveTasksStatus.GetStatus(CurrentState).ResetStatus(CurrentState.TasksNum);
			}

			// Remove the state from the active states if it "changed".
			if (!bIsStateSustained)
			{
				CleanState(Exec, CurrentStateID);

				// Remove state
				const FMetaStoryStateHandle PoppedState = CurrentFrame.ActiveStates.Pop();
				check(PoppedState == CurrentHandle);
			}

			UE_METASTORY_DEBUG_STATE_EVENT(this, CurrentHandle, EMetaStoryTraceEventType::OnExited);
		}

		// The previous frame is not in the new
		ExitPreviousFrame(FrameIndex, bContinue);
	}
}

void FMetaStoryExecutionContext::RemoveAllDelegateListeners()
{
	GetExecState().DelegateActiveListeners = FMetaStoryDelegateActiveListeners();
}

void FMetaStoryExecutionContext::StateCompleted()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_StateCompleted);

	const FMetaStoryExecutionState& Exec = GetExecState();

	if (Exec.ActiveFrames.IsEmpty())
	{
		return;
	}

	METASTORY_LOG(Verbose, TEXT("State Completed %s (%d)"), *UEnum::GetDisplayValueAsText(Exec.LastTickStatus).ToString(), Exec.StateChangeCount);
	UE_METASTORY_DEBUG_SCOPED_PHASE(this, EMetaStoryUpdatePhase::StateCompleted);

	// Call from child towards root to allow to pass results back.
	// Note: Completed is assumed to be called immediately after tick or enter state, we want to preserve the status of instance data for tasks.
	for (int32 FrameIndex = Exec.ActiveFrames.Num() - 1; FrameIndex >= 0; FrameIndex--)
	{
		const FMetaStoryExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		const FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;
		const int32 CurrentActiveNodeIndex = CurrentFrame.ActiveNodeIndex.AsInt32();

		FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);

		for (int32 StateIndex = CurrentFrame.ActiveStates.Num() - 1; StateIndex >= 0; StateIndex--)
		{
			const FMetaStoryStateHandle CurrentHandle = CurrentFrame.ActiveStates[StateIndex];
			const FMetaStoryCompactState& State = CurrentMetaStory->States[CurrentHandle.Index];

			FCurrentlyProcessedStateScope StateScope(*this, CurrentHandle);
			UE_METASTORY_DEBUG_STATE_EVENT(this, CurrentHandle, EMetaStoryTraceEventType::OnStateCompleted);
			METASTORY_LOG(Verbose, TEXT("%*sState '%s'"), (FrameIndex + StateIndex + 1) * UE::MetaStory::Debug::IndentSize, TEXT(""), *GetSafeStateName(CurrentFrame, CurrentHandle));

			// Notify Tasks
			for (int32 TaskIndex = (State.TasksBegin + State.TasksNum) - 1; TaskIndex >= State.TasksBegin; TaskIndex--)
			{
				// Call task completed only if EnterState() was called.
				if (TaskIndex <= CurrentActiveNodeIndex)
				{
					const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();

					// Ignore disabled task
					if (Task.bTaskEnabled == false)
					{
						METASTORY_LOG(VeryVerbose, TEXT("%*sSkipped 'StateCompleted' for disabled Task: '%s'"), UE::MetaStory::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
						continue;
					}

					const FMetaStoryDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
					FNodeInstanceDataScope DataScope(*this, &Task, TaskIndex, Task.InstanceDataHandle, TaskInstanceView);

					METASTORY_LOG(Verbose, TEXT("%*sTask '%s'.StateCompleted()"), (FrameIndex + StateIndex + 1) * UE::MetaStory::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
					Task.StateCompleted(*this, Exec.LastTickStatus, CurrentFrame.ActiveStates);
				}
			}

			// @todo: this needs to support EvaluationScopedData
			// Call state change events on conditions if needed.
			if (State.bHasStateChangeConditions)
			{
				for (int32 ConditionIndex = (State.EnterConditionsBegin + State.EnterConditionsNum) - 1; ConditionIndex >= State.EnterConditionsBegin; ConditionIndex--)
				{
					const FMetaStoryConditionBase& Cond = CurrentFrame.MetaStory->Nodes[ConditionIndex].Get<const FMetaStoryConditionBase>();
					if (Cond.bHasShouldCallStateChangeEvents)
					{
						const FMetaStoryDataView ConditionInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Cond.InstanceDataHandle);
						FNodeInstanceDataScope DataScope(*this, &Cond, ConditionIndex, Cond.InstanceDataHandle, ConditionInstanceView);

						if (Cond.BindingsBatch.IsValid())
						{
							CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, ConditionInstanceView, Cond.BindingsBatch);
						}

						Cond.StateCompleted(*this, Exec.LastTickStatus, CurrentFrame.ActiveStates);

						// Reset copied properties that might contain object references.
						if (Cond.BindingsBatch.IsValid())
						{
							CurrentFrame.MetaStory->PropertyBindings.Super::ResetObjects(Cond.BindingsBatch, ConditionInstanceView);
						}
					}
				}
			}
		}
	}
}

void FMetaStoryExecutionContext::TickGlobalEvaluatorsForFrameOnActiveInstances(const float DeltaTime, const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& Frame)
{
	constexpr bool bOnActiveInstances = true;
	TickGlobalEvaluatorsForFrameInternal<bOnActiveInstances>(DeltaTime, ParentFrame, Frame);
}

void FMetaStoryExecutionContext::TickGlobalEvaluatorsForFrameWithValidation(const float DeltaTime, const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& Frame)
{
	constexpr bool bOnActiveInstances = false;
	TickGlobalEvaluatorsForFrameInternal<bOnActiveInstances>(DeltaTime, ParentFrame, Frame);
}

template<bool bOnActiveInstances>
void FMetaStoryExecutionContext::TickGlobalEvaluatorsForFrameInternal(const float DeltaTime, const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& Frame)
{
	check(Frame.bIsGlobalFrame);

	const UMetaStory* CurrentMetaStory = Frame.MetaStory;
	const int32 EvaluatorEnd = CurrentMetaStory->EvaluatorsBegin + CurrentMetaStory->EvaluatorsNum;
	if (CurrentMetaStory->EvaluatorsBegin < EvaluatorEnd)
	{
		FCurrentlyProcessedFrameScope FrameScope(*this, ParentFrame, Frame);

		for (int32 EvalIndex = CurrentMetaStory->EvaluatorsBegin; EvalIndex < EvaluatorEnd; ++EvalIndex)
		{
			const FMetaStoryEvaluatorBase& Eval = CurrentMetaStory->Nodes[EvalIndex].Get<const FMetaStoryEvaluatorBase>();
			FMetaStoryDataView EvalInstanceView = bOnActiveInstances
				? GetDataView(ParentFrame, Frame, Eval.InstanceDataHandle)
				: GetDataViewOrTemporary(ParentFrame, Frame, Eval.InstanceDataHandle);
			FNodeInstanceDataScope DataScope(*this, &Eval, EvalIndex, Eval.InstanceDataHandle, EvalInstanceView);

			// Copy bound properties.
			if (Eval.BindingsBatch.IsValid())
			{
				if constexpr (bOnActiveInstances)
				{
					CopyBatchOnActiveInstances(ParentFrame, Frame, EvalInstanceView, Eval.BindingsBatch);
				}
				else
				{
					CopyBatchWithValidation(ParentFrame, Frame, EvalInstanceView, Eval.BindingsBatch);
				}
			}

			METASTORY_LOG(VeryVerbose, TEXT("  Tick: '%s'"), *Eval.Name.ToString());
			UE_METASTORY_DEBUG_EVALUATOR_TICK(this, CurrentMetaStory, EvalIndex);
			{
				QUICK_SCOPE_CYCLE_COUNTER(MetaStory_Eval_Tick);
				Eval.Tick(*this, DeltaTime);
				UE_METASTORY_DEBUG_EVALUATOR_EVENT(this, EvalIndex, EvalInstanceView, EMetaStoryTraceEventType::OnTicked);
			}

			// Copy bound properties.
			if (Eval.OutputBindingsBatch.IsValid())
			{
				if constexpr (bOnActiveInstances)
				{
					CopyBatchOnActiveInstances(ParentFrame, Frame, EvalInstanceView, Eval.OutputBindingsBatch);
				}
				else
				{
					CopyBatchWithValidation(ParentFrame, Frame, EvalInstanceView, Eval.OutputBindingsBatch);
				}
			}
		}
	}
}

EMetaStoryRunStatus FMetaStoryExecutionContext::TickEvaluatorsAndGlobalTasks(const float DeltaTime, const bool bTickGlobalTasks)
{
	// When a global task is completed it completes the tree execution.
	// A global task can complete async. See CompletedStates.
	// When a global task fails, stop ticking the following tasks.

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_TickEvaluators);
	UE_METASTORY_DEBUG_SCOPED_PHASE(this, EMetaStoryUpdatePhase::TickingGlobalTasks);

	METASTORY_LOG(VeryVerbose, TEXT("Ticking Evaluators & Global Tasks"));

	FMetaStoryExecutionState& Exec = GetExecState();

	EMetaStoryRunStatus Result = EMetaStoryRunStatus::Running;

	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
	{
		FMetaStoryExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		if (CurrentFrame.bIsGlobalFrame)
		{
			FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);

			const EMetaStoryRunStatus FrameResult = TickEvaluatorsAndGlobalTasksForFrame(DeltaTime, bTickGlobalTasks, FrameIndex, CurrentParentFrame, &CurrentFrame);
			Result = UE::MetaStory::ExecutionContext::GetPriorityRunStatus(Result, FrameResult);

			if (Result == EMetaStoryRunStatus::Failed)
			{
				break;
			}
		}
	}

	Exec.bHasPendingCompletedState = Exec.bHasPendingCompletedState || Result != EMetaStoryRunStatus::Running;
	return Result;
}

EMetaStoryRunStatus FMetaStoryExecutionContext::TickEvaluatorsAndGlobalTasksForFrame(const float DeltaTime, const bool bTickGlobalTasks, const int32 FrameIndex, const FMetaStoryExecutionFrame* CurrentParentFrame, const TNotNull<FMetaStoryExecutionFrame*> CurrentFrame)
{
	check(CurrentFrame->bIsGlobalFrame);

	EMetaStoryRunStatus Result = EMetaStoryRunStatus::Running;

	// Tick evaluators
	TickGlobalEvaluatorsForFrameOnActiveInstances(DeltaTime, CurrentParentFrame, *CurrentFrame);

	if (bTickGlobalTasks)
	{
		using namespace UE::MetaStory;

		const UMetaStory* CurrentMetaStory = CurrentFrame->MetaStory;
		FTasksCompletionStatus CurrentGlobalTasksStatus = CurrentFrame->ActiveTasksStatus.GetStatus(CurrentMetaStory);
		if (!CurrentGlobalTasksStatus.HasAnyFailed())
		{
			const bool bHasEvents = EventQueue && EventQueue->HasEvents();
			if (ExecutionContext::Private::bCopyBoundPropertiesOnNonTickedTask || CurrentMetaStory->ShouldTickGlobalTasks(bHasEvents))
			{
				// Update Tasks data and tick if possible (ie. if no task has yet failed and bShouldTickTasks is true)
				FTickTaskArguments TickArgs;
				TickArgs.DeltaTime = DeltaTime;
				TickArgs.TasksBegin = CurrentMetaStory->GlobalTasksBegin;
				TickArgs.TasksNum = CurrentMetaStory->GlobalTasksNum;
				TickArgs.Indent = FrameIndex + 1;
				TickArgs.ParentFrame = CurrentParentFrame;
				TickArgs.Frame = CurrentFrame;
				TickArgs.TasksCompletionStatus = &CurrentGlobalTasksStatus;
				TickArgs.bIsGlobalTasks = true;
				TickArgs.bShouldTickTasks = true;
				TickTasks(TickArgs);
			}
		}

		// Completed global task stops the frame execution.
		const ETaskCompletionStatus GlobalTaskStatus = CurrentGlobalTasksStatus.GetCompletionStatus();
		Result = ExecutionContext::CastToRunStatus(GlobalTaskStatus);
	}

	return Result;
}

EMetaStoryRunStatus FMetaStoryExecutionContext::StartGlobalsForFrameOnActiveInstances(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, FMetaStoryTransitionResult& Transition)
{
	constexpr bool bOnActiveInstances = true;
	return StartGlobalsForFrameInternal<bOnActiveInstances>(ParentFrame, Frame, Transition);
}

EMetaStoryRunStatus FMetaStoryExecutionContext::StartGlobalsForFrameWithValidation(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, FMetaStoryTransitionResult& Transition)
{
	constexpr bool bOnActiveInstances = false;
	return StartGlobalsForFrameInternal<bOnActiveInstances>(ParentFrame, Frame, Transition);
}

template<bool bOnActiveInstances>
EMetaStoryRunStatus FMetaStoryExecutionContext::StartGlobalsForFrameInternal(const FMetaStoryExecutionFrame* CurrentParentFrame, FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryTransitionResult& Transition)
{
	if (!CurrentFrame.bIsGlobalFrame)
	{
		return EMetaStoryRunStatus::Running;
	}

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_StartEvaluators);
	UE_METASTORY_DEBUG_SCOPED_PHASE(this, EMetaStoryUpdatePhase::StartGlobalTasks);

	FMetaStoryExecutionState& Exec = GetExecState();
	FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
	if constexpr (!bOnActiveInstances)
	{
		UE_METASTORY_DEBUG_ENTER_PHASE(this, EMetaStoryUpdatePhase::StartGlobalTasksForSelection);
	}

	CurrentFrame.bHaveEntered = true;

	EMetaStoryRunStatus Result = EMetaStoryRunStatus::Running;
	const UE::MetaStory::FActiveFrameID CurrentFrameID = CurrentFrame.FrameID;
	const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;
	UE::MetaStory::FTasksCompletionStatus CurrentTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(CurrentMetaStory);

	// Start evaluators
	const int32 EvaluatorsEnd = CurrentMetaStory->EvaluatorsBegin + CurrentMetaStory->EvaluatorsNum;
	for (int32 EvalIndex = CurrentMetaStory->EvaluatorsBegin; EvalIndex < EvaluatorsEnd; ++EvalIndex)
	{
		const FMetaStoryEvaluatorBase& Eval = CurrentMetaStory->Nodes[EvalIndex].Get<const FMetaStoryEvaluatorBase>();
		FMetaStoryDataView EvalInstanceView = bOnActiveInstances
			? GetDataView(CurrentParentFrame, CurrentFrame, Eval.InstanceDataHandle)
			: GetDataViewOrTemporary(CurrentParentFrame, CurrentFrame, Eval.InstanceDataHandle);
		if constexpr (!bOnActiveInstances)
		{
			if (!EvalInstanceView.IsValid())
			{
				EvalInstanceView = AddTemporaryInstance(CurrentFrame, FMetaStoryIndex16(EvalIndex), Eval.InstanceDataHandle, CurrentFrame.MetaStory->DefaultInstanceData.GetStruct(Eval.InstanceTemplateIndex.Get()));
				check(EvalInstanceView.IsValid());
			}
		}

		FNodeInstanceDataScope DataScope(*this, &Eval, EvalIndex, Eval.InstanceDataHandle, EvalInstanceView);
		CurrentFrame.ActiveNodeIndex = FMetaStoryIndex16(EvalIndex);

		// Copy bound properties.
		if (Eval.BindingsBatch.IsValid())
		{
			if constexpr (bOnActiveInstances)
			{
				CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, EvalInstanceView, Eval.BindingsBatch);
			}
			else
			{
				CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, EvalInstanceView, Eval.BindingsBatch);
			}
		}

		METASTORY_LOG(Verbose, TEXT("  Start: '%s'"), *Eval.Name.ToString());
		UE_METASTORY_DEBUG_EVALUATOR_ENTER_TREE(this, CurrentMetaStory, FMetaStoryIndex16(EvalIndex));
		{
			QUICK_SCOPE_CYCLE_COUNTER(MetaStory_Eval_TreeStart);
			Eval.TreeStart(*this);

			UE_METASTORY_DEBUG_EVALUATOR_EVENT(this, EvalIndex, EvalInstanceView, EMetaStoryTraceEventType::OnTreeStarted);
		}

		// Copy output bound properties.
		if (Eval.OutputBindingsBatch.IsValid())
		{
			if constexpr (bOnActiveInstances)
			{
				CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, EvalInstanceView, Eval.OutputBindingsBatch);
			}
			else
			{
				CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, EvalInstanceView, Eval.OutputBindingsBatch);
			}
		}
	}

	// Start Global tasks
	// Even if we call Enter/ExitState() on global tasks, they do not enter any specific state.
	for (int32 GlobalTaskIndex = 0; GlobalTaskIndex < CurrentMetaStory->GlobalTasksNum; ++GlobalTaskIndex)
	{
		const int32 AssetTaskIndex = CurrentMetaStory->GlobalTasksBegin + GlobalTaskIndex;
		const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[AssetTaskIndex].Get<const FMetaStoryTaskBase>();

		// Ignore disabled task
		if (Task.bTaskEnabled == false)
		{
			METASTORY_LOG(VeryVerbose, TEXT("%*sSkipped 'EnterState' for disabled Task: '%s'"), UE::MetaStory::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
			continue;
		}

		FMetaStoryDataView TaskDataView = bOnActiveInstances
			? GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle)
			: GetDataViewOrTemporary(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
		if constexpr (!bOnActiveInstances)
		{
			if (!TaskDataView.IsValid())
			{
				TaskDataView = AddTemporaryInstance(CurrentFrame, FMetaStoryIndex16(AssetTaskIndex), Task.InstanceDataHandle, CurrentFrame.MetaStory->DefaultInstanceData.GetStruct(Task.InstanceTemplateIndex.Get()));
				check(TaskDataView.IsValid())
			}
		}

		FNodeInstanceDataScope DataScope(*this, &Task, AssetTaskIndex, Task.InstanceDataHandle, TaskDataView);
		CurrentFrame.ActiveNodeIndex = FMetaStoryIndex16(AssetTaskIndex);

		// Copy bound properties.
		if (Task.BindingsBatch.IsValid())
		{
			if constexpr (bOnActiveInstances)
			{
				CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskDataView, Task.BindingsBatch);
			}
			else
			{
				CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, TaskDataView, Task.BindingsBatch);
			}
		}

		METASTORY_LOG(Verbose, TEXT("  Start: '%s'"), *Task.Name.ToString());
		UE_METASTORY_DEBUG_TASK_ENTER_STATE(this, CurrentMetaStory, FMetaStoryIndex16(AssetTaskIndex));

		EMetaStoryRunStatus TaskRunStatus = EMetaStoryRunStatus::Unset;
		{
			QUICK_SCOPE_CYCLE_COUNTER(MetaStory_Task_TreeStart);
			TaskRunStatus = Task.EnterState(*this, Transition);
		}

		UE::MetaStory::ETaskCompletionStatus TaskStatus = UE::MetaStory::ExecutionContext::CastToTaskStatus(TaskRunStatus);
		TaskStatus = CurrentTasksStatus.SetStatusWithPriority(GlobalTaskIndex, TaskStatus);

		TaskRunStatus = UE::MetaStory::ExecutionContext::CastToRunStatus(TaskStatus);
		UE_METASTORY_DEBUG_TASK_EVENT(this, AssetTaskIndex, TaskDataView, EMetaStoryTraceEventType::OnEntered, TaskRunStatus);

		// Copy output bound properties if the task didn't fail
		if (TaskRunStatus != EMetaStoryRunStatus::Failed && Task.OutputBindingsBatch.IsValid())
		{
			if constexpr (bOnActiveInstances)
			{
				CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskDataView, Task.OutputBindingsBatch);
			}
			else
			{
				CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, TaskDataView, Task.OutputBindingsBatch);
			}
		}

		if (CurrentTasksStatus.IsConsideredForCompletion(GlobalTaskIndex))
		{
			Result = UE::MetaStory::ExecutionContext::GetPriorityRunStatus(Result, TaskRunStatus);
			if (Result == EMetaStoryRunStatus::Failed)
			{
				break;
			}
		}
	}

	if constexpr (!bOnActiveInstances)
	{
		UE_METASTORY_DEBUG_EXIT_PHASE(this, EMetaStoryUpdatePhase::StartGlobalTasksForSelection);
	}

	return Result;
}

// Deprecated
EMetaStoryRunStatus FMetaStoryExecutionContext::StartEvaluatorsAndGlobalTasks(FMetaStoryIndex16& OutLastInitializedTaskIndex)
{
	return StartEvaluatorsAndGlobalTasks();
}

EMetaStoryRunStatus FMetaStoryExecutionContext::StartEvaluatorsAndGlobalTasks()
{
	UE_METASTORY_DEBUG_SCOPED_PHASE(this, EMetaStoryUpdatePhase::StartGlobalTasks);

	METASTORY_LOG(Verbose, TEXT("Start Evaluators & Global tasks"));

	FMetaStoryExecutionState& Exec = GetExecState();

	EMetaStoryRunStatus Result = EMetaStoryRunStatus::Running;
	FMetaStoryTransitionResult Transition{};
	Transition.TargetState = FMetaStoryStateHandle::Root;
	Transition.CurrentRunStatus = EMetaStoryRunStatus::Running;

	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); FrameIndex++)
	{
		const FMetaStoryExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		EMetaStoryRunStatus FrameResult = StartGlobalsForFrameOnActiveInstances(CurrentParentFrame, CurrentFrame, Transition);
		Result = UE::MetaStory::ExecutionContext::GetPriorityRunStatus(Result, FrameResult);
		if (FrameResult == EMetaStoryRunStatus::Failed)
		{
			break;
		}
	}

	return Result;
}

EMetaStoryRunStatus FMetaStoryExecutionContext::StartTemporaryEvaluatorsAndGlobalTasks(const FMetaStoryExecutionFrame* CurrentParentFrame, FMetaStoryExecutionFrame& CurrentFrame)
{
	METASTORY_LOG(Verbose, TEXT("Start Temporary Evaluators & Global tasks while trying to select linked asset: %s"), *GetNameSafe(CurrentFrame.MetaStory));

	FMetaStoryTransitionResult Transition;
	Transition.ChangeType = EMetaStoryStateChangeType::Changed;
	Transition.CurrentRunStatus = EMetaStoryRunStatus::Running;
	return StartGlobalsForFrameWithValidation(CurrentParentFrame, CurrentFrame, Transition);
}

// Deprecated
void FMetaStoryExecutionContext::StopEvaluatorsAndGlobalTasks(const EMetaStoryRunStatus CompletionStatus, const FMetaStoryIndex16 LastInitializedTaskIndex)
{
	StopEvaluatorsAndGlobalTasks(CompletionStatus);
}

void FMetaStoryExecutionContext::StopEvaluatorsAndGlobalTasks(const EMetaStoryRunStatus CompletionStatus)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_StopEvaluators);
	UE_METASTORY_DEBUG_SCOPED_PHASE(this, EMetaStoryUpdatePhase::StopGlobalTasks);

	METASTORY_LOG(Verbose, TEXT("Stop Evaluators & Global Tasks"));

	FMetaStoryExecutionState& Exec = GetExecState();

	// Update bindings
	CopyAllBindingsOnActiveInstances(ECopyBindings::ExitState);

	// Call in reverse order.
	FMetaStoryTransitionResult Transition;
	Transition.TargetState = FMetaStoryStateHandle::FromCompletionStatus(CompletionStatus);
	Transition.CurrentRunStatus = CompletionStatus;

	for (int32 FrameIndex = Exec.ActiveFrames.Num() - 1; FrameIndex >= 0; FrameIndex--)
	{
		const FMetaStoryExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		if (CurrentFrame.bIsGlobalFrame)
		{
			StopGlobalsForFrameOnActiveInstances(CurrentParentFrame, CurrentFrame, Transition);
		}
	}
}

void FMetaStoryExecutionContext::StopGlobalsForFrameOnActiveInstances(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, const FMetaStoryTransitionResult& Transition)
{
	constexpr bool bOnActiveInstances = true;
	StopGlobalsForFrameInternal<bOnActiveInstances>(ParentFrame, Frame, Transition);
}

void FMetaStoryExecutionContext::StopGlobalsForFrameWithValidation(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, const FMetaStoryTransitionResult& Transition)
{
	constexpr bool bOnActiveInstances = false;
	StopGlobalsForFrameInternal<bOnActiveInstances>(ParentFrame, Frame, Transition);
}

template<bool bOnActiveInstances>
void FMetaStoryExecutionContext::StopGlobalsForFrameInternal(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, const FMetaStoryTransitionResult& Transition)
{
	// Special case when we select a new root. See SelectState. We don't want to stop the globals.
	if (!Frame.bIsGlobalFrame || !Frame.bHaveEntered)
	{
		return;
	}

	FCurrentlyProcessedFrameScope FrameScope(*this, ParentFrame, Frame);
	if constexpr (bOnActiveInstances)
	{
		UE_METASTORY_DEBUG_ENTER_PHASE(this, EMetaStoryUpdatePhase::StopGlobalTasksForSelection);
	}

	const UMetaStory* CurrentMetaStory = Frame.MetaStory;

	const int32 GlobalTasksEnd = CurrentMetaStory->GlobalTasksBegin + CurrentMetaStory->GlobalTasksNum;
	for (int32 TaskIndex = GlobalTasksEnd - 1; TaskIndex >= CurrentMetaStory->GlobalTasksBegin; --TaskIndex)
	{
		if (TaskIndex <= Frame.ActiveNodeIndex.AsInt32())
		{
			const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
			const FMetaStoryDataView TaskInstanceView = bOnActiveInstances
				? GetDataView(ParentFrame, Frame, Task.InstanceDataHandle)
				: GetDataViewOrTemporary(ParentFrame, Frame, Task.InstanceDataHandle);
			FNodeInstanceDataScope DataScope(*this, &Task, TaskIndex, Task.InstanceDataHandle, TaskInstanceView);

			// Ignore disabled task
			if (Task.bTaskEnabled == false)
			{
				METASTORY_LOG(VeryVerbose, TEXT("%*sSkipped 'ExitState' for disabled Task: '%s'"), UE::MetaStory::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
			}
			else
			{
				METASTORY_LOG(Verbose, TEXT("  Stop: '%s'"), *Task.Name.ToString());
				UE_METASTORY_DEBUG_TASK_EXIT_STATE(this, CurrentMetaStory, FMetaStoryIndex16(TaskIndex));
				{
					QUICK_SCOPE_CYCLE_COUNTER(MetaStory_Task_TreeStop);
					Task.ExitState(*this, Transition);
				}

				if (Task.OutputBindingsBatch.IsValid())
				{
					if constexpr (bOnActiveInstances)
					{
						CopyBatchOnActiveInstances(ParentFrame, Frame, TaskInstanceView, Task.OutputBindingsBatch);
					}
					else
					{
						CopyBatchWithValidation(ParentFrame, Frame, TaskInstanceView, Task.OutputBindingsBatch);
					}
				}

				UE_METASTORY_DEBUG_TASK_EVENT(this, TaskIndex, TaskInstanceView, EMetaStoryTraceEventType::OnExited, Transition.CurrentRunStatus);
			}
		}
		Frame.ActiveNodeIndex = FMetaStoryIndex16(TaskIndex - 1);
	}

	const int32 EvaluatorsEnd = CurrentMetaStory->EvaluatorsBegin + CurrentMetaStory->EvaluatorsNum;
	for (int32 EvalIndex = EvaluatorsEnd - 1; EvalIndex >= CurrentMetaStory->EvaluatorsBegin; --EvalIndex)
	{
		if (EvalIndex <= Frame.ActiveNodeIndex.AsInt32())
		{
			const FMetaStoryEvaluatorBase& Eval = CurrentMetaStory->Nodes[EvalIndex].Get<const FMetaStoryEvaluatorBase>();
			const FMetaStoryDataView EvalInstanceView = bOnActiveInstances
				? GetDataView(ParentFrame, Frame, Eval.InstanceDataHandle)
				: GetDataViewOrTemporary(ParentFrame, Frame, Eval.InstanceDataHandle);
			FNodeInstanceDataScope DataScope(*this, &Eval, EvalIndex, Eval.InstanceDataHandle, EvalInstanceView);

			METASTORY_LOG(Verbose, TEXT("  Stop: '%s'"), *Eval.Name.ToString());
			UE_METASTORY_DEBUG_EVALUATOR_EXIT_TREE(this, CurrentMetaStory, FMetaStoryIndex16(EvalIndex));
			{
				QUICK_SCOPE_CYCLE_COUNTER(MetaStory_Eval_TreeStop);
				Eval.TreeStop(*this);

				if (Eval.OutputBindingsBatch.IsValid())
				{
					if constexpr (bOnActiveInstances)
					{
						CopyBatchOnActiveInstances(ParentFrame, Frame, EvalInstanceView, Eval.OutputBindingsBatch);
					}
					else
					{
						CopyBatchWithValidation(ParentFrame, Frame, EvalInstanceView, Eval.OutputBindingsBatch);
					}
				}

				UE_METASTORY_DEBUG_EVALUATOR_EVENT(this, EvalIndex, EvalInstanceView, EMetaStoryTraceEventType::OnTreeStopped);
			}
		}
		Frame.ActiveNodeIndex = FMetaStoryIndex16(EvalIndex - 1);
	}

	Frame.ActiveNodeIndex = FMetaStoryIndex16::Invalid;
	Frame.bHaveEntered = false;

	if constexpr (bOnActiveInstances)
	{
		UE_METASTORY_DEBUG_EXIT_PHASE(this, EMetaStoryUpdatePhase::StopGlobalTasksForSelection);
	}
}

// Deprecated
void FMetaStoryExecutionContext::CallStopOnEvaluatorsAndGlobalTasks(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& Frame, const FMetaStoryTransitionResult& Transition, const FMetaStoryIndex16 LastInitializedTaskIndex /*= FMetaStoryIndex16()*/)
{
	constexpr bool bOnActiveInstances = true;
	StopGlobalsForFrameInternal<bOnActiveInstances>(ParentFrame, const_cast<FMetaStoryExecutionFrame&>(Frame), Transition);
}

// Deprecated
void FMetaStoryExecutionContext::StopTemporaryEvaluatorsAndGlobalTasks(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame)
{
	StopTemporaryEvaluatorsAndGlobalTasks(CurrentParentFrame, const_cast<FMetaStoryExecutionFrame&>(CurrentFrame), EMetaStoryRunStatus::Running);
}

void FMetaStoryExecutionContext::StopTemporaryEvaluatorsAndGlobalTasks(const FMetaStoryExecutionFrame* CurrentParentFrame, FMetaStoryExecutionFrame& CurrentFrame, EMetaStoryRunStatus StartResult)
{
	METASTORY_LOG(Verbose, TEXT("Stop Temporary Evaluators & Global tasks"));

	// Create temporary transition to stop the unused global tasks and evaluators.
	FMetaStoryTransitionResult Transition;
	Transition.TargetState = FMetaStoryStateHandle::FromCompletionStatus(StartResult);
	Transition.CurrentRunStatus = StartResult;
	Transition.ChangeType = EMetaStoryStateChangeType::Changed;
	StopGlobalsForFrameWithValidation(CurrentParentFrame, CurrentFrame, Transition);
}

EMetaStoryRunStatus FMetaStoryExecutionContext::TickTasks(const float DeltaTime)
{
	// When a task is completed it also completes the state and triggers the completion transition (because LastTickStatus is set).
	// A task can complete async.
	// When a task fails, stop ticking the following tasks.
	// When no task ticks, then the leaf completes.

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_TickTasks);
	UE_METASTORY_DEBUG_SCOPED_PHASE(this, EMetaStoryUpdatePhase::TickingTasks);

	using namespace UE::MetaStory;

	FMetaStoryExecutionState& Exec = GetExecState();
	Exec.bHasPendingCompletedState = false;

	if (Exec.ActiveFrames.IsEmpty())
	{
		return EMetaStoryRunStatus::Failed;
	}

	int32 NumTotalEnabledTasks = 0;
	const bool bCopyBoundPropertiesOnNonTickedTask = ExecutionContext::Private::bCopyBoundPropertiesOnNonTickedTask;

	FTickTaskArguments TickArgs;
	TickArgs.DeltaTime = DeltaTime;
	TickArgs.bIsGlobalTasks = false;
	TickArgs.bShouldTickTasks = true;

	METASTORY_CLOG(Exec.ActiveFrames.Num() > 0, VeryVerbose, TEXT("Ticking Tasks"));

	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
	{
		TickArgs.ParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		TickArgs.Frame = &Exec.ActiveFrames[FrameIndex];
		const UMetaStory* CurrentMetaStory = TickArgs.Frame->MetaStory;

		FCurrentlyProcessedFrameScope FrameScope(*this, TickArgs.ParentFrame, *TickArgs.Frame);

		if (ExecutionContext::Private::bTickGlobalNodesFollowingTreeHierarchy)
		{
			if (TickArgs.Frame->bIsGlobalFrame)
			{
				constexpr bool bTickGlobalTasks = true;
				const EMetaStoryRunStatus FrameResult = TickEvaluatorsAndGlobalTasksForFrame(DeltaTime, bTickGlobalTasks, FrameIndex, TickArgs.ParentFrame, TickArgs.Frame);
				if (FrameResult != EMetaStoryRunStatus::Running)
				{
					if (ExecutionContext::Private::bGlobalTasksCompleteOwningFrame == false || FrameIndex == 0)
					{
						// Stop the tree execution when it's the root frame or if the previous behavior is desired.
						Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, FrameResult);
					}
					TickArgs.bShouldTickTasks = false;
					break;
				}
			}
		}

		for (int32 StateIndex = 0; StateIndex < TickArgs.Frame->ActiveStates.Num(); ++StateIndex)
		{
			const FMetaStoryStateHandle CurrentHandle = TickArgs.Frame->ActiveStates[StateIndex];
			const FMetaStoryCompactState& CurrentState = CurrentMetaStory->States[CurrentHandle.Index];
			FTasksCompletionStatus CurrentCompletionStatus = TickArgs.Frame->ActiveTasksStatus.GetStatus(CurrentState);

			TickArgs.StateID = TickArgs.Frame->ActiveStates.StateIDs[StateIndex];
			TickArgs.TasksCompletionStatus = &CurrentCompletionStatus;

			FCurrentlyProcessedStateScope StateScope(*this, CurrentHandle);
			UE_METASTORY_DEBUG_SCOPED_STATE(this, CurrentHandle);

			METASTORY_CLOG(CurrentState.TasksNum > 0, VeryVerbose, TEXT("%*sState '%s'")
				, (FrameIndex + StateIndex + 1) * Debug::IndentSize, TEXT("")
				, *DebugGetStatePath(Exec.ActiveFrames, TickArgs.Frame, StateIndex));

			if (CurrentState.Type == EMetaStoryStateType::Linked || CurrentState.Type == EMetaStoryStateType::LinkedAsset)
			{
				if (CurrentState.ParameterDataHandle.IsValid() && CurrentState.ParameterBindingsBatch.IsValid())
				{
					const FMetaStoryDataView StateParamsDataView = GetDataView(TickArgs.ParentFrame, *TickArgs.Frame, CurrentState.ParameterDataHandle);
					CopyBatchOnActiveInstances(TickArgs.ParentFrame, *TickArgs.Frame, StateParamsDataView, CurrentState.ParameterBindingsBatch);
				}
			}

			const bool bHasEvents = EventQueue && EventQueue->HasEvents();
			bool bRequestLoopStop = false;
			if (bCopyBoundPropertiesOnNonTickedTask || CurrentState.ShouldTickTasks(bHasEvents))
			{
				// Update Tasks data and tick if possible (ie. if no task has yet failed and bShouldTickTasks is true)
				TickArgs.TasksBegin = CurrentState.TasksBegin;
				TickArgs.TasksNum = CurrentState.TasksNum;
				TickArgs.Indent = (FrameIndex + StateIndex + 1);
				const FTickTaskResult TickTasksResult = TickTasks(TickArgs);

				// Keep updating the binding but do not call tick on tasks if there's a failure.
				TickArgs.bShouldTickTasks = TickTasksResult.bShouldTickTasks
					&& !CurrentCompletionStatus.HasAnyFailed();
				// If a failure and we do not copy then bindings, then we can stop.
				bRequestLoopStop = !bCopyBoundPropertiesOnNonTickedTask && !TickTasksResult.bShouldTickTasks;
			}

			NumTotalEnabledTasks += CurrentState.EnabledTasksNum;

			if (bRequestLoopStop)
			{
				break;
			}
		}
	}

	// Collect the result after every tasks has the chance to tick.
	//An async or delegate might complete a global or "previous" task (in a different order).
	EMetaStoryRunStatus FirstFrameResult = EMetaStoryRunStatus::Running;
	EMetaStoryRunStatus FrameResult = EMetaStoryRunStatus::Running;
	EMetaStoryRunStatus StateResult = EMetaStoryRunStatus::Running;
	for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
	{
		using namespace UE::MetaStory::ExecutionContext;
		using namespace UE::MetaStory::ExecutionContext::Private;

		const FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;
		if (CurrentFrame.bIsGlobalFrame)
		{
			const ETaskCompletionStatus GlobalTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(CurrentMetaStory).GetCompletionStatus();
			if (FrameIndex == 0)
			{
				FirstFrameResult = CastToRunStatus(GlobalTasksStatus);
			}
			FrameResult = GetPriorityRunStatus(FrameResult, CastToRunStatus(GlobalTasksStatus));
		}

		for (int32 StateIndex = 0; StateIndex < CurrentFrame.ActiveStates.Num() && StateResult != EMetaStoryRunStatus::Failed; ++StateIndex)
		{
			const FMetaStoryStateHandle CurrentHandle = CurrentFrame.ActiveStates[StateIndex];
			const FMetaStoryCompactState& State = CurrentMetaStory->States[CurrentHandle.Index];
			const ETaskCompletionStatus StateTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(State).GetCompletionStatus();
			StateResult = GetPriorityRunStatus(StateResult, CastToRunStatus(StateTasksStatus));
		}
	}

	if (ExecutionContext::Private::bGlobalTasksCompleteOwningFrame && FirstFrameResult != EMetaStoryRunStatus::Running)
	{
		Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, FrameResult);
	}
	else if (ExecutionContext::Private::bGlobalTasksCompleteOwningFrame == false && FrameResult != EMetaStoryRunStatus::Running)
	{
		Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, FrameResult);
	}
	else if (NumTotalEnabledTasks == 0 && StateResult == EMetaStoryRunStatus::Running && FrameResult == EMetaStoryRunStatus::Running)
	{
		// No enabled tasks, done ticking.
		//Complete the the bottom state in the bottom frame (to trigger the completion transitions).
		if (Exec.ActiveFrames.Num() > 0)
		{
			FMetaStoryExecutionFrame& LastFrame = Exec.ActiveFrames.Last();
			const int32 NumberOfActiveState = LastFrame.ActiveStates.Num();
			if (ensureMsgf(NumberOfActiveState != 0, TEXT("No task is allowed to clear/stop/transition. Those action should be delayed inside the execution context.")))
			{
				const FMetaStoryStateHandle CurrentHandle = LastFrame.ActiveStates[NumberOfActiveState - 1];
				const FMetaStoryCompactState& State = LastFrame.MetaStory->States[CurrentHandle.Index];
				LastFrame.ActiveTasksStatus.GetStatus(State).SetCompletionStatus(ETaskCompletionStatus::Succeeded);
			}
			else
			{
				LastFrame.ActiveTasksStatus.GetStatus(LastFrame.MetaStory).SetCompletionStatus(ETaskCompletionStatus::Succeeded);
			}
		}
		else
		{
			Exec.RequestedStop = ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, EMetaStoryRunStatus::Stopped);
		}
		StateResult = EMetaStoryRunStatus::Succeeded;
	}

	Exec.bHasPendingCompletedState = StateResult != EMetaStoryRunStatus::Running || FrameResult != EMetaStoryRunStatus::Running;
	return StateResult;
}

FMetaStoryExecutionContext::FTickTaskResult FMetaStoryExecutionContext::TickTasks(const FTickTaskArguments& Args)
{
	using namespace UE::MetaStory;

	check(Args.Frame);
	check(Args.TasksCompletionStatus);

	bool bShouldTickTasks = Args.bShouldTickTasks;

	FMetaStoryExecutionState& Exec = GetExecState();
	const bool bCopyBoundPropertiesOnNonTickedTask = ExecutionContext::Private::bCopyBoundPropertiesOnNonTickedTask;
	const UMetaStory* CurrentMetaStory = Args.Frame->MetaStory;
	const FActiveFrameID CurrentFrameID = Args.Frame->FrameID;
	const int32 CurrentActiveNodeIndex = Args.Frame->ActiveNodeIndex.AsInt32();
	check(CurrentMetaStory);

	for (int32 OwnerTaskIndex = 0; OwnerTaskIndex < Args.TasksNum; ++OwnerTaskIndex)
	{
		const int32 AssetTaskIndex = Args.TasksBegin + OwnerTaskIndex;
		const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[AssetTaskIndex].Get<const FMetaStoryTaskBase>();

		// Ignore disabled task
		if (Task.bTaskEnabled == false)
		{
			METASTORY_LOG(VeryVerbose, TEXT("%*sSkipped 'Tick' for disabled Task: '%s'"), Debug::IndentSize, TEXT(""), *Task.Name.ToString());
			continue;
		}

		if (AssetTaskIndex > CurrentActiveNodeIndex)
		{
			METASTORY_LOG(VeryVerbose, TEXT("%*sSkipped 'Tick' for task that didn't get the EnterState Task: '%s'"), Debug::IndentSize, TEXT(""), *Task.Name.ToString());
			bShouldTickTasks = false;
			break;
		}

		const FMetaStoryDataView TaskInstanceView = GetDataView(Args.ParentFrame, *Args.Frame, Task.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Task, AssetTaskIndex, Task.InstanceDataHandle, TaskInstanceView);

		const bool bHasEvents = EventQueue && EventQueue->HasEvents();
		const bool bIsTaskRunning = Args.TasksCompletionStatus->IsRunning(OwnerTaskIndex);
		const bool bNeedsTick = bShouldTickTasks
			&& bIsTaskRunning
			&& (Task.bShouldCallTick || (bHasEvents && Task.bShouldCallTickOnlyOnEvents));
		METASTORY_LOG(VeryVerbose, TEXT("%*s  Tick: '%s' %s"), Args.Indent * Debug::IndentSize, TEXT("")
			, *Task.Name.ToString()
			, !bNeedsTick ? TEXT("[not ticked]") : TEXT(""));

		// Copy bound properties.
		// Only copy properties when the task is actually ticked, and copy properties at tick is requested.
		const bool bCopyBatch = (bCopyBoundPropertiesOnNonTickedTask || bNeedsTick)
			&& Task.BindingsBatch.IsValid()
			&& Task.bShouldCopyBoundPropertiesOnTick;
		if (bCopyBatch)
		{
			CopyBatchOnActiveInstances(Args.ParentFrame, *Args.Frame, TaskInstanceView, Task.BindingsBatch);
		}

		if (!bNeedsTick)
		{
			// Task didn't tick because it failed.
			//The following tasks should not tick but we might still need to update their bindings.
			if (!bIsTaskRunning && bShouldTickTasks && Args.TasksCompletionStatus->HasAnyFailed())
			{
				bShouldTickTasks = false;
			}
			continue;
		}

		//UE_METASTORY_DEBUG_TASK_EVENT(this, AssetTaskIndex, TaskDataView, EMetaStoryTraceEventType::OnTickingTask, EMetaStoryRunStatus::Running);
		UE_METASTORY_DEBUG_TASK_TICK(this, CurrentMetaStory, AssetTaskIndex);

		EMetaStoryRunStatus TaskRunStatus = EMetaStoryRunStatus::Unset;
		{
			QUICK_SCOPE_CYCLE_COUNTER(MetaStory_Task_Tick);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_Task_Tick);

			TaskRunStatus = Task.Tick(*this, Args.DeltaTime);
		}

		// Set the new status and fetch back the status with priority.
		//In case an async task completes the same task.
		//Or in case FinishTask() inside the Task.Tick()
		ETaskCompletionStatus TaskStatus = UE::MetaStory::ExecutionContext::CastToTaskStatus(TaskRunStatus);
		TaskStatus = Args.TasksCompletionStatus->SetStatusWithPriority(OwnerTaskIndex, TaskStatus);
		TaskRunStatus = ExecutionContext::CastToRunStatus(TaskStatus);

		// Only copy output bound properties if the task wasn't failed already
		if (TaskRunStatus != EMetaStoryRunStatus::Failed && Task.OutputBindingsBatch.IsValid())
		{
			CopyBatchOnActiveInstances(Args.ParentFrame, *Args.Frame, TaskInstanceView, Task.OutputBindingsBatch);
		}

		UE_METASTORY_DEBUG_TASK_EVENT(this, AssetTaskIndex, TaskInstanceView,
			TaskRunStatus != EMetaStoryRunStatus::Running ? EMetaStoryTraceEventType::OnTaskCompleted : EMetaStoryTraceEventType::OnTicked,
			TaskRunStatus);

		if (Args.TasksCompletionStatus->IsConsideredForCompletion(OwnerTaskIndex))
		{
			if (TaskRunStatus == EMetaStoryRunStatus::Failed)
			{
				bShouldTickTasks = false;
				break; // Stop copy binding.
			}
		}
	}

	return FTickTaskResult{ bShouldTickTasks };
}

// Deprecated
bool FMetaStoryExecutionContext::TestAllConditions(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const int32 ConditionsOffset, const int32 ConditionsNum)
{
	return TestAllConditionsInternal<false>(CurrentParentFrame, CurrentFrame, FMetaStoryStateHandle(), FMemoryRequirement(), ConditionsOffset, ConditionsNum, EMetaStoryUpdatePhase::EnterConditions);
}

bool FMetaStoryExecutionContext::TestAllConditionsOnActiveInstances(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryStateHandle CurrentStateHandle, const FMemoryRequirement& MemoryRequirement, const int32 ConditionsOffset, const int32 ConditionsNum, EMetaStoryUpdatePhase Phase)
{
	return TestAllConditionsInternal<true>(CurrentParentFrame, CurrentFrame, CurrentStateHandle, MemoryRequirement, ConditionsOffset, ConditionsNum, Phase);
}

bool FMetaStoryExecutionContext::TestAllConditionsWithValidation(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryStateHandle CurrentStateHandle, const FMemoryRequirement& MemoryRequirement, const int32 ConditionsOffset, const int32 ConditionsNum, EMetaStoryUpdatePhase Phase)
{
	return TestAllConditionsInternal<false>(CurrentParentFrame, CurrentFrame, CurrentStateHandle, MemoryRequirement, ConditionsOffset, ConditionsNum, Phase);
}

template<bool bOnActiveInstances>
bool FMetaStoryExecutionContext::TestAllConditionsInternal(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryStateHandle CurrentStateHandle, const FMemoryRequirement& MemoryRequirement, const int32 ConditionsOffset, const int32 ConditionsNum, EMetaStoryUpdatePhase Phase)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_TestConditions);

	using namespace UE::MetaStory::InstanceData;
	using namespace UE::MetaStory::ExecutionContext::Private;

	if (ConditionsNum == 0)
	{
		return true;
	}

	FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
	FCurrentlyProcessedStateScope StateScope(*this, CurrentStateHandle);

	UE_METASTORY_DEBUG_SCOPED_PHASE(this, Phase);

	TStaticArray<EMetaStoryExpressionOperand, UE::MetaStory::MaxExpressionIndent + 1> Operands(InPlace, EMetaStoryExpressionOperand::Copy);
	TStaticArray<bool, UE::MetaStory::MaxExpressionIndent + 1> Values(InPlace, false);

	FEvaluationScopeInstanceContainer EvaluationScopeContainer;
	bool bEvaluationScopeContainerPushed = false;

	if (MemoryRequirement.Size > 0)
	{
		void* FunctionBindingMemory = FMemory_Alloca_Aligned(MemoryRequirement.Size, MemoryRequirement.Alignment);
		EvaluationScopeContainer = FEvaluationScopeInstanceContainer(FunctionBindingMemory, MemoryRequirement);
		PushEvaluationScopeInstanceContainer(EvaluationScopeContainer, CurrentFrame);
		InitEvaluationScopeInstanceData(EvaluationScopeContainer, CurrentFrame.MetaStory, ConditionsOffset, ConditionsOffset + ConditionsNum);
		bEvaluationScopeContainerPushed = true;
	}

	int32 Level = 0;

	for (int32 Index = 0; Index < ConditionsNum; Index++)
	{
		const int32 ConditionIndex = ConditionsOffset + Index;
		const FMetaStoryConditionBase& Cond = CurrentFrame.MetaStory->Nodes[ConditionIndex].Get<const FMetaStoryConditionBase>();

		const FMetaStoryDataView ConditionInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Cond.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Cond, ConditionIndex, Cond.InstanceDataHandle, ConditionInstanceView);

		bool bValue = false;
		if (Cond.EvaluationMode == EMetaStoryConditionEvaluationMode::Evaluated)
		{
			// Copy bound properties.
			if (Cond.BindingsBatch.IsValid())
			{
				const bool bBatchCopied = bOnActiveInstances
					? CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, ConditionInstanceView, Cond.BindingsBatch)
					: CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, ConditionInstanceView, Cond.BindingsBatch);
				if (!bBatchCopied)
				{
					// If the source data cannot be accessed, the whole expression evaluates to false.
					static constexpr TCHAR Message[] = TEXT("Evaluation forced to false: source data cannot be accessed (e.g. enter conditions trying to access inactive parent state)");
					UE_METASTORY_DEBUG_CONDITION_EVENT(this, ConditionIndex, ConditionInstanceView, EMetaStoryTraceEventType::InternalForcedFailure);
					UE_METASTORY_DEBUG_LOG_EVENT(this, Warning, Message);
					METASTORY_LOG(Warning, TEXT("%s"), Message);
					Values[0] = false;
					break;
				}
			}

			UE_METASTORY_DEBUG_CONDITION_TEST_CONDITION(this, CurrentFrame.MetaStory, Index);

			bValue = Cond.TestCondition(*this);
			UE_METASTORY_DEBUG_CONDITION_EVENT(this, ConditionIndex, ConditionInstanceView, bValue ? EMetaStoryTraceEventType::Passed : EMetaStoryTraceEventType::Failed);

			// Reset copied properties that might contain object references.
			if (Cond.BindingsBatch.IsValid())
			{
				CurrentFrame.MetaStory->PropertyBindings.Super::ResetObjects(Cond.BindingsBatch, ConditionInstanceView);
			}
		}
		else
		{
			bValue = Cond.EvaluationMode == EMetaStoryConditionEvaluationMode::ForcedTrue;
			UE_METASTORY_DEBUG_CONDITION_EVENT(this, ConditionIndex, FMetaStoryDataView{}, bValue ? EMetaStoryTraceEventType::ForcedSuccess : EMetaStoryTraceEventType::ForcedFailure);
		}

		const int32 DeltaIndent = Cond.DeltaIndent;
		const int32 OpenParens = FMath::Max(0, DeltaIndent) + 1;	// +1 for the current value that is stored at the empty slot at the top of the value stack.
		const int32 ClosedParens = FMath::Max(0, -DeltaIndent) + 1;

		// Store the operand to apply when merging higher level down when returning to this level.
		const EMetaStoryExpressionOperand Operand = Index == 0 ? EMetaStoryExpressionOperand::Copy : Cond.Operand;
		Operands[Level] = Operand;

		// Store current value at the top of the stack.
		Level += OpenParens;
		Values[Level] = bValue;

		// Evaluate and merge down values based on closed braces.
		// The current value is placed in parens (see +1 above), which makes merging down and applying the new value consistent.
		// The default operand is copy, so if the value is needed immediately, it is just copied down, or if we're on the same level,
		// the operand storing above gives handles with the right logic.
		for (int32 Paren = 0; Paren < ClosedParens; Paren++)
		{
			Level--;
			switch (Operands[Level])
			{
			case EMetaStoryExpressionOperand::Copy:
				Values[Level] = Values[Level + 1];
				break;
			case EMetaStoryExpressionOperand::And:
				Values[Level] &= Values[Level + 1];
				break;
			case EMetaStoryExpressionOperand::Or:
				Values[Level] |= Values[Level + 1];
				break;
			case EMetaStoryExpressionOperand::Multiply:
			default:
				checkf(false, TEXT("Unhandled operand %s"), *UEnum::GetValueAsString(Operand));
				break;
			}
			Operands[Level] = EMetaStoryExpressionOperand::Copy;
		}
	}

	if (bEvaluationScopeContainerPushed)
	{
		PopEvaluationScopeInstanceContainer(EvaluationScopeContainer);
	}

	return Values[0];
}

//Deprecated
float FMetaStoryExecutionContext::EvaluateUtility(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const int32 ConsiderationsBegin, const int32 ConsiderationsNum, const float StateWeight)
{
	const FMetaStoryStateHandle StateHandle = FMetaStoryStateHandle::Invalid;
	return EvaluateUtilityWithValidation(CurrentParentFrame, CurrentFrame, StateHandle, FMemoryRequirement(), ConsiderationsBegin, ConsiderationsNum, StateWeight);
}

float FMetaStoryExecutionContext::EvaluateUtilityWithValidation(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryStateHandle CurrentStateHandle, const FMemoryRequirement& MemoryRequirement, const int32 ConsiderationsBegin, const int32 ConsiderationsNum, const float StateWeight)
{
	// @todo: Tracing support
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_EvaluateUtility);

	using namespace UE::MetaStory::InstanceData;
	using namespace UE::MetaStory::ExecutionContext::Private;

	if (ConsiderationsNum == 0)
	{
		return 0.0f;
	}

	FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
	FCurrentlyProcessedStateScope NextStateScope(*this, CurrentStateHandle);

	TStaticArray<EMetaStoryExpressionOperand, UE::MetaStory::MaxExpressionIndent + 1> Operands(InPlace, EMetaStoryExpressionOperand::Copy);
	TStaticArray<float, UE::MetaStory::MaxExpressionIndent + 1> Values(InPlace, false);

	FEvaluationScopeInstanceContainer EvaluationScopeContainer;
	bool bEvaluationScopeContainerPushed = false;

	if (MemoryRequirement.Size > 0)
	{
		void* FunctionBindingMemory = FMemory_Alloca_Aligned(MemoryRequirement.Size, MemoryRequirement.Alignment);
		EvaluationScopeContainer = FEvaluationScopeInstanceContainer(FunctionBindingMemory, MemoryRequirement);
		PushEvaluationScopeInstanceContainer(EvaluationScopeContainer, CurrentFrame);
		InitEvaluationScopeInstanceData(EvaluationScopeContainer, CurrentFrame.MetaStory, ConsiderationsBegin, ConsiderationsBegin + ConsiderationsNum);
		bEvaluationScopeContainerPushed = true;
	}

	int32 Level = 0;
	float Value = 0.0f;
	for (int32 Index = 0; Index < ConsiderationsNum; Index++)
	{
		const int32 ConsiderationIndex = ConsiderationsBegin + Index;
		const FMetaStoryConsiderationBase& Consideration = CurrentFrame.MetaStory->Nodes[ConsiderationIndex].Get<const FMetaStoryConsiderationBase>();

		const FMetaStoryDataView ConsiderationInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Consideration.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Consideration, ConsiderationIndex, Consideration.InstanceDataHandle, ConsiderationInstanceView);

		// Copy bound properties.
		if (Consideration.BindingsBatch.IsValid())
		{
			// Use validated copy, since we test in situations where the sources are not always valid (e.g. considerations may try to access inactive parent state). 
			if (!CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, ConsiderationInstanceView, Consideration.BindingsBatch))
			{
				// If the source data cannot be accessed, the whole expression evaluates to zero.
				Values[0] = 0.0f;
				break;
			}
		}

		Value = Consideration.GetNormalizedScore(*this);

		// Reset copied properties that might contain object references.
		if (Consideration.BindingsBatch.IsValid())
		{
			CurrentFrame.MetaStory->PropertyBindings.Super::ResetObjects(Consideration.BindingsBatch, ConsiderationInstanceView);
		}

		const int32 DeltaIndent = Consideration.DeltaIndent;
		const int32 OpenParens = FMath::Max(0, DeltaIndent) + 1;	// +1 for the current value that is stored at the empty slot at the top of the value stack.
		const int32 ClosedParens = FMath::Max(0, -DeltaIndent) + 1;

		// Store the operand to apply when merging higher level down when returning to this level.
		const EMetaStoryExpressionOperand Operand = Index == 0 ? EMetaStoryExpressionOperand::Copy : Consideration.Operand;
		Operands[Level] = Operand;

		// Store current value at the top of the stack.
		Level += OpenParens;
		Values[Level] = Value;

		// Evaluate and merge down values based on closed braces.
		// The current value is placed in parens (see +1 above), which makes merging down and applying the new value consistent.
		// The default operand is copy, so if the value is needed immediately, it is just copied down, or if we're on the same level,
		// the operand storing above gives handles with the right logic.
		for (int32 Paren = 0; Paren < ClosedParens; Paren++)
		{
			Level--;
			switch (Operands[Level])
			{
			case EMetaStoryExpressionOperand::Copy:
				Values[Level] = Values[Level + 1];
				break;
			case EMetaStoryExpressionOperand::And:
				Values[Level] = FMath::Min(Values[Level], Values[Level + 1]);
				break;
			case EMetaStoryExpressionOperand::Or:
				Values[Level] = FMath::Max(Values[Level], Values[Level + 1]);
				break;
			case EMetaStoryExpressionOperand::Multiply:
				Values[Level] = Values[Level] * Values[Level + 1];
				break;
			default:
				checkf(false, TEXT("Unhandled operand %s"), *UEnum::GetValueAsString(Operand));
				break;
			}
			Operands[Level] = EMetaStoryExpressionOperand::Copy;
		}
	}

	if (bEvaluationScopeContainerPushed)
	{
		PopEvaluationScopeInstanceContainer(EvaluationScopeContainer);
	}

	return StateWeight * Values[0];
}

void FMetaStoryExecutionContext::EvaluatePropertyFunctionsOnActiveInstances(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryIndex16 FuncsBegin, uint16 FuncsNum)
{
	constexpr bool bOnActiveInstances = true;
	EvaluatePropertyFunctionsInternal<bOnActiveInstances>(CurrentParentFrame, CurrentFrame, FuncsBegin, FuncsNum);
}

void FMetaStoryExecutionContext::EvaluatePropertyFunctionsWithValidation(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryIndex16 FuncsBegin, uint16 FuncsNum)
{
	constexpr bool bOnActiveInstances = false;
	EvaluatePropertyFunctionsInternal<bOnActiveInstances>(CurrentParentFrame, CurrentFrame, FuncsBegin, FuncsNum);
}

template<bool bOnActiveInstances>
void FMetaStoryExecutionContext::EvaluatePropertyFunctionsInternal(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryIndex16 FuncsBegin, uint16 FuncsNum)
{
	const int32 FuncsEnd = FuncsBegin.AsInt32() + FuncsNum;
	for (int32 FuncIndex = FuncsBegin.AsInt32(); FuncIndex < FuncsEnd; ++FuncIndex)
	{
		const FMetaStoryPropertyFunctionBase& Func = CurrentFrame.MetaStory->Nodes[FuncIndex].Get<const FMetaStoryPropertyFunctionBase>();
		const FMetaStoryDataView FuncInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Func.InstanceDataHandle);
		FNodeInstanceDataScope DataScope(*this, &Func, FuncIndex, Func.InstanceDataHandle, FuncInstanceView);

		// Copy bound properties.
		if (Func.BindingsBatch.IsValid())
		{
			if constexpr (bOnActiveInstances)
			{
				CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, FuncInstanceView, Func.BindingsBatch);
			}
			else
			{
				CopyBatchWithValidation(CurrentParentFrame, CurrentFrame, FuncInstanceView, Func.BindingsBatch);
			}
		}

		Func.Execute(*this);
	}
}

FString FMetaStoryExecutionContext::DebugGetEventsAsString() const
{
	TStringBuilder<512> StrBuilder;

	if (EventQueue)
	{
		for (const FMetaStorySharedEvent& Event : EventQueue->GetEventsView())
		{
			if (Event.IsValid())
			{
				if (StrBuilder.Len() > 0)
				{
					StrBuilder << TEXT(", ");
				}

				const bool bHasTag = Event->Tag.IsValid();
				const bool bHasPayload = Event->Payload.GetScriptStruct() != nullptr;

				if (bHasTag || bHasPayload)
				{
					StrBuilder << (TEXT('('));

					if (bHasTag)
					{
						StrBuilder << TEXT("Tag: '");
						StrBuilder << Event->Tag.GetTagName();
						StrBuilder << TEXT('\'');
					}
					if (bHasTag && bHasPayload)
					{
						StrBuilder << TEXT(", ");
					}
					if (bHasPayload)
					{
						StrBuilder << TEXT(" Payload: '");
						StrBuilder << Event->Payload.GetScriptStruct()->GetFName();
						StrBuilder << TEXT('\'');
					}
					StrBuilder << TEXT(") ");
				}
			}
		}
	}

	return StrBuilder.ToString();
}

// Deprecated
bool FMetaStoryExecutionContext::RequestTransition(
	const FMetaStoryExecutionFrame& CurrentFrame,
	const FMetaStoryStateHandle NextState,
	const EMetaStoryTransitionPriority Priority,
	const FMetaStorySharedEvent* TransitionEvent,
	const EMetaStorySelectionFallback Fallback)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;

	const FStateHandleContext TargetState = FStateHandleContext(CurrentFrame.MetaStory, NextState);
	FTransitionArguments TransitionArgs = FTransitionArguments{ .Priority = Priority, .Fallback = Fallback };
	if (TransitionEvent)
	{
		TransitionArgs.TransitionEvent = *TransitionEvent;
	}
	
	return RequestTransitionInternal(CurrentFrame, FActiveStateID(), TargetState, TransitionArgs);
}

bool FMetaStoryExecutionContext::RequestTransitionInternal(
	const FMetaStoryExecutionFrame& SourceFrame,
	const UE::MetaStory::FActiveStateID SourceStateID,
	const UE::MetaStory::ExecutionContext::FStateHandleContext TargetState,
	const FTransitionArguments& Args)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	// Skip lower priority transitions.
	if (RequestedTransition.IsValid() && RequestedTransition->Transition.Priority >= Args.Priority)
	{
		return false;
	}

	if (TargetState.StateHandle.IsCompletionState())
	{
		SetupNextTransition(SourceFrame, SourceStateID, TargetState, Args);
		METASTORY_LOG(Verbose, TEXT("Transition on state '%s' -> state '%s'"),
			*GetSafeStateName(SourceFrame, SourceFrame.ActiveStates.Last()), *TargetState.StateHandle.Describe());
		return true;
	}
	if (!TargetState.StateHandle.IsValid() || TargetState.MetaStory == nullptr)
	{
		// NotSet is no-operation, but can be used to mask a transition at parent state. Returning unset keeps updating current state.
		SetupNextTransition(SourceFrame, SourceStateID, FStateHandleContext(), Args);
		return true;
	}

	FActiveStateInlineArray CurrentActiveStatePath;
	GetActiveStatePath(GetExecState().ActiveFrames, CurrentActiveStatePath);

	FSelectStateArguments SelectStateArgs;
	SelectStateArgs.ActiveStates = MakeConstArrayView(CurrentActiveStatePath);

	// Source can be a GlobalTask (not in active state)
	FMetaStoryStateHandle SourceStateHandle;
	if (SourceStateID.IsValid())
	{
		SourceStateHandle = SourceFrame.ActiveStates.FindStateHandle(SourceStateID);
		if (!ensure(SourceStateHandle.IsValid()))
		{
			return false;
		}
		SelectStateArgs.SourceState = FActiveState(SourceFrame.FrameID, SourceStateID, SourceStateHandle);
	}
	else
	{
		SelectStateArgs.SourceState = FActiveState(SourceFrame.FrameID, FActiveStateID(), FMetaStoryStateHandle());
	}
	SelectStateArgs.TargetState = TargetState;
	SelectStateArgs.TransitionEvent = Args.TransitionEvent;
	SelectStateArgs.Fallback = Args.Fallback;
	SelectStateArgs.Behavior = ESelectStateBehavior::StateTransition;
	SelectStateArgs.SelectionRules = SourceFrame.MetaStory->StateSelectionRules;

	TSharedRef<FSelectStateResult> StateSelectionResult = MakeShared<FSelectStateResult>();
	TGuardValue<TSharedPtr<FSelectStateResult>> TemporaryStorageScope(CurrentlyProcessedTemporaryStorage, StateSelectionResult);
	if (SelectState(SelectStateArgs, StateSelectionResult))
	{
		if (RequestedTransition)
		{
			// If we have a previous selection(i.e. we succeeded a selection already from a previous transition), we need to clean up temporary frames if any.
			if (FSelectStateResult* PrevSelectStateResult = RequestedTransition->Selection.Get())
			{
				TGuardValue<TSharedPtr<FSelectStateResult>> PrevTemporaryStorageScope(CurrentlyProcessedTemporaryStorage, RequestedTransition->Selection);

				for (int32 Idx = PrevSelectStateResult->SelectedFrames.Num() - 1; Idx >= 0; --Idx)
				{
					const FActiveFrameID TemporaryFrameID = PrevSelectStateResult->SelectedFrames[Idx];

					FSelectStateResult::FFrameAndParent TemporaryFrameAndParent = PrevSelectStateResult->GetExecutionFrame(TemporaryFrameID);
					FMetaStoryExecutionFrame* TemporaryFrame = TemporaryFrameAndParent.Frame;

					// stop as soon as the frame is active already: no more temporary frames forward
					if (!TemporaryFrame)
					{
						break;
					}

					FMetaStoryExecutionState& Exec = Storage.GetMutableExecutionState();
					const FMetaStoryExecutionFrame* ParentFrame = FindExecutionFrame(TemporaryFrameAndParent.ParentFrameID, MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(PrevSelectStateResult->TemporaryFrames));

					if (TemporaryFrame->bIsGlobalFrame && TemporaryFrame->bHaveEntered)
					{
						constexpr EMetaStoryRunStatus Status = EMetaStoryRunStatus::Stopped;
						StopTemporaryEvaluatorsAndGlobalTasks(ParentFrame, *TemporaryFrame, Status);
					}

					CleanFrame(Exec, TemporaryFrameID);
				}
			}
		}

		const FMetaStoryExecutionState& Exec = GetExecState();

		SetupNextTransition(SourceFrame, SourceStateID, TargetState, Args);
		RequestedTransition->Selection = StateSelectionResult; // It will keep the temporary storage alive until the end of the EnterState

		// Fill NextActiveFrames & NextActiveFrameEvents for backward compatibility
		if (bSetDeprecatedTransitionResultProperties)
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			RequestedTransition->Transition.NextActiveFrames.Reset();
			RequestedTransition->Transition.NextActiveFrameEvents.Reset();
			FActiveFrameID PreviousFrameID;
			FMetaStoryExecutionFrame* CurrentFrame = nullptr;
			FMetaStoryFrameStateSelectionEvents* CurrentSelectionEvents = nullptr;
			int32 CurrentSelectionEventsIndex = 0;
			for (const FActiveState& SelectedState : RequestedTransition->Selection->SelectedStates)
			{
				if (SelectedState.GetFrameID() != PreviousFrameID)
				{
					PreviousFrameID = SelectedState.GetFrameID();
					const FMetaStoryExecutionFrame* Frame = FindExecutionFrame(SelectedState.GetFrameID(), MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(RequestedTransition->Selection->TemporaryFrames));
					if (ensure(Frame))
					{
						RequestedTransition->Transition.NextActiveFrames.Add(*Frame);
						CurrentFrame = &RequestedTransition->Transition.NextActiveFrames.Last();

						RequestedTransition->Transition.NextActiveFrameEvents.AddDefaulted();
						CurrentSelectionEvents = &RequestedTransition->Transition.NextActiveFrameEvents.Last();
						CurrentSelectionEventsIndex = 0;

						CurrentFrame->ActiveStates = FMetaStoryActiveStates();
					}
				}
				check(CurrentFrame);
				check(CurrentSelectionEvents);
				CurrentFrame->ActiveStates.Push(SelectedState.GetStateHandle(), SelectedState.GetStateID());

				const FSelectionEventWithID* FoundEvent = RequestedTransition->Selection->SelectionEvents.FindByPredicate(
					[SelectedState](const FSelectionEventWithID& Event)
					{
						return Event.State == SelectedState;
					});
				if (FoundEvent)
				{
					CurrentSelectionEvents->Events[CurrentSelectionEventsIndex] = FoundEvent->Event;
				}
			}
		}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		//@todo In TriggerTransitions, the events are processed bottom up. If an event with a higher priority is process after, the event will already be consumed.
		// Consume events from states, if required. The transition might also want to consume the event. (Not a bug.)
		for (FSelectionEventWithID& Event : RequestedTransition->Selection->SelectionEvents)
		{
			const FMetaStoryExecutionFrame* Frame = FindExecutionFrame(Event.State.GetFrameID(), MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(RequestedTransition->Selection->TemporaryFrames));
			if (ensure(Frame))
			{
				const FMetaStoryCompactState* State = Frame->MetaStory->GetStateFromHandle(Event.State.GetStateHandle());
				if (ensure(State) && State->bConsumeEventOnSelect)
				{
					ConsumeEvent(Event.Event);
				}
			}
		}

		UE_SUPPRESS(LogMetaStory, Verbose,
		{
			check(RequestedTransition->Selection->SelectedStates.Num() > 0);
			const FActiveState& LastSelectedState = RequestedTransition->Selection->SelectedStates.Last();
			const FMetaStoryExecutionFrame* SelectedFrame = FindExecutionFrame(LastSelectedState.GetFrameID(), MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(RequestedTransition->Selection->TemporaryFrames));
			check(SelectedFrame);
			METASTORY_LOG(Verbose, TEXT("Transition Request. Source:'%s'. Target:'%s'. Selected:'%s'"),
				*GetSafeStateName(SourceFrame, SourceStateHandle),
				*GetSafeStateName(TargetState.MetaStory, TargetState.StateHandle),
				*GetSafeStateName(*SelectedFrame, LastSelectedState.GetStateHandle())
			);
		})

		return true;
	}

	return false;
}

void FMetaStoryExecutionContext::SetupNextTransition(
	const FMetaStoryExecutionFrame& SourceFrame,
	const UE::MetaStory::FActiveStateID SourceStateID,
	const UE::MetaStory::ExecutionContext::FStateHandleContext TargetState,
	const FTransitionArguments& Args)
{
	RequestedTransition = MakeUnique<FRequestTransitionResult>();
	SetupNextTransition(SourceFrame, SourceStateID, TargetState, Args, RequestedTransition->Transition);
}

void FMetaStoryExecutionContext::SetupNextTransition(
	const FMetaStoryExecutionFrame& SourceFrame,
	const UE::MetaStory::FActiveStateID SourceStateID,
	const UE::MetaStory::ExecutionContext::FStateHandleContext TargetState,
	const FTransitionArguments& Args,
	FMetaStoryTransitionResult& OutTransitionResult)
{
	const FMetaStoryExecutionState& Exec = GetExecState();

	OutTransitionResult.SourceFrameID = SourceFrame.FrameID;
	OutTransitionResult.SourceStateID = SourceStateID;
	OutTransitionResult.TargetState = TargetState.StateHandle;
	OutTransitionResult.CurrentState = FMetaStoryStateHandle::Invalid;
	OutTransitionResult.CurrentRunStatus = Exec.LastTickStatus;
	OutTransitionResult.ChangeType = EMetaStoryStateChangeType::Changed;
	OutTransitionResult.Priority = Args.Priority;

	// Fill NextActiveFrames & NextActiveFrameEvents for backward compatibility
	if (UE::MetaStory::ExecutionContext::Private::bSetDeprecatedTransitionResultProperties)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (SourceStateID.IsValid())
		{
			OutTransitionResult.SourceState = SourceFrame.ActiveStates.FindStateHandle(SourceStateID);
		}
		OutTransitionResult.SourceMetaStory = SourceFrame.MetaStory;
		OutTransitionResult.SourceRootState = SourceFrame.RootState;

		FMetaStoryExecutionFrame& NewFrame = OutTransitionResult.NextActiveFrames.AddDefaulted_GetRef();
		NewFrame.MetaStory = SourceFrame.MetaStory;
		NewFrame.RootState = SourceFrame.RootState;
		NewFrame.ActiveTasksStatus = SourceFrame.ActiveTasksStatus;
		if (TargetState.StateHandle == FMetaStoryStateHandle::Invalid)
		{
			NewFrame.ActiveStates = {};
		}
		else
		{
			NewFrame.ActiveStates = FMetaStoryActiveStates(TargetState.StateHandle, UE::MetaStory::FActiveStateID::Invalid);
		}
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool FMetaStoryExecutionContext::TriggerTransitions()
{
	//1. Process transition requests. Keep the single request with the highest priority.
	//2. Process tick/event/delegate transitions and tasks. TriggerTransitions, from bottom to top.
	// If delayed,
	//	If delayed completed, then process.
	//	Else add them to the delayed transition list.
	//3. If no transition, Process completion transitions, from bottom to top.
	//4. If transition occurs, check if there are any frame (sub-tree) that completed.

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_TriggerTransition);
	UE_METASTORY_DEBUG_SCOPED_PHASE(this, EMetaStoryUpdatePhase::TriggerTransitions);

	// Set flag for the scope of this function to allow direct transitions without buffering.
	FAllowDirectTransitionsScope AllowDirectTransitionsScope(*this);

	FMetaStoryExecutionState& Exec = GetExecState();

	if (EventQueue && EventQueue->HasEvents())
	{
		METASTORY_LOG(Verbose, TEXT("Trigger transitions with events: %s"), *DebugGetEventsAsString());
		UE_METASTORY_DEBUG_LOG_EVENT(this, Log, TEXT("Trigger transitions with events: %s"), *DebugGetEventsAsString());
	}

	RequestedTransition.Reset();

	//
	// Process transition requests
	//
	for (const FMetaStoryTransitionRequest& Request : InstanceData.GetTransitionRequests())
	{
		// Find frame associated with the request.
		const FMetaStoryExecutionFrame* CurrentFrame = Exec.FindActiveFrame(Request.SourceFrameID);
		if (CurrentFrame && CurrentFrame->ActiveStates.Contains(Request.SourceStateID))
		{
			const UE::MetaStory::ExecutionContext::FStateHandleContext TargetState(CurrentFrame->MetaStory, Request.TargetState);
			const FTransitionArguments TransitionArgs = FTransitionArguments{.Priority = Request.Priority, .Fallback = Request.Fallback};
			if (RequestTransitionInternal(*CurrentFrame, Request.SourceStateID, TargetState, TransitionArgs))
			{
				check(RequestedTransition.IsValid());
				RequestedTransition->Source = FMetaStoryTransitionSource(CurrentFrame->MetaStory, EMetaStoryTransitionSourceType::ExternalRequest, Request.TargetState, Request.Priority);
			}
		}
	}

	//@todo should only clear once when the transition is successful.
	//to prevent 2 async requests and the first requests fails for X reason.
	//they will be identified by a Frame/StateID so it's fine if they stay in the array.
	InstanceData.ResetTransitionRequests();

	//
	// Collect expired delayed transitions
	//
	TArray<FMetaStoryTransitionDelayedState, TInlineAllocator<8>> ExpiredTransitionsDelayed;
	for (TArray<FMetaStoryTransitionDelayedState>::TIterator It = Exec.DelayedTransitions.CreateIterator(); It; ++It)
	{
		if (It->TimeLeft <= 0.0f)
		{
			ExpiredTransitionsDelayed.Emplace(MoveTemp(*It));
			It.RemoveCurrentSwap();
		}
	}

	//
	// Collect tick, event, and task based transitions.
	//
	struct FTransitionHandler
	{
		FTransitionHandler() = default;

		FTransitionHandler(const int32 InFrameIndex, const FMetaStoryStateHandle InStateHandle, const UE::MetaStory::FActiveStateID InStateID, const EMetaStoryTransitionPriority InPriority)
			: StateHandle(InStateHandle)
			, StateID(InStateID)
			, TaskIndex(FMetaStoryIndex16::Invalid)
			, FrameIndex(InFrameIndex)
			, Priority(InPriority)
		{
		}

		FTransitionHandler(const int32 InFrameIndex, const FMetaStoryStateHandle InStateHandle, const UE::MetaStory::FActiveStateID InStateID, const FMetaStoryIndex16 InTaskIndex, const EMetaStoryTransitionPriority InPriority)
			: StateHandle(InStateHandle)
			, StateID(InStateID)
			, TaskIndex(InTaskIndex)
			, FrameIndex(InFrameIndex)
			, Priority(InPriority)
		{
		}

		FMetaStoryStateHandle StateHandle;
		UE::MetaStory::FActiveStateID StateID;
		FMetaStoryIndex16 TaskIndex = FMetaStoryIndex16::Invalid;
		int32 FrameIndex = 0;
		EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::Normal;

		bool operator<(const FTransitionHandler& Other) const
		{
			// Highest priority first.
			return Priority > Other.Priority;
		}
	};

	TArray<FTransitionHandler, TInlineAllocator<16>> TransitionHandlers;

	if (Exec.ActiveFrames.Num() > 0)
	{
		// Re-cache bHasEvents, RequestTransition above can create new events.
		const bool bHasEvents = EventQueue && EventQueue->HasEvents();
		const bool bHasBroadcastedDelegates = Storage.HasBroadcastedDelegates();

		// Transition() can TriggerTransitions() in a loop when a sub-frame completes.
		//We do not want to evaluate the transition from that sub-frame.
		const int32 EndFrameIndex = TriggerTransitionsFromFrameIndex.Get(Exec.ActiveFrames.Num() - 1);
		for (int32 FrameIndex = EndFrameIndex; FrameIndex >= 0; FrameIndex--)
		{
			FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
			const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;
			const int32 CurrentActiveNodeIndex = CurrentFrame.ActiveNodeIndex.AsInt32();

			for (int32 StateIndex = CurrentFrame.ActiveStates.Num() - 1; StateIndex >= 0; StateIndex--)
			{
				const FMetaStoryStateHandle StateHandle = CurrentFrame.ActiveStates[StateIndex];
				const UE::MetaStory::FActiveStateID StateID = CurrentFrame.ActiveStates.StateIDs[StateIndex];
				const FMetaStoryCompactState& State = CurrentMetaStory->States[StateHandle.Index];

				checkf(State.bEnabled, TEXT("Only enabled states are in ActiveStates."));

				// Transition tasks.
				if (State.bHasTransitionTasks)
				{
					bool bAdded = false;
					for (int32 TaskIndex = (State.TasksBegin + State.TasksNum) - 1; TaskIndex >= State.TasksBegin; TaskIndex--)
					{
						const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
						if (Task.bShouldAffectTransitions && Task.bTaskEnabled && TaskIndex <= CurrentActiveNodeIndex)
						{
							TransitionHandlers.Emplace(FrameIndex, StateHandle, StateID, FMetaStoryIndex16(TaskIndex), Task.TransitionHandlingPriority);
							bAdded = true;
						}
					}
					ensureMsgf(bAdded, TEXT("bHasTransitionTasks is set but not task were added for the State: '%s' inside the MetaStory %s"), *State.Name.ToString(), *CurrentMetaStory->GetPathName());
				}

				// Has expired transition delayed.
				const bool bHasActiveTransitionDelayed = ExpiredTransitionsDelayed.ContainsByPredicate([StateID](const FMetaStoryTransitionDelayedState& Other)
					{
						return Other.StateID == StateID;
					});

				// Regular transitions on state
				//or A transition task can trigger an event. We need to add the state if that is a possibility
				//or Expired transition delayed
				if (State.ShouldTickTransitions(bHasEvents, bHasBroadcastedDelegates) || State.bHasTransitionTasks || bHasActiveTransitionDelayed)
				{
					TransitionHandlers.Emplace(FrameIndex, StateHandle, StateID, EMetaStoryTransitionPriority::Normal);
				}
			}

			if (CurrentFrame.bIsGlobalFrame)
			{
				// Global transition tasks.
				if (CurrentFrame.MetaStory->bHasGlobalTransitionTasks)
				{
					bool bAdded = false;
					for (int32 TaskIndex = (CurrentMetaStory->GlobalTasksBegin + CurrentMetaStory->GlobalTasksNum) - 1; TaskIndex >= CurrentFrame.MetaStory->GlobalTasksBegin; TaskIndex--)
					{
						const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
						if (Task.bShouldAffectTransitions && Task.bTaskEnabled)
						{
							TransitionHandlers.Emplace(FrameIndex, FMetaStoryStateHandle(), UE::MetaStory::FActiveStateID::Invalid, FMetaStoryIndex16(TaskIndex), Task.TransitionHandlingPriority);
							bAdded = true;
						}
					}
					ensureMsgf(bAdded, TEXT("bHasGlobalTransitionTasks is set but not task were added for the MetaStory `%s`"), *CurrentMetaStory->GetPathName());
				}
			}
		}

		// Sort by priority and adding order.
		TransitionHandlers.StableSort();
	}

	//
	// Process task and state transitions in priority order. 
	//
	for (const FTransitionHandler& Handler : TransitionHandlers)
	{
		const int32 FrameIndex = Handler.FrameIndex;
		FMetaStoryExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
		FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
		const UE::MetaStory::FActiveFrameID CurrentFrameID = CurrentFrame.FrameID;
		const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;

		FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);
		FCurrentlyProcessedStateScope StateScope(*this, Handler.StateHandle);
		UE_METASTORY_DEBUG_SCOPED_STATE(this, Handler.StateHandle);

		if (Handler.TaskIndex.IsValid())
		{
			const FMetaStoryTaskBase& Task = CurrentMetaStory->Nodes[Handler.TaskIndex.Get()].Get<const FMetaStoryTaskBase>();

			// Ignore disabled task
			if (Task.bTaskEnabled == false)
			{
				METASTORY_LOG(VeryVerbose, TEXT("%*sSkipped 'TriggerTransitions' for disabled Task: '%s'"), UE::MetaStory::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
				continue;
			}

			const FMetaStoryDataView TaskInstanceView = GetDataView(CurrentParentFrame, CurrentFrame, Task.InstanceDataHandle);
			FNodeInstanceDataScope DataScope(*this, &Task, Handler.TaskIndex.Get(), Task.InstanceDataHandle, TaskInstanceView);

			// Copy bound properties.
			if (Task.BindingsBatch.IsValid())
			{
				CopyBatchOnActiveInstances(CurrentParentFrame, CurrentFrame, TaskInstanceView, Task.BindingsBatch);
			}

			METASTORY_LOG(VeryVerbose, TEXT("%*sTriggerTransitions: '%s'"), UE::MetaStory::Debug::IndentSize, TEXT(""), *Task.Name.ToString());
			UE_METASTORY_DEBUG_TASK_EVENT(this, Handler.TaskIndex.Get(), TaskInstanceView, EMetaStoryTraceEventType::OnEvaluating, EMetaStoryRunStatus::Running);
			check(TaskInstanceView.IsValid());

			Task.TriggerTransitions(*this);
		}
		else if (Handler.StateHandle.IsValid())
		{
			check(Handler.StateID.IsValid());
			const FMetaStoryCompactState& State = CurrentMetaStory->States[Handler.StateHandle.Index];

			// Transitions
			for (uint8 TransitionCounter = 0; TransitionCounter < State.TransitionsNum; ++TransitionCounter)
			{
				// All transition conditions must pass
				const int16 TransitionIndex = State.TransitionsBegin + TransitionCounter;
				const FMetaStoryCompactStateTransition& Transition = CurrentMetaStory->Transitions[TransitionIndex];

				// Skip disabled transitions
				if (Transition.bTransitionEnabled == false)
				{
					continue;
				}

				// No need to test the transition if same or higher priority transition has already been processed.
				if (RequestedTransition.IsValid() && Transition.Priority <= RequestedTransition->Transition.Priority)
				{
					continue;
				}

				// Skip completion transitions
				if (EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnStateCompleted))
				{
					continue;
				}

				// If a delayed transition has passed the delay, try trigger it.
				if (Transition.HasDelay())
				{
					bool bTriggeredDelayedTransition = false;
					for (const FMetaStoryTransitionDelayedState& DelayedTransition : ExpiredTransitionsDelayed)
					{
						if (DelayedTransition.StateID == Handler.StateID && DelayedTransition.TransitionIndex == FMetaStoryIndex16(TransitionIndex))
						{
							METASTORY_LOG(Verbose, TEXT("Passed delayed transition from '%s' (%s) -> '%s'"),
								*GetSafeStateName(CurrentFrame, CurrentFrame.ActiveStates.Last()), *State.Name.ToString(), *GetSafeStateName(CurrentFrame, Transition.State));

							// Trigger Delayed Transition when the delay has passed.
							const UE::MetaStory::ExecutionContext::FStateHandleContext TargetState(CurrentFrame.MetaStory, Transition.State);
							FTransitionArguments TransitionArgs;
							TransitionArgs.Priority = Transition.Priority;
							TransitionArgs.TransitionEvent = DelayedTransition.CapturedEvent;
							TransitionArgs.Fallback = Transition.Fallback;
							if (RequestTransitionInternal(CurrentFrame, Handler.StateID, TargetState, TransitionArgs))
							{
								// If the transition was successfully requested with a specific event, consume and remove the event, it's been used.
								if (DelayedTransition.CapturedEvent.IsValid() && Transition.bConsumeEventOnSelect)
								{
									ConsumeEvent(DelayedTransition.CapturedEvent);
								}

								RequestedTransition->Source = FMetaStoryTransitionSource(CurrentFrame.MetaStory, FMetaStoryIndex16(TransitionIndex), Transition.State, Transition.Priority);
								bTriggeredDelayedTransition = true;
								break;
							}
						}
					}

					if (bTriggeredDelayedTransition)
					{
						continue;
					}
				}

				UE::MetaStory::ExecutionContext::Private::FSharedEventInlineArray TransitionEvents;
				UE::MetaStory::ExecutionContext::Private::GetTriggerTransitionEvent(Transition, Storage, FMetaStorySharedEvent(), GetEventsToProcessView(), TransitionEvents);

				for (const FMetaStorySharedEvent& TransitionEvent : TransitionEvents)
				{
					bool bPassed = false;
					{
						FCurrentlyProcessedTransitionEventScope TransitionEventScope(*this, TransitionEvent.IsValid() ? TransitionEvent.Get() : nullptr);
						UE_METASTORY_DEBUG_TRANSITION_EVENT(this, FMetaStoryTransitionSource(CurrentFrame.MetaStory, FMetaStoryIndex16(TransitionIndex), Transition.State, Transition.Priority), EMetaStoryTraceEventType::OnEvaluating);
						bPassed = TestAllConditionsOnActiveInstances(CurrentParentFrame, CurrentFrame, Handler.StateHandle, Transition.ConditionEvaluationScopeMemoryRequirement, Transition.ConditionsBegin, Transition.ConditionsNum, EMetaStoryUpdatePhase::TransitionConditions);
					}

					if (bPassed)
					{
						// If the transitions is delayed, set up the delay. 
						if (Transition.HasDelay())
						{
							uint32 TransitionEventHash = 0u;
							if (TransitionEvent.IsValid())
							{
								TransitionEventHash = GetTypeHash(TransitionEvent.Get());
							}

							const bool bIsDelayedTransitionExisting = Exec.DelayedTransitions.ContainsByPredicate([CurrentStateID = Handler.StateID, TransitionIndex, TransitionEventHash](const FMetaStoryTransitionDelayedState& DelayedState)
								{
									return DelayedState.StateID == CurrentStateID
										&& DelayedState.TransitionIndex.Get() == TransitionIndex
										&& DelayedState.CapturedEventHash == TransitionEventHash;
								});

							if (!bIsDelayedTransitionExisting)
							{
								// Initialize new delayed transition.
								const float DelayDuration = Transition.Delay.GetRandomDuration(Exec.RandomStream);
								if (DelayDuration > 0.0f)
								{
									FMetaStoryTransitionDelayedState& DelayedState = Exec.DelayedTransitions.AddDefaulted_GetRef();
									DelayedState.StateID = Handler.StateID;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
									DelayedState.MetaStory = CurrentFrame.MetaStory;
									DelayedState.StateHandle = Handler.StateHandle;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
									DelayedState.TransitionIndex = FMetaStoryIndex16(TransitionIndex);
									DelayedState.TimeLeft = DelayDuration;
									if (TransitionEvent.IsValid())
									{
										DelayedState.CapturedEvent = TransitionEvent;
										DelayedState.CapturedEventHash = TransitionEventHash;
									}

									BeginDelayedTransition(DelayedState);
									METASTORY_LOG(Verbose, TEXT("Delayed transition triggered from '%s' (%s) -> '%s' %.1fs"),
										*GetSafeStateName(CurrentFrame, CurrentFrame.ActiveStates.Last()), *State.Name.ToString(), *GetSafeStateName(CurrentFrame, Transition.State), DelayedState.TimeLeft);

									// Delay state added, skip requesting the transition.
									continue;
								}
								// Fallthrough to request transition if duration was zero.
							}
							else
							{
								// We get here if the transitions re-triggers during the delay, on which case we'll just ignore it.
								continue;
							}
						}

						UE_METASTORY_DEBUG_TRANSITION_EVENT(this, FMetaStoryTransitionSource(CurrentFrame.MetaStory, FMetaStoryIndex16(TransitionIndex), Transition.State, Transition.Priority), EMetaStoryTraceEventType::OnRequesting);
						const UE::MetaStory::ExecutionContext::FStateHandleContext TargetState(CurrentFrame.MetaStory, Transition.State);
						FTransitionArguments TransitionArgs;
						TransitionArgs.Priority = Transition.Priority;
						TransitionArgs.TransitionEvent = TransitionEvent;
						TransitionArgs.Fallback = Transition.Fallback;
						if (RequestTransitionInternal(CurrentFrame, Handler.StateID, TargetState, TransitionArgs))
						{
							// If the transition was successfully requested with a specific event, consume and remove the event, it's been used.
							if (TransitionEvent.IsValid() && Transition.bConsumeEventOnSelect)
							{
								ConsumeEvent(TransitionEvent);
							}

							RequestedTransition->Source = FMetaStoryTransitionSource(CurrentFrame.MetaStory, FMetaStoryIndex16(TransitionIndex), Transition.State, Transition.Priority);
							break;
						}
					}
				}
			}
		}
	}

	// All events have had the change to be reacted to, clear the event queue (if this instance owns it).
	if (InstanceData.IsOwningEventQueue() && EventQueue)
	{
		EventQueue->Reset();
	}

	Storage.ResetBroadcastedDelegates();

	//
	// Check state completion transitions.
	//
	bool bProcessSubTreeCompletion = true;

	if (!RequestedTransition.IsValid()
		&& (Exec.LastTickStatus != EMetaStoryRunStatus::Running || Exec.bHasPendingCompletedState))
	{
		// Find the pending completed frame/state. Don't cache the result because this function is reentrant.
		//Stop at the first completion.
		int32 FrameIndexToStart = -1;
		int32 StateIndexToStart = -1;
		EMetaStoryRunStatus CurrentStatus = EMetaStoryRunStatus::Unset;
		for (int32 FrameIndex = 0; FrameIndex < Exec.ActiveFrames.Num(); ++FrameIndex)
		{
			using namespace UE::MetaStory;
			using namespace UE::MetaStory::ExecutionContext;
			using namespace UE::MetaStory::ExecutionContext::Private;

			const FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
			const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;
			const ETaskCompletionStatus FrameTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(CurrentMetaStory).GetCompletionStatus();
			if (CurrentFrame.bIsGlobalFrame && FrameTasksStatus != ETaskCompletionStatus::Running)
			{
				if (FrameIndex == 0)
				{
					// If first frame, then complete the tree execution.
					Exec.RequestedStop = GetPriorityRunStatus(Exec.RequestedStop, CastToRunStatus(FrameTasksStatus));
					break;
				}
				else if (bGlobalTasksCompleteOwningFrame)
				{
					const int32 ParentFrameIndex = FrameIndex - 1;
					FMetaStoryExecutionFrame& ParentFrame = Exec.ActiveFrames[ParentFrameIndex];
					const FMetaStoryStateHandle ParentLinkedState = ParentFrame.ActiveStates.Last();
					if (ensure(ParentLinkedState.IsValid()))
					{
						// Set the parent linked state as last completed state, and update tick status to the status from the transition.
						METASTORY_LOG(Verbose, TEXT("Completed subtree '%s' from global: %s"),
							*GetSafeStateName(ParentFrame, ParentLinkedState),
							*UEnum::GetDisplayValueAsText(CastToRunStatus(FrameTasksStatus)).ToString()
						);

						const FMetaStoryCompactState& State = ParentFrame.MetaStory->States[ParentLinkedState.Index];
						ParentFrame.ActiveTasksStatus.GetStatus(State).SetCompletionStatus(FrameTasksStatus);
						Exec.bHasPendingCompletedState = true;

						CurrentStatus = CastToRunStatus(FrameTasksStatus);
						FrameIndexToStart = ParentFrameIndex;
						StateIndexToStart = ParentFrame.ActiveStates.Num() - 1;
						break;
					}
				}
			}

			for (int32 StateIndex = 0; StateIndex < CurrentFrame.ActiveStates.Num(); ++StateIndex)
			{
				const FMetaStoryStateHandle CurrentHandle = CurrentFrame.ActiveStates[StateIndex];
				const FMetaStoryCompactState& State = CurrentMetaStory->States[CurrentHandle.Index];
				const ETaskCompletionStatus StateTasksStatus = CurrentFrame.ActiveTasksStatus.GetStatus(State).GetCompletionStatus();
				if (StateTasksStatus != ETaskCompletionStatus::Running)
				{
					CurrentStatus = CastToRunStatus(StateTasksStatus);
					FrameIndexToStart = FrameIndex;
					StateIndexToStart = StateIndex;
					break;
				}
			}

			if (CurrentStatus != EMetaStoryRunStatus::Unset)
			{
				break;
			}
		}

		if (CurrentStatus != EMetaStoryRunStatus::Unset)
		{
			const bool bIsCurrentStatusSucceeded = CurrentStatus == EMetaStoryRunStatus::Succeeded;
			const bool bIsCurrentStatusFailed = CurrentStatus == EMetaStoryRunStatus::Failed;
			const bool bIsCurrentStatusStopped = CurrentStatus == EMetaStoryRunStatus::Stopped;
			checkf(bIsCurrentStatusSucceeded || bIsCurrentStatusFailed || bIsCurrentStatusStopped, TEXT("Running is not accepted in the CurrentStatus loop."));

			const EMetaStoryTransitionTrigger CompletionTrigger = bIsCurrentStatusSucceeded ? EMetaStoryTransitionTrigger::OnStateSucceeded : EMetaStoryTransitionTrigger::OnStateFailed;

			// Start from the last completed state and move up to the first state.
			for (int32 FrameIndex = FrameIndexToStart; FrameIndex >= 0; --FrameIndex)
			{
				FMetaStoryExecutionFrame* CurrentParentFrame = FrameIndex > 0 ? &Exec.ActiveFrames[FrameIndex - 1] : nullptr;
				FMetaStoryExecutionFrame& CurrentFrame = Exec.ActiveFrames[FrameIndex];
				const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;

				FCurrentlyProcessedFrameScope FrameScope(*this, CurrentParentFrame, CurrentFrame);

				const int32 CurrentStateIndexToStart = FrameIndex == FrameIndexToStart ? StateIndexToStart : CurrentFrame.ActiveStates.Num() - 1;

				// Check completion transitions
				for (int32 StateIndex = CurrentStateIndexToStart; StateIndex >= 0; --StateIndex)
				{
					const FMetaStoryStateHandle CurrentStateHandle = CurrentFrame.ActiveStates[StateIndex];
					const UE::MetaStory::FActiveStateID CurrentStateID = CurrentFrame.ActiveStates.StateIDs[StateIndex];
					const FMetaStoryCompactState& CurrentState = CurrentMetaStory->States[CurrentStateHandle.Index];

					if (CurrentState.ShouldTickCompletionTransitions(bIsCurrentStatusSucceeded, bIsCurrentStatusFailed))
					{
						FCurrentlyProcessedStateScope StateScope(*this, CurrentStateHandle);
						UE_METASTORY_DEBUG_SCOPED_STATE_PHASE(this, CurrentStateHandle, EMetaStoryUpdatePhase::TriggerTransitions);

						for (uint8 TransitionCounter = 0; TransitionCounter < CurrentState.TransitionsNum; ++TransitionCounter)
						{
							// All transition conditions must pass
							const int16 TransitionIndex = CurrentState.TransitionsBegin + TransitionCounter;
							const FMetaStoryCompactStateTransition& Transition = CurrentMetaStory->Transitions[TransitionIndex];

							// Skip disabled transitions
							if (!Transition.bTransitionEnabled)
							{
								continue;
							}

							const bool bTransitionAccepted = !bIsCurrentStatusStopped
								? EnumHasAnyFlags(Transition.Trigger, CompletionTrigger)
								: Transition.Trigger == EMetaStoryTransitionTrigger::OnStateCompleted;
							if (bTransitionAccepted)
							{
								bool bPassed = false;
								{
									UE_METASTORY_DEBUG_TRANSITION_EVENT(this, FMetaStoryTransitionSource(CurrentFrame.MetaStory, FMetaStoryIndex16(TransitionIndex), Transition.State, Transition.Priority), EMetaStoryTraceEventType::OnEvaluating);
									bPassed = TestAllConditionsOnActiveInstances(CurrentParentFrame, CurrentFrame, CurrentStateHandle, Transition.ConditionEvaluationScopeMemoryRequirement, Transition.ConditionsBegin, Transition.ConditionsNum, EMetaStoryUpdatePhase::TransitionConditions);
								}

								if (bPassed)
								{
									// No delay allowed on completion conditions.
									// No priority on completion transitions, use the priority to signal that state is selected.
									UE_METASTORY_DEBUG_TRANSITION_EVENT(this, FMetaStoryTransitionSource(CurrentFrame.MetaStory, FMetaStoryIndex16(TransitionIndex), Transition.State, Transition.Priority), EMetaStoryTraceEventType::OnRequesting);
									const UE::MetaStory::ExecutionContext::FStateHandleContext TargetState(CurrentFrame.MetaStory, Transition.State);
									FTransitionArguments TransitionArgs;
									TransitionArgs.Priority = EMetaStoryTransitionPriority::Normal;
									TransitionArgs.Fallback = Transition.Fallback;
									if (RequestTransitionInternal(CurrentFrame, CurrentStateID, TargetState, TransitionArgs))
									{
										RequestedTransition->Source = FMetaStoryTransitionSource(CurrentFrame.MetaStory, FMetaStoryIndex16(TransitionIndex), Transition.State, Transition.Priority);
										break;
									}
								}
							}
						}

						if (RequestedTransition.IsValid())
						{
							break;
						}
					}
				}

				// if a valid completion transition has already been found, the remaining transitions in parent frames won't have a higher priority than the found one
				// so skip the remainder. this also prevented false positive warnings and ensures from STDebugger
				if (RequestedTransition.IsValid())
				{
					break;
				}
			}

			// Handle the case where no transition was found.
			if (!RequestedTransition.IsValid())
			{
				METASTORY_LOG(Verbose, TEXT("Could not trigger completion transition, jump back to root state."));
				UE_METASTORY_DEBUG_LOG_EVENT(this, Log, TEXT("Could not trigger completion transition, jump back to root state."));

				check(!Exec.ActiveFrames.IsEmpty());
				FMetaStoryExecutionFrame& RootFrame = Exec.ActiveFrames[0];
				check(RootFrame.ActiveStates.Num() != 0);
				FCurrentlyProcessedFrameScope RootFrameScope(*this, nullptr, RootFrame);
				FCurrentlyProcessedStateScope RootStateScope(*this, FMetaStoryStateHandle::Root);

				UE::MetaStory::ExecutionContext::FStateHandleContext TargetState(RootFrame.MetaStory, FMetaStoryStateHandle::Root);
				UE_METASTORY_DEBUG_TRANSITION_EVENT(this, FMetaStoryTransitionSource(RootFrame.MetaStory, EMetaStoryTransitionSourceType::Internal, FMetaStoryStateHandle::Root, EMetaStoryTransitionPriority::Normal), EMetaStoryTraceEventType::OnRequesting);

				if (RequestTransitionInternal(RootFrame, RootFrame.ActiveStates.StateIDs[0], TargetState, FTransitionArguments()))
				{
					RequestedTransition->Source = FMetaStoryTransitionSource(RootFrame.MetaStory, EMetaStoryTransitionSourceType::Internal, FMetaStoryStateHandle::Root, EMetaStoryTransitionPriority::Normal);
				}
				else
				{
					METASTORY_LOG(Warning, TEXT("Failed to select root state. Stopping the tree with failure."));
					UE_METASTORY_DEBUG_LOG_EVENT(this, Error, TEXT("Failed to select root state. Stopping the tree with failure."));

					Exec.RequestedStop = UE::MetaStory::ExecutionContext::GetPriorityRunStatus(Exec.RequestedStop, EMetaStoryRunStatus::Failed);

					// In this case we don't want to complete subtrees, we want to force the whole tree to stop.
					bProcessSubTreeCompletion = false;
				}
			}
		}
	}

	// Check if the transition was succeed/failed, if we're on a sub-tree, complete the subtree instead of transition.
	if (RequestedTransition.IsValid() && RequestedTransition->Transition.TargetState.IsCompletionState() && bProcessSubTreeCompletion)
	{
		// Check that the transition source frame is a sub-tree, the first frame (0 index) is not a subtree. 
		const int32 SourceFrameIndex = Exec.IndexOfActiveFrame(RequestedTransition->Transition.SourceFrameID);
		if (SourceFrameIndex > 0)
		{
			const FMetaStoryExecutionFrame& SourceFrame = Exec.ActiveFrames[SourceFrameIndex];
			const int32 ParentFrameIndex = SourceFrameIndex - 1;
			FMetaStoryExecutionFrame& ParentFrame = Exec.ActiveFrames[ParentFrameIndex];
			const FMetaStoryStateHandle ParentLinkedState = ParentFrame.ActiveStates.Last();

			if (ParentLinkedState.IsValid())
			{
				const EMetaStoryRunStatus RunStatus = RequestedTransition->Transition.TargetState.ToCompletionStatus();

#if ENABLE_VISUAL_LOG
				const int32 NextTransitionSourceIndex = SourceFrame.ActiveStates.IndexOfReverse(RequestedTransition->Transition.SourceStateID);
				const FMetaStoryStateHandle NextTransitionSourceState = NextTransitionSourceIndex != INDEX_NONE
					? SourceFrame.ActiveStates[NextTransitionSourceIndex]
					: FMetaStoryStateHandle::Invalid;
				METASTORY_LOG(Verbose, TEXT("Completed subtree '%s' from state '%s': %s"),
					*GetSafeStateName(ParentFrame, ParentLinkedState),
					*GetSafeStateName(SourceFrame, NextTransitionSourceState),
					*UEnum::GetDisplayValueAsText(RunStatus).ToString()
				);
#endif

				// Set the parent linked state as last completed state, and update tick status to the status from the transition.
				const UE::MetaStory::ETaskCompletionStatus TaskStatus = UE::MetaStory::ExecutionContext::CastToTaskStatus(RunStatus);
				const FMetaStoryCompactState& State = ParentFrame.MetaStory->States[ParentLinkedState.Index];
				ParentFrame.ActiveTasksStatus.GetStatus(State).SetCompletionStatus(TaskStatus);
				Exec.bHasPendingCompletedState = true;
				Exec.LastTickStatus = RunStatus;

				// Clear the transition and return that no transition took place.
				// Since the LastTickStatus != running, the transition loop will try another transition
				// now starting from the linked parent state. If we run out of retires in the selection loop (e.g. very deep hierarchy)
				// we will continue on next tick.
				TriggerTransitionsFromFrameIndex = ParentFrameIndex;
				RequestedTransition.Reset();
				return false;
			}
		}
	}

	// Request can be no-op, used for blocking other transitions.
	if (RequestedTransition.IsValid() && !RequestedTransition->Transition.TargetState.IsValid())
	{
		RequestedTransition.Reset();
	}

	return RequestedTransition.IsValid();
}

// Deprecated
TOptional<FMetaStoryTransitionResult> FMetaStoryExecutionContext::MakeTransitionResult(const FMetaStoryRecordedTransitionResult& RecordedTransition) const
{
	FMetaStoryTransitionResult Result;

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	for (int32 RecordedFrameIndex = 0; RecordedFrameIndex < RecordedTransition.NextActiveFrames.Num(); RecordedFrameIndex++)
	{
		const FMetaStoryRecordedExecutionFrame& RecordedExecutionFrame = RecordedTransition.NextActiveFrames[RecordedFrameIndex];

		if (RecordedExecutionFrame.MetaStory == nullptr)
		{
			return {};
		}

		if (RecordedExecutionFrame.MetaStory->GetStateFromHandle(RecordedExecutionFrame.RootState) == nullptr)
		{
			return {};
		}

		const FMetaStoryCompactFrame* CompactFrame = RecordedExecutionFrame.MetaStory->GetFrameFromHandle(RecordedExecutionFrame.RootState);
		if (CompactFrame == nullptr)
		{
			return {};
		}

		FMetaStoryExecutionFrame& ExecutionFrame = Result.NextActiveFrames.AddDefaulted_GetRef();
		ExecutionFrame.MetaStory = RecordedExecutionFrame.MetaStory;
		ExecutionFrame.RootState = RecordedExecutionFrame.RootState;
		ExecutionFrame.ActiveStates = RecordedExecutionFrame.ActiveStates;
		ExecutionFrame.ActiveTasksStatus = FMetaStoryTasksCompletionStatus(*CompactFrame);
		ExecutionFrame.bIsGlobalFrame = RecordedExecutionFrame.bIsGlobalFrame;
		ExecutionFrame.ExternalDataBaseIndex = const_cast<FMetaStoryExecutionContext*>(this)->CollectExternalData(RecordedExecutionFrame.MetaStory);

		FMetaStoryFrameStateSelectionEvents& MetaStoryFrameStateSelectionEvents = Result.NextActiveFrameEvents.AddDefaulted_GetRef();
		for (int32 EventIdx = 0; EventIdx < RecordedExecutionFrame.EventIndices.Num(); EventIdx++)
		{
			if (RecordedTransition.NextActiveFrameEvents.IsValidIndex(EventIdx))
			{
				const FMetaStoryEvent& RecordedMetaStoryEvent = RecordedTransition.NextActiveFrameEvents[EventIdx];
				MetaStoryFrameStateSelectionEvents.Events[EventIdx] = FMetaStorySharedEvent(RecordedMetaStoryEvent);
			}
		}
	}


	if (Result.NextActiveFrames.Num() != Result.NextActiveFrameEvents.Num())
	{
		return {};
	}

	if (RecordedTransition.SourceMetaStory == nullptr)
	{
		return {};
	}

	if (RecordedTransition.SourceMetaStory->GetFrameFromHandle(RecordedTransition.SourceRootState) == nullptr)
	{
		return {};
	}

	// Try to find the same frame and the same state in the currently active frames.
	// Recorded transitions can be saved and replayed out of context.
	const FMetaStoryExecutionState& Exec = GetExecState();
	const FMetaStoryExecutionFrame* ExecFrame = Exec.ActiveFrames.FindByPredicate([MetaStory = RecordedTransition.SourceMetaStory, RootState = RecordedTransition.SourceRootState](const FMetaStoryExecutionFrame& Frame)
		{
			return Frame.HasRoot(MetaStory, RootState);
		});
	if (ExecFrame)
	{
		Result.SourceFrameID = ExecFrame->FrameID;
		const int32 SourceStateIndex = ExecFrame->ActiveStates.IndexOfReverse(RecordedTransition.SourceState);
		if (SourceStateIndex != INDEX_NONE)
		{
			Result.SourceStateID = ExecFrame->ActiveStates.StateIDs[SourceStateIndex];
		}
	}
	Result.TargetState = RecordedTransition.TargetState;
	Result.Priority = RecordedTransition.Priority;
	Result.SourceState = RecordedTransition.SourceState;
	Result.SourceMetaStory = RecordedTransition.SourceMetaStory;
	Result.SourceRootState = RecordedTransition.SourceRootState;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif //WITH_EDITOR

	return Result;
}

// Deprecated
FMetaStoryRecordedTransitionResult FMetaStoryExecutionContext::MakeRecordedTransitionResult(const FMetaStoryTransitionResult& Transition) const
{
	FMetaStoryRecordedTransitionResult Result;

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	for (int32 FrameIndex = 0; FrameIndex < Transition.NextActiveFrames.Num(); FrameIndex++)
	{
		const FMetaStoryExecutionFrame& ExecutionFrame = Transition.NextActiveFrames[FrameIndex];
		const FMetaStoryFrameStateSelectionEvents& StateSelectionEvents = Transition.NextActiveFrameEvents[FrameIndex];

		FMetaStoryRecordedExecutionFrame& RecordedFrame = Result.NextActiveFrames.AddDefaulted_GetRef();
		RecordedFrame.MetaStory = ExecutionFrame.MetaStory;
		RecordedFrame.RootState = ExecutionFrame.RootState;
		RecordedFrame.ActiveStates = ExecutionFrame.ActiveStates;
		RecordedFrame.bIsGlobalFrame = ExecutionFrame.bIsGlobalFrame;

		for (int32 StateIndex = 0; StateIndex < ExecutionFrame.ActiveStates.Num(); StateIndex++)
		{
			const FMetaStoryEvent* Event = StateSelectionEvents.Events[StateIndex].Get();
			if (Event)
			{
				const int32 EventIndex = Result.NextActiveFrameEvents.Add(*Event);
				RecordedFrame.EventIndices[StateIndex] = static_cast<uint8>(EventIndex);
			}
		}
	}

	const FMetaStoryExecutionState& Exec = GetExecState();
	if (const FMetaStoryExecutionFrame* FoundSourceFrame = Exec.FindActiveFrame(Transition.SourceFrameID))
	{
		Result.SourceMetaStory = FoundSourceFrame->MetaStory;
		Result.SourceRootState = FoundSourceFrame->RootState;
		const int32 ActiveStateIndex = FoundSourceFrame->ActiveStates.IndexOfReverse(Transition.SourceStateID);
		if (ActiveStateIndex != INDEX_NONE)
		{
			Result.SourceState = FoundSourceFrame->ActiveStates[ActiveStateIndex];
		}
	}
	else
	{
		Result.SourceMetaStory = Transition.SourceMetaStory;
		Result.SourceRootState = Transition.SourceRootState;
		Result.SourceState = Transition.SourceState;
	}
	Result.TargetState = Transition.TargetState;
	Result.Priority = Transition.Priority;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif //WITH_EDITOR

	return Result;
}

FMetaStoryRecordedTransitionResult FMetaStoryExecutionContext::MakeRecordedTransitionResult(const TSharedRef<FSelectStateResult>& SelectStateResult, const FMetaStoryTransitionResult& TransitionResult) const
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	const FMetaStoryExecutionState& Exec = GetExecState();

	FMetaStoryRecordedTransitionResult Recorded;
	Recorded.States.Reserve(SelectStateResult->SelectedStates.Num());
	for (const UE::MetaStory::FActiveState& SelectedState : SelectStateResult->SelectedStates)
	{
		const FMetaStoryExecutionFrame* ProcessFrame = FindExecutionFrame(SelectedState.GetFrameID(), MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(SelectStateResult->TemporaryFrames));
		if (ProcessFrame == nullptr)
		{
			return FMetaStoryRecordedTransitionResult();
		}
		FMetaStoryRecordedActiveState& RecordedState = Recorded.States.AddDefaulted_GetRef();
		RecordedState.MetaStory = ProcessFrame->MetaStory;
		RecordedState.State = SelectedState.GetStateHandle();
	}

	Recorded.Events.Reserve(SelectStateResult->SelectionEvents.Num());
	for (const FSelectionEventWithID& SelectedEvent : SelectStateResult->SelectionEvents)
	{
		if (SelectedEvent.Event.IsValid())
		{
			const int32 RecordedEventIndex = Recorded.Events.Add(*SelectedEvent.Event.Get());
			if (const int32 SeletedStateIndex = FActiveStatePath::IndexOf(SelectStateResult->SelectedStates, SelectedEvent.State); SeletedStateIndex != INDEX_NONE)
			{
				Recorded.States[SeletedStateIndex].EventIndex = RecordedEventIndex;
			}
		}
	}

	Recorded.Priority = TransitionResult.Priority;

	return Recorded;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Deprecated
bool FMetaStoryExecutionContext::SelectState(const FMetaStoryExecutionFrame& CurrentFrame,
	const FMetaStoryStateHandle NextState,
	FStateSelectionResult& OutSelectionResult,
	const FMetaStorySharedEvent* TransitionEvent,
	const EMetaStorySelectionFallback Fallback)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	FActiveStateInlineArray CurrentActiveStatePath;
	GetActiveStatePath(GetExecState().ActiveFrames, CurrentActiveStatePath);

	FSelectStateArguments SelectStateArgs;
	SelectStateArgs.ActiveStates = MakeConstArrayView(CurrentActiveStatePath);
	SelectStateArgs.SourceState = FActiveState();
	SelectStateArgs.TargetState = FStateHandleContext(CurrentFrame.MetaStory, NextState);
	SelectStateArgs.TransitionEvent = TransitionEvent != nullptr ? *TransitionEvent : FMetaStorySharedEvent();
	SelectStateArgs.Fallback = Fallback;
	SelectStateArgs.SelectionRules = CurrentFrame.MetaStory->StateSelectionRules;

	TSharedRef<FSelectStateResult> SelectStateResult = MakeShared<FSelectStateResult>();
	TGuardValue<TSharedPtr<FSelectStateResult>> ExecutionFrameHolderScope(CurrentlyProcessedTemporaryStorage, SelectStateResult);
	return SelectState(SelectStateArgs, SelectStateResult);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

bool FMetaStoryExecutionContext::SelectState(const FSelectStateArguments& Args, const TSharedRef<FSelectStateResult>& OutSelectionResult)
{
	// 1. Find the target root state.
	// 2. If the the target frame is active, then copy all previous states from the previous frames.
	//   If any state inside the frame is completed (before the target), then the transition fails.
	//   See EMetaStoryStateSelectionRules::CompletedStateBeforeTargetFailTransition.
	// 3. In the target frame, start adding states that match the source/target.
	//   If the state is completed (before the target), then the transition fails.
	//   See EMetaStoryStateSelectionRules::CompletedStateBeforeTargetFailTransition.
	// 4. New/Sustained states need to be reevaluated (see SelectStateInternal).
	// 5. Else, handle fallback.
	// 
	// Source is from where the transition request occurs.
	//  The source frame is valid but the source state can be invalid. It will be the top root state if needed.
	//  It doesn't need to be in the same frame as the target.
	// Target is where we wish to go. The selection can stop or select another state (depending on the state's type).
	// Selected is the selection result.
	//
	// ExitState: If the state >= Target and is in the selected, then the transition is "sustained" and the instance data is untouched.
	// ExitState: If the state not in the selected (removed state), then the transition is "changed" and the state is removed.
	// EnterState: If the state >= Target and it is in the actives, then the transition is "sustained" and the instance data is untouched.
	// EnterState: If the state >= Target and it is not in the actives (new state), then the transition is "changed" and the state is added.
	// 
	// Rules might impact the results.
	// In the examples,  "New State if completed" is only valid if EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates
	// Examples: Active | Source | Target | Selected
	//
	//           ABCD   | ABCD   | ABCDE  | ABCDEF, ABCD'EF
	//  New D if completed, always new EF.
	//  ExitState: Sustained -. Changed -, D. EnterState: Sustained -. Changed EF, D'EF.
	//
	//           ABCD   | AB     | ABI    | ABIJ, AB'IJ
	//  New B if completed, always new IJ.
	//  ExitState: Sustained -. Changed CD, BCD. EnterState: Sustained -. Changed IJ, B'IJ.
	// 
	//           ABCD   | AB     | ABC    | ABCD, ABCD', ABC'D', AB'C'D'
	//  New D, CD, BCD if completed.
	//  ExitState: Sustained CD, C, -, -. Changed -, D, CD, BCD. EnterState: Sustained CD, C, -, -. Changed -, D', C'D', B'C'D'.
	//
	//           ABCD   | ABC     | AB    | ABCD, ABCD', ABC'D', AB'C'D'
	//  New D, CD, BCD if completed.
	//  ExitState: Sustained BCD, BC, B, -. Changed -, D, CD, BCD. EnterState: Sustained BCD, BC, B, -. Changed -, D', C'D', B'C'D'.
	// 
	//           ABCD   | AB     | AX     | AXY
	//  Source is not in target. New XY.
	//  ExitState: Sustained -. Changed BCD. EnterState: Sustained -. Changed XY
	//
	//           ABCD   | AB     | AB     | ABCD, ABCD', ABC'D', AB'C'D'
	//  Source is target. New D, CD, BCD if completed.
	//  ExitState: Sustained BCD, BC, B, -. Changed -, D, CD, BCD. EnterState: Sustained BCD, BC, B, -. Changed -, D', C'D', B'C'D'.

	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	FMetaStoryExecutionState& Exec = GetExecState();

	// Not a valid target
	if (!ensure(Args.TargetState.StateHandle.IsValid() && !Args.TargetState.StateHandle.IsCompletionState()))
	{
		return false;
	}
	if (!ensure(Args.TargetState.MetaStory && Args.TargetState.MetaStory->GetStateFromHandle(Args.TargetState.StateHandle)))
	{
		return false;
	}
	// Not a valid source. Note Source can be a GlobalTask (not in active state)
	if (!ensure(Args.SourceState.GetFrameID().IsValid()))
	{
		return false;
	}

	TArray<FStateHandleContext, TInlineAllocator<FMetaStoryActiveStates::MaxStates>> PathToTargetState;
	if (!GetStatesListToState(Args.TargetState, PathToTargetState))
	{
		METASTORY_LOG(Error, TEXT("%hs: Reached max execution depth when trying to select state %s from '%s'.  '%s' using MetaStory '%s'."),
			__FUNCTION__,
			*GetSafeStateName(Args.TargetState.MetaStory, Args.TargetState.StateHandle),
			*GetStateStatusString(Exec),
			*GetNameSafe(&Owner),
			*GetFullNameSafe(&RootMetaStory)
		);
		return false;
	}
	check(!PathToTargetState.IsEmpty());

	const FMetaStoryExecutionFrameHandle TargetFrameHandle = FMetaStoryExecutionFrameHandle(Args.TargetState.MetaStory, PathToTargetState[0].StateHandle);
	const FMetaStoryCompactFrame* TargetFrame = TargetFrameHandle.GetMetaStory()->GetFrameFromHandle(TargetFrameHandle.GetRootState());
	checkf(TargetFrame, TEXT("The frame was not compiled."));

	// Build the source path from the active path. Includes all the states from the previous frames.
	TArrayView<const FActiveState> SourceStates;
	{
		if (Args.SourceState.GetStateID().IsValid())
		{
			const int32 FoundIndex = FActiveStatePath::IndexOf(Args.ActiveStates, Args.SourceState);
			if (FoundIndex == INDEX_NONE)
			{
				METASTORY_LOG(Error, TEXT("%hs: The source do not exist in the active path when trying to select state %s from '%s'. '%s' using MetaStory '%s'."),
					__FUNCTION__,
					*GetSafeStateName(Args.TargetState.MetaStory, Args.TargetState.StateHandle),
					*GetStateStatusString(Exec),
					*GetNameSafe(&Owner),
					*GetFullNameSafe(&RootMetaStory)
				);
				return false;
			}

			SourceStates = Args.ActiveStates.Left(FoundIndex + 1);
		}
		else
		{
			// Pick the source frame first state.
			//This is usually when the request is out of the scope or from a global task. It should start from the root of the frame.
			int32 ActiveStateIndex = 0;
			for (; ActiveStateIndex < Args.ActiveStates.Num(); ++ActiveStateIndex)
			{
				const FActiveFrameID CurrentActiveFrameID = Args.ActiveStates[ActiveStateIndex].GetFrameID();
				if (CurrentActiveFrameID == Args.SourceState.GetFrameID())
				{
					break;
				}
			}
			if (ActiveStateIndex < Args.ActiveStates.Num())
			{
				SourceStates = Args.ActiveStates.Left(ActiveStateIndex + 1);
			}
			else
			{
				// SourceFrame exists, so the state must be in the active states.
				if (!ensure(Args.ActiveStates.Num() == 0))
				{
					return false;
				}
			}
		}
	}
	checkf(SourceStates.Num() == 0 || FActiveStatePath::StartsWith(Args.ActiveStates, SourceStates), TEXT("Source is part of the active path."));
	FMetaStoryExecutionFrame* SourceExecFrame = FindExecutionFrame(Args.SourceState.GetFrameID(), MakeArrayView(Exec.ActiveFrames), MakeArrayView(OutSelectionResult->TemporaryFrames));
	if (!ensure(SourceExecFrame))
	{
		return false;
	}

	const int32 SelectedStatesSlack = Args.ActiveStates.Num() + 10; // Number to help with buffer reallocation
	OutSelectionResult->SelectedStates.Empty(SelectedStatesSlack);

	// Find the Target inside the ActiveStates.
	//Look in the SourceStates because you can select in a different frame up the tree but not down the tree.
	FActiveFrameID TargetFrameID;
	{
		if (SourceExecFrame->HasRoot(TargetFrameHandle))
		{
			TargetFrameID = SourceExecFrame->FrameID;

			// Copy active to the new selected until we reach the frame.
			FActiveFrameID CurrentFrameID;
			for (const FActiveState& ActiveState : Args.ActiveStates)
			{
				if (CurrentFrameID != ActiveState.GetFrameID())
				{
					CurrentFrameID = ActiveState.GetFrameID();
					if (ActiveState.GetFrameID() == Args.SourceState.GetFrameID())
					{
						break;
					}
					OutSelectionResult->SelectedFrames.Add(CurrentFrameID);
				}
				OutSelectionResult->SelectedStates.Add(ActiveState);
			}
		}
		else if (SourceStates.Num() >= 0)
		{
			// Can jump to a state from a previous frame.
			//Find the common frame or the first frame with the same tree asset.
			int32 FoundActiveStateIndex = SourceStates.Num() - 1;
			{
				bool bFoundRootState = false;
				bool bFoundMetaStory = false;
				FActiveFrameID CurrentFrameID;
				for (; FoundActiveStateIndex >= 0; --FoundActiveStateIndex)
				{
					const FActiveFrameID CurrentActiveFrameID = SourceStates[FoundActiveStateIndex].GetFrameID();
					if (CurrentActiveFrameID != CurrentFrameID)
					{
						FMetaStoryExecutionFrame* ProcessFrame = FindExecutionFrame(CurrentActiveFrameID, MakeArrayView(Exec.ActiveFrames), MakeArrayView(OutSelectionResult->TemporaryFrames));
						if (!ensure(ProcessFrame))
						{
							return false;
						}
						if (ProcessFrame->MetaStory == TargetFrameHandle.GetMetaStory())
						{
							bFoundMetaStory = true;
							if (ProcessFrame->RootState == TargetFrameHandle.GetRootState())
							{
								bFoundRootState = true;
							}
							else if (bFoundRootState)
							{
								TargetFrameID = CurrentActiveFrameID;
								break;
							}
						}
						else if (bFoundMetaStory)
						{
							break;
						}
					}
				}

				if (FoundActiveStateIndex < 0 && !bFoundMetaStory)
				{
					METASTORY_LOG(Error, TEXT("%hs: Encountered unrecognized state %s during state selection from '%s'.  '%s' using MetaStory '%s'."),
						__FUNCTION__,
						*GetSafeStateName(Args.TargetState.MetaStory, Args.TargetState.StateHandle),
						*GetStateStatusString(Exec),
						*GetNameSafe(&Owner),
						*GetFullNameSafe(&RootMetaStory)
					);
					return false;
				}

				// Copy active states to the new selected until we reach the frame.
				for (int32 ActiveStateIndex = 0; ActiveStateIndex < FoundActiveStateIndex + 1; ++ActiveStateIndex)
				{
					const FActiveState& ActiveState = Args.ActiveStates[ActiveStateIndex];
					if (CurrentFrameID != ActiveState.GetFrameID())
					{
						CurrentFrameID = ActiveState.GetFrameID();
						OutSelectionResult->SelectedFrames.Add(CurrentFrameID);
					}
					OutSelectionResult->SelectedStates.Add(ActiveState);
				}
			}
		}
		else
		{
			// SourceExecFrame do not matches and there are no source states.
			METASTORY_LOG(Error, TEXT("%hs: Encountered out of range state %s during state selection from '%s'.  '%s' using MetaStory '%s'."),
				__FUNCTION__,
				*GetSafeStateName(Args.TargetState.MetaStory, Args.TargetState.StateHandle),
				*GetStateStatusString(Exec),
				*GetNameSafe(&Owner),
				*GetFullNameSafe(&RootMetaStory)
			);
			return false;
		}
	}

	FSelectStateInternalArguments InternalArgs;
	InternalArgs.MissingActiveStates = Args.ActiveStates.Mid(OutSelectionResult->SelectedStates.Num());
	InternalArgs.MissingSourceStates = SourceStates.Mid(OutSelectionResult->SelectedStates.Num());
	InternalArgs.MissingStatesToReachTarget = PathToTargetState;
	InternalArgs.MissingSourceFrameID = TargetFrameID;

	// Add the state and check the prerequisites.
	if (SelectStateInternal(Args, InternalArgs, OutSelectionResult))
	{
		return true;
	}

	// Failed to Select Target State, handle fallback here
	if (Args.Fallback == EMetaStorySelectionFallback::NextSelectableSibling
		&& PathToTargetState.Num() >= 2 // we are not selecting the root
		&& TargetFrameID.IsValid()) // we are not selecting a new frame on purpose
	{
		const FStateHandleContext Parent = PathToTargetState.Last(1);
		FStateHandleContext& Target = PathToTargetState.Last();
		if (ensure(Parent.MetaStory && Parent.StateHandle.IsValid() && Parent.MetaStory == Target.MetaStory && Target.StateHandle.IsValid()))
		{
			// Get the next sibling
			const FMetaStoryCompactState& ParentState = Parent.MetaStory->States[Parent.StateHandle.Index];
			for (uint16 ChildStateIndex = Target.MetaStory->States[Target.StateHandle.Index].GetNextSibling(); ChildStateIndex < ParentState.ChildrenEnd; ChildStateIndex = Target.MetaStory->States[Target.StateHandle.Index].GetNextSibling())
			{
				const FMetaStoryStateHandle NextChildStateHandle = FMetaStoryStateHandle(ChildStateIndex);

				FSelectStateArguments NewArgs = Args;
				NewArgs.TargetState.StateHandle = NextChildStateHandle;
				Target = FStateHandleContext(Parent.MetaStory, NextChildStateHandle);
				if (SelectStateInternal(NewArgs, InternalArgs, OutSelectionResult))
				{
					return true;
				}
			}
		}
	}

	return false;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Deprecated
bool FMetaStoryExecutionContext::SelectStateInternal(
	const FMetaStoryExecutionFrame* CurrentParentFrame,
	FMetaStoryExecutionFrame& CurrentFrame,
	const FMetaStoryExecutionFrame* CurrentFrameInActiveFrames,
	TConstArrayView<FMetaStoryStateHandle> PathToNextState,
	FStateSelectionResult& OutSelectionResult,
	const FMetaStorySharedEvent* TransitionEvent)
{
	return false;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

bool FMetaStoryExecutionContext::SelectStateInternal(
	const FSelectStateArguments& Args,
	const FSelectStateInternalArguments& InternalArgs,
	const TSharedRef<FSelectStateResult>& OutSelectionResult)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(MetaStory_SelectState);

	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	const FMetaStoryExecutionState& Exec = GetExecState();

	if (InternalArgs.MissingStatesToReachTarget.IsEmpty())
	{
		return true;
	}

	const FStateHandleContext& NextStateContext = InternalArgs.MissingStatesToReachTarget[0];
	const UMetaStory* NextMetaStory = NextStateContext.MetaStory;
	const FMetaStoryStateHandle NextStateHandle = NextStateContext.StateHandle;
	if (!ensure(NextMetaStory != nullptr && NextStateHandle.IsValid()))
	{
		return false;
	}

	const FMetaStoryCompactState& NextState = NextMetaStory->States[NextStateHandle.Index];
	if (!NextState.bEnabled)
	{
		METASTORY_LOG(VeryVerbose, TEXT("%hs: Ignoring disabled state '%s'.  '%s' using MetaStory '%s'."),
			__FUNCTION__,
			*NextState.Name.ToString(),
			*GetNameSafe(&Owner),
			*GetFullNameSafe(NextMetaStory)
		);
		return false;
	}
	if (NextState.SelectionBehavior == EMetaStoryStateSelectionBehavior::None)
	{
		METASTORY_LOG(VeryVerbose, TEXT("%hs: Selection Behavior is none for state '%s'.  '%s' using MetaStory '%s'."),
			__FUNCTION__,
			*NextState.Name.ToString(),
			*GetNameSafe(&Owner),
			*GetFullNameSafe(NextMetaStory)
		);
		return false;
	}

	struct FCleanUpOnExit
	{
		FCleanUpOnExit(const TSharedRef<FSelectStateResult>& InSelectionResult)
			: SelectionResult(InSelectionResult)
		{ }
		~FCleanUpOnExit()
		{
			if (bStateAdded && bSucceededToSelectState == false)
			{
				SelectionResult->SelectedStates.Pop();
			}
			if (bFrameAdded && bSucceededToSelectState == false)
			{
				SelectionResult->SelectedFrames.Pop();
			}
		}

		const TSharedRef<FSelectStateResult>& SelectionResult;
		bool bSucceededToSelectState = false;
		bool bFrameAdded = false;
		bool bStateAdded = false;
	};
	FCleanUpOnExit OnExitScope = FCleanUpOnExit(OutSelectionResult);

	// Does it need a new root tree. It can create a new frame or reused an existing one.
	bool bNewFrameCreated = false;
	{
		if (InternalArgs.MissingSourceStates.Num() == 0 && !InternalArgs.MissingSourceFrameID.IsValid())
		{
			// Must be in a valid selection (state are already been selected).
			if (OutSelectionResult->SelectedStates.Num() == 0)
			{
				METASTORY_LOG(Verbose, TEXT("%hs: No root state to select '%s'.  '%s' using MetaStory '%s'."),
					__FUNCTION__,
					*NextState.Name.ToString(),
					*GetNameSafe(&Owner),
					*GetFullNameSafe(NextMetaStory)
				);
				return false;
			}

			const FActiveState PreviousSelectedState = OutSelectionResult->SelectedStates.Last();
			FMetaStoryExecutionFrame* PreviousFramePtr = FindExecutionFrame(PreviousSelectedState.GetFrameID(), MakeArrayView(GetExecState().ActiveFrames), MakeArrayView(OutSelectionResult->TemporaryFrames));
			check(PreviousFramePtr);
			check(PreviousFramePtr->MetaStory == NextMetaStory);

			if (PreviousSelectedState.GetStateHandle() != NextState.Parent)
			{
				const FMetaStoryExecutionFrameHandle FrameHandle = FMetaStoryExecutionFrameHandle(NextStateContext.MetaStory, NextStateContext.StateHandle);
				FMetaStoryExecutionFrame& NewFrame = OutSelectionResult->MakeAndAddTemporaryFrameWithNewRoot(FActiveFrameID(Storage.GenerateUniqueId()), FrameHandle, *PreviousFramePtr);
				NewFrame.ExecutionRuntimeIndexBase = FMetaStoryIndex16(Storage.AddExecutionRuntimeData(GetOwner(), FrameHandle));
				OutSelectionResult->SelectedFrames.Add(NewFrame.FrameID);
				OnExitScope.bFrameAdded = true;
				bNewFrameCreated = true;
			}
		}
		else
		{
			// We are building the path toward the desired target.
			const FActiveFrameID MissingFrameID = InternalArgs.MissingSourceStates.Num() != 0 ? InternalArgs.MissingSourceStates[0].GetFrameID() : InternalArgs.MissingSourceFrameID;
			FMetaStoryExecutionFrame* MissingFramePtr = FindExecutionFrame(MissingFrameID, MakeArrayView(GetExecState().ActiveFrames), MakeArrayView(OutSelectionResult->TemporaryFrames));
			check(MissingFramePtr);
			if (!ensure(MissingFramePtr->MetaStory == NextMetaStory))
			{
				return false;
			}

			FActiveFrameID NextFrameID = MissingFrameID;
			if (InternalArgs.MissingSourceStates.Num() != 0)
			{
				if (!NextState.Parent.IsValid() && MissingFramePtr->RootState != NextStateHandle)
				{
					const FMetaStoryExecutionFrameHandle FrameHandle = FMetaStoryExecutionFrameHandle(NextStateContext.MetaStory, NextStateContext.StateHandle);
					FMetaStoryExecutionFrame& NewFrame = OutSelectionResult->MakeAndAddTemporaryFrameWithNewRoot(FActiveFrameID(Storage.GenerateUniqueId()), FrameHandle, *MissingFramePtr);
					NewFrame.ExecutionRuntimeIndexBase = FMetaStoryIndex16(Storage.AddExecutionRuntimeData(GetOwner(), FrameHandle));
					NextFrameID = NewFrame.FrameID;
					bNewFrameCreated = true;
				}
			}
			else if (MissingFramePtr->RootState != NextStateHandle)
			{
				const FMetaStoryExecutionFrameHandle FrameHandle = FMetaStoryExecutionFrameHandle(NextStateContext.MetaStory, NextStateContext.StateHandle);
				FMetaStoryExecutionFrame& NewFrame = OutSelectionResult->MakeAndAddTemporaryFrameWithNewRoot(FActiveFrameID(Storage.GenerateUniqueId()), FrameHandle, *MissingFramePtr);
				NewFrame.ExecutionRuntimeIndexBase = FMetaStoryIndex16(Storage.AddExecutionRuntimeData(GetOwner(), FrameHandle));
				NextFrameID = NewFrame.FrameID;
				bNewFrameCreated = true;
			}

			// Add new frames.
			if (OutSelectionResult->SelectedFrames.Num() > 0)
			{
				if (OutSelectionResult->SelectedFrames.Last() != NextFrameID)
				{
					OutSelectionResult->SelectedFrames.Add(NextFrameID);
					OnExitScope.bFrameAdded = true;
				}
			}
			else
			{
				OutSelectionResult->SelectedFrames.Add(NextFrameID);
				OnExitScope.bFrameAdded = true;
			}

			TOptional<bool> bSelected = SelectStateFromSourceInternal(Args, InternalArgs, OutSelectionResult, *MissingFramePtr, NextState, NextStateHandle, bNewFrameCreated);
			if (bSelected.IsSet())
			{
				OnExitScope.bSucceededToSelectState = bSelected.GetValue();
				return OnExitScope.bSucceededToSelectState;
			}
		}
	}

	// Use the ID to get the ExecutionFrame because it is saved in an array (and this is a recursive function), the array can grow and the pointer won't be valid.
	check(OutSelectionResult->SelectedFrames.Num() != 0);
	const UE::MetaStory::FActiveFrameID NextFrameID = OutSelectionResult->SelectedFrames.Last();

	const FMetaStoryExecutionFrame* NextFrame = nullptr;
	const FMetaStoryExecutionFrame* NextParentFrame = nullptr;
	auto CacheNextFrame = [&NextFrame, &NextParentFrame, &NextFrameID, &Exec, &OutSelectionResult]()
		{
			NextFrame = FindExecutionFrame(NextFrameID, MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(OutSelectionResult->TemporaryFrames));
			check(NextFrame);

	if (OutSelectionResult->SelectedFrames.Num() > 1)
	{
		NextParentFrame = FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(1), MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(OutSelectionResult->TemporaryFrames));
		check(NextParentFrame);
	}
		};
	CacheNextFrame();

	// Save the current result to use SelectionEvents in GetDataView
	TGuardValue<FSelectStateResult*> GuardCurrentlyProcessedStateSelectionResult(CurrentlyProcessedStateSelectionResult, &OutSelectionResult.Get());

	UE_METASTORY_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EMetaStoryUpdatePhase::StateSelection);

	// Look up linked state overrides
	const UMetaStory* NextLinkedStateAsset = NextState.LinkedAsset;
	const FInstancedPropertyBag* NextLinkedStateParameterOverride = nullptr;
	if (NextState.Type == EMetaStoryStateType::LinkedAsset)
	{
		if (const FMetaStoryReference* Override = GetLinkedMetaStoryOverrideForTag(NextState.Tag))
		{
			NextLinkedStateAsset = Override->GetMetaStory();
			NextLinkedStateParameterOverride = &Override->GetParameters();

			METASTORY_LOG(VeryVerbose, TEXT("%hs: In state '%s', overriding linked asset '%s' with '%s'. '%s' using MetaStory '%s'."),
				__FUNCTION__,
				*GetSafeStateName(*NextFrame, NextStateHandle),
				*GetFullNameSafe(NextState.LinkedAsset),
				*GetFullNameSafe(NextLinkedStateAsset),
				*GetNameSafe(&Owner),
				*GetFullNameSafe(NextFrame->MetaStory)
			);
		}
	}

	// Update state parameters.
	if (NextState.ParameterDataHandle.IsValid())
	{
		FCurrentlyProcessedFrameScope FrameScope(*this, NextParentFrame, *NextFrame);
		FCurrentlyProcessedStateScope NextStateScope(*this, NextStateHandle);

		// Instantiate temporary state parameters if not done yet.
		FMetaStoryDataView NextStateParametersView = GetDataViewOrTemporary(NextParentFrame, *NextFrame, NextState.ParameterDataHandle);
		if (!NextStateParametersView.IsValid())
		{
			// Allocate temporary instance for parameters if the state has params.
			// The subtree state selection below assumes that this creates always a valid temporary, we'll create the temp data even if parameters are empty.
			// @todo: Empty params is valid and common case, we should not require to create empty parameters data (this needs to be handle in compiler and UpdateInstanceData too).
			if (NextLinkedStateParameterOverride)
			{
				// Create from an override.
				FMetaStoryDataView TempStateParametersView = AddTemporaryInstance(*NextFrame, FMetaStoryIndex16::Invalid, NextState.ParameterDataHandle, FConstStructView(TBaseStructure<FMetaStoryCompactParameters>::Get()));
				check(TempStateParametersView.IsValid());
				FMetaStoryCompactParameters& StateParams = TempStateParametersView.GetMutable<FMetaStoryCompactParameters>();
				StateParams.Parameters = *NextLinkedStateParameterOverride;
				NextStateParametersView = FMetaStoryDataView(StateParams.Parameters.GetMutableValue());
			}
			else
			{
				// Create from template in the asset.
				const FConstStructView DefaultStateParamsInstanceData = NextFrame->MetaStory->DefaultInstanceData.GetStruct(NextState.ParameterTemplateIndex.Get());
				FMetaStoryDataView TempStateParametersView = AddTemporaryInstance(*NextFrame, FMetaStoryIndex16::Invalid, NextState.ParameterDataHandle, DefaultStateParamsInstanceData);
				check(TempStateParametersView.IsValid());
				FMetaStoryCompactParameters& StateParams = TempStateParametersView.GetMutable<FMetaStoryCompactParameters>();
				NextStateParametersView = FMetaStoryDataView(StateParams.Parameters.GetMutableValue());
			}
		}

		// Copy parameters if needed
		if (NextStateParametersView.IsValid()
			&& NextState.ParameterDataHandle.IsValid()
			&& NextState.ParameterBindingsBatch.IsValid())
		{
			// Note: the parameters are for the current (linked) state, stored in current frame.
			// The copy can fail, if the overridden parameters do not match, this is by design.
			CopyBatchWithValidation(NextParentFrame, *NextFrame, NextStateParametersView, NextState.ParameterBindingsBatch);
		}
	}

	const bool bIsTargetState = InternalArgs.MissingStatesToReachTarget.Num() <= 1;
	const bool bShouldPrerequisitesBeChecked = Args.Behavior != ESelectStateBehavior::Forced
		&& (bIsTargetState || NextState.bCheckPrerequisitesWhenActivatingChildDirectly);

	// Check if the events are accepted.
	TArray<FMetaStorySharedEvent, TInlineAllocator<FMetaStoryEventQueue::MaxActiveEvents>> StateSelectionEvents;
	if (NextState.EventDataIndex.IsValid())
	{
		if (ensure(NextState.RequiredEventToEnter.IsValid()))
		{
			// Use the same event as performed transition unless it didn't lead to this state as only state selected by the transition should get it's event.
			const bool bCanUseTransitionEvent = Args.TransitionEvent.IsValid()
				&& Args.TargetState == NextStateContext
				&& bIsTargetState;
			if (bCanUseTransitionEvent && bTargetStateRequiresTheSameEventForStateSelectionAsTheRequestedTransition)
			{
				if (NextState.RequiredEventToEnter.DoesEventMatchDesc(*Args.TransitionEvent))
				{
					StateSelectionEvents.Emplace(Args.TransitionEvent);
				}
			}
			else if (bCanUseTransitionEvent && NextState.RequiredEventToEnter.DoesEventMatchDesc(*Args.TransitionEvent))
			{
				StateSelectionEvents.Emplace(Args.TransitionEvent);
			}
			else
			{
				TArrayView<FMetaStorySharedEvent> EventsQueue = GetMutableEventsToProcessView();
				for (FMetaStorySharedEvent& Event : EventsQueue)
				{
					check(Event.IsValid());
					if (NextState.RequiredEventToEnter.DoesEventMatchDesc(*Event))
					{
						StateSelectionEvents.Emplace(Event);
					}
				}

				// Couldn't find matching state's event, but it's marked as not required. Adding an empty event which allows us to continue the state selection.
				if (!bShouldPrerequisitesBeChecked && StateSelectionEvents.IsEmpty())
				{
					StateSelectionEvents.Emplace();
				}
			}
		}

		if (StateSelectionEvents.IsEmpty())
		{
			return false;
		}
	}
	else
	{
		StateSelectionEvents.Add(FMetaStorySharedEvent());
	}

	bool bShouldCreateNewState = true;
	const bool bIsNextTargetStateInActiveStates = !bNewFrameCreated
		&& InternalArgs.MissingActiveStates.Num() > 0
		&& InternalArgs.MissingActiveStates[0].GetStateHandle() == NextStateHandle
		&& InternalArgs.MissingActiveStates[0].GetFrameID() == NextFrame->FrameID;
	if (EnumHasAnyFlags(Args.SelectionRules, EMetaStoryStateSelectionRules::ReselectedStateCreatesNewStates | EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates))
	{
		bShouldCreateNewState = false;
		if (EnumHasAllFlags(Args.SelectionRules, EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates))
		{
			// Request a new state if it doesn't match the previous states or if the state is completed.
			bShouldCreateNewState = !bIsNextTargetStateInActiveStates || NextFrame->ActiveTasksStatus.GetStatus(NextState).IsCompleted();
		}
		if (EnumHasAnyFlags(Args.SelectionRules, EMetaStoryStateSelectionRules::ReselectedStateCreatesNewStates))
		{
			bShouldCreateNewState = bShouldCreateNewState || bIsTargetState || !bIsNextTargetStateInActiveStates;
		}
	}
	else
	{
		bShouldCreateNewState = !bIsNextTargetStateInActiveStates;
	}

	// Add the state to the selected list. It will be removed if the selection fails.
	if (bShouldCreateNewState)
	{
		OutSelectionResult->SelectedStates.Emplace(NextFrame->FrameID, FActiveStateID(Storage.GenerateUniqueId()), NextStateHandle);
		OnExitScope.bStateAdded = true;

		int32 StateInFrameCounter = 0;
		for (int32 Index = OutSelectionResult->SelectedStates.Num() - 1; Index >= 0; --Index)
		{
			if (OutSelectionResult->SelectedStates[Index].GetFrameID() != NextFrame->FrameID)
			{
				break;
			}
			++StateInFrameCounter;
		}

		if (StateInFrameCounter > FMetaStoryActiveStates::MaxStates)
		{
			METASTORY_LOG(Error, TEXT("%hs: Reached max execution depth when trying to select state %s from '%s'.  '%s' using MetaStory '%s'."),
				__FUNCTION__,
				*GetSafeStateName(*NextFrame, NextStateHandle),
				*GetFullNameSafe(NextFrame->MetaStory),
				*GetNameSafe(&Owner),
				*GetFullNameSafe(&RootMetaStory)
			);
			return false;
		}
	}
	else
	{
		check(InternalArgs.MissingActiveStates.Num() > 0); // bIsNextTargetStateInActiveStates is false
		OutSelectionResult->SelectedStates.Add(InternalArgs.MissingActiveStates[0]);
		OnExitScope.bStateAdded = true;
	}

	// Set the target, since this is recursive (can be called for linked state), it only set once the original target is found.
	if (bIsTargetState && !OutSelectionResult->TargetState.IsValid())
	{
		OutSelectionResult->TargetState = OutSelectionResult->SelectedStates.Last();
	}

	for (int32 EventIndex = 0; EventIndex < StateSelectionEvents.Num(); ++EventIndex)
	{
		const FMetaStorySharedEvent& StateSelectionEvent = StateSelectionEvents[EventIndex];

		// A SelectStateInternal_X might have changed the TemporaryFrames, re-cache the values
		if (EventIndex != 0)
		{
			CacheNextFrame();
		}

		// Add the event for GetDataView and for consuming it later.
		const bool bContainsSelectionEvent = OutSelectionResult->SelectionEvents.ContainsByPredicate(
			[&LatestState = OutSelectionResult->SelectedStates.Last()](const FSelectionEventWithID& Other)
			{
				return Other.State == LatestState;
			});
		ensureMsgf(!bContainsSelectionEvent, TEXT("The event should be remove at the end of the scope."));

		bool bRemoveSelectionEvents = false;
		if (StateSelectionEvent.IsValid())
		{
			
			OutSelectionResult->SelectionEvents.Add(FSelectionEventWithID{.State = OutSelectionResult->SelectedStates.Last(), .Event = StateSelectionEvent});
			bRemoveSelectionEvents = true;
		}

		auto RemoveStateSelectionEvent = [bRemoveSelectionEvents](const TSharedRef<FSelectStateResult>& SelectionResult)
			{
				if (bRemoveSelectionEvents)
				{
					check(SelectionResult->SelectionEvents.Num() > 0);
					ensureMsgf(SelectionResult->SelectionEvents.Last().State == SelectionResult->SelectedStates.Last(), TEXT("We should remove from the same element it was added."));
					SelectionResult->SelectionEvents.Pop();
				}
			};

		if (bShouldPrerequisitesBeChecked)
		{
			// Check that the state can be entered
			const bool bEnterConditionsPassed = TestAllConditionsWithValidation(NextParentFrame, *NextFrame, NextStateHandle, NextState.EnterConditionEvaluationScopeMemoryRequirement, NextState.EnterConditionsBegin, NextState.EnterConditionsNum, EMetaStoryUpdatePhase::EnterConditions);

			if (!bEnterConditionsPassed)
			{
				RemoveStateSelectionEvent(OutSelectionResult);
				continue;
			}
		}

		const FSelectStateInternalArguments NewInternalArgs = FSelectStateInternalArguments{
			.MissingActiveStates = bShouldCreateNewState ? TArrayView<const FActiveState>() : InternalArgs.MissingActiveStates.Mid(1),
			.MissingSourceFrameID = FActiveFrameID(),
			.MissingSourceStates = TArrayView<const FActiveState>(),
			.MissingStatesToReachTarget = bIsTargetState ? TArrayView<const FStateHandleContext>() : InternalArgs.MissingStatesToReachTarget.Mid(1)
		};
		if (NextState.Type == EMetaStoryStateType::Linked)
		{
			// MissingStatesToReachTarget can include a linked state and the frame needs to be constructed (if needed).
			OnExitScope.bSucceededToSelectState = SelectStateInternal_Linked(Args, NewInternalArgs, OutSelectionResult, NextFrame->MetaStory, NextState, bShouldCreateNewState);
		}
		else if (NextState.Type == EMetaStoryStateType::LinkedAsset)
		{
			// MissingStatesToReachTarget can include a linked asset state and the frame needs to be constructed (if needed).
			OnExitScope.bSucceededToSelectState = SelectStateInternal_LinkedAsset(Args, NewInternalArgs, OutSelectionResult, NextFrame->MetaStory, NextState, NextLinkedStateAsset, bShouldCreateNewState);
		}
		else if (!bIsTargetState)
		{
			check(NewInternalArgs.MissingStatesToReachTarget.Num() > 0);
			// Next child state is already known. Passing TransitionEvent further, states selected directly by transition can use it.
			OnExitScope.bSucceededToSelectState = SelectStateInternal(Args, NewInternalArgs, OutSelectionResult);
		}
		else if (Args.Behavior != ESelectStateBehavior::Forced)
		{
			switch (NextState.SelectionBehavior)
			{
			case EMetaStoryStateSelectionBehavior::TryEnterState:
				UE_METASTORY_DEBUG_STATE_EVENT(this, NextStateHandle, EMetaStoryTraceEventType::OnStateSelected);
				OnExitScope.bSucceededToSelectState = true;
				break;
			case EMetaStoryStateSelectionBehavior::TrySelectChildrenInOrder:
				OnExitScope.bSucceededToSelectState = SelectStateInternal_TrySelectChildrenInOrder(Args, NewInternalArgs, OutSelectionResult, NextFrame->MetaStory, NextState, NextStateHandle);
				break;
			case EMetaStoryStateSelectionBehavior::TrySelectChildrenAtRandom:
				OnExitScope.bSucceededToSelectState = SelectStateInternal_TrySelectChildrenAtRandom(Args, NewInternalArgs, OutSelectionResult, NextFrame->MetaStory, NextState, NextStateHandle);
				break;
			case EMetaStoryStateSelectionBehavior::TrySelectChildrenWithHighestUtility:
				OnExitScope.bSucceededToSelectState = SelectStateInternal_TrySelectChildrenWithHighestUtility(Args, NewInternalArgs, OutSelectionResult, NextFrame->MetaStory, NextState, NextStateHandle);
				break;
			case EMetaStoryStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility:
				OnExitScope.bSucceededToSelectState = SelectStateInternal_TrySelectChildrenAtRandomWeightedByUtility(Args, NewInternalArgs, OutSelectionResult, NextFrame->MetaStory, NextState, NextStateHandle);
				break;
			case EMetaStoryStateSelectionBehavior::TryFollowTransitions:
				OnExitScope.bSucceededToSelectState = SelectStateInternal_TryFollowTransitions(Args, NewInternalArgs, OutSelectionResult, NextFrame->MetaStory, NextState, NextStateHandle);
				break;
			}
		}
		else
		{
			if (NewInternalArgs.MissingStatesToReachTarget.Num() == 0)
			{
				OnExitScope.bSucceededToSelectState = true;
			}
			else
			{
				// Continue the force selection. Next child state is already known.
				OnExitScope.bSucceededToSelectState = SelectStateInternal(Args, NewInternalArgs, OutSelectionResult);
			}
		}


		if (OnExitScope.bSucceededToSelectState)
		{
			break;
		}
		else
		{
			RemoveStateSelectionEvent(OutSelectionResult);
		}
	}

	return OnExitScope.bSucceededToSelectState;
}

TOptional<bool> FMetaStoryExecutionContext::SelectStateFromSourceInternal(
	const FSelectStateArguments& Args,
	const FSelectStateInternalArguments& InternalArgs,
	const TSharedRef<FSelectStateResult>& OutSelectionResult,
	const FMetaStoryExecutionFrame& NextFrame,
	const FMetaStoryCompactState& NextState,
	const FMetaStoryStateHandle NextStateHandle,
	const bool bNewFrameCreated)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	if (InternalArgs.MissingSourceStates.Num() > 0
		&& InternalArgs.MissingStatesToReachTarget.Num() > 1
		&& !bNewFrameCreated)
	{
		check(InternalArgs.MissingActiveStates.Num() > 0);
		check(InternalArgs.MissingSourceStates[0] == InternalArgs.MissingActiveStates[0]);
		const FActiveState& MissingActiveState = InternalArgs.MissingSourceStates[0];
		bool bContinueWithStateSelection = NextStateHandle != MissingActiveState.GetStateHandle();
		bool bCompletedState = false;

		if (!bContinueWithStateSelection)
		{
			if (InternalArgs.MissingSourceStates.Num() == 1)
			{
				// MissingActiveState is the transition source.
				if (EnumHasAllFlags(Args.SelectionRules, EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates))
				{
					bContinueWithStateSelection = NextFrame.ActiveTasksStatus.GetStatus(NextState).IsCompleted();
				}
			}
			else
			{
				if (EnumHasAllFlags(Args.SelectionRules, EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates))
				{
					bCompletedState = NextFrame.ActiveTasksStatus.GetStatus(NextState).IsCompleted();
				}
			}
		}

		if (!bContinueWithStateSelection)
		{
			OutSelectionResult->SelectedStates.Add(InternalArgs.MissingSourceStates[0]);

			bool bSelectStateInternalSucceeded = false;
			ON_SCOPE_EXIT{
				if (bSelectStateInternalSucceeded == false)
				{
					OutSelectionResult->SelectedStates.Pop();
				}
			};

			FSelectStateInternalArguments NewInternalArgs = FSelectStateInternalArguments{
				.MissingActiveStates = InternalArgs.MissingActiveStates.Mid(1),
				.MissingSourceFrameID = FActiveFrameID(),
				.MissingSourceStates = InternalArgs.MissingSourceStates.Mid(1),
				.MissingStatesToReachTarget = InternalArgs.MissingStatesToReachTarget.Mid(1)
			};
			bSelectStateInternalSucceeded = SelectStateInternal(Args, NewInternalArgs, OutSelectionResult);

			if (bSelectStateInternalSucceeded && bCompletedState)
			{
				check(EnumHasAllFlags(Args.SelectionRules, EMetaStoryStateSelectionRules::CompletedStateBeforeTransitionSourceFailsTransition));
				// Before the source, there's a completed state.
				//The completed state should fail the transition(because of EMetaStoryStateSelectionRules::CompletedStateBeforeTargetFailsTransition).
				//The transition can contain a state with "follow transition" that would remove this state from the transition.
				if (OutSelectionResult->SelectedStates.Contains(MissingActiveState))
				{
					METASTORY_LOG(VeryVerbose, TEXT("%hs: Selection fails because state '%s' is completed.  '%s' using MetaStory '%s'."),
						__FUNCTION__,
						*NextState.Name.ToString(),
						*GetNameSafe(&Owner),
						*GetFullNameSafe(NextFrame.MetaStory)
					);

					UE_METASTORY_DEBUG_LOG_EVENT(this, Log, TEXT("Selection fails because parent state '%s' is completed."), *NextState.Name.ToString());

					bSelectStateInternalSucceeded = false;
				}
			}
			return bSelectStateInternalSucceeded;
		}
	}
	return {};
}

namespace UE::MetaStory::ExecutionContext::Private
{
	bool PreventRecursionCheck(const FMetaStoryExecutionFrameHandle& LinkStateFrameHandle,
		TArrayView<const FActiveFrameID> SelectedFrames,
		TArrayView<const FMetaStoryExecutionFrame> ActiveFrames,
		TArrayView<const FMetaStoryExecutionFrame> TemporaryFrames)
	{
		// Check and prevent recursion.
		return SelectedFrames.ContainsByPredicate(
			[&LinkStateFrameHandle, &ActiveFrames, &TemporaryFrames](const FActiveFrameID& FrameID)
			{
				const FMetaStoryExecutionFrame* Frame = FindExecutionFrame(FrameID, ActiveFrames, TemporaryFrames);
				return Frame && Frame->HasRoot(LinkStateFrameHandle);
			});
	}

	FMetaStoryExecutionFrame* SelectedFrameLinkedFrame(FMetaStoryExecutionState& Exec,
		const bool bShouldCreateNewState,
		TArrayView<const UE::MetaStory::FActiveState> MatchingActiveStates,
		const FMetaStoryExecutionFrameHandle& LinkStateFrameHandle,
		const EMetaStoryStateSelectionRules StateSelectionRules
	)
	{
		check(LinkStateFrameHandle.IsValid());

		FMetaStoryExecutionFrame* Result = nullptr;
		if (!bShouldCreateNewState)
		{
			// Get the next frame ID
			if (ensure(MatchingActiveStates.Num() >= 1))
			{
				const FActiveFrameID ExistingFrameID = MatchingActiveStates[0].GetFrameID();
				Result = Exec.FindActiveFrame(ExistingFrameID);
				check(Result);
				if (Result->HasRoot(LinkStateFrameHandle))
				{
					if (EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates)
						&& Result->ActiveTasksStatus.GetStatus(Result->MetaStory).IsCompleted()
						)
					{
						return nullptr;
					}
				}
			}
		}
		return Result;
	}
}

bool FMetaStoryExecutionContext::SelectStateInternal_Linked(
	const FSelectStateArguments& Args,
	const FSelectStateInternalArguments& InternalArgs,
	const TSharedRef<FSelectStateResult>& OutSelectionResult,
	TNotNull<const UMetaStory*> NextMetaStory,
	const FMetaStoryCompactState& TargetState,
	bool bShouldCreateNewState)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	FMetaStoryExecutionState& Exec = GetExecState();

	if (!TargetState.LinkedState.IsValid())
	{
		METASTORY_LOG(Warning, TEXT("%hs: Trying to enter invalid linked subtree from '%s'. '%s' using MetaStory '%s'."),
			__FUNCTION__,
			*GetStateStatusString(Exec),
			*Owner.GetName(),
			*NextMetaStory->GetFullName()
		);
		return false;
	}

	const FMetaStoryExecutionFrameHandle LinkStateFrameHandle = FMetaStoryExecutionFrameHandle(NextMetaStory, TargetState.LinkedState);

	const bool bHasMissingState = InternalArgs.MissingStatesToReachTarget.Num() > 0 && Args.Behavior == ESelectStateBehavior::Forced;
	if (bHasMissingState)
	{
		// In a force transition, the root could be different from what is expected.
		//Ex: a previous transition go to root, then another transition go to another top level state (new root)
		if (InternalArgs.MissingStatesToReachTarget[0].MetaStory != LinkStateFrameHandle.GetMetaStory()
			|| InternalArgs.MissingStatesToReachTarget[0].StateHandle != LinkStateFrameHandle.GetRootState())
		{
			METASTORY_LOG(Error, TEXT("%hs: The missing state is not from the same MetaStory. '%s' using MetaStory '%s'."),
				__FUNCTION__,
				*Owner.GetName(),
				*NextMetaStory->GetFullName()
			);
			return false;
		}
	}

	if (PreventRecursionCheck(LinkStateFrameHandle, OutSelectionResult->SelectedFrames, MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(OutSelectionResult->TemporaryFrames)))
	{
		METASTORY_LOG(Error, TEXT("%hs: Trying to recursively enter subtree '%s' from '%s'. '%s' using MetaStory '%s'."),
			__FUNCTION__,
			*GetSafeStateName(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState()),
			*GetStateStatusString(Exec),
			*Owner.GetName(),
			*NextMetaStory->GetFullName()
		);
		return false;
	}

	const FMetaStoryCompactFrame* LinkMetaStoryFrame = FindMetaStoryFrame(LinkStateFrameHandle);
	if (LinkMetaStoryFrame == nullptr)
	{
		METASTORY_LOG(Error, TEXT("%hs: The frame '%s' from '%s' does not exist. '%s' using MetaStory '%s'."),
			__FUNCTION__,
			*GetSafeStateName(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState()),
			*GetStateStatusString(Exec),
			*Owner.GetName(),
			*NextMetaStory->GetFullName()
		);
		return false;
	}

	// Do we have an existing frame.
	FMetaStoryExecutionFrame* SelectedFrame = SelectedFrameLinkedFrame(Exec, bShouldCreateNewState, InternalArgs.MissingActiveStates, LinkStateFrameHandle, Args.SelectionRules);
	if (SelectedFrame && !SelectedFrame->HasRoot(LinkStateFrameHandle))
	{
		if (Args.Behavior == ESelectStateBehavior::Forced)
		{
			SelectedFrame = nullptr;
		}
		else
		{
			METASTORY_LOG(Error, TEXT("%hs: The frame '%s' from '%s' does not have the same root as the active frame. '%s' using MetaStory '%s'."),
				__FUNCTION__,
				*GetSafeStateName(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState()),
				*GetStateStatusString(Exec),
				*Owner.GetName(),
				*NextMetaStory->GetFullName()
			);
			return false;
		}
	}

	const bool bIsNewFrame = SelectedFrame == nullptr;
	if (bIsNewFrame)
	{
		// Note. Adding to TemporaryFrame can invalidate TargetFrame.
		FMetaStoryIndex16 ExternalDataBaseIndex;
		FMetaStoryDataHandle GlobalParameterDataHandle;
		FMetaStoryIndex16 GlobalInstanceIndexBase;
		{
			const FMetaStoryExecutionFrame* NextFrame = FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(), MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(OutSelectionResult->TemporaryFrames));
			check(NextFrame);
			ExternalDataBaseIndex = NextFrame->ExternalDataBaseIndex;
			GlobalParameterDataHandle = NextFrame->GlobalParameterDataHandle;
			GlobalInstanceIndexBase = NextFrame->GlobalInstanceIndexBase;
		}

		constexpr bool bIsGlobalFrame = false;
		FMetaStoryExecutionFrame& NewFrame = OutSelectionResult->MakeAndAddTemporaryFrame(FActiveFrameID(Storage.GenerateUniqueId()), LinkStateFrameHandle, bIsGlobalFrame);
		NewFrame.ExternalDataBaseIndex = ExternalDataBaseIndex;
		NewFrame.GlobalInstanceIndexBase = GlobalInstanceIndexBase;
		NewFrame.ExecutionRuntimeIndexBase = FMetaStoryIndex16(Storage.AddExecutionRuntimeData(GetOwner(), LinkStateFrameHandle));
		NewFrame.StateParameterDataHandle = TargetState.ParameterDataHandle; // Temporary allocated earlier if did not exists.
		NewFrame.GlobalParameterDataHandle = GlobalParameterDataHandle;

		SelectedFrame = &NewFrame;
	}

	OutSelectionResult->SelectedFrames.Add(SelectedFrame->FrameID);

	// Select the root state of the new frame.
	const FStateHandleContext RootState = FStateHandleContext(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState());
	const FSelectStateInternalArguments NewInternalArgs = FSelectStateInternalArguments{
		.MissingActiveStates = bIsNewFrame ? TArrayView<const FActiveState>() : InternalArgs.MissingActiveStates,
		.MissingSourceFrameID = SelectedFrame->FrameID,
		.MissingSourceStates = TArrayView<const FActiveState>(),
		.MissingStatesToReachTarget = bHasMissingState ? InternalArgs.MissingStatesToReachTarget : MakeConstArrayView(&RootState, 1)
	};
	const bool bSucceededToSelectState = SelectStateInternal(Args, NewInternalArgs, OutSelectionResult);

	if (!bSucceededToSelectState)
	{
		if (bIsNewFrame)
		{
			CleanFrame(Exec, NewInternalArgs.MissingSourceFrameID);
		}
		OutSelectionResult->SelectedFrames.Pop();
	}

	return bSucceededToSelectState;
}

bool FMetaStoryExecutionContext::SelectStateInternal_LinkedAsset(
	const FSelectStateArguments& Args,
	const FSelectStateInternalArguments& InternalArgs,
	const TSharedRef<FSelectStateResult>& OutSelectionResult,
	TNotNull<const UMetaStory*> NextMetaStory,
	const FMetaStoryCompactState& NextState,
	const UMetaStory* NextLinkedStateAsset,
	bool bShouldCreateNewState)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	FMetaStoryExecutionState& Exec = GetExecState();

	if (NextLinkedStateAsset == nullptr)
	{
		return false;
	}

	if (NextLinkedStateAsset->States.Num() == 0
		|| !NextLinkedStateAsset->IsReadyToRun())
	{
		METASTORY_LOG(Error, TEXT("%hs: The linked MetaStory is invalid. '%s' using MetaStory '%s'."),
			__FUNCTION__,
			*Owner.GetName(),
			*NextMetaStory->GetFullName()
		);
		return false;
	}

	FMetaStoryStateHandle NextLinkedStateRoot = FMetaStoryStateHandle::Root;
	const bool bHasMissingState = InternalArgs.MissingStatesToReachTarget.Num() > 0 && Args.Behavior == ESelectStateBehavior::Forced;
	if (bHasMissingState)
	{
		// In a force transition, the root could be different from what is expected.
		//Ex: a previous transition go to root, then another transition go to another top level state (new root)
		if (InternalArgs.MissingStatesToReachTarget[0].MetaStory != NextLinkedStateAsset)
		{
			METASTORY_LOG(Error, TEXT("%hs: The missing state is not from the same MetaStory. '%s' using MetaStory '%s'."),
				__FUNCTION__,
				*Owner.GetName(),
				*NextMetaStory->GetFullName()
			);
			return false;
		}
		NextLinkedStateRoot = InternalArgs.MissingStatesToReachTarget[0].StateHandle;
	}

	const FMetaStoryExecutionFrameHandle LinkStateFrameHandle = FMetaStoryExecutionFrameHandle(NextLinkedStateAsset, NextLinkedStateRoot);

	// The linked MetaStory should have compatible context requirements.
	if (!NextLinkedStateAsset->HasCompatibleContextData(RootMetaStory)
		|| NextLinkedStateAsset->GetSchema()->GetClass() != NextMetaStory->GetSchema()->GetClass())
	{
		METASTORY_LOG(Error, TEXT("%hs: The linked MetaStory '%s' does not have compatible schema, trying to select state %s from '%s'. '%s' using MetaStory '%s'."),
			__FUNCTION__,
			*GetFullNameSafe(NextLinkedStateAsset),
			*GetSafeStateName(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState()),
			*GetStateStatusString(Exec),
			*Owner.GetName(),
			*NextMetaStory->GetFullName()
		);
		return false;
	}

	if (PreventRecursionCheck(LinkStateFrameHandle, OutSelectionResult->SelectedFrames, MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(OutSelectionResult->TemporaryFrames)))
	{
		METASTORY_LOG(Error, TEXT("%hs: Trying to recursively enter subtree '%s' from '%s'. '%s' using MetaStory '%s'."),
			__FUNCTION__,
			*GetSafeStateName(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState()),
			*GetStateStatusString(Exec),
			*Owner.GetName(),
			*NextMetaStory->GetFullName()
		);
		return false;
	}

	const FMetaStoryCompactFrame* LinkMetaStoryFrame = FindMetaStoryFrame(LinkStateFrameHandle);
	if (LinkMetaStoryFrame == nullptr)
	{
		METASTORY_LOG(Error, TEXT("%hs: The frame '%s' from '%s' does not exist. '%s' using MetaStory '%s'."),
			__FUNCTION__,
			*GetSafeStateName(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState()),
			*GetStateStatusString(Exec),
			*Owner.GetName(),
			*NextMetaStory->GetFullName()
		);
		return false;
	}

	// Do we have an existing frame.
	// Do not use the transition override selection rules. A transition outside the frame should not impact the current frame.
	const EMetaStoryStateSelectionRules StateSelectionRules = LinkStateFrameHandle.GetMetaStory()->GetStateSelectionRules();
	FMetaStoryExecutionFrame* SelectedFrame = SelectedFrameLinkedFrame(Exec, bShouldCreateNewState, InternalArgs.MissingActiveStates, LinkStateFrameHandle, StateSelectionRules);
	if (SelectedFrame && !SelectedFrame->HasRoot(LinkStateFrameHandle))
	{
		if (Args.Behavior == ESelectStateBehavior::Forced)
		{
			SelectedFrame = nullptr;
		}
		else
		{
			METASTORY_LOG(Error, TEXT("%hs: The frame '%s' from '%s' does not have the same root as the active frame. '%s' using MetaStory '%s'."),
				__FUNCTION__,
				*GetSafeStateName(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState()),
				*GetStateStatusString(Exec),
				*Owner.GetName(),
				*NextMetaStory->GetFullName()
			);
			return false;
		}
	}

	const bool bIsNewFrame = SelectedFrame == nullptr;
	if (bIsNewFrame)
	{
		// Collect external data if needed
		const FMetaStoryIndex16 ExternalDataBaseIndex = CollectExternalData(LinkStateFrameHandle.GetMetaStory());
		if (!ExternalDataBaseIndex.IsValid())
		{
			METASTORY_LOG(VeryVerbose, TEXT("%hs: Cannot select state '%s' because failed to collect external data for nested tree '%s' from '%s'. '%s' using MetaStory '%s'."),
				__FUNCTION__,
				*GetSafeStateName(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState()),
				*GetFullNameSafe(NextLinkedStateAsset),
				*GetStateStatusString(Exec),
				*Owner.GetName(),
				*NextMetaStory->GetFullName()
			);
			return false;
		}

		FMetaStoryExecutionFrame& NewFrame = OutSelectionResult->MakeAndAddTemporaryFrame(FActiveFrameID(Storage.GenerateUniqueId()), LinkStateFrameHandle, true);
		NewFrame.ExternalDataBaseIndex = ExternalDataBaseIndex;
		NewFrame.ExecutionRuntimeIndexBase = FMetaStoryIndex16(Storage.AddExecutionRuntimeData(GetOwner(), LinkStateFrameHandle));
		// Pass the linked state's parameters as global parameters to the linked asset.
		NewFrame.GlobalParameterDataHandle = NextState.ParameterDataHandle;
		// The state parameters will be from the root state.
		const FMetaStoryCompactState& NewFrameRootState = LinkStateFrameHandle.GetMetaStory()->States[NewFrame.RootState.Index];
		NewFrame.StateParameterDataHandle = NewFrameRootState.ParameterDataHandle;

		SelectedFrame = &NewFrame;
	}

	OutSelectionResult->SelectedFrames.Add(SelectedFrame->FrameID);

	if (bIsNewFrame)
	{
		// Start global tasks and evaluators temporarily, so that their data is available already during select.
		const FMetaStoryExecutionFrame* NextParentFrame = FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(1), MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(OutSelectionResult->TemporaryFrames));
		FMetaStoryExecutionFrame* NextFrame = FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(), MakeArrayView(Exec.ActiveFrames), MakeArrayView(OutSelectionResult->TemporaryFrames));
		check(NextParentFrame);
		check(SelectedFrame == NextFrame);

		const EMetaStoryRunStatus StartResult = StartTemporaryEvaluatorsAndGlobalTasks(NextParentFrame, *NextFrame);
		if (StartResult != EMetaStoryRunStatus::Running)
		{
			METASTORY_LOG(VeryVerbose, TEXT("%hs: Cannot select state '%s' because cannot start nested tree's '%s' global tasks and evaluators. '%s' using MetaStory '%s'."),
				__FUNCTION__,
				*GetSafeStateName(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState()),
				*GetFullNameSafe(NextLinkedStateAsset),
				*Owner.GetName(),
				*NextMetaStory->GetFullName()
			);

			StopTemporaryEvaluatorsAndGlobalTasks(NextParentFrame, *NextFrame, StartResult);
			CleanFrame(Exec, SelectedFrame->FrameID);

			OutSelectionResult->SelectedFrames.Pop();
			return false;
		}
	}

	// Select the root state of the new frame.
	const FStateHandleContext RootState = FStateHandleContext(LinkStateFrameHandle.GetMetaStory(), LinkStateFrameHandle.GetRootState());
	FSelectStateArguments NewArgs = Args;
	NewArgs.SelectionRules = StateSelectionRules;
	const FSelectStateInternalArguments NewInternalArgs = FSelectStateInternalArguments{
		.MissingActiveStates = bIsNewFrame ? TArrayView<const FActiveState>() : InternalArgs.MissingActiveStates,
		.MissingSourceFrameID = SelectedFrame->FrameID,
		.MissingSourceStates = TArrayView<const FActiveState>(),
		.MissingStatesToReachTarget = bHasMissingState ? InternalArgs.MissingStatesToReachTarget : MakeConstArrayView(&RootState, 1)
	};
	const bool bSucceededToSelectState = SelectStateInternal(NewArgs, NewInternalArgs, OutSelectionResult);

	if (!bSucceededToSelectState)
	{
		if (bIsNewFrame)
		{
			const FMetaStoryExecutionFrame* NextParentFrame = FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(1), MakeConstArrayView(Exec.ActiveFrames), MakeConstArrayView(OutSelectionResult->TemporaryFrames));
			FMetaStoryExecutionFrame* NextFrame = FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(), MakeArrayView(Exec.ActiveFrames), MakeArrayView(OutSelectionResult->TemporaryFrames));
			check(NextParentFrame);
			check(NextFrame);

			constexpr EMetaStoryRunStatus CompletionStatus = EMetaStoryRunStatus::Stopped;
			StopTemporaryEvaluatorsAndGlobalTasks(NextParentFrame, *NextFrame, CompletionStatus);
			CleanFrame(Exec, SelectedFrame->FrameID);
		}
		OutSelectionResult->SelectedFrames.Pop();
	}

	return bSucceededToSelectState;
}

bool FMetaStoryExecutionContext::SelectStateInternal_TrySelectChildrenInOrder(
	const FSelectStateArguments& Args,
	const FSelectStateInternalArguments& InternalArgs,
	const TSharedRef<FSelectStateResult>& OutSelectionResult,
	TNotNull<const UMetaStory*> NextMetaStory,
	const FMetaStoryCompactState& NextState,
	const FMetaStoryStateHandle NextStateHandle)
{
	using namespace UE::MetaStory::ExecutionContext;

	if (!NextState.HasChildren())
	{
		// Select this state (For backwards compatibility)
		UE_METASTORY_DEBUG_STATE_EVENT(this, NextStateHandle, EMetaStoryTraceEventType::OnStateSelected);
		return true;
	}

	UE_METASTORY_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EMetaStoryUpdatePhase::TrySelectBehavior);

	// If the state has children, proceed to select children.
	bool bSucceededToSelectState = false;
	for (uint16 ChildStateIndex = NextState.ChildrenBegin; ChildStateIndex < NextState.ChildrenEnd; ChildStateIndex = NextMetaStory->States[ChildStateIndex].GetNextSibling())
	{
		FStateHandleContext ChildState = FStateHandleContext(NextMetaStory, FMetaStoryStateHandle(ChildStateIndex));
		FSelectStateInternalArguments NewInternalArgs = InternalArgs;
		NewInternalArgs.MissingStatesToReachTarget = MakeArrayView(&ChildState, 1);
		if (SelectStateInternal(Args, NewInternalArgs, OutSelectionResult))
		{
			// Selection succeeded
			bSucceededToSelectState = true;
			break;
		}
	}

	return bSucceededToSelectState;
}

bool FMetaStoryExecutionContext::SelectStateInternal_TrySelectChildrenAtRandom(
	const FSelectStateArguments& Args,
	const FSelectStateInternalArguments& InternalArgs,
	const TSharedRef<FSelectStateResult>& OutSelectionResult,
	TNotNull<const UMetaStory*> NextMetaStory,
	const FMetaStoryCompactState& NextState,
	const FMetaStoryStateHandle NextStateHandle)
{
	using namespace UE::MetaStory::ExecutionContext;

	if (!NextState.HasChildren())
	{
		// Select this state (For backwards compatibility)
		UE_METASTORY_DEBUG_STATE_EVENT(this, NextStateHandle, EMetaStoryTraceEventType::OnStateSelected);
		return true;
	}

	UE_METASTORY_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EMetaStoryUpdatePhase::TrySelectBehavior);

	TArray<uint16, TInlineAllocator<16, FNonconcurrentLinearArrayAllocator>> NextLevelChildStates;
	NextLevelChildStates.Reserve(NextState.ChildrenEnd - NextState.ChildrenBegin);
	for (uint16 ChildStateIndex = NextState.ChildrenBegin; ChildStateIndex < NextState.ChildrenEnd; ChildStateIndex = NextMetaStory->States[ChildStateIndex].GetNextSibling())
	{
		NextLevelChildStates.Push(ChildStateIndex);
	}

	const FMetaStoryExecutionState& Exec = GetExecState();
	while (!NextLevelChildStates.IsEmpty())
	{
		const int32 ChildStateIndex = Exec.RandomStream.RandRange(0, NextLevelChildStates.Num() - 1);
		FStateHandleContext ChildState = FStateHandleContext(NextMetaStory, FMetaStoryStateHandle(NextLevelChildStates[ChildStateIndex]));
		FSelectStateInternalArguments NewInternalArgs = InternalArgs;
		NewInternalArgs.MissingStatesToReachTarget = MakeArrayView(&ChildState, 1);
		if (SelectStateInternal(Args, NewInternalArgs, OutSelectionResult))
		{
			// Selection succeeded
			return true;
		}

		constexpr EAllowShrinking AllowShrinking = EAllowShrinking::No;
		NextLevelChildStates.RemoveAtSwap(ChildStateIndex, AllowShrinking);
	}

	return false;
}

bool FMetaStoryExecutionContext::SelectStateInternal_TrySelectChildrenWithHighestUtility(
	const FSelectStateArguments& Args,
	const FSelectStateInternalArguments& InternalArgs,
	const TSharedRef<FSelectStateResult>& OutSelectionResult,
	TNotNull<const UMetaStory*> NextMetaStory,
	const FMetaStoryCompactState& NextState,
	const FMetaStoryStateHandle NextStateHandle)
{
	using namespace UE::MetaStory::ExecutionContext;

	if (!NextState.HasChildren())
	{
		// Select this state (For backwards compatibility)
		UE_METASTORY_DEBUG_STATE_EVENT(this, NextStateHandle, EMetaStoryTraceEventType::OnStateSelected);
		return true;
	}

	UE_METASTORY_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EMetaStoryUpdatePhase::TrySelectBehavior);

	TArray<TPair<uint16, float>, TInlineAllocator<16, FNonconcurrentLinearArrayAllocator>> NextLevelChildStates;
	NextLevelChildStates.Reserve(NextState.ChildrenEnd - NextState.ChildrenBegin);
	{
		using namespace UE::MetaStory::ExecutionContext::Private;
		const FMetaStoryExecutionFrame* NextParentFrame = OutSelectionResult->SelectedFrames.Num() > 1
			? FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(1), MakeConstArrayView(GetExecState().ActiveFrames), MakeConstArrayView(OutSelectionResult->TemporaryFrames))
			: nullptr;
		FMetaStoryExecutionFrame* NextFrame = FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(), MakeArrayView(GetExecState().ActiveFrames), MakeArrayView(OutSelectionResult->TemporaryFrames));
		check(NextFrame);

		for (uint16 ChildState = NextState.ChildrenBegin; ChildState < NextState.ChildrenEnd; ChildState = NextMetaStory->States[ChildState].GetNextSibling())
		{
			const FMetaStoryCompactState& CurrentState = NextMetaStory->States[ChildState];
			const float Score = EvaluateUtilityWithValidation(NextParentFrame, *NextFrame, FMetaStoryStateHandle(ChildState), CurrentState.ConsiderationEvaluationScopeMemoryRequirement, CurrentState.UtilityConsiderationsBegin, CurrentState.UtilityConsiderationsNum, CurrentState.Weight);
			NextLevelChildStates.Emplace(ChildState, Score);
		}
	}

	while (!NextLevelChildStates.IsEmpty())
	{
		//Find one with highest score in the remaining candidates
		float HighestScore = -std::numeric_limits<float>::infinity();
		int32 ArrayIndexWithHighestScore = INDEX_NONE;
		for (int32 Index = 0; Index < NextLevelChildStates.Num(); ++Index)
		{
			const float Score = NextLevelChildStates[Index].Get<1>();
			if (Score > HighestScore)
			{
				HighestScore = Score;
				ArrayIndexWithHighestScore = Index;
			}
		}

		if (!NextLevelChildStates.IsValidIndex(ArrayIndexWithHighestScore))
		{
			return false;
		}

		FStateHandleContext ChildState = FStateHandleContext(NextMetaStory, FMetaStoryStateHandle(NextLevelChildStates[ArrayIndexWithHighestScore].Get<0>()));
		FSelectStateInternalArguments NewInternalArgs = InternalArgs;
		NewInternalArgs.MissingStatesToReachTarget = MakeArrayView(&ChildState, 1);
		if (SelectStateInternal(Args, NewInternalArgs, OutSelectionResult))
		{
			return true;
		}

		// Disqualify the state we failed to enter
		constexpr EAllowShrinking AllowShrinking = EAllowShrinking::No;
		NextLevelChildStates.RemoveAtSwap(ArrayIndexWithHighestScore, AllowShrinking);
	}

	return false;
}

bool FMetaStoryExecutionContext::SelectStateInternal_TrySelectChildrenAtRandomWeightedByUtility(
	const FSelectStateArguments& Args,
	const FSelectStateInternalArguments& InternalArgs,
	const TSharedRef<FSelectStateResult>& OutSelectionResult,
	TNotNull<const UMetaStory*> NextMetaStory,
	const FMetaStoryCompactState& NextState,
	const FMetaStoryStateHandle NextStateHandle)
{
	using namespace UE::MetaStory::ExecutionContext;

	if (!NextState.HasChildren())
	{
		// Select this state (For backwards compatibility)
		UE_METASTORY_DEBUG_STATE_EVENT(this, NextStateHandle, EMetaStoryTraceEventType::OnStateSelected);
		return true;
	}

	UE_METASTORY_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EMetaStoryUpdatePhase::TrySelectBehavior);

	TArray<TTuple<uint16, float>, TInlineAllocator<16, FNonconcurrentLinearArrayAllocator>> NextLevelChildStates;
	NextLevelChildStates.Reserve(NextState.ChildrenEnd - NextState.ChildrenBegin);

	float TotalScore = 0.0f;
	{
		using namespace UE::MetaStory::ExecutionContext::Private;
		const FMetaStoryExecutionFrame* NextParentFrame = OutSelectionResult->SelectedFrames.Num() > 1
			? FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(1), MakeConstArrayView(GetExecState().ActiveFrames), MakeConstArrayView(OutSelectionResult->TemporaryFrames))
			: nullptr;
		FMetaStoryExecutionFrame* NextFrame = FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(), MakeArrayView(GetExecState().ActiveFrames), MakeArrayView(OutSelectionResult->TemporaryFrames));
		check(NextFrame);

		for (uint16 ChildState = NextState.ChildrenBegin; ChildState < NextState.ChildrenEnd; ChildState = NextMetaStory->States[ChildState].GetNextSibling())
		{
			const FMetaStoryCompactState& CurrentState = NextMetaStory->States[ChildState];
			const float Score = EvaluateUtilityWithValidation(NextParentFrame, *NextFrame, FMetaStoryStateHandle(ChildState), CurrentState.ConsiderationEvaluationScopeMemoryRequirement, CurrentState.UtilityConsiderationsBegin, CurrentState.UtilityConsiderationsNum, CurrentState.Weight);
			if (Score > 0.0f)
			{
				NextLevelChildStates.Emplace(ChildState, Score);
				TotalScore += Score;
			}
		}
	}

	const FMetaStoryExecutionState& Exec = GetExecState();
	while (!NextLevelChildStates.IsEmpty())
	{
		const float RandomScore = Exec.RandomStream.FRand() * TotalScore;
		float AccumulatedScore = 0.0f;
		for (int32 Index = 0; Index < NextLevelChildStates.Num(); ++Index)
		{
			const TTuple<uint16, float>& StateScorePair = NextLevelChildStates[Index];
			const uint16 StateIndex = StateScorePair.Key;
			const float StateScore = StateScorePair.Value;
			AccumulatedScore += StateScore;

			if (RandomScore <= AccumulatedScore || (Index == (NextLevelChildStates.Num() - 1)))
			{
				FStateHandleContext ChildState = FStateHandleContext(NextMetaStory, FMetaStoryStateHandle(StateIndex));
				FSelectStateInternalArguments NewInternalArgs = InternalArgs;
				NewInternalArgs.MissingStatesToReachTarget = MakeArrayView(&ChildState, 1);
				if (SelectStateInternal(Args, NewInternalArgs, OutSelectionResult))
				{
					return true;
				}

				// Disqualify the state we failed to enter
				constexpr EAllowShrinking AllowShrinking = EAllowShrinking::No;
				NextLevelChildStates.RemoveAtSwap(Index, AllowShrinking);
				break;
			}
		}
	}

	return false;
}

bool FMetaStoryExecutionContext::SelectStateInternal_TryFollowTransitions(
	const FSelectStateArguments& Args,
	const FSelectStateInternalArguments& InternalArgs,
	const TSharedRef<FSelectStateResult>& OutSelectionResult,
	TNotNull<const UMetaStory*> NextMetaStory,
	const FMetaStoryCompactState& NextState,
	const FMetaStoryStateHandle NextStateHandle)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::ExecutionContext;
	using namespace UE::MetaStory::ExecutionContext::Private;

	UE_METASTORY_DEBUG_SCOPED_STATE_PHASE(this, NextStateHandle, EMetaStoryUpdatePhase::TrySelectBehavior);

	bool bSucceededToSelectState = false;
	EMetaStoryTransitionPriority CurrentPriority = EMetaStoryTransitionPriority::None;

	for (uint8 Index = 0; Index < NextState.TransitionsNum; ++Index)
	{
		const int32 TransitionIndex = NextState.TransitionsBegin + Index;
		const FMetaStoryCompactStateTransition& Transition = NextMetaStory->Transitions[TransitionIndex];

		// Skip disabled transitions
		if (Transition.bTransitionEnabled == false)
		{
			continue;
		}

		// No need to test the transition if same or higher priority transition has already been processed.
		if (Transition.Priority <= CurrentPriority)
		{
			continue;
		}

		// Skip completion transitions
		if (EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnStateCompleted))
		{
			continue;
		}

		// Skip invalid state or completion state
		if (!Transition.State.IsValid() || Transition.State.IsCompletionState())
		{
			continue;
		}

		// Cannot follow transitions with delay.
		if (Transition.HasDelay())
		{
			continue;
		}

		// Try to prevent (infinite) loops in the selection.
		const bool bSelectionContainsState = OutSelectionResult->SelectedStates.ContainsByPredicate(
			[State = Transition.State](const FActiveState& Other)
			{
				return State == Other.GetStateHandle();
			});
		if (bSelectionContainsState)
		{
			METASTORY_LOG(Warning, TEXT("%hs: Loop detected when trying to select state %s from '%s'. Prior states: %s.  '%s' using MetaStory '%s'.")
				, __FUNCTION__
				, *GetSafeStateName(NextMetaStory, Transition.State)
				, *GetStateStatusString(GetExecState())
				, *GetStatePathAsString(&RootMetaStory, OutSelectionResult->SelectedStates)
				, *Owner.GetName()
				, *NextMetaStory->GetFullName());
			continue;
		}

		FSharedEventInlineArray TransitionEvents;
		GetTriggerTransitionEvent(Transition, Storage, Args.TransitionEvent, GetEventsToProcessView(), TransitionEvents);

		for (const FMetaStorySharedEvent& SelectedStateTransitionEvent : TransitionEvents)
		{
			bool bTransitionConditionsPassed = false;
			{
				using namespace UE::MetaStory::ExecutionContext::Private;
				const FMetaStoryExecutionFrame* NextParentFrame = OutSelectionResult->SelectedFrames.Num() > 1
					? FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(1), MakeConstArrayView(GetExecState().ActiveFrames), MakeConstArrayView(OutSelectionResult->TemporaryFrames))
					: nullptr;
				FMetaStoryExecutionFrame* NextFrame = FindExecutionFrame(OutSelectionResult->SelectedFrames.Last(), MakeArrayView(GetExecState().ActiveFrames), MakeArrayView(OutSelectionResult->TemporaryFrames));
				check(NextFrame);

				FCurrentlyProcessedTransitionEventScope TransitionEventScope(*this, SelectedStateTransitionEvent.IsValid() ? SelectedStateTransitionEvent.Get() : nullptr);
				UE_METASTORY_DEBUG_TRANSITION_EVENT(this, FMetaStoryTransitionSource(NextMetaStory, FMetaStoryIndex16(TransitionIndex), Transition.State, Transition.Priority), EMetaStoryTraceEventType::OnEvaluating);
				bTransitionConditionsPassed = TestAllConditionsWithValidation(NextParentFrame, *NextFrame, NextStateHandle, Transition.ConditionEvaluationScopeMemoryRequirement, Transition.ConditionsBegin, Transition.ConditionsNum, EMetaStoryUpdatePhase::TransitionConditions);
			}

			if (bTransitionConditionsPassed)
			{
				// Using SelectState() instead of SelectStateInternal to treat the transitions the same way as regular transitions,
				// e.g. it may jump to a completely different branch.

				FSelectStateResult CopySelectStateResult = OutSelectionResult.Get();

				FSelectStateArguments SelectStateArgs;
				SelectStateArgs.ActiveStates = MakeConstArrayView(CopySelectStateResult.SelectedStates);
				if (CopySelectStateResult.SelectedStates.Num() > 0)
				{
					SelectStateArgs.SourceState = CopySelectStateResult.SelectedStates.Last();
				}
				SelectStateArgs.TargetState = FStateHandleContext(NextMetaStory, Transition.State);
				SelectStateArgs.TransitionEvent = SelectedStateTransitionEvent;
				SelectStateArgs.Fallback = Transition.Fallback;
				SelectStateArgs.SelectionRules = NextMetaStory->StateSelectionRules;

				OutSelectionResult->SelectedStates.Reset();
				OutSelectionResult->SelectedFrames.Reset();
				if (SelectState(SelectStateArgs, OutSelectionResult))
				{
					CurrentPriority = Transition.Priority;
					bSucceededToSelectState = true;

					//@todo sort the transition by priority at compile time. This will solve having to loop back once we found a valid transition.
					// Consume the transition event
					if (SelectedStateTransitionEvent.IsValid() && Transition.bConsumeEventOnSelect)
					{
						ConsumeEvent(SelectedStateTransitionEvent);
					}

					// Cannot return because higher priority transitions may override the selection. 
					break;
				}
				else
				{
					FSelectStateResult& SelectionResult = OutSelectionResult.Get();
					SelectionResult = MoveTemp(CopySelectStateResult);
				}
			}
		}
	}

	return bSucceededToSelectState;
}

FString FMetaStoryExecutionContext::GetSafeStateName(const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryStateHandle State) const
{
	return GetSafeStateName(CurrentFrame.MetaStory, State);
}

FString FMetaStoryExecutionContext::GetSafeStateName(const UMetaStory* MetaStory, const FMetaStoryStateHandle State) const
{
	if (State == FMetaStoryStateHandle::Invalid)
	{
		return TEXT("(State Invalid)");
	}
	else if (State == FMetaStoryStateHandle::Succeeded)
	{
		return TEXT("(State Succeeded)");
	}
	else if (State == FMetaStoryStateHandle::Failed)
	{
		return TEXT("(State Failed)");
	}
	else if (MetaStory && MetaStory->States.IsValidIndex(State.Index))
	{
		return *MetaStory->States[State.Index].Name.ToString();
	}
	return TEXT("(Unknown)");
}

FString FMetaStoryExecutionContext::DebugGetStatePath(TConstArrayView<FMetaStoryExecutionFrame> ActiveFrames, const FMetaStoryExecutionFrame* CurrentFrame, const int32 ActiveStateIndex) const
{
	FString StatePath;
	const UMetaStory* LastMetaStory = &RootMetaStory;

	for (const FMetaStoryExecutionFrame& Frame : ActiveFrames)
	{
		if (!ensure(Frame.MetaStory))
		{
			return StatePath;
		}

		// If requested up the active state, clamp count.
		int32 Num = Frame.ActiveStates.Num();
		if (CurrentFrame == &Frame && Frame.ActiveStates.IsValidIndex(ActiveStateIndex))
		{
			Num = ActiveStateIndex + 1;
		}

		if (Frame.MetaStory != LastMetaStory)
		{
			StatePath.Appendf(TEXT("[%s]"), *GetNameSafe(Frame.MetaStory));
			LastMetaStory = Frame.MetaStory;
		}

		for (int32 i = 0; i < Num; i++)
		{
			const FMetaStoryCompactState& State = Frame.MetaStory->States[Frame.ActiveStates[i].Index];
			StatePath.Appendf(TEXT("%s%s"), i == 0 ? TEXT("") : TEXT("."), *State.Name.ToString());
		}
	}

	return StatePath;
}

FString FMetaStoryExecutionContext::GetStateStatusString(const FMetaStoryExecutionState& ExecState) const
{
	if (ExecState.TreeRunStatus != EMetaStoryRunStatus::Running)
	{
		return TEXT("--:") + UEnum::GetDisplayValueAsText(ExecState.LastTickStatus).ToString();
	}

	if (ExecState.ActiveFrames.Num())
	{
		const FMetaStoryExecutionFrame& LastFrame = ExecState.ActiveFrames.Last();
		if (LastFrame.ActiveStates.Num() > 0)
		{
			return GetSafeStateName(LastFrame, LastFrame.ActiveStates.Last()) + TEXT(":") + UEnum::GetDisplayValueAsText(ExecState.LastTickStatus).ToString();
		}
	}
	return FString();
}

// Deprecated
FString FMetaStoryExecutionContext::GetInstanceDescription() const
{
	return GetInstanceDescriptionInternal();
}


#undef METASTORY_LOG
#undef METASTORY_CLOG
