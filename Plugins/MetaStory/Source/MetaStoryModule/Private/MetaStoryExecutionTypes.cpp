// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryExecutionTypes.h"
#include "MetaStory.h"
#include "MetaStoryDelegate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryExecutionTypes)

const FMetaStoryExternalDataHandle FMetaStoryExternalDataHandle::Invalid = FMetaStoryExternalDataHandle();

#if WITH_METASTORY_TRACE
const FMetaStoryInstanceDebugId FMetaStoryInstanceDebugId::Invalid = FMetaStoryInstanceDebugId();
#endif // WITH_METASTORY_TRACE

//----------------------------------------------------------------------//
// FMetaStoryTransitionSource
//----------------------------------------------------------------------//
FMetaStoryTransitionSource::FMetaStoryTransitionSource(
	const UMetaStory* MetaStory,
	const EMetaStoryTransitionSourceType SourceType,
	const FMetaStoryIndex16 TransitionIndex,
	const FMetaStoryStateHandle TargetState,
	const EMetaStoryTransitionPriority Priority
	)
	: Asset(MetaStory)
	, SourceType(SourceType)
	, TransitionIndex(TransitionIndex)
	, TargetState(TargetState)
	, Priority(Priority)
{
}

//----------------------------------------------------------------------//
// FMetaStoryTransitionResult
//----------------------------------------------------------------------//
//Deprecated
FMetaStoryTransitionResult::FMetaStoryTransitionResult(const FMetaStoryRecordedTransitionResult& RecordedTransition)
{
}

//----------------------------------------------------------------------//
// FMetaStoryRecordedTransitionResult
//----------------------------------------------------------------------//
//Deprecated
FMetaStoryRecordedTransitionResult::FMetaStoryRecordedTransitionResult(const FMetaStoryTransitionResult& Transition)
{
}


//----------------------------------------------------------------------//
// FMetaStoryExecutionState
//----------------------------------------------------------------------//
UE::MetaStory::FActiveStatePath FMetaStoryExecutionState::GetActiveStatePath() const
{
	int32 NewNum = 0;
	for (const FMetaStoryExecutionFrame& Frame : ActiveFrames)
	{
		NewNum += Frame.ActiveStates.Num();
	}

	if (NewNum == 0 || ActiveFrames[0].MetaStory == nullptr)
	{
		return UE::MetaStory::FActiveStatePath();
	}

	TArray<UE::MetaStory::FActiveState> Elements;
	Elements.Reserve(NewNum);

	for (const FMetaStoryExecutionFrame& Frame : ActiveFrames)
	{
		for (int32 StateIndex = 0; StateIndex < Frame.ActiveStates.Num(); ++StateIndex)
		{
			Elements.Emplace(Frame.FrameID, Frame.ActiveStates.StateIDs[StateIndex], Frame.ActiveStates.States[StateIndex]);
		}
	}

	return UE::MetaStory::FActiveStatePath(ActiveFrames[0].MetaStory, MoveTemp(Elements));
}

const FMetaStoryExecutionFrame* FMetaStoryExecutionState::FindActiveFrame(UE::MetaStory::FActiveFrameID FrameID) const
{
	return ActiveFrames.FindByPredicate([FrameID](const FMetaStoryExecutionFrame& Other)
		{
			return Other.FrameID == FrameID;
		});
}

FMetaStoryExecutionFrame* FMetaStoryExecutionState::FindActiveFrame(UE::MetaStory::FActiveFrameID FrameID)
{
	return ActiveFrames.FindByPredicate([FrameID](const FMetaStoryExecutionFrame& Other)
		{
			return Other.FrameID == FrameID;
		});
}

int32 FMetaStoryExecutionState::IndexOfActiveFrame(UE::MetaStory::FActiveFrameID FrameID) const
{
	return ActiveFrames.IndexOfByPredicate([FrameID](const FMetaStoryExecutionFrame& Other)
		{
			return Other.FrameID == FrameID;
		});
}

UE::MetaStory::FScheduledTickHandle FMetaStoryExecutionState::AddScheduledTickRequest(FMetaStoryScheduledTick ScheduledTick)
{
	UE::MetaStory::FScheduledTickHandle Result = UE::MetaStory::FScheduledTickHandle::GenerateNewHandle();
	ScheduledTickRequests.Add(FScheduledTickRequest{.Handle = Result, .ScheduledTick = ScheduledTick});
	CacheScheduledTickRequest();
	return Result;
}

bool FMetaStoryExecutionState::UpdateScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle, FMetaStoryScheduledTick ScheduledTick)
{
	FScheduledTickRequest* Found = ScheduledTickRequests.FindByPredicate([Handle](const FScheduledTickRequest& Other) { return Other.Handle == Handle; });
	if (Found && Found->ScheduledTick != ScheduledTick)
	{
		Found->ScheduledTick = ScheduledTick;
		CacheScheduledTickRequest();
		return true;
	}
	return false;
}

