// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/InstancedStructContainer.h"
#include "Debugger/MetaStoryRuntimeValidation.h"
#include "MetaStoryEvents.h"
#include "MetaStoryExecutionRuntimeDataTypes.h"
#include "MetaStoryInstanceContainer.h"
#include "MetaStoryTypes.h"
#include "MetaStoryExecutionTypes.h"
#include "Misc/MTAccessDetector.h"
#include "Templates/PimplPtr.h"
#include "Templates/SharedPointer.h"
#include "MetaStoryInstanceData.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryTransitionRequest;
struct FMetaStoryExecutionState;
struct FMetaStoryDelegateDispatcher;

#if WITH_METASTORY_DEBUG
namespace UE::MetaStory::Debug
{
	class FRuntimeValidationInstanceData;
}
#endif

namespace UE::MetaStory::InstanceData
{
	/** @return data view of the specified handle relative to the given frame. */
	[[nodiscard]] UE_API FMetaStoryDataView GetDataView(
		FMetaStoryInstanceStorage& InstanceStorage,
		FMetaStoryInstanceStorage* SharedInstanceStorage,
		const FMetaStoryExecutionFrame* ParentFrame,
		const FMetaStoryExecutionFrame& CurrentFrame,
		const FMetaStoryDataHandle& Handle);

	/** @return data view of the specified handle relative to the given frame, or tries to find a matching temporary instance. */
	[[nodiscard]] UE_API FMetaStoryDataView GetDataViewOrTemporary(
		FMetaStoryInstanceStorage& InstanceStorage,
		FMetaStoryInstanceStorage* SharedInstanceStorage,
		const FMetaStoryExecutionFrame* ParentFrame,
		const FMetaStoryExecutionFrame& CurrentFrame,
		const FMetaStoryDataHandle& Handle);
}

/**
 * Holds temporary instance data created during state selection.
 * The data is identified by Frame and DataHandle.
 */
USTRUCT()
struct FMetaStoryTemporaryInstanceData
{
	GENERATED_BODY()

	FMetaStoryTemporaryInstanceData() = default;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryTemporaryInstanceData(const FMetaStoryTemporaryInstanceData& Other) = default;
	FMetaStoryTemporaryInstanceData(FMetaStoryTemporaryInstanceData&& Other) = default;
	FMetaStoryTemporaryInstanceData& operator=(FMetaStoryTemporaryInstanceData const& Other) = default;
	FMetaStoryTemporaryInstanceData& operator=(FMetaStoryTemporaryInstanceData&& Other) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE::MetaStory::FActiveFrameID FrameID;

	UPROPERTY()
	FMetaStoryDataHandle DataHandle = FMetaStoryDataHandle::Invalid;

	UPROPERTY()
	FMetaStoryIndex16 OwnerNodeIndex = FMetaStoryIndex16::Invalid; 
	
	UPROPERTY()
	FInstancedStruct Instance;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.6, "Use the frame id to identify the frame.")
	UPROPERTY()
	TObjectPtr<const UMetaStory> MetaStory = nullptr;

	UE_DEPRECATED(5.6, "Use the frame id to identify the frame.")
	UPROPERTY()
	FMetaStoryStateHandle RootState = FMetaStoryStateHandle::Invalid; 
#endif
};


struct FMetaStoryInstanceStorageCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,
		// Added custom serialization
		AddedCustomSerialization,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	/** The GUID for this custom version number */
	UE_API const static FGuid GUID;

private:
	FMetaStoryInstanceStorageCustomVersion() = default;
};

/**
 * State Tree instance data is used to store the runtime state of a State Tree. It is used together with FMetaStoryExecution context to tick the state tree.
 * You are supposed to use FMetaStoryInstanceData as a property to store the instance data. That ensures that any UObject references will get GC'd correctly.
 *
 * The FMetaStoryInstanceData wraps FMetaStoryInstanceStorage, where the data is actually stored. This indirection is done in order to allow the FMetaStoryInstanceData
 * to be bitwise relocatable (e.g. you can put it in an array), and we can still allow delegates to bind to the instance data of individual tasks.
 *
 * Since the tasks in the instance data are stored in a array that may get resized you will need to use TStateTreeInstanceDataStructRef
 * to reference a struct based task instance data. It is defined below, and has example how to use it.
 */

