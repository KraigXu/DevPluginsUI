// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MetaStory.h"
#include "MetaStoryExecutionContextTypes.h"
#include "MetaStoryInstanceData.h"

#define UE_API METASTORYMODULE_API

namespace UE::MetaStory
{
struct FScheduledTickHandle;
}

struct FMetaStoryExecutionFrame;
struct FMetaStoryTaskBase;
struct FMetaStoryScheduledTick;
struct FMetaStoryWeakTaskRef;
struct FMetaStoryExecutionState;
struct FMetaStoryInstanceStorage;
struct FMetaStoryExecutionContext;
struct FMetaStoryWeakExecutionContext;

enum class EMetaStoryFinishTaskType : uint8;


namespace UE::MetaStory::Async
{
	struct FActivePathInfo
	{
		bool IsValid() const
		{
			return Frame != nullptr;
		}

		const FMetaStoryNodeBase& GetNode() const;

		FMetaStoryExecutionFrame* Frame = nullptr;
		FMetaStoryExecutionFrame* ParentFrame = nullptr;
		FMetaStoryStateHandle StateHandle;
		FMetaStoryIndex16 NodeIndex;
	};
}

/**
 * Execution context to interact with the state tree instance data asynchronously.
 * It should only be allocated on the stack.
 * You are responsible for making it thread-safe if needed.
 *		ThreadSafeAsyncCallback.AddLambda(
 *			[MyTag, WeakContext = Context.MakeWeakExecutionContext()]()
 *			{
 *				TStateTreeStrongExecutionContext<true> StrongContext = WeakContext.CreateStrongContext();
 *				if (StrongContext.SendEvent())
 *				{
 *					...
 *				}
 *			});
 */
template<bool bWithWriteAccess>
struct TStateTreeStrongExecutionContext
{
	TStateTreeStrongExecutionContext() = default;
	UE_API explicit TStateTreeStrongExecutionContext(const FMetaStoryWeakExecutionContext& WeakContext);
	UE_API ~TStateTreeStrongExecutionContext();

	TStateTreeStrongExecutionContext(const TStateTreeStrongExecutionContext& Other) = delete;
	TStateTreeStrongExecutionContext& operator=(const TStateTreeStrongExecutionContext& Other) = delete;

	/** @return The owner of the context */
	TStrongObjectPtr<UObject> GetOwner() const
	{
		return Owner;
	}

	/** @return the MetaStory asset in use. */
	TStrongObjectPtr<const UMetaStory> GetStateTree() const
	{
		return MetaStory;
	}

	/** @return the Instance Storage. */
	TSharedPtr<const FMetaStoryInstanceStorage> GetStorage() const
	{
		return Storage;
	}

	//@todo: Replace UE_REQUIRES with requires and UE_API once past 5.6 for c++20 support

	/**
	 * Sends event for the MetaStory.
	 * @return false if the context is not valid or the event could not be sent.
	 */
	template<bool bWriteAccess = bWithWriteAccess UE_REQUIRES(bWriteAccess)>
	bool SendEvent(const FGameplayTag Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName()) const;

	/**
	 * Requests transition to a state.
	 * If called during transition processing (e.g. from FMetaStoryTaskBase::TriggerTransitions()) the transition
	 * is attempted to be activated immediately (it can fail e.g. because of preconditions on a target state).
	 * If called outside the transition handling, the request is buffered and handled at the beginning of next transition processing.
	 * @param TargetState The state to transition to.
	 * @param Priority The priority of the transition.
	 * @param Fallback of the transition if it fails to select the target state.
	 * @return false if the context is not valid or doesn't have a valid frame anymore or the request failed.
	 */
	template<bool bWriteAccess = bWithWriteAccess UE_REQUIRES(bWriteAccess)>
	bool RequestTransition(FMetaStoryStateHandle TargetState, EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::Normal, EMetaStorySelectionFallback Fallback = EMetaStorySelectionFallback::None) const;

	/**
	 * Broadcasts the delegate.
	 * It executes bound delegates immediately and triggers bound transitions (when transitions are evaluated).
	 * @return false if the context is not valid or doesn't have a valid frame anymore or the broadcast failed.
	 */
	template<bool bWriteAccess = bWithWriteAccess UE_REQUIRES(bWriteAccess)>
	bool BroadcastDelegate(const FMetaStoryDelegateDispatcher& Dispatcher) const;