bool FMetaStoryExecutionState::RemoveScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle)
{
	const int32 IndexOf = ScheduledTickRequests.IndexOfByPredicate([Handle](const FScheduledTickRequest& Other) { return Other.Handle == Handle; });
	if (IndexOf != INDEX_NONE)
	{
		ScheduledTickRequests.RemoveAtSwap(IndexOf);
		CacheScheduledTickRequest();
	}
	return IndexOf != INDEX_NONE;
}

void FMetaStoryExecutionState::CacheScheduledTickRequest()
{
	auto GetBestRequest = [](TConstArrayView<FScheduledTickRequest> Requests)
		{
			const int32 ScheduledTickRequestsNum = Requests.Num();
			if (ScheduledTickRequestsNum == 0)
			{
				return FMetaStoryScheduledTick();
			}
			if (ScheduledTickRequestsNum == 1)
			{
				return Requests[0].ScheduledTick;
			}

			for (const FScheduledTickRequest& Request : Requests)
			{
				if (Request.ScheduledTick.ShouldTickEveryFrames())
				{
					return Request.ScheduledTick;
				}
			}

			for (const FScheduledTickRequest& Request : Requests)
			{
				if (Request.ScheduledTick.ShouldTickOnceNextFrame())
				{
					return Request.ScheduledTick;
				}
			}

			TOptional<float> CustomTickRate;
			for (const FScheduledTickRequest& Request : Requests)
			{
				const float CachedTickRate = Request.ScheduledTick.GetTickRate();
				CustomTickRate = CustomTickRate.IsSet() ? FMath::Min(CustomTickRate.GetValue(), CachedTickRate) : CachedTickRate;
			}
			return FMetaStoryScheduledTick::MakeCustomTickRate(CustomTickRate.GetValue(), UE::MetaStory::EMetaStoryTickReason::ScheduledTickRequest);
		};

	CachedScheduledTickRequest = GetBestRequest(ScheduledTickRequests);
}

//----------------------------------------------------------------------//
// FMetaStoryExecutionFrame
//----------------------------------------------------------------------//
//Deprecated
PRAGMA_DISABLE_DEPRECATION_WARNINGS
FMetaStoryExecutionFrame::FMetaStoryExecutionFrame(const FMetaStoryRecordedExecutionFrame& RecordedExecutionFrame)
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

//----------------------------------------------------------------------//
// FMetaStoryRecordedExecutionFrame
//----------------------------------------------------------------------//
//Deprecated
PRAGMA_DISABLE_DEPRECATION_WARNINGS
FMetaStoryRecordedExecutionFrame::FMetaStoryRecordedExecutionFrame(const FMetaStoryExecutionFrame& ExecutionFrame)
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

//Deprecated
//----------------------------------------------------------------------//
// FFinishedTask
//----------------------------------------------------------------------//
UE::MetaStory::FFinishedTask::FFinishedTask(FActiveFrameID InFrameID, FActiveStateID InStateID, FMetaStoryIndex16 InTaskIndex, EMetaStoryRunStatus InRunStatus, EReasonType InReason, bool bInTickProcessed)
	: FrameID(InFrameID)
	, StateID(InStateID)
	, TaskIndex(InTaskIndex)
	, RunStatus(InRunStatus)
	, Reason(InReason)
	, bTickProcessed(bInTickProcessed)
{
}

//----------------------------------------------------------------------//
// FMetaStoryScheduledTick
//----------------------------------------------------------------------//
FMetaStoryScheduledTick FMetaStoryScheduledTick::MakeSleep()
{
	return FMetaStoryScheduledTick(UE_FLOAT_NON_FRACTIONAL, UE::MetaStory::EMetaStoryTickReason::None);
}

FMetaStoryScheduledTick FMetaStoryScheduledTick::MakeEveryFrames(UE::MetaStory::EMetaStoryTickReason Reason)
{
	return FMetaStoryScheduledTick(0.0f, Reason);
}

FMetaStoryScheduledTick FMetaStoryScheduledTick::MakeNextFrame(UE::MetaStory::EMetaStoryTickReason Reason)
{
	return FMetaStoryScheduledTick(UE_KINDA_SMALL_NUMBER, Reason);
}

FMetaStoryScheduledTick FMetaStoryScheduledTick::MakeCustomTickRate(float DeltaTime, UE::MetaStory::EMetaStoryTickReason Reason)
{
	ensureMsgf(DeltaTime >= 0.0f, TEXT("Use a value greater than zero."));
	if (DeltaTime > 0.0f)
	{
		return FMetaStoryScheduledTick(DeltaTime, Reason);
	}
	return MakeEveryFrames(Reason);
}

bool FMetaStoryScheduledTick::ShouldSleep() const
{
	return NextDeltaTime >= UE_FLOAT_NON_FRACTIONAL;
}

bool FMetaStoryScheduledTick::ShouldTickEveryFrames() const
{
	return NextDeltaTime == 0.0f;
}

bool FMetaStoryScheduledTick::ShouldTickOnceNextFrame() const
{
	return NextDeltaTime == UE_KINDA_SMALL_NUMBER;
}