/** Storage for the actual instance data. */
USTRUCT()
struct FMetaStoryInstanceStorage
{
	GENERATED_BODY()

	UE_API FMetaStoryInstanceStorage();
	UE_API FMetaStoryInstanceStorage(const FMetaStoryInstanceStorage& Other);
	UE_API FMetaStoryInstanceStorage(FMetaStoryInstanceStorage&& Other) noexcept;

	UE_API FMetaStoryInstanceStorage& operator=(const FMetaStoryInstanceStorage& Other);
	UE_API FMetaStoryInstanceStorage& operator=(FMetaStoryInstanceStorage&& Other) noexcept;

	/** @return reference to the event queue. */
	FMetaStoryEventQueue& GetMutableEventQueue()
	{
		return *EventQueue;
	}

	/** @return reference to the event queue. */
	const FMetaStoryEventQueue& GetEventQueue() const
	{
		return *EventQueue;
	}

	/** @return true if the storage owns the event queue. */
	bool IsOwningEventQueue() const
	{
		return bIsOwningEventQueue;
	}

	/** @return shared pointer to the event queue. */
	const TSharedRef<FMetaStoryEventQueue>& GetSharedMutableEventQueue()
	{
		return EventQueue;
	}

	/** Sets event queue from another storage. Marks the event queue not owned. */
	UE_API void SetSharedEventQueue(const TSharedRef<FMetaStoryEventQueue>& InSharedEventQueue);
	
	/** 
	 * Buffers a transition request to be sent to the State Tree.
	 * @param Owner Optional pointer to an owner UObject that is used for logging errors.
	 * @param Request transition to request.
	*/
	UE_API void AddTransitionRequest(const UObject* Owner, const FMetaStoryTransitionRequest& Request);

	/** Marks delegate as broadcasted. Use for transitions. */
	UE_API void MarkDelegateAsBroadcasted(const FMetaStoryDelegateDispatcher& Dispatcher);

	/** @return true if a delegate was broadcasted. */
	UE_API bool IsDelegateBroadcasted(const FMetaStoryDelegateDispatcher& Dispatcher) const;

	/** Resets the list of broadcasted delegates. */
	UE_API void ResetBroadcastedDelegates();

	/** @return true if there's any broadcasted delegates. */
	UE_API bool HasBroadcastedDelegates() const;

	/** @return currently pending transition requests. */
	TConstArrayView<FMetaStoryTransitionRequest> GetTransitionRequests() const
	{
		return TransitionRequests;
	}

	/** Reset all pending transition requests. */
	UE_API void ResetTransitionRequests();

	/** @return true if all instances are valid. */
	UE_API bool AreAllInstancesValid() const;

	/** @return number of items in the storage. */
	int32 Num() const
	{
		return InstanceStructs.Num();
	}

	/** @return true if the index can be used to get data. */
	bool IsValidIndex(const int32 Index) const
	{
		return InstanceStructs.IsValidIndex(Index);
	}

	/** @return true if item at the specified index is object type. */
	bool IsObject(const int32 Index) const
	{
		return InstanceStructs[Index].GetScriptStruct() == TBaseStructure<FMetaStoryInstanceObjectWrapper>::Get();
	}

	/** @return specified item as struct. */
	FConstStructView GetStruct(const int32 Index) const
	{
		return InstanceStructs[Index];
	}
	
	/** @return specified item as mutable struct. */
	FStructView GetMutableStruct(const int32 Index)
	{
		return InstanceStructs[Index];
	}

	/** @return specified item as object, will check() if the item is not an object. */
	const UObject* GetObject(const int32 Index) const
	{
		const FMetaStoryInstanceObjectWrapper& Wrapper = InstanceStructs[Index].Get<const FMetaStoryInstanceObjectWrapper>();
		return Wrapper.InstanceObject;
	}

	/** @return specified item as mutable Object, will check() if the item is not an object. */
	UObject* GetMutableObject(const int32 Index) const
	{
		const FMetaStoryInstanceObjectWrapper& Wrapper = InstanceStructs[Index].Get<const FMetaStoryInstanceObjectWrapper>();
		return Wrapper.InstanceObject;
	}

