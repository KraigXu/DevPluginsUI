// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStory.h"
#include "MetaStoryExecutionExtension.h"
#include "MetaStoryExecutionTypes.h"
#include "MetaStoryExecutionContextTypes.h"
#include "MetaStoryNodeBase.h"
#include "MetaStoryNodeRef.h"
#include "MetaStoryReference.h"
#include "Debugger/MetaStoryTrace.h"
#include "Experimental/ConcurrentLinearAllocator.h"
#include "Templates/IsInvocable.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MetaStoryAsyncExecutionContext.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6

#define UE_API METASTORYMODULE_API

struct FGameplayTag;
struct FInstancedPropertyBag;
struct FMetaStoryExecutionContext;
struct FMetaStoryEvaluatorBase;
struct FMetaStoryTaskBase;
struct FMetaStoryConditionBase;
struct FMetaStoryDelegateDispatcher;
struct FMetaStoryEvent;
struct FMetaStoryMinimalExecutionContext;
struct FMetaStoryTransitionRequest;
struct FMetaStoryInstanceDebugId;
struct FMetaStoryWeakExecutionContext;

namespace UE::MetaStory::InstanceData
{
	struct FEvaluationScopeInstanceContainer;
}

namespace UE::MetaStory::ExecutionContext
{
	UE_API bool MarkDelegateAsBroadcasted(FMetaStoryDelegateDispatcher Dispatcher, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryInstanceStorage& Storage);
	UE_API EMetaStoryRunStatus GetPriorityRunStatus(EMetaStoryRunStatus A, EMetaStoryRunStatus B);
	UE_API UE::MetaStory::ETaskCompletionStatus CastToTaskStatus(EMetaStoryFinishTaskType FinishTask);
	UE_API EMetaStoryRunStatus CastToRunStatus(EMetaStoryFinishTaskType FinishTask);
	UE_API UE::MetaStory::ETaskCompletionStatus CastToTaskStatus(EMetaStoryRunStatus InStatus);
	UE_API EMetaStoryRunStatus CastToRunStatus(UE::MetaStory::ETaskCompletionStatus InStatus);
}

/**
 * Delegate used by the execution context to collect external data views for a given MetaStory asset.
 * The caller is expected to iterate over the ExternalDataDescs array, find the matching external data,
 * and store it in the OutDataViews at the same index:
 *
 *	for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
 *	{
 *		const FMetaStoryExternalDataDesc& Desc = ExternalDataDescs[Index];
 *		// Find data requested by Desc
 *		OutDataViews[Index] = ...;
 *	}
 */
DECLARE_DELEGATE_RetVal_FourParams(bool, FOnCollectMetaStoryExternalData, const FMetaStoryExecutionContext& /*Context*/, const UMetaStory* /*MetaStory*/, TArrayView<const FMetaStoryExternalDataDesc> /*ExternalDataDescs*/, TArrayView<FMetaStoryDataView> /*OutDataViews*/);

/**
 * Read-only execution context to interact with the MetaStory instance data. Only const and read accesses are available.
 * Multiple FMetaStoryReadOnlyExecutionContext can coexist on different threads as long no other (minimal, weak, regular) execution context exists.
 * The user is responsible for preventing invalid multi-threaded access.
 */
struct FMetaStoryReadOnlyExecutionContext
{
public:
	UE_API explicit FMetaStoryReadOnlyExecutionContext(TNotNull<UObject*> Owner, TNotNull<const UMetaStory*> MetaStory, FMetaStoryInstanceData& InInstanceData);
	UE_API explicit FMetaStoryReadOnlyExecutionContext(TNotNull<UObject*> Owner, TNotNull<const UMetaStory*> MetaStory, FMetaStoryInstanceStorage& Storage);
	UE_API virtual ~FMetaStoryReadOnlyExecutionContext();

private:
	FMetaStoryReadOnlyExecutionContext(const FMetaStoryReadOnlyExecutionContext&) = delete;
	FMetaStoryReadOnlyExecutionContext& operator=(const FMetaStoryReadOnlyExecutionContext&) = delete;

public:
	/**
	 * Indicates if the instance is valid and would be able to run the instance of the associated MetaStory asset with a regular execution context.
	 * @return True if the MetaStory asset assigned to the execution context is valid
	 * (i.e., not empty) and successfully initialized (i.e., linked and all bindings resolved).
	 */
	bool IsValid() const
	{
		return RootMetaStory.IsReadyToRun();
	}

	/** @return The owner of the context */
	TNotNull<UObject*> GetOwner() const
	{
		return &Owner;
	}

	/** @return The world of the owner or nullptr if the owner is not set. */
	UWorld* GetWorld() const
	{
		return Owner.GetWorld();
	}

	/** @return the MetaStory asset in use by the instance. It is the root asset. */
	TNotNull<const UMetaStory*> GetMetaStory() const
	{
		return &RootMetaStory;
	}

	/** @return true if there is a pending event with specified tag. */
	bool HasEventToProcess(const FGameplayTag Tag) const
	{
		return Storage.GetEventQueue().GetEventsView().ContainsByPredicate([Tag](const FMetaStorySharedEvent& Event)
			{
				check(Event.IsValid());
				return Event->Tag.MatchesTag(Tag);
			});
	}

	/** @return Pointer to a State or null if state not found */
	const FMetaStoryCompactState* GetStateFromHandle(const FMetaStoryStateHandle StateHandle) const
	{
		return RootMetaStory.GetStateFromHandle(StateHandle);
	}

	/** @return the delta time for the next execution context tick. */
	UE_API FMetaStoryScheduledTick GetNextScheduledTick() const;

	/** @return the tree run status. */
	UE_API EMetaStoryRunStatus GetMetaStoryRunStatus() const;

	/** @return the status of the last tick function */
	UE_API EMetaStoryRunStatus GetLastTickStatus() const;

	/** @return reference to the list of currently active frames and states. */
	UE_API TConstArrayView<FMetaStoryExecutionFrame> GetActiveFrames() const;

	/** @return the name of the active state. */
	UE_API FString GetActiveStateName() const;

	/** @return the names of all the active state. */
	UE_API TArray<FName> GetActiveStateNames() const;

#if WITH_GAMEPLAY_DEBUGGER
	/** @return Debug string describing the current state of the execution */
	UE_API FString GetDebugInfoString() const;
#endif // WITH_GAMEPLAY_DEBUGGER

#if WITH_METASTORY_DEBUG
	UE_API int32 GetStateChangeCount() const;

	UE_API void DebugPrintInternalLayout();
#endif

#if WITH_METASTORY_TRACE
	/** A unique Id used by debugging tools to identify the instance. */
	UE_API FMetaStoryInstanceDebugId GetInstanceDebugId() const;

	/** @return Short description of the instance for debug/trace purposes */
	FString GetInstanceDebugDescription() const
	{
		return GetInstanceDescriptionInternal();
	}

	void SetOuterTraceId(const uint64 Id) const
	{
		OuterTraceId = Id;
	}

	uint64 GetOuterTraceId() const
	{
		return OuterTraceId;
	}

	void SetNodeCustomDebugTraceData(UE::MetaStoryTrace::FNodeCustomDebugData&& DebugData) const
	{
		ensureMsgf(!NodeCustomDebugTraceData .IsSet()
			, TEXT("CustomData is not expected to be already set."
				" This might indicate nested calls to SetNodeCustomDebugTraceData without calls to a trace macro"));
		NodeCustomDebugTraceData = MoveTemp(DebugData);
	}

	UE::MetaStoryTrace::FNodeCustomDebugData StealNodeCustomDebugTraceData() const
	{
		return MoveTemp(NodeCustomDebugTraceData);
	}
#else
	void SetOuterTraceId(const uint64 Id) const
	{
	}

	uint64 GetOuterTraceId() const
	{
		return 0;
	}
#endif // WITH_METASTORY_TRACE

protected:
	/** @return Description used as prefix by METASTORY_LOG and METASTORY_CLOG, Owner name by default. */
	UE_API FString GetInstanceDescriptionInternal() const;

	/** Owner of the instance data. */
	UObject& Owner;

	/** The MetaStory asset the context is initialized for */
	const UMetaStory& RootMetaStory;

	/** Data storage of the instance data. */
	FMetaStoryInstanceStorage& Storage;

#if WITH_METASTORY_TRACE
	mutable UE::MetaStoryTrace::FNodeCustomDebugData NodeCustomDebugTraceData;
	mutable uint64 OuterTraceId = 0;
#endif
};

/**
 * Minimal execution context to interact with the MetaStory instance data.
 * A regular execution context requires the context data and external data to be valid to execute all possible operations.
 * The minimal execution context doesn't requires those data but supports only a subset of operations.
 */
struct FMetaStoryMinimalExecutionContext : public FMetaStoryReadOnlyExecutionContext
{
public:
	UE_DEPRECATED(5.6, "Use FMetaStoryMinimalExecutionContext with the not null pointer.")
	UE_API explicit FMetaStoryMinimalExecutionContext(UObject& Owner, const UMetaStory& MetaStory, FMetaStoryInstanceData& InInstanceData);
	UE_DEPRECATED(5.6, "Use FMetaStoryMinimalExecutionContext with the not null pointer.")
	UE_API explicit FMetaStoryMinimalExecutionContext(UObject& Owner, const UMetaStory& MetaStory, FMetaStoryInstanceStorage& Storage);
	UE_API explicit FMetaStoryMinimalExecutionContext(TNotNull<UObject*> Owner, TNotNull<const UMetaStory*> MetaStory, FMetaStoryInstanceData& InInstanceData);
	UE_API explicit FMetaStoryMinimalExecutionContext(TNotNull<UObject*> Owner, TNotNull<const UMetaStory*> MetaStory, FMetaStoryInstanceStorage& Storage);
	UE_API virtual ~FMetaStoryMinimalExecutionContext();

private:
	FMetaStoryMinimalExecutionContext(const FMetaStoryMinimalExecutionContext&) = delete;
	FMetaStoryMinimalExecutionContext& operator=(const FMetaStoryMinimalExecutionContext&) = delete;

public:
	/** 
	 * Adds a scheduled tick request.
	 * The result of GetNextScheduledTick is affected by the request.
	 * This allows a specific task to control when the tree ticks.
	 * @note A request with a higher priority will supersede all other requests.
	 * ex: Task A request a custom time of 1FPS and Task B request a custom time of 2FPS. Both tasks will tick at 1FPS.
	 */
	UE_API UE::MetaStory::FScheduledTickHandle AddScheduledTickRequest(FMetaStoryScheduledTick ScheduledTick);