	/**
	 * Registers the delegate to the listener.
	 * If the listener was previously registered, then unregister it first before registering it again with the new delegate callback.
	 * The listener is bound to a dispatcher in the editor.
	 * @return false if the context is not valid or doesn't have a valid frame anymore or the bind failed.
	 */
	template<bool bWriteAccess = bWithWriteAccess UE_REQUIRES(bWriteAccess)>
	bool BindDelegate(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate) const;

	/**
	 * Unregisters the callback bound to the listener.
	 * @return false if the context is not valid or the unbind failed.
	 */
	template<bool bWriteAccess = bWithWriteAccess UE_REQUIRES(bWriteAccess)>
	bool UnbindDelegate(const FMetaStoryDelegateListener& Listener) const;

	/**
	 * Finishes a task.
	 * If called during tick processing, then the state completes immediately.
	 * If called outside of the tick processing, then the request is buffered and handled on the next tick.
	 * @return false if the context is not valid or doesn't have a valid frame anymore or finish task failed.
	 */
	template<bool bWriteAccess = bWithWriteAccess UE_REQUIRES(bWriteAccess)>
	bool FinishTask(EMetaStoryFinishTaskType FinishType) const;

	/**
	 * Updates the scheduled tick of a previous request.
	 * @return false if the context is not valid.
	 */
	template<bool bWriteAccess = bWithWriteAccess UE_REQUIRES(bWriteAccess)>
	bool UpdateScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle, FMetaStoryScheduledTick ScheduledTick) const;

	/**
	 * Get the Instance Data of recorded node. Only callable on lvalue because it would have lost the access track.
	 * @return nullptr if the frame or state containing the node is no longer active.
	 */
	template<typename T>
	std::conditional_t<bWithWriteAccess, T*, const T*> GetInstanceDataPtr() const&
	{
		FMetaStoryDataView DataView = GetInstanceDataPtrInternal();
		if (DataView.IsValid() && ensure(DataView.GetStruct() == T::StaticStruct()))
		{
			return static_cast<T*>(DataView.GetMutableMemory());
		}

		return nullptr;
	}

	template<typename T>
	std::conditional_t<bWithWriteAccess, T*, const T*> GetInstanceDataPtr() const&& = delete;

	/**
	 * Get the info of active frame, state and node this Context is based on
	 * @return an invalid FActivePathInfo if the frame or state containing the node is no longer active.
	 */
	UE_API UE::MetaStory::Async::FActivePathInfo GetActivePathInfo() const;

	/**
	 * Checks if the context is valid.
	 * Validity: Pinned Members are valid AND (WeakContext is created outside the ExecContext loop OR the recorded frame and state are still active)    
	 * @return false if the context is not valid.
	 */
	bool IsValid() const
	{
		// skipped checking Pinned members, because the expression will be false if those members are not valid in ctor anyway.
		return IsValidInstanceStorage() && (!FrameID.IsValid() || GetActivePathInfo().IsValid());
	}

private:
	bool IsValidInstanceStorage() const
	{
		return Owner && MetaStory && Storage;
	}

	UE_API FMetaStoryDataView GetInstanceDataPtrInternal() const;

	template<bool bWriteAccess = bWithWriteAccess UE_REQUIRES(bWriteAccess)>
	static void ScheduleNextTick(TNotNull<UObject*> Owner, TNotNull<const UMetaStory*> RootStateTree, FMetaStoryInstanceStorage& Storage, UE::MetaStory::EMetaStoryTickReason Reason);

	TStrongObjectPtr<UObject> Owner;
	TStrongObjectPtr<const UMetaStory> MetaStory;
	TSharedPtr<FMetaStoryInstanceStorage> Storage;
	TSharedPtr<UE::MetaStory::ExecutionContext::ITemporaryStorage> TemporaryStorage;

	UE::MetaStory::FActiveFrameID FrameID;
	UE::MetaStory::FActiveStateID StateID;
	FMetaStoryIndex16 NodeIndex;
	uint8 bAccessAcquired : 1 = false;

	friend struct FMetaStoryPropertyRef;
};

using FMetaStoryStrongExecutionContext = TStateTreeStrongExecutionContext<true>;
using FMetaStoryStrongReadOnlyExecutionContext = TStateTreeStrongExecutionContext<false>;

/**
 * Execution context that can be saved/copied and used asynchronously. 
 * You are responsible for making it thread-safe if needed.
 * The context is valid if the state (or global context) from which it was created is still
 * active. The owner, state tree, and storage also need to be valid. 
 *
 *		ThreadSafeAsyncCallback.AddLambda(
 *			[MyTag, WeakContext = Context.MakeWeakExecutionContext]()
 *			{
 *				if (WeakContext.SendEvent(MyTag))
 *				{
 *					...
 *				}
 *			});
 */