bool FMetaStoryScheduledTick::HasCustomTickRate() const
{
	return NextDeltaTime > 0.0f;
}

float FMetaStoryScheduledTick::GetTickRate() const
{
	return NextDeltaTime;
}

//----------------------------------------------------------------------//
// FScheduledTickHandle
//----------------------------------------------------------------------//
UE::MetaStory::FScheduledTickHandle UE::MetaStory::FScheduledTickHandle::GenerateNewHandle()
{
	static std::atomic<uint32> Value = 0;

	uint32 Result = 0;
	UE_AUTORTFM_OPEN
	{
		Result = ++Value;

		// Check that we wrap round to 0, because we reserve 0 for invalid.
		if (Result == 0)
		{
			Result = ++Value;
		}
	};

	return FScheduledTickHandle(Result);
}


//----------------------------------------------------------------------//
// FMetaStoryDelegateActiveListeners
//----------------------------------------------------------------------//
FMetaStoryDelegateActiveListeners::FActiveListener::FActiveListener(const FMetaStoryDelegateListener& InListener, FSimpleDelegate InDelegate, UE::MetaStory::FActiveFrameID InFrameID, UE::MetaStory::FActiveStateID InStateID, FMetaStoryIndex16 InOwningNodeIndex)
	: Listener(InListener)
	, Delegate(MoveTemp(InDelegate))
	, FrameID(InFrameID)
	, StateID(InStateID)
	, OwningNodeIndex(InOwningNodeIndex)
{}

FMetaStoryDelegateActiveListeners::~FMetaStoryDelegateActiveListeners()
{
	check(BroadcastingLockCount == 0);
}

void FMetaStoryDelegateActiveListeners::Add(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate, UE::MetaStory::FActiveFrameID InFrameID, UE::MetaStory::FActiveStateID InStateID, FMetaStoryIndex16 OwningNodeIndex)
{
	check(Listener.IsValid());
	Remove(Listener);
	Listeners.Emplace(Listener, MoveTemp(Delegate), InFrameID, InStateID, OwningNodeIndex);
}

void FMetaStoryDelegateActiveListeners::Remove(const FMetaStoryDelegateListener& Listener)
{
	check(Listener.IsValid());

	const int32 Index = Listeners.IndexOfByPredicate([Listener](const FActiveListener& ActiveListener)
		{
			return ActiveListener.Listener == Listener;
		});

	if (Index == INDEX_NONE)
	{
		return;
	}

	if (BroadcastingLockCount > 0)
	{
		Listeners[Index] = FActiveListener();
		bContainsUnboundListeners = true;
	}
	else
	{
		Listeners.RemoveAtSwap(Index);
	}
}

void FMetaStoryDelegateActiveListeners::RemoveAll(UE::MetaStory::FActiveFrameID FrameID)
{
	check(BroadcastingLockCount == 0);
	Listeners.RemoveAllSwap([FrameID](const FActiveListener& Listener)
		{
			return Listener.FrameID == FrameID;
		});
}

void FMetaStoryDelegateActiveListeners::RemoveAll(UE::MetaStory::FActiveStateID StateID)
{
	check(BroadcastingLockCount == 0);
	Listeners.RemoveAllSwap([StateID](const FActiveListener& Listener)
		{
			return Listener.StateID == StateID;
		});
}

void FMetaStoryDelegateActiveListeners::BroadcastDelegate(const FMetaStoryDelegateDispatcher& Dispatcher, const FMetaStoryExecutionState& Exec)
{
	check(Dispatcher.IsValid());

	++BroadcastingLockCount;

	const int32 NumListeners = Listeners.Num();
	for (int32 Index = 0; Index < NumListeners; ++Index)
	{
		FActiveListener& ActiveListener = Listeners[Index];
		if (ActiveListener.Listener.GetDispatcher() == Dispatcher)
		{
			if (const FMetaStoryExecutionFrame* ExectionFrame = Exec.FindActiveFrame(ActiveListener.FrameID))
			{
				// Is the node active and is the state active.
				if (ActiveListener.OwningNodeIndex.Get() <= ExectionFrame->ActiveNodeIndex.Get())
				{
					if (!ActiveListener.StateID.IsValid())
					{
						// It's a global task, no need to check for the state.
						ActiveListener.Delegate.ExecuteIfBound();
					}
					else
					{
						const int32 FoundStateIndex = ExectionFrame->ActiveStates.IndexOfReverse(ActiveListener.StateID);
						if (FoundStateIndex != INDEX_NONE)
						{
							ActiveListener.Delegate.ExecuteIfBound();
						}
					}
				}
			}
		}
	}

	--BroadcastingLockCount;

	if (BroadcastingLockCount == 0)
	{
		RemoveUnbounds();
	}
}

void FMetaStoryDelegateActiveListeners::RemoveUnbounds()
{
	check(BroadcastingLockCount == 0);
	if (!bContainsUnboundListeners)
	{
		return;
	}

	Listeners.RemoveAllSwap([](const FActiveListener& Listener) { return !Listener.IsValid(); });
	bContainsUnboundListeners = false;
}