	/** Updates the scheduled tick of a previous request. */
	UE_API void UpdateScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle, FMetaStoryScheduledTick ScheduledTick);

	/** Removes a scheduled tick request. */
	UE_API void RemoveScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle);

	/** Sends event for the MetaStory. */
	UE_API void SendEvent(const FGameplayTag Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName());

protected:
	/**
	 * Get ExecutionExtension from InstanceStorage for less indirections
	 * The user is responsible for validity of the result, re-call the getter when needed
	 * @return ExecutionExtension from InstanceStorage. could be null if Extension hasn't been set or Instance Storage has been reset.
	 */
	UE_API FMetaStoryExecutionExtension* GetMutableExecutionExtension() const;

	/** Informs the owner when the instance of the tree must woke up from a scheduled tick sleep. */
	UE_API void ScheduleNextTick(UE::MetaStory::EMetaStoryTickReason Reason = UE::MetaStory::EMetaStoryTickReason::None);

protected:
	/** The context is processing the tree. We do not need to inform the owner that something changed. */
	bool bAllowedToScheduleNextTick = true;
};

/**
 * MetaStory Execution Context is a helper that is used to update MetaStory instance data.
 *
 * The context is meant to be temporary, you should not store a context across multiple frames.
 *
 * The owner is used as the owner of the instantiated UObjects in the instance data and logging,
 * it should have same or greater lifetime as the InstanceData. 
 *
 * In common case you can use the constructor to initialize the context, and us a helper struct
 * to set up the context data and external data getter:
 *
 *		FMetaStoryExecutionContext Context(*GetOwner(), *MetaStoryRef.GetMetaStory(), InstanceData);
 *		if (SetContextRequirements(Context))
 *		{
 *			Context.Tick(DeltaTime);
 * 		}
 *
 * 
 *		bool UMyComponent::SetContextRequirements(FMetaStoryExecutionContext& Context)
 *		{
 *			if (!Context.IsValid())
 *			{
 *				return false;
 *			}
 *			// Setup context data
 *			Context.SetContextDataByName(...);
 *			...
 *
 *			Context.SetCollectExternalDataCallback(FOnCollectMetaStoryExternalData::CreateUObject(this, &UMyComponent::CollectExternalData);
 *
 *			return Context.AreContextDataViewsValid();
 *		}
 *
 *		bool UMyComponent::CollectExternalData(const FMetaStoryExecutionContext& Context, const UMetaStory* MetaStory, TArrayView<const FMetaStoryExternalDataDesc> ExternalDataDescs, TArrayView<FMetaStoryDataView> OutDataViews)
 *		{
 *			...
 *			for (int32 Index = 0; Index < ExternalDataDescs.Num(); Index++)
 *			{
 *				const FMetaStoryExternalDataDesc& Desc = ExternalDataDescs[Index];
 *				if (Desc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
 *				{
 *					UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(Desc.Struct.Get())));
 *					OutDataViews[Index] = FMetaStoryDataView(Subsystem);
 *				}
 *				...
 *			}
 *			return true;
 *		}
 *
 * In this example the SetContextRequirements() method is used to set the context defined in the schema,
 * and the delegate FOnCollectMetaStoryExternalData is used to query the external data required by the tasks and conditions.
 *
 * In case the MetaStory links to other MetaStory assets, the collect external data might get called
 * multiple times, once for each asset.
 */
struct FMetaStoryExecutionContext : public FMetaStoryMinimalExecutionContext
{
public:
	UE_API FMetaStoryExecutionContext(UObject& InOwner, const UMetaStory& InMetaStory, FMetaStoryInstanceData& InInstanceData, const FOnCollectMetaStoryExternalData& CollectExternalDataCallback = {}, const EMetaStoryRecordTransitions RecordTransitions = EMetaStoryRecordTransitions::No);
	/** Construct an execution context from a parent context and another tree. Useful to run a subtree from the parent context with the same schema. */
	UE_API FMetaStoryExecutionContext(const FMetaStoryExecutionContext& InContextToCopy, const UMetaStory& InMetaStory, FMetaStoryInstanceData& InInstanceData);
	UE_API FMetaStoryExecutionContext(TNotNull<UObject*> Owner, TNotNull<const UMetaStory*> MetaStory, FMetaStoryInstanceData& InInstanceData, const FOnCollectMetaStoryExternalData& CollectExternalDataCallback = {}, const EMetaStoryRecordTransitions RecordTransitions = EMetaStoryRecordTransitions::No);
	/** Construct an execution context from a parent context and another tree. Useful to run a subtree from the parent context with the same schema. */
	UE_API FMetaStoryExecutionContext(const FMetaStoryExecutionContext& InContextToCopy, TNotNull<const UMetaStory*> MetaStory, FMetaStoryInstanceData& InInstanceData);
	UE_API virtual ~FMetaStoryExecutionContext();

private:
	FMetaStoryExecutionContext(const FMetaStoryExecutionContext&) = delete;
	FMetaStoryExecutionContext& operator=(const FMetaStoryExecutionContext&) = delete;

public:
	/** Sets callback used to collect external data views during MetaStory execution. */
	UE_API void SetCollectExternalDataCallback(const FOnCollectMetaStoryExternalData& Callback);

	UE_DEPRECATED(5.6, "Use SetLinkedMetaStoryOverrides that creates a copy.")
	/**
	 * Overrides for linked MetaStorys. This table is used to override MetaStory references on linked states.
	 * If a linked state's tag is exact match of the tag specified on the table, the reference from the table is used instead.
	 */
	UE_API void SetLinkedMetaStoryOverrides(const FMetaStoryReferenceOverrides* InLinkedMetaStoryOverrides);

	/**
	 * Overrides for linked MetaStorys. This table is used to override MetaStory references on linked states.
	 * If a linked state's tag is exact match of the tag specified on the table, the reference from the table is used instead.
	 */
	UE_API void SetLinkedMetaStoryOverrides(FMetaStoryReferenceOverrides InLinkedMetaStoryOverrides);

	/** @return the first MetaStory reference set by SetLinkedMetaStoryOverrides that matches the StateTag. Or null if not found. */
	UE_API const FMetaStoryReference* GetLinkedMetaStoryOverrideForTag(const FGameplayTag StateTag) const;

	/** Structure to-be-populated and set for any MetaStory using any EMetaStoryDataSourceType::ExternalGlobalParameterData bindings */
	struct FExternalGlobalParameters
	{
		/* Add memory mapping, this expects InParameterMemory to resolve correctly for the SourceLeafProperty and SourceIndirection */
		UE_API bool Add(const FPropertyBindingCopyInfo& Copy, uint8* InParameterMemory);
		UE_API uint8* Find(const FPropertyBindingCopyInfo& Copy) const;
		UE_API void Reset();
	private:
		TMap<uint32, uint8*> Mappings;
	};
	UE_API void SetExternalGlobalParameters(const FExternalGlobalParameters* Parameters);

	/** @return const references to the instance data in use, or nullptr if the context is not valid. */
	const FMetaStoryInstanceData* GetInstanceData() const
	{
		return &InstanceData;
	}

	/** @return mutable references to the instance data in use, or nullptr if the context is not valid. */
	FMetaStoryInstanceData* GetMutableInstanceData() const
	{
		return &InstanceData;
	}

	/** @return mutable references to the instance data in use. */
	const FMetaStoryEventQueue& GetEventQueue() const
	{
		return InstanceData.GetEventQueue();
	}

	/** @return mutable references to the instance data in use. */
	FMetaStoryEventQueue& GetMutableEventQueue() const
	{
		return InstanceData.GetMutableEventQueue();
	}

	/** @return a weak context to interact with the MetaStory instance data that can be stored for later uses. */
	UE_API FMetaStoryWeakExecutionContext MakeWeakExecutionContext() const;
	
	/**
	 * @return a weak reference for a task that can be stored for later uses.
	 * @note similar to GetInstanceData, the node needs to be the current processing node.
	 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "FMetaStoryWeakTaskRef is no longer used.")
	UE_API FMetaStoryWeakTaskRef MakeWeakTaskRef(const FMetaStoryTaskBase& Node) const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * @return a weak reference for a task that can be stored for later uses.
	 * @note similar to GetInstanceData, the instance data needs to be the current processing node.
	 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	template <typename T>
	UE_DEPRECATED(5.6, "FMetaStoryWeakTaskRef is no longer used.")
	FMetaStoryWeakTaskRef MakeWeakTaskRefFromInstanceData(const T& InInstanceData) const
	{
		check(&CurrentNodeInstanceData.template GetMutable<T>() == &InInstanceData);
		return MakeWeakTaskRefInternal();
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * @return a weak reference for a task that can be stored for later uses.
	 * @note similar to GetInstanceData, the instance data needs to be the current processing node.
	 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	template <typename T>
	UE_DEPRECATED(5.6, "FMetaStoryWeakTaskRef is no longer used.")
	FMetaStoryWeakTaskRef MakeWeakTaskRefFromInstanceDataPtr(const T* InInstanceData) const
	{
		check(CurrentNodeInstanceData.template GetMutablePtr<T>() == InInstanceData);
		return MakeWeakTaskRefInternal();
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	struct FStartParameters
	{
		/** Optional override of parameters initial values. */
		const FInstancedPropertyBag* GlobalParameters = nullptr;

		/** Optional extension for the execution context. */
		TInstancedStruct<FMetaStoryExecutionExtension> ExecutionExtension;

		/** Optional event queue from another instance data. Marks the event queue not owned. */
		const TSharedPtr<FMetaStoryEventQueue> SharedEventQueue;