struct FMetaStoryWeakExecutionContext
{
	template<bool bRequireWriteAccess>
	friend struct TStateTreeStrongExecutionContext;

public:
	FMetaStoryWeakExecutionContext() = default;
	UE_API explicit FMetaStoryWeakExecutionContext(const FMetaStoryExecutionContext& Context);

public:
	/** @return The owner of the context */
	TStrongObjectPtr<UObject> GetOwner() const
	{
		return Owner.Pin();
	}

	/** @return the MetaStory asset in use. */
	TStrongObjectPtr<const UMetaStory> GetStateTree() const
	{
		return MetaStory.Pin();
	}

	[[nodiscard]] FMetaStoryStrongReadOnlyExecutionContext MakeStrongReadOnlyExecutionContext() const
	{
		return FMetaStoryStrongReadOnlyExecutionContext(*this);
	}

	[[nodiscard]] FMetaStoryStrongExecutionContext MakeStrongExecutionContext() const
	{
		return FMetaStoryStrongExecutionContext(*this);
	}

	/**
	 * Sends event for the MetaStory.
	 * @return false if the context is not valid or the event could not be sent.
	 */
	UE_API bool SendEvent(const FGameplayTag Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName()) const;

	/**
	 * Requests transition to a state.
	 * If called during transition processing (e.g. from FMetaStoryTaskBase::TriggerTransitions()) the transition
	 * is attempted to be activated immediately (it can fail e.g. because of preconditions on a target state).
	 * If called outside the transition handling, the request is buffered and handled at the beginning of next transition processing.
	 * @param TargetState The state to transition to.
	 * @param Priority The priority of the transition.
	 * @param Fallback of the transition if it fails to select the target state.
	 * @return false if the context is not valid or doesn't have a valid frame anymore or the request failed.
	 */
	UE_API bool RequestTransition(FMetaStoryStateHandle TargetState, EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::Normal, EMetaStorySelectionFallback Fallback = EMetaStorySelectionFallback::None) const;

	/**
	 * Broadcasts the delegate.
	 * It executes bound delegates immediately and triggers bound transitions (when transitions are evaluated).
	 * @return false if the context is not valid or doesn't have a valid frame anymore or the broadcast failed.
	 */
	UE_API bool BroadcastDelegate(const FMetaStoryDelegateDispatcher& Dispatcher) const;

	/**
	 * Registers the delegate to the listener.
	 * If the listener was previously registered, then unregister it first before registering it again with the new delegate callback.
	 * The listener is bound to a dispatcher in the editor.
	 * @return false if the context is not valid or doesn't have a valid frame anymore or the bind failed.
	 */
	UE_API bool BindDelegate(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate) const;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		UE_DEPRECATED(5.6, "Use the version without FMetaStoryWeakTaskRef")
		UE_API bool BindDelegate(const FMetaStoryWeakTaskRef& Task, const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate) const;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.6, "Use UnbindDelegate")
	/** Removes Delegate Listener. */
	UE_API bool RemoveDelegateListener(const FMetaStoryDelegateListener& Listener) const;

	/**
	 * Unregisters the callback bound to the listener.
	 * @return false if the context is not valid or the unbind failed.
	 */
	UE_API bool UnbindDelegate(const FMetaStoryDelegateListener& Listener) const;

	/**
	 * Finishes a task.
	 * If called during tick processing, then the state completes immediately.
	 * If called outside of the tick processing, then the request is buffered and handled on the next tick.
	 * @return false if the context is not valid or doesn't have a valid frame anymore or finish task failed.
	 */
	UE_API bool FinishTask(EMetaStoryFinishTaskType FinishType) const;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		UE_DEPRECATED(5.6, "Use the version without FMetaStoryWeakTaskRef.")
		UE_API bool FinishTask(const FMetaStoryWeakTaskRef& Task, EMetaStoryFinishTaskType FinishType) const;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Updates the scheduled tick of a previous request.
	 * @return false if the context is not valid.
	 */
	UE_API bool UpdateScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle, FMetaStoryScheduledTick ScheduledTick) const;

private:
	TWeakObjectPtr<UObject> Owner;
	TWeakObjectPtr<const UMetaStory> MetaStory;
	TWeakPtr<FMetaStoryInstanceStorage> Storage;
	TWeakPtr<UE::MetaStory::ExecutionContext::ITemporaryStorage> TemporaryStorage;

	UE::MetaStory::FActiveFrameID FrameID;
	UE::MetaStory::FActiveStateID StateID;
	FMetaStoryIndex16 NodeIndex;
};

#undef UE_API