	/** @return reference to MetaStory execution state. */
	const FMetaStoryExecutionState& GetExecutionState() const
	{
		return ExecutionState;
	}

	/** @return reference to MetaStory execution state. */
	FMetaStoryExecutionState& GetMutableExecutionState()
	{
		return ExecutionState;
	}

	/** @return the storage's execution runtime data for the state tree. */
	[[nodiscard]] const UE::MetaStory::InstanceData::FMetaStoryInstanceContainer& GetExecutionRuntimeData() const
	{
		return ExecutionRuntimeData;
	}

	/** @return the storage's execution runtime data for the state tree. */
	[[nodiscard]] UE::MetaStory::InstanceData::FMetaStoryInstanceContainer& GetExecutionRuntimeData()
	{
		return ExecutionRuntimeData;
	}

	/**
	 * Add or reuse the execution runtime data for the state tree.
	 * Return the base index for the execution runtime data.
	 */
	[[nodiscard]] UE_API int32 AddExecutionRuntimeData(TNotNull<UObject*> Owner, UE::MetaStory::FMetaStoryExecutionFrameHandle FrameHandle);

	/**
	 * Adds temporary instance data associated with specified frame and data handle.
	 * @returns mutable struct view to the instance.
	 */
	UE_API FStructView AddTemporaryInstance(UObject& InOwner, const FMetaStoryExecutionFrame& Frame, const FMetaStoryIndex16 OwnerNodeIndex, const FMetaStoryDataHandle DataHandle, FConstStructView NewInstanceData);
	
	/** @returns mutable view to the specified instance data, or invalid view if not found. */
	UE_API FStructView GetMutableTemporaryStruct(const FMetaStoryExecutionFrame& Frame, const FMetaStoryDataHandle DataHandle);

	/** @returns mutable pointer to the specified instance data object, or invalid view if not found. Will check() if called on non-object data. */
	UE_API UObject* GetMutableTemporaryObject(const FMetaStoryExecutionFrame& Frame, const FMetaStoryDataHandle DataHandle);

	/** Empties the temporary instances. */
	UE_API void ResetTemporaryInstances();

	/** @return mutable array view to the temporary instances */
	TArrayView<FMetaStoryTemporaryInstanceData> GetMutableTemporaryInstances()
	{
		return TemporaryInstances;
	}

	/** Stores copy of provided parameters as State Tree global parameters. */
	UE_API void SetGlobalParameters(const FInstancedPropertyBag& Parameters);

	/** @return view to global parameters. */
	FConstStructView GetGlobalParameters() const
	{
		return GlobalParameters.GetValue();
	}

	/** @return mutable view to global parameters. */
	FStructView GetMutableGlobalParameters()
	{
		return GlobalParameters.GetMutableValue();
	}

	/** @return a unique number used to make active frame id and active state id. */
	UE_API uint32 GenerateUniqueId(); //@TODO rename to GenerateUniqueID

	/** Note, called by FMetaStoryInstanceData. */
	UE_API void AddStructReferencedObjects(FReferenceCollector& Collector);

	/** Resets the storage to initial state. */
	UE_API void Reset();

	/** Start the invalid multithreading read-only access detection. */
	UE_API void AcquireReadAccess();

	/** Stop the multitheading read-only access detection. */
	UE_API void ReleaseReadAccess();

	/** Start the invalid multithreading write access detection. */
	UE_API void AcquireWriteAccess();

	/** Stop the multitheading write access detection. */
	UE_API void ReleaseWriteAccess();

	/** Get the data used at runtime to confirm the inner working of the MetaStory. */
	UE_API UE::MetaStory::Debug::FRuntimeValidation GetRuntimeValidation() const;

protected:
	/**
	 * Struct for global and active instances.
	 * The buffer format is:
	 *  for each frames
	 *    Global parameters, if it's a global frame.
	 *    Global node instances, if it's a global frame. (evaluator, global tasks)
	 *    Active state parameters
	 *    Active node instances (tasks)
	 * @note Not transient, as we use FMetaStoryInstanceData to store default values for instance data.
	 */
	UPROPERTY()
	FInstancedStructContainer InstanceStructs;