		/** Optional override of initial seed for RandomStream. By default FPlatformTime::Cycles() will be used. */
		TOptional<int32> RandomSeed;
	};

	/**
	 * Start executing.
	 * @param InitialParameters Optional override of parameters initial values
	 * @param RandomSeed Optional override of initial seed for RandomStream. By default FPlatformTime::Cycles() will be used.
	 * @return Tree execution status after the start.
	 */
	UE_API EMetaStoryRunStatus Start(const FInstancedPropertyBag* InitialParameters = nullptr, int32 RandomSeed = -1);
	
	/**
	 * Start executing.
	 * @return Tree execution status after the start.
	 */
	UE_API EMetaStoryRunStatus Start(FStartParameters Parameter);
	
	/**
	 * Stop executing if the tree is running.
	 * @param CompletionStatus Status (and terminal state) reported in the transition when the tree is stopped.
	 * @return Tree execution status at stop, can be CompletionStatus, or earlier status if the tree is not running. 
	 */
	UE_API EMetaStoryRunStatus Stop(const EMetaStoryRunStatus CompletionStatus = EMetaStoryRunStatus::Stopped);

	/**
	 * Tick the MetaStory logic, updates the tasks and triggers transitions.
	 * @param DeltaTime time to advance the logic.
	 * @returns tree run status after the tick.
	 */
	UE_API EMetaStoryRunStatus Tick(const float DeltaTime);

	/**
	 * Tick the MetaStory logic partially, updates the tasks.
	 * For full update TickTriggerTransitions() should be called after.
	 * @param DeltaTime time to advance the logic.
	 * @returns tree run status after the partial tick.
	 */
	UE_API EMetaStoryRunStatus TickUpdateTasks(const float DeltaTime);
	
	/**
	 * Tick the MetaStory logic partially, triggers the transitions.
	 * For full update TickUpdateTasks() should be called before.
	 * @returns tree run status after the partial tick.
	 */
	UE_API EMetaStoryRunStatus TickTriggerTransitions();

	/**
	 * Broadcasts the delegate.
	 * It executes bound delegates immediately and triggers bound transitions (when transitions are evaluated).
	 */
	UE_API void BroadcastDelegate(const FMetaStoryDelegateDispatcher& Dispatcher);

	UE_DEPRECATED(5.6, "Use BindDelegate")
	/** Adds delegate listener. */
	UE_API bool AddDelegateListener(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate);

	/**
	 * Registers the delegate to the listener.
	 * If the listener was previously registered, then unregister it first before registering it again with the new delegate callback.
	 * The listener is bound to a dispatcher in the editor.
	 */
	UE_API void BindDelegate(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate);

	UE_DEPRECATED(5.6, "Use UnbindDelegate")
	/** Removes delegate listener. */
	UE_API void RemoveDelegateListener(const FMetaStoryDelegateListener& Listener);

	/** Unregisters the callback bound to the listener. */
	UE_API void UnbindDelegate(const FMetaStoryDelegateListener& Listener);

	/**
	 * Iterates over all events.
	 * @param Function a lambda which takes const FMetaStorySharedEvent& Event, and returns EMetaStoryLoopEvents.
	 */
	template<typename TFunc>
	typename TEnableIf<TIsInvocable<TFunc, FMetaStorySharedEvent>::Value, void>::Type ForEachEvent(TFunc&& Function) const
	{
		if (!EventQueue)
		{
			return;
		}
		EventQueue->ForEachEvent(Function);
	}

	/**
	 * Iterates over all events.
	 * @param Function a lambda which takes const FMetaStoryEvent& Event, and returns EMetaStoryLoopEvents.
	 * Less preferable than FMetaStorySharedEvent version.
	 */
	template<typename TFunc>
	typename TEnableIf<TIsInvocable<TFunc, FMetaStoryEvent>::Value, void>::Type ForEachEvent(TFunc&& Function) const
	{
		if (!EventQueue)
		{
			return;
		}
		EventQueue->ForEachEvent([Function](const FMetaStorySharedEvent& Event)
		{
			return Function(*Event);
		});
	}

	/** @return events to process this tick. */
	TArrayView<FMetaStorySharedEvent> GetMutableEventsToProcessView()
	{
		return EventQueue ? EventQueue->GetMutableEventsView() : TArrayView<FMetaStorySharedEvent>();
	}

	/** @return events to process this tick. */
	TConstArrayView<FMetaStorySharedEvent> GetEventsToProcessView() const
	{
		return EventQueue ? EventQueue->GetMutableEventsView() : TArrayView<FMetaStorySharedEvent>();
	}

	/** Consumes and removes the specified event from the event queue. */
	UE_API void ConsumeEvent(const FMetaStorySharedEvent& Event);

	FMetaStoryIndex16 GetCurrentlyProcessedNodeIndex() const
	{
		return FMetaStoryIndex16(CurrentNodeIndex);
	}

	FMetaStoryDataHandle GetCurrentlyProcessedNodeInstanceData() const
	{
		return CurrentNodeDataHandle;
	}

	UE_DEPRECATED(5.6, "Use GetCurrentlyProcessedNodeInstanceData() instead.")
	/** @return the currently processed node if applicable. */
	FMetaStoryDataHandle GetCurrentlyProcessedNode() const
	{
		return CurrentNodeDataHandle;
	}

	/** @return the currently processed state if applicable. */
	FMetaStoryStateHandle GetCurrentlyProcessedState() const
	{
		return CurrentlyProcessedState;
	}

	/** @return the currently processed execution frame if applicable. */
	const FMetaStoryExecutionFrame* GetCurrentlyProcessedFrame() const
	{
		return CurrentlyProcessedFrame;
	}

	/** @return the currently processed execution parent frame if applicable. */
	const FMetaStoryExecutionFrame* GetCurrentlyProcessedParentFrame() const
	{
		return CurrentlyProcessedParentFrame;
	}

	/** @return in progress transition or the latest requested transition. */
	TSharedPtr<UE::MetaStory::ExecutionContext::ITemporaryStorage> GetCurrentlyProcessedTemporaryStorage() const
	{
		// A transition requests can be in progress or already succeeded.
		//Use the current transition request first (for linked frame).
		return CurrentlyProcessedTemporaryStorage ? CurrentlyProcessedTemporaryStorage : RequestedTransition ? RequestedTransition->Selection : nullptr;
	}

	/** @return Array view to named external data descriptors associated with this context. Note: Init() must be called before calling this method. */
	TConstArrayView<FMetaStoryExternalDataDesc> GetContextDataDescs() const
	{
		return RootMetaStory.GetContextDataDescs();
	}

	/** Sets context data view value for specific item. */
	void SetContextData(const FMetaStoryExternalDataHandle Handle, FMetaStoryDataView DataView)
	{
		check(Handle.IsValid());
		check(Handle.DataHandle.GetSource() == EMetaStoryDataSourceType::ContextData);
		ContextAndExternalDataViews[Handle.DataHandle.GetIndex()] = DataView;
	}

	/** Sets the context data based on name (name is defined in the schema), returns true if data was found */
	UE_API bool SetContextDataByName(const FName Name, FMetaStoryDataView DataView);

	/** @return the context data based on name (name is defined in the schema) */
	UE_API FMetaStoryDataView GetContextDataByName(const FName Name) const;

	/** @return True if all context data pointers are set. */ 
	UE_API bool AreContextDataViewsValid() const;

	/**
	 * Returns reference to external data based on provided handle. The return type is deduced from the handle's template type.
     * @param Handle Valid TMetaStoryExternalDataHandle<> handle. 
	 * @return reference to external data based on handle or null if data is not set.
	 */ 
	template <typename T>
	typename T::DataType& GetExternalData(const T Handle) const
	{
		check(Handle.IsValid());
		check(Handle.DataHandle.GetSource() == EMetaStoryDataSourceType::ExternalData);
		check(CurrentlyProcessedFrame);
		check(CurrentlyProcessedFrame->MetaStory->ExternalDataDescs[Handle.DataHandle.GetIndex()].Requirement != EMetaStoryExternalDataRequirement::Optional); // Optionals should query pointer instead.
		return ContextAndExternalDataViews[CurrentlyProcessedFrame->ExternalDataBaseIndex.Get() + Handle.DataHandle.GetIndex()].template GetMutable<typename T::DataType>();
	}

	/**
	 * Returns pointer to external data based on provided item handle. The return type is deduced from the handle's template type.
     * @param Handle Valid TMetaStoryExternalDataHandle<> handle.
	 * @return pointer to external data based on handle or null if item is not set or handle is invalid.
	 */ 
	template <typename T>
	typename T::DataType* GetExternalDataPtr(const T Handle) const
	{
		if (Handle.IsValid())
		{
			check(CurrentlyProcessedFrame);
			check(Handle.DataHandle.GetSource() == EMetaStoryDataSourceType::ExternalData);
			return ContextAndExternalDataViews[CurrentlyProcessedFrame->ExternalDataBaseIndex.Get() + Handle.DataHandle.GetIndex()].template GetMutablePtr<typename T::DataType>();
		}
		return nullptr;
	}

	FMetaStoryDataView GetExternalDataView(const FMetaStoryExternalDataHandle Handle)
	{
		if (Handle.IsValid())
		{
			check(CurrentlyProcessedFrame);
			check(Handle.DataHandle.GetSource() == EMetaStoryDataSourceType::ExternalData);
			return ContextAndExternalDataViews[CurrentlyProcessedFrame->ExternalDataBaseIndex.Get() + Handle.DataHandle.GetIndex()];
		}
		return FMetaStoryDataView();
	}

	/** @returns pointer to the instance data of specified node. */
	template <typename T>
	T* GetInstanceDataPtr(const FMetaStoryNodeBase& Node) const
	{
		check(CurrentNodeDataHandle == Node.InstanceDataHandle);
		return CurrentNodeInstanceData.template GetMutablePtr<T>();
	}

	/** @returns reference to the instance data of specified node. */
	template <typename T>
	T& GetInstanceData(const FMetaStoryNodeBase& Node) const
	{
		check(CurrentNodeDataHandle == Node.InstanceDataHandle);
		return CurrentNodeInstanceData.template GetMutable<T>();
	}

	/** @returns reference to the instance data of specified node. Infers the instance data type from the node's FInstanceDataType. */
	template <typename T>
	typename T::FInstanceDataType& GetInstanceData(const T& Node) const
	{
		static_assert(TIsDerivedFrom<T, FMetaStoryNodeBase>::IsDerived, "Expecting Node to derive from FMetaStoryNodeBase.");
		check(CurrentNodeDataHandle == Node.InstanceDataHandle);
		return CurrentNodeInstanceData.template GetMutable<typename T::FInstanceDataType>();
	}

	/** @returns pointer to the execution runtime data of specified node. */
	template <typename T>
	T* GetExecutionRuntimeDataPtr(const FMetaStoryNodeBase& Node) const
	{
		check(CurrentNodeDataHandle == Node.InstanceDataHandle && Node.InstanceDataHandle.IsValid());
		return GetExecutionRuntimeDataView().template GetMutablePtr<T>();
	}

	/** @returns reference to the execution runtime data of specified node. */
	template <typename T>
	T& GetExecutionRuntimeData(const FMetaStoryNodeBase& Node) const
	{
		check(CurrentNodeDataHandle == Node.InstanceDataHandle && Node.InstanceDataHandle.IsValid());
		return GetExecutionRuntimeDataView().template GetMutable<T>();
	}

	/** @returns reference to the execution runtime data of specified node. Infers the execution runtime data type from the node's FExecutionRuntimeDataType. */
	template <typename T>
	typename T::FExecutionRuntimeDataType& GetExecutionRuntimeData(const T& Node) const
	{
		static_assert(TIsDerivedFrom<T, FMetaStoryNodeBase>::IsDerived, "Expecting Node to derive from FMetaStoryNodeBase.");
		check(CurrentNodeDataHandle == Node.InstanceDataHandle && Node.InstanceDataHandle.IsValid());
		return GetExecutionRuntimeDataView().template GetMutable<typename T::FExecutionRuntimeDataType>();
	}

	/** @returns reference to instance data struct that can be passed to lambdas. See TMetaStoryInstanceDataStructRef for usage. */
	template <typename T>
	TMetaStoryInstanceDataStructRef<typename T::FInstanceDataType> GetInstanceDataStructRef(const T& Node) const
	{
		static_assert(TIsDerivedFrom<T, FMetaStoryNodeBase>::IsDerived, "Expecting Node to derive from FMetaStoryNodeBase.");
		check(CurrentlyProcessedFrame);
		return TMetaStoryInstanceDataStructRef<typename T::FInstanceDataType>(InstanceData, *CurrentlyProcessedFrame, Node.InstanceDataHandle);
	}

	/**
	 * Requests transition to a state.
	 * If called during during transition processing (e.g. from FMetaStoryTaskBase::TriggerTransitions()) the transition
	 * is attempted to be activate immediately (it can fail e.g. because of preconditions on a target state).
	 * If called outside the transition handling, the request is buffered and handled at the beginning of next transition processing.
	 * @param Request The state to transition to.
	 */
	UE_API void RequestTransition(const FMetaStoryTransitionRequest& Request);

	/**
	 * Requests transition to a state.
	 * If called during during transition processing (e.g. from FMetaStoryTaskBase::TriggerTransitions()) the transition
	 * is attempted to be activate immediately (it can fail e.g. because of preconditions on a target state).
	 * If called outside the transition handling, the request is buffered and handled at the beginning of next transition processing.
	 * @param TargetState The state to transition to.
	 * @param Priority The priority of the transition.
	 * @param Fallback of the transition if it fails to select the target state.
	 */
	UE_API void RequestTransition(FMetaStoryStateHandle TargetState, EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::Normal, EMetaStorySelectionFallback Fallback = EMetaStorySelectionFallback::None);

	/**
	 * Finishes a task. This fails if the Task is not currently the processed node.
	 * ie. Must be called from inside a FMetaStoryTaskBase EnterState, ExitState, StateCompleted, Tick, TriggerTransitions.
	 * If called during tick processing, then the state completes immediately.
	 * If called outside of the tick processing, then the request is buffered and handled on the next tick.
	 */
	UE_API void FinishTask(const FMetaStoryTaskBase& Task, EMetaStoryFinishTaskType FinishType);
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use the weak context to finish a task async or FinishTask(FMetaStoryTaskBase, EMetaStoryFinishTaskType) to finish the current task.")
	/**
	 * Finishes a task.
	 * If called during tick processing, then the state completes immediately.
	 * If called outside of the tick processing, then the request is buffered and handled on the next tick.
	 */
	UE_API void FinishTask(const UE::MetaStory::FFinishedTask& Task, EMetaStoryFinishTaskType FinishType);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** @return data view of the specified handle relative to given frame. */
	UE_DEPRECATED(5.6, "Use UE::MetaStory::InstanceData::GetDataView instead.")
	static FMetaStoryDataView GetDataViewFromInstanceStorage(FMetaStoryInstanceStorage& InstanceDataStorage, FMetaStoryInstanceStorage* CurrentlyProcessedSharedInstanceStorage, const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle)
	{
		return UE::MetaStory::InstanceData::GetDataView(InstanceDataStorage, CurrentlyProcessedSharedInstanceStorage, ParentFrame, CurrentFrame, Handle);
	}

	//@todo deprecate in favor of a find with frameID
	/** Looks for a frame in provided list of frames. */
	static UE_API const FMetaStoryExecutionFrame* FindFrame(const UMetaStory* MetaStory, FMetaStoryStateHandle RootState, TConstArrayView<FMetaStoryExecutionFrame> Frames, const FMetaStoryExecutionFrame*& OutParentFrame);

	/**
	 * Forces transition to a state. It will skip all conditions.
	 * Primarily used for replication purposes so that a client MetaStory stays in sync with its server counterpart.
	 * It has to be a running instance. It will not work if you didn't call Start or if the execution previously failed.
	 * @param Recorded state transition to run on the MetaStory.
	 * @return The new run status for the MetaStory.
	 */
	UE_API EMetaStoryRunStatus ForceTransition(const FMetaStoryRecordedTransitionResult& Transition);
	
	/** Returns the recorded transitions for this context. */
	TConstArrayView<FMetaStoryRecordedTransitionResult> GetRecordedTransitions() const
	{
		return RecordedTransitions;
	}

