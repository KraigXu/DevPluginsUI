// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryAsyncExecutionContext.h"
#include "MetaStoryExecutionContext.h"

namespace UE::MetaStory::Async
{
	const FMetaStoryNodeBase& FActivePathInfo::GetNode() const
	{
		check(IsValid() && Frame->MetaStory && NodeIndex.IsValid());

		return *(Frame->MetaStory->GetNode(NodeIndex.AsInt32()).GetPtr<const FMetaStoryNodeBase>());
	}
}

template <bool bWithWriteAccess>
TStateTreeStrongExecutionContext<bWithWriteAccess>::TStateTreeStrongExecutionContext(const FMetaStoryWeakExecutionContext& WeakContext)
	: Owner(WeakContext.Owner.Pin()),
	MetaStory(WeakContext.MetaStory.Pin()),
	Storage(WeakContext.Storage.Pin()),
	TemporaryStorage(WeakContext.TemporaryStorage.Pin()),
	FrameID(WeakContext.FrameID),
	StateID(WeakContext.StateID),
	NodeIndex(WeakContext.NodeIndex)
{
	if (Owner && MetaStory && Storage)
	{
		bAccessAcquired = true;
		if constexpr (bWithWriteAccess)
		{
			Storage->AcquireWriteAccess();
		}
		else
		{
			Storage->AcquireReadAccess();
		}
	}
}