	/** Execution state of the state tree instance. */
	UPROPERTY(Transient)
	FMetaStoryExecutionState ExecutionState;

	/**
	 * Struct for the execution runtime data.
	 * They stay alive until the owning execution context stops.
	 */
	UPROPERTY(Transient)
	UE::MetaStory::InstanceData::FMetaStoryInstanceContainer ExecutionRuntimeData;

	/** Info to find the index where the execution runtime data starts for a specific state tree. */
	struct FExecutionRuntimeInfo
	{
		FObjectKey MetaStory;
		int32 StartIndex = 0;
	};
	TArray<FExecutionRuntimeInfo, TInlineAllocator<1>> ExecutionRuntimeDataInfos;

	/** Temporary instances */
	UPROPERTY(Transient)
	TArray<FMetaStoryTemporaryInstanceData> TemporaryInstances;

	/** Events (Transient) */
	TSharedRef<FMetaStoryEventQueue> EventQueue = MakeShared<FMetaStoryEventQueue>();
	
	/** Array of broadcasted delegates. */
	TArray<FMetaStoryDelegateDispatcher> BroadcastedDelegates;

	/** Requested transitions */
	UPROPERTY(Transient)
	TArray<FMetaStoryTransitionRequest> TransitionRequests;

	/** Global parameters */
	UPROPERTY(Transient)
	FInstancedPropertyBag GlobalParameters;

	/** Unique id.  */
	UPROPERTY(Transient)
	uint32 UniqueIdGenerator = 0;

	/**
	 * Used to detect if we are using the instance data on multiple threads in a safe way.
	 * The instance data supports multiple reader threads or a single writer thread.
	 * The detector supports recursive access.
	 */
	UE_MT_DECLARE_MRSW_RECURSIVE_ACCESS_DETECTOR(AccessDetector);

	/* True if the storage owns the event queue. */
	bool bIsOwningEventQueue = true;

#if WITH_METASTORY_DEBUG
	TPimplPtr<UE::MetaStory::Debug::FRuntimeValidationInstanceData> RuntimeValidationData;
#endif

	friend struct FMetaStoryInstanceData;
};

/**
 * MetaStory instance data is used to store the runtime state of a MetaStory.
 * The layout of the data is described in a FMetaStoryInstanceDataLayout.
 *
 * Note: If FMetaStoryInstanceData is placed on an struct, you must call AddStructReferencedObjects() manually,
 *		 as it is not automatically called recursively.   
 * Note: Serialization is supported only for FArchive::IsModifyingWeakAndStrongReferences(), that is replacing object references.
 */
USTRUCT()
struct FMetaStoryInstanceData
{
	GENERATED_BODY()

	UE_API FMetaStoryInstanceData();
	UE_API FMetaStoryInstanceData(const FMetaStoryInstanceData& Other);
	UE_API FMetaStoryInstanceData(FMetaStoryInstanceData&& Other) noexcept;

	UE_API FMetaStoryInstanceData& operator=(const FMetaStoryInstanceData& Other);
	UE_API FMetaStoryInstanceData& operator=(FMetaStoryInstanceData&& Other) noexcept;

	UE_API ~FMetaStoryInstanceData();

	struct FAddArgs
	{
		METASTORYMODULE_API static FAddArgs Default;
		/** Duplicate the object contained by object wrapper. */
		bool bDuplicateWrappedObject = true;
	};

	/** Initializes the array with specified items. */
	UE_API void Init(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, FAddArgs Args = FAddArgs::Default);
	UE_API void Init(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, FAddArgs Args = FAddArgs::Default);