protected:
	/** Event used during state selection. Identified by the ID of the active state that consumed it. */
	struct FSelectionEventWithID
	{
		UE::MetaStory::FActiveState State;
		FMetaStorySharedEvent Event;
	};

	/** Describes a result of States Selection. */
	struct FSelectStateResult : public UE::MetaStory::ExecutionContext::ITemporaryStorage
	{
		FSelectStateResult() = default;
		virtual ~FSelectStateResult() = default;

		/** The active states selected. They are in order from the root to the leaf. */
		TArray<UE::MetaStory::FActiveState> SelectedStates;

		/** The selected frame ID. The frame can be in the current active list or in the TemporaryFrames list. */
		TArray<UE::MetaStory::FActiveFrameID, TInlineAllocator<4>> SelectedFrames;

		/**
		 * New execution frame created during the state selection.
		 * The active state list is empty and will be filled during EnterState()
		 */
		TArray<FMetaStoryExecutionFrame, TInlineAllocator<2>> TemporaryFrames;

		/** Events used during the state selection. */
		TArray<FSelectionEventWithID, TInlineAllocator<2>> SelectionEvents;

		/** The requested target of the selection. */
		UE::MetaStory::FActiveState TargetState;

		/** @return a new temporary frame */
		UE_API FMetaStoryExecutionFrame& MakeAndAddTemporaryFrame(UE::MetaStory::FActiveFrameID FrameID, const UE::MetaStory::FMetaStoryExecutionFrameHandle& FrameHandle, bool bIsGlobalFrame);
		UE_API FMetaStoryExecutionFrame& MakeAndAddTemporaryFrameWithNewRoot(UE::MetaStory::FActiveFrameID FrameID, const UE::MetaStory::FMetaStoryExecutionFrameHandle& FrameHandle, FMetaStoryExecutionFrame& OtherFrame);

		/** @return a temporary frame by ID. */
		FMetaStoryExecutionFrame* FindTemporaryFrame(UE::MetaStory::FActiveFrameID FrameID)
		{
			return TemporaryFrames.FindByPredicate(
				[FrameID = FrameID](const FMetaStoryExecutionFrame& Frame)
				{
					return Frame.FrameID == FrameID;
				});
		}

		//~ITemporaryStorage
		virtual FFrameAndParent GetExecutionFrame(UE::MetaStory::FActiveFrameID ID) override;
		virtual UE::MetaStory::FActiveState GetStateHandle(UE::MetaStory::FActiveStateID ID) const override;
	};

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	struct
	UE_DEPRECATED(5.7, "The selection result changed to include the state ID and frame ID")
	FStateSelectionResult
	{
		/** Max number of execution frames handled during state selection. */
		static constexpr int32 MaxExecutionFrames = 8;

		FStateSelectionResult() = default;
		explicit FStateSelectionResult(TConstArrayView<FMetaStoryExecutionFrame> InFrames)
		{
			SelectedFrames = InFrames;
			FramesStateSelectionEvents.SetNum(SelectedFrames.Num());
		}

		bool IsFull() const
		{
			return SelectedFrames.Num() == MaxExecutionFrames;
		}

		void PushFrame(FMetaStoryExecutionFrame Frame)
		{
			SelectedFrames.Add(Frame);
			FramesStateSelectionEvents.AddDefaulted();
		}

		void PopFrame()
		{
			SelectedFrames.Pop();
			FramesStateSelectionEvents.Pop();
		}

		bool ContainsFrames() const
		{
			return !SelectedFrames.IsEmpty();
		}

		int32 FramesNum() const
		{
			return SelectedFrames.Num();
		}

		TArrayView<FMetaStoryExecutionFrame> GetSelectedFrames()
		{
			return SelectedFrames;
		}

		TConstArrayView<FMetaStoryExecutionFrame> GetSelectedFrames() const
		{
			return SelectedFrames;
		}

		TArrayView<FMetaStoryFrameStateSelectionEvents> GetFramesStateSelectionEvents()
		{
			return FramesStateSelectionEvents;
		}

	protected:
		TArray<FMetaStoryExecutionFrame, TFixedAllocator<MaxExecutionFrames>> SelectedFrames;
		TArray<FMetaStoryFrameStateSelectionEvents, TFixedAllocator<MaxExecutionFrames>> FramesStateSelectionEvents;
	};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** The result of the RequestTransition function. */
	struct FRequestTransitionResult
	{
		TSharedPtr<FSelectStateResult> Selection;
		FMetaStoryTransitionResult Transition;
		FMetaStoryTransitionSource Source;
	};

	UE_DEPRECATED(5.6, "Use FMetaStoryExecutionExtension::GetInstanceDescription instead")
	/** @return Prefix that will be used by METASTORY_LOG and METASTORY_CLOG, Owner name by default. */
	UE_API virtual FString GetInstanceDescription() const final;

	/** Callback when delayed transition is triggered. Contexts that are event based can use this to trigger a future event. */
	virtual void BeginDelayedTransition(const FMetaStoryTransitionDelayedState& DelayedState) {};

	UE_DEPRECATED(5.7, "Use the UpdateInstanceData with the FSelectStateResult argument.")
	UE_API void UpdateInstanceData(TConstArrayView<FMetaStoryExecutionFrame> CurrentActiveFrames, TArrayView<FMetaStoryExecutionFrame> NextActiveFrames);

	/** Allocate the instance data. Fixup the TemporarySelectedFrame with the correct data. */
	UE_API void UpdateInstanceData(const TSharedPtr<FSelectStateResult>& SelectStateResult);

	UE_DEPRECATED(5.7, "Use the EnterState with the FSelectStateResult argument.")
	UE_API EMetaStoryRunStatus EnterState(FMetaStoryTransitionResult& Transition);

	/**
	 * Handles logic for entering State. EnterState is called on new active Evaluators and Tasks that are part of the re-planned tree.
	 * Re-planned tree is from the transition target up to the leaf state. States that are parent to the transition target state
	 * and still active after the transition will remain intact.
	 * @return run status returned by the tasks.
	 */
	UE_API EMetaStoryRunStatus EnterState(const TSharedPtr<FSelectStateResult>& SelectStateResult, const FMetaStoryTransitionResult& Transition);

	UE_DEPRECATED(5.7, "Use the ExitState with the FSelectStateResult argument.")
	UE_API void ExitState(const FMetaStoryTransitionResult& Transition);

	/**
	 * Handles logic for exiting State. ExitState is called on current active Evaluators and Tasks that are part of the re-planned tree.
	 * Re-planned tree is from the transition target up to the leaf state. States that are parent to the transition target state
	 * and still active after the transition will remain intact.
	 */
	UE_API void ExitState(const TSharedPtr<const FSelectStateResult>& SelectStateResult, const FMetaStoryTransitionResult& Transition);

	/**
	 * Removes all delegate listeners.
	 */
	UE_API void RemoveAllDelegateListeners();

	/**
	 * Handles logic for signaling State completed. StateCompleted is called on current active Evaluators and Tasks in reverse order (from leaf to root).
	 */
	UE_API void StateCompleted();