template <bool bWithWriteAccess>
TStateTreeStrongExecutionContext<bWithWriteAccess>::~TStateTreeStrongExecutionContext()
{
	if (bAccessAcquired)
	{
		check(IsValidInstanceStorage());

		if constexpr (bWithWriteAccess)
		{
			Storage->ReleaseWriteAccess();
		}
		else
		{
			Storage->ReleaseReadAccess();
		}
	}
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin) const
{
	static_assert(bWithWriteAccess);

	if (IsValid())
	{
		FMetaStoryMinimalExecutionContext Context(Owner.Get(), MetaStory.Get(), *Storage.Get());
		Context.SendEvent(Tag, Payload, Origin);
		return true;
	}

	return false;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::RequestTransition(FMetaStoryStateHandle TargetState,
	EMetaStoryTransitionPriority Priority, EMetaStorySelectionFallback Fallback) const
{
	static_assert(bWithWriteAccess);

	UE::MetaStory::Async::FActivePathInfo ActivePath = GetActivePathInfo();
	if (ActivePath.IsValid())
	{
		FMetaStoryTransitionRequest Request;
		Request.SourceFrameID = FrameID;
		Request.SourceStateID = StateID;
		Request.TargetState = TargetState;
		Request.Priority = Priority;
		Request.Fallback = Fallback;

		Storage->AddTransitionRequest(Owner.Get(), Request);
		ScheduleNextTick(Owner.Get(), MetaStory.Get(), *Storage, UE::MetaStory::EMetaStoryTickReason::TransitionRequest);

		return true;
	}

	return false;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::BroadcastDelegate(const FMetaStoryDelegateDispatcher& Dispatcher)
const
{
	static_assert(bWithWriteAccess);

	if (!Dispatcher.IsValid())
	{
		// Nothings binds to the delegate, not an error.
		return true;
	}

	UE::MetaStory::Async::FActivePathInfo ActivePath = GetActivePathInfo();
	if (ActivePath.IsValid())
	{
		FMetaStoryExecutionState& Exec = Storage->GetMutableExecutionState();
		Exec.DelegateActiveListeners.BroadcastDelegate(Dispatcher, Exec);
		if (UE::MetaStory::ExecutionContext::MarkDelegateAsBroadcasted(Dispatcher, *ActivePath.Frame, *Storage))
		{
			ScheduleNextTick(Owner.Get(), MetaStory.Get(), *Storage, UE::MetaStory::EMetaStoryTickReason::Delegate);
		}

		return true;
	}

	return false;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::BindDelegate(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate) const
{
	static_assert(bWithWriteAccess);

	if (!Listener.IsValid())
	{
		// Nothing binds to the delegate, not an error.
		return true;
	}

	UE::MetaStory::Async::FActivePathInfo ActivePath = GetActivePathInfo();
	if (ActivePath.IsValid())
	{
		FMetaStoryExecutionState& Exec = Storage->GetMutableExecutionState();
		if (ensure(ActivePath.Frame->MetaStory))
		{
			Exec.DelegateActiveListeners.Add(Listener, MoveTemp(Delegate), FrameID, StateID, NodeIndex);
			return true;
		}
	}

	return false;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::UnbindDelegate(const FMetaStoryDelegateListener& Listener) const
{
	static_assert(bWithWriteAccess);

	if (!Listener.IsValid())
	{
		// The listener is not bound to a dispatcher. It will never trigger the delegate. It is not an error.
		return true;
	}

	// Allow unbinding from context created outside the ExecContext loop
	if (IsValid())
	{
		FMetaStoryExecutionState& Exec = Storage->GetMutableExecutionState();
		Exec.DelegateActiveListeners.Remove(Listener);

		return true;
	}

	return false;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::FinishTask(EMetaStoryFinishTaskType FinishType) const
{
	static_assert(bWithWriteAccess);

	bool bSucceed = false;

	UE::MetaStory::Async::FActivePathInfo ActivePath = GetActivePathInfo();
	if (ActivePath.IsValid())
	{
		FMetaStoryExecutionState& Exec = Storage->GetMutableExecutionState();
		const UMetaStory* FrameMetaStory = ActivePath.Frame->MetaStory;
		if (ensure(FrameMetaStory))
		{
			using namespace UE::MetaStory;

			const int32 AssetNodeIndex = ActivePath.NodeIndex.AsInt32();
			const int32 GlobalTaskBeginIndex = FrameMetaStory->GlobalTasksBegin;
			const int32 GlobalTaskEndIndex = FrameMetaStory->GlobalTasksBegin + FrameMetaStory->GlobalTasksNum;
			const bool bIsGlobalTask = AssetNodeIndex >= GlobalTaskBeginIndex && AssetNodeIndex < GlobalTaskEndIndex;
			const ETaskCompletionStatus TaskStatus = ExecutionContext::CastToTaskStatus(FinishType);

			bool bCompleted = false;
			if (ActivePath.Frame->bIsGlobalFrame && bIsGlobalTask)
			{
				check(!ActivePath.StateHandle.IsValid());
				const int32 FrameTaskIndex = AssetNodeIndex - GlobalTaskBeginIndex;
				FTasksCompletionStatus GlobalTasksStatus = ActivePath.Frame->ActiveTasksStatus.GetStatus(FrameMetaStory);
				GlobalTasksStatus.SetStatusWithPriority(FrameTaskIndex, TaskStatus);
				bCompleted = GlobalTasksStatus.IsCompleted();
				bSucceed = true;
			}
			else if (ensure(ActivePath.StateHandle.IsValid()))
			{
				const FMetaStoryCompactState& State = FrameMetaStory->States[ActivePath.StateHandle.Index];
				const int32 StateTaskBeginIndex = State.TasksBegin;
				const int32 StateTaskEndIndex = State.TasksBegin + State.TasksNum;
				const bool bIsStateTask = AssetNodeIndex >= StateTaskBeginIndex && AssetNodeIndex < StateTaskEndIndex;
				if (bIsStateTask)
				{
					const int32 StateTaskIndex = AssetNodeIndex - State.TasksBegin;
					FTasksCompletionStatus StateTasksStatus = ActivePath.Frame->ActiveTasksStatus.GetStatus(State);
					StateTasksStatus.SetStatusWithPriority(StateTaskIndex, TaskStatus);
					bCompleted = StateTasksStatus.IsCompleted();
					bSucceed = true;
				}
			}

			if (bCompleted)
			{
				Exec.bHasPendingCompletedState = true;
				ScheduleNextTick(Owner.Get(), MetaStory.Get(), *Storage, UE::MetaStory::EMetaStoryTickReason::CompletedState);
			}
		}
	}

	return bSucceed;
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
bool TStateTreeStrongExecutionContext<bWithWriteAccess>::UpdateScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle, FMetaStoryScheduledTick ScheduledTick) const
{
	static_assert(bWithWriteAccess);

	if (IsValid())
	{
		FMetaStoryExecutionState& Exec = Storage->GetMutableExecutionState();
		if (Exec.UpdateScheduledTickRequest(Handle, ScheduledTick))
		{
			ScheduleNextTick(Owner.Get(), MetaStory.Get(), *Storage, UE::MetaStory::EMetaStoryTickReason::ScheduledTickRequest);
		}

		return true;
	}

	return false;
}

template <bool bWithWriteAccess>
UE::MetaStory::Async::FActivePathInfo TStateTreeStrongExecutionContext<bWithWriteAccess>::GetActivePathInfo() const
{
	if (!IsValidInstanceStorage())
	{
		return {};
	}

	UE::MetaStory::Async::FActivePathInfo Result;
	FMetaStoryExecutionState& Exec = Storage->GetMutableExecutionState();
	if (const int32 ActiveFrameIndex = Exec.IndexOfActiveFrame(FrameID); ActiveFrameIndex != INDEX_NONE)
	{
		Result.Frame = &Exec.ActiveFrames[ActiveFrameIndex];
		Result.ParentFrame = ActiveFrameIndex != 0 ? &Exec.ActiveFrames[ActiveFrameIndex - 1] : nullptr;
	}
	else if (TemporaryStorage)
	{
		using FFrameAndParent = UE::MetaStory::ExecutionContext::ITemporaryStorage::FFrameAndParent;
		const FFrameAndParent FrameInfo = TemporaryStorage->GetExecutionFrame(FrameID);
		Result.Frame = FrameInfo.Frame;
		if (FrameInfo.ParentFrameID.IsValid())
		{
			if (const int32 ActiveParentFrameIndex = Exec.IndexOfActiveFrame(FrameInfo.ParentFrameID); ActiveParentFrameIndex != INDEX_NONE)
			{
				Result.ParentFrame = &Exec.ActiveFrames[ActiveParentFrameIndex];
			}
			else
			{
				const FFrameAndParent ParentFrameInfo = TemporaryStorage->GetExecutionFrame(FrameInfo.ParentFrameID);
				Result.ParentFrame = ParentFrameInfo.Frame;
			}

			ensureMsgf(Result.Frame, TEXT("The frame ID exist in the frame holder. It should exist either in the active of the holder."));
		}
	}

	if (Result.Frame == nullptr)
	{
		return {};
	}

	if (NodeIndex.AsInt32() > Result.Frame->ActiveNodeIndex.AsInt32())
	{
		return {};
	}
	Result.NodeIndex = NodeIndex;

	if (StateID.IsValid())
	{
		Result.StateHandle = Result.Frame->ActiveStates.FindStateHandle(StateID);
		if (!Result.StateHandle.IsValid() && TemporaryStorage)
		{
			Result.StateHandle = TemporaryStorage->GetStateHandle(StateID).GetStateHandle();
		}

		if (!Result.StateHandle.IsValid())
		{
			return {};
		}
	}

	return Result;
}

template <bool bWithWriteAccess>
FMetaStoryDataView TStateTreeStrongExecutionContext<bWithWriteAccess>::GetInstanceDataPtrInternal() const
{
	using namespace UE::MetaStory;

	Async::FActivePathInfo ActivePath = GetActivePathInfo();
	if (ActivePath.IsValid())
	{
		const FMetaStoryNodeBase& Node = ActivePath.GetNode();
		FMetaStoryDataView InstanceDataView = InstanceData::GetDataViewOrTemporary(
			*Storage,
			/* SharedInstanceStorage */ nullptr,
			ActivePath.ParentFrame,
			*ActivePath.Frame,
			ActivePath.GetNode().InstanceDataHandle);

		return InstanceDataView;
	}

	return {};
}

template <bool bWithWriteAccess>
template<bool bWriteAccess UE_REQUIRES_DEFINITION(bWriteAccess)>
void TStateTreeStrongExecutionContext<bWithWriteAccess>::ScheduleNextTick(TNotNull<UObject*> Owner, TNotNull<const UMetaStory*> RootMetaStory, FMetaStoryInstanceStorage& Storage, UE::MetaStory::EMetaStoryTickReason Reason)
{
	static_assert(bWithWriteAccess);

	TInstancedStruct<FMetaStoryExecutionExtension>& ExecutionExtension = Storage.GetMutableExecutionState().ExecutionExtension;
	if (RootMetaStory->IsScheduledTickAllowed() && ExecutionExtension.IsValid())
	{
		ExecutionExtension.GetMutable().ScheduleNextTick(FMetaStoryExecutionExtension::FContextParameters(*Owner, *RootMetaStory, Storage), FMetaStoryExecutionExtension::FNextTickArguments(Reason));
	}
}

template struct TStateTreeStrongExecutionContext<false>;
template struct TStateTreeStrongExecutionContext<true>;

//@todo: remove all these instantiations once past 5.6 because of c++20 support
template bool METASTORYMODULE_API TStateTreeStrongExecutionContext<true>::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin) const;
template bool METASTORYMODULE_API TStateTreeStrongExecutionContext<true>::RequestTransition(FMetaStoryStateHandle TargetState, EMetaStoryTransitionPriority Priority, EMetaStorySelectionFallback Fallback) const;
template bool METASTORYMODULE_API TStateTreeStrongExecutionContext<true>::BroadcastDelegate(const FMetaStoryDelegateDispatcher& Dispatcher) const;
template bool METASTORYMODULE_API TStateTreeStrongExecutionContext<true>::BindDelegate(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate) const;
template bool METASTORYMODULE_API TStateTreeStrongExecutionContext<true>::UnbindDelegate(const FMetaStoryDelegateListener& Listener) const;
template bool METASTORYMODULE_API TStateTreeStrongExecutionContext<true>::FinishTask(EMetaStoryFinishTaskType FinishType) const;
template bool METASTORYMODULE_API TStateTreeStrongExecutionContext<true>::UpdateScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle, FMetaStoryScheduledTick ScheduledTick) const;
template void METASTORYMODULE_API TStateTreeStrongExecutionContext<true>::ScheduleNextTick(TNotNull<UObject*> Owner, TNotNull<const UMetaStory*> RootMetaStory, FMetaStoryInstanceStorage& Storage, UE::MetaStory::EMetaStoryTickReason Reason);

FMetaStoryWeakExecutionContext::FMetaStoryWeakExecutionContext(const FMetaStoryExecutionContext& Context)
	: Owner(Context.GetOwner())
	, MetaStory(Context.GetStateTree())
	, Storage(Context.GetMutableInstanceData()->GetWeakMutableStorage())
{
	if (const FMetaStoryExecutionFrame* Frame = Context.GetCurrentlyProcessedFrame())
	{
		FrameID = Frame->FrameID;
		StateID = Frame->ActiveStates.FindStateID(Context.GetCurrentlyProcessedState());
		NodeIndex = Context.GetCurrentlyProcessedNodeIndex();
	}
	TemporaryStorage = Context.GetCurrentlyProcessedTemporaryStorage();
}

bool FMetaStoryWeakExecutionContext::SendEvent(const FGameplayTag Tag, const FConstStructView Payload, const FName Origin) const
{
	return MakeStrongExecutionContext().SendEvent(Tag, Payload, Origin);
}

bool FMetaStoryWeakExecutionContext::RequestTransition(FMetaStoryStateHandle TargetState, EMetaStoryTransitionPriority Priority, const EMetaStorySelectionFallback Fallback) const
{
	return MakeStrongExecutionContext().RequestTransition(TargetState, Priority, Fallback);
}

bool FMetaStoryWeakExecutionContext::BroadcastDelegate(const FMetaStoryDelegateDispatcher& Dispatcher) const
{
	return MakeStrongExecutionContext().BroadcastDelegate(Dispatcher);
}

bool FMetaStoryWeakExecutionContext::BindDelegate(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate) const
{
	constexpr bool bWithWriteAccess = true;
	return MakeStrongExecutionContext().BindDelegate(Listener, Delegate);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
bool FMetaStoryWeakExecutionContext::BindDelegate(const FMetaStoryWeakTaskRef& Task, const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate) const
{
	// Deprecated. Use the version without the TaskRef.
	return BindDelegate(Listener, Delegate);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

bool FMetaStoryWeakExecutionContext::RemoveDelegateListener(const FMetaStoryDelegateListener& Listener) const
{
	return UnbindDelegate(Listener);
}

bool FMetaStoryWeakExecutionContext::UnbindDelegate(const FMetaStoryDelegateListener& Listener) const
{
	return MakeStrongExecutionContext().UnbindDelegate(Listener);
}

bool FMetaStoryWeakExecutionContext::FinishTask(EMetaStoryFinishTaskType FinishType) const
{
	return MakeStrongExecutionContext().FinishTask(FinishType);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
bool FMetaStoryWeakExecutionContext::FinishTask(const FMetaStoryWeakTaskRef& Task, EMetaStoryFinishTaskType FinishType) const
{
	return FinishTask(FinishType);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

bool FMetaStoryWeakExecutionContext::UpdateScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle, FMetaStoryScheduledTick ScheduledTick) const
{
	return MakeStrongExecutionContext().UpdateScheduledTickRequest(Handle, ScheduledTick);
}