	/** Appends new items to the instance. */
	UE_API void Append(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, FAddArgs Args = FAddArgs::Default);
	UE_API void Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, FAddArgs Args = FAddArgs::Default);

	/** Appends new items to the instance, and moves existing data into the allocated instances. */
	UE_API void Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<FInstancedStruct*> InInstancesToMove, FAddArgs Args = FAddArgs::Default);

	/** Shrinks the array sizes to specified lengths. Sizes must be small or equal than current size. */
	UE_API void ShrinkTo(const int32 Num);
	
	/** Shares the layout from another instance data, and copies the data over. */
	UE_API void CopyFrom(UObject& InOwner, const FMetaStoryInstanceData& InOther);

	/** Resets the data to empty. */
	UE_API void Reset();

	/** @return Number of items in the instance data. */
	int32 Num() const
	{
		return GetStorage().Num();
	}

	/** @return true if the specified index is valid index into the instance data container. */
	bool IsValidIndex(const int32 Index) const
	{
		return GetStorage().IsValidIndex(Index);
	}

	/** @return true if the data at specified index is object. */
	bool IsObject(const int32 Index) const
	{
		return GetStorage().IsObject(Index);
	}

	/** @return mutable view to the struct at specified index. */
	FStructView GetMutableStruct(const int32 Index)
	{
		return GetMutableStorage().GetMutableStruct(Index);
	}

	/** @return const view to the struct at specified index. */
	FConstStructView GetStruct(const int32 Index) const
	{
		return GetStorage().GetStruct(Index);
	}

	/** @return pointer to an instance object   */
	UObject* GetMutableObject(const int32 Index)
	{
		return GetMutableStorage().GetMutableObject(Index);
	}

	/** @return const pointer to an instance object   */
	const UObject* GetObject(const int32 Index) const
	{
		return GetStorage().GetObject(Index);
	}

	/** @return pointer to MetaStory execution state, or null if the instance data is not initialized. */
	const FMetaStoryExecutionState* GetExecutionState() const
	{
		return &GetStorage().GetExecutionState();
	}

	/** @return mutable pointer to MetaStory execution state, or null if the instance data is not initialized. */
	FMetaStoryExecutionState* GetMutableExecutionState()
	{
		return &GetMutableStorage().GetMutableExecutionState();
	}

	/** @return reference to the event queue. */
	UE_API FMetaStoryEventQueue& GetMutableEventQueue();
	UE_API const FMetaStoryEventQueue& GetEventQueue() const;
	UE_API const TSharedRef<FMetaStoryEventQueue>& GetSharedMutableEventQueue();

	/** @return true if the instance data owns its' event queue. */
	UE_API bool IsOwningEventQueue() const;

	/** Sets event queue from another instance data. Marks the event queue not owned. */
	UE_API void SetSharedEventQueue(const TSharedRef<FMetaStoryEventQueue>& InSharedEventQueue);

	/** 
	 * Buffers a transition request to be sent to the State Tree.
	 * @param Owner Optional pointer to an owner UObject that is used for logging errors.
	 * @param Request transition to request.
	*/
	UE_API void AddTransitionRequest(const UObject* Owner, const FMetaStoryTransitionRequest& Request);

	/** @return currently pending transition requests. */
	UE_API TConstArrayView<FMetaStoryTransitionRequest> GetTransitionRequests() const;

	/** Reset all pending transition requests. */
	UE_API void ResetTransitionRequests();

	/** @return true if all instances are valid. */
	UE_API bool AreAllInstancesValid() const;

	UE_API FMetaStoryInstanceStorage& GetMutableStorage();
	UE_API const FMetaStoryInstanceStorage& GetStorage() const;

	UE_API TWeakPtr<FMetaStoryInstanceStorage> GetWeakMutableStorage();
	UE_API TWeakPtr<const FMetaStoryInstanceStorage> GetWeakStorage() const;

	UE_API int32 GetEstimatedMemoryUsage() const;
	
	/** Type traits */
	UE_API bool Identical(const FMetaStoryInstanceData* Other, uint32 PortFlags) const;
	UE_API void AddStructReferencedObjects(FReferenceCollector& Collector);
	UE_API bool Serialize(FArchive& Ar);
	UE_API void GetPreloadDependencies(TArray<UObject*>& OutDeps);

	/**
	 * Adds temporary instance data associated with specified frame and data handle.
	 * @returns mutable struct view to the instance.
	 */
	FStructView AddTemporaryInstance(UObject& InOwner, const FMetaStoryExecutionFrame& Frame, const FMetaStoryIndex16 OwnerNodeIndex, const FMetaStoryDataHandle DataHandle, FConstStructView NewInstanceData)
	{
		return GetMutableStorage().AddTemporaryInstance(InOwner, Frame, OwnerNodeIndex, DataHandle, NewInstanceData);
	}
	
	/** @returns mutable view to the specified instance data, or invalid view if not found. */
	FStructView GetMutableTemporaryStruct(const FMetaStoryExecutionFrame& Frame, const FMetaStoryDataHandle DataHandle)
	{
		return GetMutableStorage().GetMutableTemporaryStruct(Frame, DataHandle);
	}

	/** @returns mutable pointer to the specified instance data object, or invalid view if not found. Will check() if called on non-object data. */
	UObject* GetMutableTemporaryObject(const FMetaStoryExecutionFrame& Frame, const FMetaStoryDataHandle DataHandle)
	{
		return GetMutableStorage().GetMutableTemporaryObject(Frame, DataHandle);
	}

	/** Empties the temporary instances. */
	void ResetTemporaryInstances()
	{
		return GetMutableStorage().ResetTemporaryInstances();
	}

	/** Get the data used at runtime to confirm the inner working of the MetaStory. */
	UE::MetaStory::Debug::FRuntimeValidation GetRuntimeValidation() const
	{
		return GetStorage().GetRuntimeValidation();
	}