private:
	template<bool bOnActiveInstances>
	void TickGlobalEvaluatorsForFrameInternal(float DeltaTime, const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& Frame);

protected:
	/**
	 * Stop the frame's evaluators.
	 * Should be used only on active node instances, assumes valid data handles and does not consider temporary node instances.
	 */
	UE_API void TickGlobalEvaluatorsForFrameOnActiveInstances(float DeltaTime, const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& Frame);

	/**
	 * Tick the frame's evaluators.
	 * This version validates the data handles and looks up temporary instances.
	 */
	UE_API void TickGlobalEvaluatorsForFrameWithValidation(float DeltaTime, const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& Frame);

	/**
	 * Tick evaluators and global tasks by delta time.
	 */
	UE_API EMetaStoryRunStatus TickEvaluatorsAndGlobalTasks(float DeltaTime, bool bTickGlobalTasks = true);

	/**
	 * Tick the frame's evaluators and the frame's tasks.
	 */
	UE_API EMetaStoryRunStatus TickEvaluatorsAndGlobalTasksForFrame(float DeltaTime, bool bTickGlobalTasks, int32 FrameIndex, const FMetaStoryExecutionFrame* CurrentParentFrame, const TNotNull<FMetaStoryExecutionFrame*> CurrentFrame);

private:
	template<bool bOnActiveInstances>
	EMetaStoryRunStatus StartGlobalsForFrameInternal(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, FMetaStoryTransitionResult& Transition);

protected:
	/**
	 * Start the frame's evaluators and the frame's tasks.
	 * Should be used only on active node instances, assumes valid data handles and does not consider temporary node instances.
	 */
	UE_API EMetaStoryRunStatus StartGlobalsForFrameOnActiveInstances(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, FMetaStoryTransitionResult& Transition);

	/**
	 * Start the frame's evaluators and the frame's tasks.
	 * This version validates the data handles and looks up temporary instances.
	 */
	UE_API EMetaStoryRunStatus StartGlobalsForFrameWithValidation(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, FMetaStoryTransitionResult& Transition);

	UE_DEPRECATED(5.7, "Use StartEvaluatorsAndGlobalTasks without the OutLastInitializedTaskIndex argument")
	UE_API EMetaStoryRunStatus StartEvaluatorsAndGlobalTasks(FMetaStoryIndex16& OutLastInitializedTaskIndex);

	/**
	 * Starts active global evaluators and global tasks.
	 * @return run status returned by the global tasks.
	 */
	UE_API EMetaStoryRunStatus StartEvaluatorsAndGlobalTasks();

private:
	template<bool bOnActiveInstances>
	void StopGlobalsForFrameInternal(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, const FMetaStoryTransitionResult& Transition);

protected:
	/**
	 * Stop the frame's evaluators and the frame's tasks.
	 * Should be used only on active node instances, assumes valid data handles and does not consider temporary node instances.
	 */
	UE_API void StopGlobalsForFrameOnActiveInstances(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, const FMetaStoryTransitionResult& Transition);

	/**
	 * Stop the frame's evaluators and the frame's tasks.
	 * This version validates the data handles and looks up temporary instances.
	 */
	UE_API void StopGlobalsForFrameWithValidation(const FMetaStoryExecutionFrame* ParentFrame, FMetaStoryExecutionFrame& Frame, const FMetaStoryTransitionResult& Transition);

	UE_DEPRECATED(5.7, "Use StopEvaluatorsAndGlobalTasks without LastInitializedTaskIndex")
	UE_API void StopEvaluatorsAndGlobalTasks(const EMetaStoryRunStatus CompletionStatus, const FMetaStoryIndex16 LastInitializedTaskIndex);

	/**
	 * Stops active global evaluators and active global tasks.
	 */
	UE_API void StopEvaluatorsAndGlobalTasks(const EMetaStoryRunStatus CompletionStatus);

	UE_DEPRECATED(5.7, "Use StopGlobalsForFrameOnActiveInstances")
	UE_API void CallStopOnEvaluatorsAndGlobalTasks(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& Frame, const FMetaStoryTransitionResult& Transition, const FMetaStoryIndex16 LastInitializedTaskIndex = FMetaStoryIndex16());

	/** Starts temporary instances of global evaluators and tasks for a given frame. */
	UE_API EMetaStoryRunStatus StartTemporaryEvaluatorsAndGlobalTasks(const FMetaStoryExecutionFrame* CurrentParentFrame, FMetaStoryExecutionFrame& CurrentFrame);

	UE_DEPRECATED(5.6, "Use the non const version of StartTemporaryEvaluatorsAndGlobalTasks")
	EMetaStoryRunStatus StartTemporaryEvaluatorsAndGlobalTasks(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame)
	{
		return StartTemporaryEvaluatorsAndGlobalTasks(CurrentParentFrame, const_cast<FMetaStoryExecutionFrame&>(CurrentFrame));
	}

	UE_DEPRECATED(5.7, "Use the non const version of StopTemporaryEvaluatorsAndGlobalTasks")
	UE_API void StopTemporaryEvaluatorsAndGlobalTasks(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame);

	/** Stops temporary frame's evaluators and frame's tasks for the provided frame. */
	UE_API void StopTemporaryEvaluatorsAndGlobalTasks(const FMetaStoryExecutionFrame* CurrentParentFrame, FMetaStoryExecutionFrame& CurrentFrame, EMetaStoryRunStatus StartResult);

	/**
	 * Ticks tasks of all active states starting from current state by delta time.
	 * @return Run status returned by the tasks.
	 */
	UE_API EMetaStoryRunStatus TickTasks(const float DeltaTime);

	struct FTickTaskResult
	{
		bool bShouldTickTasks = true;
	};
	struct FTickTaskArguments
	{
		FTickTaskArguments() = default;
		float DeltaTime = 0.f;
		int32 TasksBegin = 0;
		int32 TasksNum = 0;
		int32 Indent = 0;
		const FMetaStoryExecutionFrame* ParentFrame = nullptr;
		FMetaStoryExecutionFrame* Frame = nullptr;
		UE::MetaStory::FActiveStateID StateID;
		UE::MetaStory::FTasksCompletionStatus* TasksCompletionStatus = nullptr;
		bool bIsGlobalTasks = false;
		bool bShouldTickTasks = true;
	};
	/** Ticks tasks and updates the bindings for a specific state or frame. */
	UE_API FTickTaskResult TickTasks(const FTickTaskArguments& Args);

	/** Common functionality shared by the tick methods. */
	UE_API EMetaStoryRunStatus TickPrelude();
	UE_API EMetaStoryRunStatus TickPostlude();

	/** Handles task ticking part of the tick. */
	UE_API void TickUpdateTasksInternal(float DeltaTime);
	
	/** Handles transition triggering part of the tick. */
	UE_API void TickTriggerTransitionsInternal();

	/** Gives Execution Extension a chance to react. */
	UE_API void BeginApplyTransition(const FMetaStoryTransitionResult& InTransitionResult);
protected:
	using FMemoryRequirement = UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer::FMemoryRequirement;

private:
	template<bool bOnActiveInstances>
	bool TestAllConditionsInternal(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryStateHandle CurrentStateHandle, const FMemoryRequirement& MemoryRequirement, const int32 ConditionsOffset, const int32 ConditionsNum, EMetaStoryUpdatePhase Phase);

protected:
	/**
	 * Checks all conditions at given range.
	 * Should be used only on active node instances, assumes valid data handles and does not consider temporary node instances.
	 * @return True if all conditions pass.
	 */
	UE_API bool TestAllConditionsOnActiveInstances(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryStateHandle CurrentStateHandle, const FMemoryRequirement& MemoryRequirement, const int32 ConditionsOffset, const int32 ConditionsNum, EMetaStoryUpdatePhase Phase);

	/**
	 * Checks all conditions at given range.
	 * This version validates the data handles and looks up temporary instances.
	 * @return True if all conditions pass.
	 */
	UE_API bool TestAllConditionsWithValidation(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryStateHandle CurrentStateHandle, const FMemoryRequirement& MemoryRequirement, const int32 ConditionsOffset, const int32 ConditionsNum, EMetaStoryUpdatePhase Phase);

	UE_DEPRECATED(5.7, "Use TestAllConditionsWithValidation or TestAllConditionsOnActiveInstances. TestAllConditionsWithValidation can get the data from a temporary frame in state selection.")
	UE_API bool TestAllConditions(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const int32 ConditionsOffset, const int32 ConditionsNum);

	/**
	 * Calculate the final score of all considerations at given range.
	 * @return the final score
	 */
	UE_API float EvaluateUtilityWithValidation(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryStateHandle StateHandle, const FMemoryRequirement& MemoryRequirement, const int32 ConsiderationsBegin, const int32 ConsiderationsNum, const float StateWeight);

	UE_DEPRECATED(5.7, "Use EvaluateUtilityWithValidation. It can get the data from a temporary frame in state selection.")
	UE_API float EvaluateUtility(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const int32 ConsiderationsOffset, const int32 ConsiderationsNum, const float StateWeight);

private:
	template<bool bOnActiveInstances>
	void EvaluatePropertyFunctionsInternal(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryIndex16 FuncsBegin, uint16 FuncsNum);

protected:
	/*
	 * Evaluate all function at given range.
	 * Should be used only on active node instances, assumes valid data handles and does not consider temporary node instances.
	 */
	UE_API void EvaluatePropertyFunctionsOnActiveInstances(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryIndex16 FuncsBegin, uint16 FuncsNum);

	/*
	 * Evaluate all function at given range.
	 * This version validates the data handles and looks up temporary instances.
	 */
	UE_API void EvaluatePropertyFunctionsWithValidation(const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, FMetaStoryIndex16 FuncsBegin, uint16 FuncsNum);

	UE_DEPRECATED(5.7, "Use the RequestTransitionInternal with the active state ID")
	UE_API bool RequestTransition(
		const FMetaStoryExecutionFrame& CurrentFrame,
		const FMetaStoryStateHandle NextState,
		const EMetaStoryTransitionPriority Priority,
		const FMetaStorySharedEvent* TransitionEvent = nullptr,
		const EMetaStorySelectionFallback Fallback = EMetaStorySelectionFallback::None);


	UE_DEPRECATED(5.7, "Use the SetupNextTransition with TransitionArguments")
	UE_API void SetupNextTransition(const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryStateHandle NextState, const EMetaStoryTransitionPriority Priority);
	
	/** Optional arguments for a transition request. */
	struct FTransitionArguments
	{
		EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::Normal;
		FMetaStorySharedEvent TransitionEvent;
		EMetaStorySelectionFallback Fallback = EMetaStorySelectionFallback::None;
	};

	/** Requests transition to a specified state. */
	UE_API bool RequestTransitionInternal(const FMetaStoryExecutionFrame& SourceFrame,
		UE::MetaStory::FActiveStateID SourceStateID,
		UE::MetaStory::ExecutionContext::FStateHandleContext TargetState,
		const FTransitionArguments& Args);
	
	/** Sets up NextTransition based on the provided parameters and the current execution status. */
	UE_API void SetupNextTransition(const FMetaStoryExecutionFrame& SourceFrame,
		UE::MetaStory::FActiveStateID SourceStateID,
		UE::MetaStory::ExecutionContext::FStateHandleContext TargetState,
		const FTransitionArguments& Args);

	/** Sets up NextTransition based on the provided parameters and the current execution status. */
	UE_API void SetupNextTransition(const FMetaStoryExecutionFrame& SourceFrame,
		UE::MetaStory::FActiveStateID SourceStateID,
		UE::MetaStory::ExecutionContext::FStateHandleContext TargetState,
		const FTransitionArguments& Args,
		FMetaStoryTransitionResult& OutTransitionResult);

	/**
	 * Tick (if needed) event, delayed, delegate and, tick state completion transitions.
	 * Test the completed state transitions.
	 * @return if the transition is valid and should be entered.
	 */
	UE_API bool TriggerTransitions();

	UE_DEPRECATED(5.7, "Use ForceTransition")
	/** Create a new transition result from a recorded transition result. It will fail if the recorded transition is malformed. */
	UE_API TOptional<FMetaStoryTransitionResult> MakeTransitionResult(const FMetaStoryRecordedTransitionResult& Transition) const;

	UE_DEPRECATED(5.7, "Use MakeRecordedTransitionResult with the FSelectStateResult argument.")
	/** Create a new recorded transition from a transition result. */
	UE_API FMetaStoryRecordedTransitionResult MakeRecordedTransitionResult(const FMetaStoryTransitionResult& Transition) const;

	/** Create a new recorded transition from a select state result. */
	UE_API FMetaStoryRecordedTransitionResult MakeRecordedTransitionResult(const TSharedRef<FSelectStateResult>& Args, const FMetaStoryTransitionResult& Transition) const;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Replaced with FMetaStoryTasksCompletionStatus")
	/** Confirms that the frame and state ID are valid and the task index is correct. */
	UE_API bool IsFinishedTaskValid(const UE::MetaStory::FFinishedTask& Task) const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.6, "Replaced with FMetaStoryTasksCompletionStatus")
	/**
	 * Removes stale entries and fills the completed states list.
	 * The tree can be in a different state from when a finished task was added to the pending list and the finished task may not be valid.
	 * @param bMarkTaskProcessed when true, marks the tasks as processed. When false, only used the already marked tasks.
	 */
	UE_API void UpdateCompletedStateList(bool bMarkTaskProcessed);

	UE_DEPRECATED(5.6, "Replaced with FMetaStoryTasksCompletionStatus")
	UE_API void UpdateCompletedStateList();

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Replaced with FMetaStoryTasksCompletionStatus")
	/** Adds the state to the completed list from a finished task. */
	UE_API void MarkStateCompleted(UE::MetaStory::FFinishedTask& FinisedTask);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.6, "Replaced with FMetaStoryTasksCompletionStatus")
	/** In the list of completed states, returns the run status when it's completed by a global task. */
	UE_API EMetaStoryRunStatus GetGlobalTasksCompletedStatesStatus() const;

	/** Forces transition to a state. It will skip all conditions. */
	UE_API bool ForceTransitionInternal(const TArrayView<const UE::MetaStory::ExecutionContext::FStateHandleContext> States, const TSharedRef<FSelectStateResult>& OutSelectionResult);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.7, "Use the SelectState with the FSelectStateArguments argument.")
	UE_API bool SelectState(const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryStateHandle NextState, FStateSelectionResult& OutSelectionResult, const FMetaStorySharedEvent* TransitionEvent = nullptr, const EMetaStorySelectionFallback Fallback = EMetaStorySelectionFallback::None);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** The different type of selections. */
	enum class ESelectStateBehavior : uint8
	{
		/** From a state transition. Normal rules apply. */
		StateTransition,
		/**
		 * From a force transition.
		 * The state's selection behavior and condition will not be respected.
		 * The target is the final state.
		 */
		Forced,
	};
	struct FSelectStateArguments
	{
		/** The current list of active states. */
		TArrayView<const UE::MetaStory::FActiveState> ActiveStates;
		/** The state that requested the transition. */
		UE::MetaStory::FActiveState SourceState;
		/** The state that we want to select. Depending on the state's selection behavior, another state can be selected. */
		UE::MetaStory::ExecutionContext::FStateHandleContext TargetState;
		/** Transition event used by transition to trigger the state selection. */
		FMetaStorySharedEvent TransitionEvent;
		/** Fallback selection behavior to execute if it fails to select the desired state. */
		EMetaStorySelectionFallback Fallback = EMetaStorySelectionFallback::None;
		/** The type of selection. */
		ESelectStateBehavior Behavior = ESelectStateBehavior::StateTransition;
		/** Rule to Selection state rules. */
		EMetaStoryStateSelectionRules SelectionRules = EMetaStoryStateSelectionRules::None;
	};

	/**
	 * Starting at the specified state, walking towards the leaf states.
	 * @param SelectStateArgs the arguments for SelectState.
	 * @param OutSelectionResult the result of the selection.
	 * @return True if succeeded to select new active states.
	 */
	UE_API bool SelectState(const FSelectStateArguments& SelectStateArgs, const TSharedRef<FSelectStateResult>& OutSelectionResult);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.7, "Use the SelectStateInternal with FSelectStateArguments argument.")
	UE_API bool SelectStateInternal(const FMetaStoryExecutionFrame* CurrentParentFrame, FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryExecutionFrame* CurrentFrameInActiveFrames, TConstArrayView<FMetaStoryStateHandle> PathToNextState, FStateSelectionResult& OutSelectionResult, const FMetaStorySharedEvent* TransitionEvent = nullptr);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	struct FSelectStateInternalArguments
	{
		TArrayView<const UE::MetaStory::FActiveState> MissingActiveStates;
		UE::MetaStory::FActiveFrameID MissingSourceFrameID;
		TArrayView<const UE::MetaStory::FActiveState> MissingSourceStates;
		TArrayView<const UE::MetaStory::ExecutionContext::FStateHandleContext> MissingStatesToReachTarget;
	};

	/** Used internally to do the recursive part of the SelectState(). */
	UE_API bool SelectStateInternal(
		const FSelectStateArguments& Args,
		const FSelectStateInternalArguments& InternalArgs,
		const TSharedRef<FSelectStateResult>& OutSelectionResult);

	/** Used internally to select state until we reach the transition source or the transition target. */
	UE_API TOptional<bool> SelectStateFromSourceInternal(
		const FSelectStateArguments& Args,
		const FSelectStateInternalArguments& InternalArgs,
		const TSharedRef<FSelectStateResult>& OutSelectionResult,
		const FMetaStoryExecutionFrame& NextFrame,
		const FMetaStoryCompactState& NextState,
		FMetaStoryStateHandle NextStateHandle,
		bool bNewFrameCreated);

	UE_API bool SelectStateInternal_Linked(
		const FSelectStateArguments& Args,
		const FSelectStateInternalArguments& InternalArgs,
		const TSharedRef<FSelectStateResult>& OutSelectionResult,
		TNotNull<const UMetaStory*> NextMetaStory,
		const FMetaStoryCompactState& NextState,
		bool bShouldCreateNewState
		);

	UE_API bool SelectStateInternal_LinkedAsset(
		const FSelectStateArguments& Args,
		const FSelectStateInternalArguments& InternalArgs,
		const TSharedRef<FSelectStateResult>& OutSelectionResult,
		TNotNull<const UMetaStory*> NextMetaStory,
		const FMetaStoryCompactState& NextState,
		const UMetaStory* NextLinkedStateAsset,
		bool bShouldCreateNewState);

	UE_API bool SelectStateInternal_TrySelectChildrenInOrder(
		const FSelectStateArguments& Args,
		const FSelectStateInternalArguments& InternalArgs,
		const TSharedRef<FSelectStateResult>& OutSelectionResult,
		TNotNull<const UMetaStory*> NextMetaStory,
		const FMetaStoryCompactState& TargetState,
		const FMetaStoryStateHandle TargetStateHandle);
		
	UE_API bool SelectStateInternal_TrySelectChildrenAtRandom(
		const FSelectStateArguments& Args,
		const FSelectStateInternalArguments& InternalArgs,
		const TSharedRef<FSelectStateResult>& OutSelectionResult,
		TNotNull<const UMetaStory*> NextMetaStory,
		const FMetaStoryCompactState& TargetState,
		const FMetaStoryStateHandle TargetStateHandle);

	UE_API bool SelectStateInternal_TrySelectChildrenWithHighestUtility(
		const FSelectStateArguments& Args,
		const FSelectStateInternalArguments& InternalArgs,
		const TSharedRef<FSelectStateResult>& OutSelectionResult,
		TNotNull<const UMetaStory*> NextMetaStory,
		const FMetaStoryCompactState& TargetState,
		const FMetaStoryStateHandle TargetStateHandle);

	UE_API bool SelectStateInternal_TrySelectChildrenAtRandomWeightedByUtility(
		const FSelectStateArguments& Args,
		const FSelectStateInternalArguments& InternalArgs,
		const TSharedRef<FSelectStateResult>& OutSelectionResult,
		TNotNull<const UMetaStory*> NextMetaStory,
		const FMetaStoryCompactState& TargetState,
		const FMetaStoryStateHandle TargetStateHandle);

	UE_API bool SelectStateInternal_TryFollowTransitions(
		const FSelectStateArguments& Args,
		const FSelectStateInternalArguments& InternalArgs,
		const TSharedRef<FSelectStateResult>& OutSelectionResult,
		TNotNull<const UMetaStory*> NextMetaStory,
		const FMetaStoryCompactState& TargetState,
		const FMetaStoryStateHandle TargetStateHandle);

	/** @return MetaStory execution state from the instance storage. */
	[[nodiscard]] FMetaStoryExecutionState& GetExecState()
	{
		return Storage.GetMutableExecutionState();
	}

	/** @return const MetaStory execution state from the instance storage. */
	[[nodiscard]] const FMetaStoryExecutionState& GetExecState() const
	{
		return Storage.GetExecutionState();
	}

	/** Updates the update phase of the MetaStory execution state. */
	UE_API void SetUpdatePhaseInExecutionState(FMetaStoryExecutionState& ExecutionState, EMetaStoryUpdatePhase UpdatePhase) const;

	/** @return String describing state status for logging and debug. */
	[[nodiscard]] UE_API FString GetStateStatusString(const FMetaStoryExecutionState& ExecState) const;

	/** @return String describing state name for logging and debug. */
	[[nodiscard]] UE_API FString GetSafeStateName(const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryStateHandle State) const;

	/** @return String describing state name for logging and debug. */
	[[nodiscard]] UE_API FString GetSafeStateName(const UMetaStory* MetaStory, const FMetaStoryStateHandle State) const;

	/** @return String describing full path of an activate state for logging and debug. */
	[[nodiscard]] UE_API FString DebugGetStatePath(TConstArrayView<FMetaStoryExecutionFrame> ActiveFrames, const FMetaStoryExecutionFrame* CurrentFrame = nullptr, const int32 ActiveStateIndex = INDEX_NONE) const;

	/** @return String describing all events that are currently being processed  for logging and debug. */
	[[nodiscard]] UE_API FString DebugGetEventsAsString() const;

	/** @return data view of the specified handle relative to given frame. */
	[[nodiscard]] UE_API FMetaStoryDataView GetDataView(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle);
	[[nodiscard]] UE_API FMetaStoryDataView GetDataView(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo);

	/** @return data view for the execution runtime view of the current node and frame. */
	[[nodiscard]] UE_API FMetaStoryDataView GetExecutionRuntimeDataView() const;

	/** @return true if handle source is valid cified handle relative to given frame. */
	[[nodiscard]] UE_API bool IsHandleSourceValid(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle) const;
	[[nodiscard]] UE_API bool IsHandleSourceValid(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo) const;
	
	/** @return data view of the specified handle relative to the given frame, or tries to find a matching temporary instance. */
	[[nodiscard]] UE_API FMetaStoryDataView GetDataViewOrTemporary(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle);
	[[nodiscard]] UE_API FMetaStoryDataView GetDataViewOrTemporary(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FPropertyBindingCopyInfo& CopyInfo);

	/** @return data view of the specified handle from temporary instance. */
	[[nodiscard]] UE_API FMetaStoryDataView GetTemporaryDataView(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle);

	/**
	 * Adds a temporary instance that can be located using frame and data handle later.
	 * @returns view to the newly added instance. If NewInstanceData is Object wrapper, the new object is returned.
	 */
	[[nodiscard]] UE_API FMetaStoryDataView AddTemporaryInstance(const FMetaStoryExecutionFrame& Frame, const FMetaStoryIndex16 OwnerNodeIndex, const FMetaStoryDataHandle DataHandle, FConstStructView NewInstanceData);

	enum class ECopyBindings : uint8
	{
		EnterState,
		Tick,
		ExitState,
	};

	/** Copy the binding on all valid active instances. */
	UE_API void CopyAllBindingsOnActiveInstances(ECopyBindings CopyType);