protected:
	/** Storage for the actual instance data, always stores FMetaStoryInstanceStorage. */
	TSharedRef<FMetaStoryInstanceStorage> InstanceStorage = MakeShared<FMetaStoryInstanceStorage>();

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS

	UPROPERTY()
	TInstancedStruct<FMetaStoryInstanceStorage> InstanceStorage_DEPRECATED;
	
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
};

#if WITH_EDITORONLY_DATA
namespace UE::MetaStory
{
	void RegisterInstanceDataForLocalization();
}
#endif // WITH_EDITORONLY_DATA

template<>
struct TStructOpsTypeTraits<FMetaStoryInstanceData> : public TStructOpsTypeTraitsBase2<FMetaStoryInstanceData>
{
	enum
	{
		WithIdentical = true,
		WithAddStructReferencedObjects = true,
		WithSerializer = true,
		WithGetPreloadDependencies = true,
	};
};


/**
 * Stores indexed reference to a instance data struct.
 * The instance data structs may be relocated when the instance data composition changed. For that reason you cannot store pointers to the instance data.
 * This is often needed for example when dealing with delegate lambda's. This helper struct stores data to be able to find the instance data in the instance data array.
 * That way we can access the instance data even of the array changes, and the instance data moves in memory.
 *
 * Note that the reference is valid only during the lifetime of a task (between a call EnterState() and ExitState()). 
 *
 * You generally do not use this directly, but via FMetaStoryExecutionContext.
 *
 *	EMetaStoryRunStatus FTestTask::EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const
 *	{
 *		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
 *
 *		Context.GetWorld()->GetTimerManager().SetTimer(
 *	        InstanceData.TimerHandle,
 *	        [InstanceDataRef = Context.GetInstanceDataStructRef(*this)]()
 *	        {
 *	            if (FInstanceDataType* InstanceData = InstanceDataRef.GetPtr())
 *				{
 *		            ...
 *				}
 *	        },
 *	        Delay, true);
 *
 *	    return EMetaStoryRunStatus::Running;
 *	}
 */