private:
	template<bool bOnActiveInstances>
	bool CopyBatchInternal(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataView TargetView, const FMetaStoryIndex16 BindingsBatch);

protected:
	/**
	 * Copies a batch of properties to the data in TargetView.
	 * Should be used only on active node instances, assumes valid data handles and does not consider temporary node instances.
	 */
	UE_API bool CopyBatchOnActiveInstances(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataView TargetView, const FMetaStoryIndex16 BindingsBatch);

	/** Copies a batch of properties to the data in TargetView. This version validates the data handles and looks up temporary instances. */
	UE_API bool CopyBatchWithValidation(const FMetaStoryExecutionFrame* ParentFrame, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataView TargetView, const FMetaStoryIndex16 BindingsBatch);

	/**
	 * Collects external data for all MetaStorys in active frames.
	 * @returns true if all external data are set successfully.
	 */
	UE_API bool CollectActiveExternalData();
	
	/**
	 * Collects external data for all MetaStorys in active frames.
	 * @returns true if all external data are set successfully.
	 */
	UE_API bool CollectActiveExternalData(const TArrayView<FMetaStoryExecutionFrame> Frames);

	/**
	 * Collects external data for specific MetaStory asset. If the data is already collected, cached index is returned.
	 * @returns index in ContextAndExternalDataViews for the first external data.
	 */
	UE_API FMetaStoryIndex16 CollectExternalData(const UMetaStory* MetaStory);

	/**
	 * Stores copy of provided parameters as MetaStory global parameters.
	 * @param Parameters parameters to copy
	 * @returns true if successfully set the parameters
	 */
	UE_API bool SetGlobalParameters(const FInstancedPropertyBag& Parameters);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.7, "Use the CaptureNewStateEvents with the FSelectStateResult argument.")
	void CaptureNewStateEvents(TConstArrayView<FMetaStoryExecutionFrame> PrevFrames, TConstArrayView<FMetaStoryExecutionFrame> NewFrames, TArrayView<FMetaStoryFrameStateSelectionEvents> FramesStateSelectionEvents);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Captures MetaStory events used during the state selection.
	 */
	UE_API void CaptureNewStateEvents(const TSharedRef<const FSelectStateResult>& Args);

	/** @return a weak reference for a task that can be stored for later uses. */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "FMetaStoryWeakTaskRef is no longer used.")
	UE_API FMetaStoryWeakTaskRef MakeWeakTaskRefInternal() const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Instance data used during current tick. */
	FMetaStoryInstanceData& InstanceData;

	UE_DEPRECATED(5.6, "Use Storage instead.")
	/** Data storage of the instance data, cached for less indirections. */
	FMetaStoryInstanceStorage* InstanceDataStorage = nullptr;

	/** Events queue to use, cached for less indirections. */
	TSharedPtr<FMetaStoryEventQueue> EventQueue;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.6, "Use LinkedAssetMetaStoryOverrides instead of the raw pointer.")
	/** Pointer to linked MetaStory overrides (editor-only legacy). */
	const FMetaStoryReferenceOverrides* LinkedMetaStoryOverrides = nullptr;
#endif
	/** Current linked MetaStory overrides. */
	FMetaStoryReferenceOverrides LinkedAssetMetaStoryOverrides;
	
	/** Data view of the context data. */
	TArray<FMetaStoryDataView> ContextAndExternalDataViews;

	FOnCollectMetaStoryExternalData CollectExternalDataDelegate;

	struct FCollectedExternalDataCache
	{
		const UMetaStory* MetaStory = nullptr;
		FMetaStoryIndex16 BaseIndex;
	};
	TArray<FCollectedExternalDataCache> CollectedExternalCache;
	bool bActiveExternalDataCollected = false;

	/** Holds the container. Multiple instances with the same MetaStory can occur in a recursive call. */
	struct FEvaluationScopeDataCache
	{
		UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer* Container;
		TObjectKey<UMetaStory> MetaStory;
	};

	static constexpr int32 ExpectedEvaluationScopeCacheLength = 4;
	/** Data view of the evaluation scope instance data. */
	TArray<FEvaluationScopeDataCache, TInlineAllocator<ExpectedEvaluationScopeCacheLength>> EvaluationScopeInstanceCaches;

	UE_API void PushEvaluationScopeInstanceContainer(UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer& Container, const FMetaStoryExecutionFrame& Frame);
	UE_API void PopEvaluationScopeInstanceContainer(UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer& Container);

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.6, "CompletedStateRunStatus is not used.")
	/** Run status set in FinishTask and that used after the task ticks. */
	TOptional<EMetaStoryRunStatus> CompletedStateRunStatus;

	UE_DEPRECATED(5.7, "Use RequestedTransition instead.")
	/** Next transition, used by RequestTransition(). */
	FMetaStoryTransitionResult NextTransition;

	UE_DEPRECATED(5.7, "Use RequestedTransition instead.")
	/** Structure describing the origin of the state transition that caused the state change. */
	FMetaStoryTransitionSource NextTransitionSource;
#endif 

	/** Current selected transition. */
	TUniquePtr<FRequestTransitionResult> RequestedTransition;

	/** When set, start the transitions loop from TriggerTransitionsFromFrameIndex. */
	TOptional<int32> TriggerTransitionsFromFrameIndex;

	/** Current frame we're processing. */
	const FMetaStoryExecutionFrame* CurrentlyProcessedParentFrame = nullptr; 
	const FMetaStoryExecutionFrame* CurrentlyProcessedFrame = nullptr; 

	/** Pointer to the shared instance data of the current frame we're processing. */
	FMetaStoryInstanceStorage* CurrentlyProcessedSharedInstanceStorage = nullptr;

	/** Helper struct to track currently processed frame. */
	struct FCurrentlyProcessedFrameScope
	{
		FCurrentlyProcessedFrameScope(FMetaStoryExecutionContext& InContext, const FMetaStoryExecutionFrame* CurrentParentFrame, const FMetaStoryExecutionFrame& CurrentFrame);
		FCurrentlyProcessedFrameScope(const FCurrentlyProcessedFrameScope&) = delete;
		FCurrentlyProcessedFrameScope& operator=(const FCurrentlyProcessedFrameScope&) = delete;
		~FCurrentlyProcessedFrameScope();

	private:
		FMetaStoryExecutionContext& Context;
		int32 SavedFrameIndex = 0;
		FMetaStoryInstanceStorage* SavedSharedInstanceDataStorage = nullptr;
		const FMetaStoryExecutionFrame* SavedFrame = nullptr;
		const FMetaStoryExecutionFrame* SavedParentFrame = nullptr;
	};

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.7, "The selection is now identified by the state ID.")
	/** Current state selection result when performing recursive state selection, or nullptr if not applicable. */
	const FStateSelectionResult* CurrentSelectionResult = nullptr;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

	/** Current state we're processing, or invalid if not applicable. */
	FMetaStoryStateHandle CurrentlyProcessedState;

	/** Helper struct to track currently processed state. */
	struct FCurrentlyProcessedStateScope
	{
		FCurrentlyProcessedStateScope(FMetaStoryExecutionContext& InContext, const FMetaStoryStateHandle State)
			: Context(InContext)
		{
			SavedState = Context.CurrentlyProcessedState;
			Context.CurrentlyProcessedState = State;
		}
		FCurrentlyProcessedStateScope(const FCurrentlyProcessedStateScope&) = delete;
		FCurrentlyProcessedStateScope& operator=(const FCurrentlyProcessedStateScope&) = delete;
		~FCurrentlyProcessedStateScope()
		{
			Context.CurrentlyProcessedState = SavedState;
		}

	private:
		FMetaStoryExecutionContext& Context;
		FMetaStoryStateHandle SavedState = FMetaStoryStateHandle::Invalid;
	};

	/** Current event we're processing in transition, or invalid if not applicable. */
	const FMetaStoryEvent* CurrentlyProcessedTransitionEvent = nullptr;

	/** Helper struct to track currently processed transition event. */
	struct FCurrentlyProcessedTransitionEventScope
	{
		FCurrentlyProcessedTransitionEventScope(FMetaStoryExecutionContext& InContext, const FMetaStoryEvent* Event)
			: Context(InContext)
		{
			check(Context.CurrentlyProcessedTransitionEvent == nullptr);
			Context.CurrentlyProcessedTransitionEvent = Event;
		}

		~FCurrentlyProcessedTransitionEventScope()
		{
			Context.CurrentlyProcessedTransitionEvent = nullptr;
		}

	private:
		FMetaStoryExecutionContext& Context;
	};

	/**
	 * Current select state result we're processing during the state selection, or invalid if not applicable.
	 * Used by the GetDataView when accessing event data.
	 * @note used only during selection and will always point to CurrentlyProcessedTemporaryStorage.
	 */
	FSelectStateResult* CurrentlyProcessedStateSelectionResult  = nullptr;

	/** Current temporary storage for instance data, frames and state. Valid during the state selection, ExitState and EnterState. */
	TSharedPtr<FSelectStateResult> CurrentlyProcessedTemporaryStorage;

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.7, "FMetaStoryFrameStateSelectionEvents is now unused. Use CurrentlyProcessedStateSelectionResult")
	FMetaStoryFrameStateSelectionEvents* CurrentlyProcessedStateSelectionEvents = nullptr;

	struct
	UE_DEPRECATED(5.7, "FMetaStoryFrameStateSelectionEvents is now unused. Use CurrentlyProcessedStateSelectionResult")
	FCurrentFrameStateSelectionEventsScope
	{
		FCurrentFrameStateSelectionEventsScope(FMetaStoryExecutionContext& InContext, FMetaStoryFrameStateSelectionEvents& InCurrentlyProcessedStateSelectionEvents)
			: Context(InContext)
		{
			SavedStateSelectionEvents = Context.CurrentlyProcessedStateSelectionEvents; 
			Context.CurrentlyProcessedStateSelectionEvents = &InCurrentlyProcessedStateSelectionEvents;
		}

		~FCurrentFrameStateSelectionEventsScope()
		{
			Context.CurrentlyProcessedStateSelectionEvents = SavedStateSelectionEvents;
		}

	private:
		FMetaStoryExecutionContext& Context;
		FMetaStoryFrameStateSelectionEvents* SavedStateSelectionEvents = nullptr;
	};
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

	/** True if transitions are allowed to be requested directly instead of buffering. */
	bool bAllowDirectTransitions = false;

	/** Helper struct to track when it is allowed to request transitions. */
	struct FAllowDirectTransitionsScope
	{
		FAllowDirectTransitionsScope(FMetaStoryExecutionContext& InContext)
			: Context(InContext)
		{
			bSavedAllowDirectTransitions = Context.bAllowDirectTransitions; 
			Context.bAllowDirectTransitions = true;
		}

		~FAllowDirectTransitionsScope()
		{
			Context.bAllowDirectTransitions = bSavedAllowDirectTransitions;
		}

	private:
		FMetaStoryExecutionContext& Context;
		bool bSavedAllowDirectTransitions = false;
	};

	/** Currently processed nodes instance data. Ideally we would pass these to the nodes directly, but do not want to change the API currently. */
	const FMetaStoryNodeBase* CurrentNode = nullptr;
	int32 CurrentNodeIndex = FMetaStoryIndex16::InvalidValue;
	FMetaStoryDataHandle CurrentNodeDataHandle;
	FMetaStoryDataView CurrentNodeInstanceData;

	/** Helper struct to set current node data. */
	struct FNodeInstanceDataScope
	{
		FNodeInstanceDataScope(FMetaStoryExecutionContext& InContext, const FMetaStoryNodeBase* InNode, const int32 InNodeIndex, const FMetaStoryDataHandle InNodeDataHandle, const FMetaStoryDataView InNodeInstanceData);
		~FNodeInstanceDataScope();

	private:
		FMetaStoryExecutionContext& Context;
		const FMetaStoryNodeBase* SavedNode = nullptr;
		int32 SavedNodeIndex = FMetaStoryIndex16::InvalidValue;
		FMetaStoryDataHandle SavedNodeDataHandle;
		FMetaStoryDataView SavedNodeInstanceData;
	};

	/** If true, the MetaStory context will create snapshots of transition events and capture them within RecordedTransitions for later use. */
	bool bRecordTransitions = false;

	/** Captured snapshots for transition results that can be used to recreate transitions. This array is only populated if bRecordTransitions is true. */
	TArray<FMetaStoryRecordedTransitionResult> RecordedTransitions;

	/** Memory mapping structure used for redirecting property-bag copies to external (raw) memory pointers */
	const FExternalGlobalParameters* ExternalGlobalParameters = nullptr;
};

/**
 * The const version of a MetaStory Execution Context that prevents using the FMetaStoryInstanceData with non-const member function.
 */
struct FConstMetaStoryExecutionContextView
{
public:
	FConstMetaStoryExecutionContextView(UObject& InOwner, const UMetaStory& InMetaStory, const FMetaStoryInstanceData& InInstanceData)
		: ExecutionContext(InOwner, InMetaStory, const_cast<FMetaStoryInstanceData&>(InInstanceData))
	{}

	operator const FMetaStoryExecutionContext& ()
	{
		return ExecutionContext;
	}

	const FMetaStoryExecutionContext& Get() const
	{
		return ExecutionContext;
	}

private:
	FConstMetaStoryExecutionContextView(const FConstMetaStoryExecutionContextView&) = delete;
	FConstMetaStoryExecutionContextView& operator=(const FConstMetaStoryExecutionContextView&) = delete;

private:
	FMetaStoryExecutionContext ExecutionContext;
};

#undef UE_API