template <typename T>
struct TStateTreeInstanceDataStructRef
{
	TStateTreeInstanceDataStructRef(FMetaStoryInstanceData& InInstanceData, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle InDataHandle)
		: WeakStorage(InInstanceData.GetWeakMutableStorage())
		, FrameID(CurrentFrame.FrameID)
		, DataHandle(InDataHandle)
	{
		checkf(InDataHandle.GetSource() == EMetaStoryDataSourceType::ActiveInstanceData
			|| InDataHandle.GetSource() == EMetaStoryDataSourceType::GlobalInstanceData,
			TEXT("TStateTreeInstanceDataStructRef supports only struct instance data."));
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TStateTreeInstanceDataStructRef(const TStateTreeInstanceDataStructRef& Other) = default;
	TStateTreeInstanceDataStructRef(TStateTreeInstanceDataStructRef&& Other) = default;
	TStateTreeInstanceDataStructRef& operator=(TStateTreeInstanceDataStructRef const& Other) = default;
	TStateTreeInstanceDataStructRef& operator=(TStateTreeInstanceDataStructRef&& Other) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	bool IsValid() const
	{
		return FrameID.IsValid() && DataHandle.IsValid();
	}

	T* GetPtr() const
	{
		TSharedPtr<FMetaStoryInstanceStorage> StoragePtr = WeakStorage.Pin();
		if (StoragePtr == nullptr)
		{
			return nullptr;
		}

		const FMetaStoryExecutionState& Exec = StoragePtr->GetExecutionState();
		const FMetaStoryExecutionFrame* CurrentFrame = Exec.FindActiveFrame(FrameID);

		FStructView Struct;
		if (CurrentFrame)
		{
			FMetaStoryInstanceStorage& Storage = *(StoragePtr.Get());
			if (IsHandleSourceValid(Storage, *CurrentFrame, DataHandle))
			{
				Struct = GetDataView(Storage, *CurrentFrame, DataHandle);
			}
			else
			{
				Struct = Storage.GetMutableTemporaryStruct(*CurrentFrame, DataHandle);
			}

			check(Struct.GetScriptStruct() == TBaseStructure<T>::Get());
		}
		else
		{
			// When selecting a state, the frame is not in the active list.
			FMetaStoryTemporaryInstanceData* ExistingInstance = StoragePtr->GetMutableTemporaryInstances().FindByPredicate([FrameID = FrameID, DataHandle = DataHandle](const FMetaStoryTemporaryInstanceData& TempInstance)
				{
					return TempInstance.FrameID == FrameID
						&& TempInstance.DataHandle == DataHandle;
				});
			if (ExistingInstance)
			{
				Struct = FStructView(ExistingInstance->Instance);
			}
		}
		return reinterpret_cast<T*>(Struct.GetMemory());
	}

protected:

	FStructView GetDataView(FMetaStoryInstanceStorage& Storage, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle) const
	{
		switch (DataHandle.GetSource())
		{
		case EMetaStoryDataSourceType::GlobalInstanceData:
			return Storage.GetMutableStruct(CurrentFrame.GlobalInstanceIndexBase.Get() + DataHandle.GetIndex());
		case EMetaStoryDataSourceType::ActiveInstanceData:
			return Storage.GetMutableStruct(CurrentFrame.ActiveInstanceIndexBase.Get() + DataHandle.GetIndex());
		default:
			checkf(false, TEXT("Unhandle case %s"), *UEnum::GetValueAsString(Handle.GetSource()));
		}
		return {};
	}
	
	bool IsHandleSourceValid(FMetaStoryInstanceStorage& Storage, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle Handle) const
	{
		switch (Handle.GetSource())
		{
		case EMetaStoryDataSourceType::GlobalInstanceData:
			return CurrentFrame.GlobalInstanceIndexBase.IsValid()
				&& Storage.IsValidIndex(CurrentFrame.GlobalInstanceIndexBase.Get() + Handle.GetIndex());

		case EMetaStoryDataSourceType::ActiveInstanceData:
			return CurrentFrame.ActiveInstanceIndexBase.IsValid()
				&& CurrentFrame.ActiveStates.Contains(Handle.GetState())
				&& Storage.IsValidIndex(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex());
		default:
			checkf(false, TEXT("Unhandle case %s"), *UEnum::GetValueAsString(Handle.GetSource()));
		}
		return false;
	}
	
	TWeakPtr<FMetaStoryInstanceStorage> WeakStorage = nullptr;
	UE::MetaStory::FActiveFrameID FrameID;
	FMetaStoryDataHandle DataHandle = FMetaStoryDataHandle::Invalid;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.6, "Use the frame id to identify the frame.")
	TWeakObjectPtr<const UMetaStory> WeakStateTree = nullptr;
	UE_DEPRECATED(5.6, "Use the frame id to identify the frame.")
	FMetaStoryStateHandle RootState = FMetaStoryStateHandle::Invalid;
#endif
};

#undef UE_API
