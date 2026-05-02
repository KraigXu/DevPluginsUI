// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MetaStoryTypes.h"
#include "MetaStoryDelegate.h"
#include "MetaStoryEvents.h"
#include "MetaStoryExecutionExtension.h"
#include "MetaStoryStatePath.h"
#include "MetaStoryTasksStatus.h"
#include "UObject/ObjectKey.h"

#include "MetaStoryExecutionTypes.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;
class UMetaStory;
struct FMetaStoryInstanceStorage;

/**
 * Enumeration for the different update phases.
 * This is used as context information when tracing debug events.
 */
UENUM()
enum class EMetaStoryUpdatePhase : uint8
{
	Unset							= 0,
	StartTree						UMETA(DisplayName = "Start Tree"),
	StopTree						UMETA(DisplayName = "Stop Tree"),
	StartGlobalTasks				UMETA(DisplayName = "Start Global Tasks & Evaluators"),
	StartGlobalTasksForSelection	UMETA(DisplayName = "Start Global Tasks & Evaluators for selection"),
	StopGlobalTasks					UMETA(DisplayName = "Stop Global Tasks & Evaluators"),
	StopGlobalTasksForSelection		UMETA(DisplayName = "Stop Global Tasks & Evaluators for selection"),
	TickMetaStory					UMETA(DisplayName = "Tick MetaStory"),
	ApplyTransitions				UMETA(DisplayName = "Transition"),
	TickTransitions					UMETA(DisplayName = "Tick Transitions"),
	TriggerTransitions				UMETA(DisplayName = "Trigger Transitions"),
	TickingGlobalTasks				UMETA(DisplayName = "Tick Global Tasks & Evaluators"),
	TickingTasks					UMETA(DisplayName = "Tick Tasks"),
	TransitionConditions			UMETA(DisplayName = "Transition conditions"),
	StateSelection					UMETA(DisplayName = "Try Enter"),
	TrySelectBehavior				UMETA(DisplayName = "Try Select Behavior"),
	EnterConditions					UMETA(DisplayName = "Enter conditions"),
	EnterStates						UMETA(DisplayName = "Enter States"),
	ExitStates						UMETA(DisplayName = "Exit States"),
	StateCompleted					UMETA(DisplayName = "State(s) Completed")
};


/** Status describing current run status of a MetaStory. */
UENUM(BlueprintType)
enum class EMetaStoryRunStatus : uint8
{
	/** Tree is still running. */
	Running,
	
	/** The MetaStory was requested to stop without a particular success or failure state. */
	Stopped,
	
	/** Tree execution has stopped on success. */
	Succeeded,

	/** Tree execution has stopped on failure. */
	Failed,

	/** Status not set. */
	Unset,
};


/** Status describing how a task finished. */
UENUM()
enum class EMetaStoryFinishTaskType : uint8
{	
	/** The task execution failed. */
	Failed,
	
	/** The task execution succeed. */
	Succeeded
};


/** State change type. Passed to EnterState() and ExitState() to indicate how the state change affects the state and Evaluator or Task is on. */
UENUM()
enum class EMetaStoryStateChangeType : uint8
{
	/** Not an activation */
	None,
	
	/** The state became activated or deactivated. */
	Changed,
	
	/** The state is parent of new active state and sustained previous active state. */
	Sustained,
};


/** Defines how to assign the result of a condition to evaluate.  */
UENUM()
enum class EMetaStoryConditionEvaluationMode : uint8
{
	/** Condition is evaluated to set the result. This is the normal behavior. */
	Evaluated,
	
	/** Do not evaluate the condition and force result to 'true'. */
	ForcedTrue,
	
	/** Do not evaluate the condition and force result to 'false'. */
	ForcedFalse,
};


/**
 * Handle to access an external struct or object.
 * Note: Use the templated version below. 
 */
USTRUCT()
struct FMetaStoryExternalDataHandle
{
	GENERATED_BODY()

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryExternalDataHandle() = default;
	FMetaStoryExternalDataHandle(const FMetaStoryExternalDataHandle& Other) = default;
	FMetaStoryExternalDataHandle(FMetaStoryExternalDataHandle&& Other) = default;
	FMetaStoryExternalDataHandle& operator=(FMetaStoryExternalDataHandle const& Other) = default;
	FMetaStoryExternalDataHandle& operator=(FMetaStoryExternalDataHandle&& Other) = default;
	UE_API PRAGMA_ENABLE_DEPRECATION_WARNINGS

	static const FMetaStoryExternalDataHandle Invalid;
	
	bool IsValid() const { return DataHandle.IsValid(); }

	UPROPERTY()
	FMetaStoryDataHandle DataHandle = FMetaStoryDataHandle::Invalid;
};

/**
 * Handle to access an external struct or object.
 * This reference handle can be used in MetaStory tasks and evaluators to have quick access to external data.
 * The type provided to the template is used by the linker and context to pass along the type.
 *
 * USTRUCT()
 * struct FExampleTask : public FMetaStoryTaskBase
 * {
 *    ...
 *
 *    bool Link(FMetaStoryLinker& Linker)
 *    {
 *      Linker.LinkExternalData(ExampleSubsystemHandle);
 *      return true;
 *    }
 * 
 *    EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const EMetaStoryStateChangeType ChangeType, const FMetaStoryTransitionResult& Transition)
 *    {
 *      const UExampleSubsystem& ExampleSubsystem = Context.GetExternalData(ExampleSubsystemHandle);
 *      ...
 *    }
 *
 *    TMetaStoryExternalDataHandle<UExampleSubsystem> ExampleSubsystemHandle;
 * }
 */
template<typename T, EMetaStoryExternalDataRequirement Req = EMetaStoryExternalDataRequirement::Required>
struct TMetaStoryExternalDataHandle : FMetaStoryExternalDataHandle
{
	typedef T DataType;
	static constexpr EMetaStoryExternalDataRequirement DataRequirement = Req;
};


/**
 * Describes an external data. The data can point to a struct or object.
 * The code that handles MetaStory ticking is responsible for passing in the actually data, see FMetaStoryExecutionContext.
 */
USTRUCT()
struct FMetaStoryExternalDataDesc
{
	GENERATED_BODY()

	FMetaStoryExternalDataDesc() = default;
	FMetaStoryExternalDataDesc(const UStruct* InStruct, const EMetaStoryExternalDataRequirement InRequirement) : Struct(InStruct), Requirement(InRequirement) {}

	FMetaStoryExternalDataDesc(const FName InName, const UStruct* InStruct, const FGuid InGuid)
		: Struct(InStruct)
		, Name(InName)
#if WITH_EDITORONLY_DATA
		, ID(InGuid)
#endif
	{}

	/** @return true if the DataView is compatible with the descriptor. */
	bool IsCompatibleWith(const FMetaStoryDataView& DataView) const
	{
		if (DataView.GetStruct()->IsChildOf(Struct))
		{
			return true;
		}
		
		if (const UClass* DataDescClass = Cast<UClass>(Struct))
		{
			if (const UClass* DataViewClass = Cast<UClass>(DataView.GetStruct()))
			{
				return DataViewClass->ImplementsInterface(DataDescClass);
			}
		}
		
		return false;
	}
	
	bool operator==(const FMetaStoryExternalDataDesc& Other) const
	{
		return Struct == Other.Struct && Requirement == Other.Requirement;
	}
	
	/** Class or struct of the external data. */
	UPROPERTY();
	TObjectPtr<const UStruct> Struct = nullptr;

	/**
	 * Name of the external data. Used only for bindable external data (enforced by the schema).
	 * External data linked explicitly by the nodes (i.e. LinkExternalData) are identified only
	 * by their type since they are used for unique instance of a given type.  
	 */
	UPROPERTY(VisibleAnywhere, Category = Common)
	FName Name;
	
	/** Handle/Index to the MetaStoryExecutionContext data views array */
	UPROPERTY();
	FMetaStoryExternalDataHandle Handle;

	/** Describes if the data is required or not. */
	UPROPERTY();
	EMetaStoryExternalDataRequirement Requirement = EMetaStoryExternalDataRequirement::Required;

#if WITH_EDITORONLY_DATA
	/** Unique identifier. Used only for bindable external data. */
	UPROPERTY()
	FGuid ID;
#endif
};


/** Transition request */
USTRUCT(BlueprintType)
struct FMetaStoryTransitionRequest
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryTransitionRequest() = default;

	explicit FMetaStoryTransitionRequest(const FMetaStoryStateLink& InStateLink)
		: TargetState(InStateLink.StateHandle)
		, Fallback(InStateLink.Fallback)
	{
	}

	explicit FMetaStoryTransitionRequest(
		const FMetaStoryStateHandle InTargetState, 
		const EMetaStoryTransitionPriority InPriority = EMetaStoryTransitionPriority::Normal,
		const EMetaStorySelectionFallback InFallback = EMetaStorySelectionFallback::None)
		: TargetState(InTargetState)
		, Priority(InPriority)
		, Fallback(InFallback)
	{
	}

	FMetaStoryTransitionRequest(const FMetaStoryTransitionRequest&) = default;
	FMetaStoryTransitionRequest(FMetaStoryTransitionRequest&&) = default;
	FMetaStoryTransitionRequest& operator=(const FMetaStoryTransitionRequest&) = default;
	FMetaStoryTransitionRequest& operator=(FMetaStoryTransitionRequest&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Target state of the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Default")
	FMetaStoryStateHandle TargetState;
	
	/** Priority of the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Default")
	EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::Normal;

	/** Fallback of the transition if it fails to select the target state */
	UPROPERTY(EditDefaultsOnly, Category = "Default")
	EMetaStorySelectionFallback Fallback = EMetaStorySelectionFallback::None;

	/** MetaStory frame that was active when the transition was requested. Filled in by the MetaStory execution context. */
	UE::MetaStory::FActiveFrameID SourceFrameID;
	/** MetaStory state that was active when the transition was requested. Filled in by the MetaStory execution context. */
	UE::MetaStory::FActiveStateID SourceStateID;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.6, "Use SourceFrameID to uniquely identify a frame.")
	/** MetaStory asset that was active when the transition was requested. Filled in by the MetaStory execution context. */
	UPROPERTY(meta = (FormerlySerializedAs = "SourceStateTree"))
	TObjectPtr<const UMetaStory> SourceMetaStory = nullptr;

	UE_DEPRECATED(5.6, "Use SourceFrameID to uniquely identify a frame.")
	/** Root state the execution frame where the transition was requested. Filled in by the MetaStory execution context. */
	UPROPERTY()
	FMetaStoryStateHandle SourceRootState = FMetaStoryStateHandle::Invalid;

	UE_DEPRECATED(5.6, "Use SourceStateID to uniquely identify a state.")
	/** Source state of the transition. Filled in by the MetaStory execution context. */
	UPROPERTY()
	FMetaStoryStateHandle SourceState;
#endif
};


/**
 * Describes an array of active states in a MetaStory.
 */
USTRUCT(BlueprintType)
struct FMetaStoryActiveStates
{
	GENERATED_BODY()

	/** Max number of active states. */
	static constexpr uint8 MaxStates = 8;

	FMetaStoryActiveStates() = default;
	
	UE_DEPRECATED(5.6, "Use the constructor with FActiveStateId.")
	explicit FMetaStoryActiveStates(const FMetaStoryStateHandle StateHandle)
	{
		Push(StateHandle, UE::MetaStory::FActiveStateID::Invalid);
	}
	explicit FMetaStoryActiveStates(const FMetaStoryStateHandle StateHandle, UE::MetaStory::FActiveStateID StateID)
	{
		Push(StateHandle, StateID);
	}
	
	/** Resets the active state array to empty. */
	void Reset()
	{
		NumStates = 0;
	}

	UE_DEPRECATED(5.6, "Use the Push override with the FActiveStateId.")
	/** Pushes new state at the back of the array and returns true if there was enough space. */
	bool Push(const FMetaStoryStateHandle StateHandle)
	{
		return Push(StateHandle, UE::MetaStory::FActiveStateID::Invalid);
	}

	/** Pushes new state at the back of the array and returns true if there was enough space. */
	bool Push(const FMetaStoryStateHandle StateHandle, UE::MetaStory::FActiveStateID StateID)
	{
		if ((NumStates + 1) > MaxStates)
		{
			return false;
		}

		States[NumStates] = StateHandle;
		StateIDs[NumStates] = StateID;
		++NumStates;
		
		return true;
	}

	UE_DEPRECATED(5.6, "Use PushFront override with FActiveStateId.")
	/** Pushes new state at the front of the array and returns true if there was enough space. */
	bool PushFront(const FMetaStoryStateHandle StateHandle)
	{
		return PushFront(StateHandle, UE::MetaStory::FActiveStateID::Invalid);
	}

	/** Pushes new state at the front of the array and returns true if there was enough space. */
	bool PushFront(const FMetaStoryStateHandle StateHandle, UE::MetaStory::FActiveStateID StateID)
	{
		if ((NumStates + 1) > MaxStates)
		{
			return false;
		}

		NumStates++;
		for (int32 Index = (int32)NumStates - 1; Index > 0; Index--)
		{
			States[Index] = States[Index - 1];
			StateIDs[Index] = StateIDs[Index - 1];
		}
		States[0] = StateHandle;
		StateIDs[0] = StateID;
		
		return true;
	}

	/** Pops a state from the back of the array and returns the popped value, or invalid handle if the array was empty. */
	FMetaStoryStateHandle Pop()
	{
		if (NumStates == 0)
		{
			return FMetaStoryStateHandle::Invalid;
		}

		const FMetaStoryStateHandle Ret = States[NumStates - 1];
		NumStates--;
		return Ret;
	}

	/** Sets the number of states, new states are set to invalid state. */
	void SetNum(const int32 NewNum)
	{
		check(NewNum >= 0 && NewNum <= MaxStates);
		if (NewNum > (int32)NumStates)
		{
			for (int32 Index = NumStates; Index < NewNum; Index++)
			{
				States[Index] = FMetaStoryStateHandle::Invalid;
				StateIDs[Index] = UE::MetaStory::FActiveStateID::Invalid;
			}
		}
		NumStates = static_cast<uint8>(NewNum);
	}

	/* Returns the corresponding state handle for the active state ID. */
	FMetaStoryStateHandle FindStateHandle(UE::MetaStory::FActiveStateID StateId) const
	{
		for (int32 Index = (int32)NumStates - 1; Index >= 0; Index--)
		{
			if (StateIDs[Index] == StateId)
			{
				return States[Index];
			}
		}

		return FMetaStoryStateHandle::Invalid;
	}

	/* Returns the corresponding state ID for the active state handle. */
	UE::MetaStory::FActiveStateID FindStateID(FMetaStoryStateHandle StateHandle) const
	{
		for (int32 Index = (int32)NumStates - 1; Index >= 0; Index--)
		{
			if (States[Index] == StateHandle)
			{
				return StateIDs[Index];
			}
		}

		return UE::MetaStory::FActiveStateID::Invalid;
	}

	/** Returns index of a state, searching in reverse order. */
	int32 IndexOfReverse(const FMetaStoryStateHandle StateHandle) const
	{
		for (int32 Index = (int32)NumStates - 1; Index >= 0; Index--)
		{
			if (States[Index] == StateHandle)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
	
	/** Returns index of a state, searching in reverse order. */
	int32 IndexOfReverse(const UE::MetaStory::FActiveStateID StateId) const
	{
		for (int32 Index = (int32)NumStates - 1; Index >= 0; Index--)
		{
			if (StateIDs[Index] == StateId)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
	
	/** Returns true of the array contains specified state. */
	bool Contains(const FMetaStoryStateHandle StateHandle) const
	{
		for (int32 Index = 0; Index < NumStates; ++Index)
		{
			if (States[Index] == StateHandle)
			{
				return true;
			}
		}
		return false;
	}

	/** Returns true of the array contains specified state within MaxNumStatesToCheck states. */
	bool Contains(const FMetaStoryStateHandle StateHandle, const uint8 MaxNumStatesToCheck) const
	{
		const int32 Num = (int32)FMath::Min(NumStates, MaxNumStatesToCheck);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			if (States[Index] == StateHandle)
			{
				return true;
			}
		}
		return false;
	}

	/** Returns true if the state id is inside the container. */
	bool Contains(const UE::MetaStory::FActiveStateID StateId) const
	{
		for (int32 Index = (int32)NumStates - 1; Index >= 0; Index--)
		{
			if (StateIDs[Index] == StateId)
			{
				return true;
			}
		}
		return false;
	}
	
	/** Returns last state in the array, or invalid state if the array is empty. */
	FMetaStoryStateHandle Last() const
	{
		return NumStates > 0 ? States[NumStates - 1] : FMetaStoryStateHandle::Invalid;
	}
	
	/** Returns number of states in the array. */
	int32 Num() const
	{
		return NumStates;
	}

	/** Returns true if the index is within array bounds. */
	bool IsValidIndex(const int32 Index) const
	{
		return Index >= 0 && Index < (int32)NumStates;
	}
	
	/** Returns true if the array is empty. */
	bool IsEmpty() const
	{
		return NumStates == 0;
	}

	/** Returns a specified state in the array. */
	inline const FMetaStoryStateHandle& operator[](const int32 Index) const
	{
		check(Index >= 0 && Index < (int32)NumStates);
		return States[Index];
	}

	/** Returns mutable reference to a specified state in the array. */
	inline FMetaStoryStateHandle& operator[](const int32 Index)
	{
		check(Index >= 0 && Index < (int32)NumStates);
		return States[Index];
	}

	/** Returns the active states from the States array. */
	operator TArrayView<FMetaStoryStateHandle>()
	{
		return TArrayView<FMetaStoryStateHandle>(&States[0], Num());
	}

	/** Returns the active states from the States array. */
	operator TConstArrayView<FMetaStoryStateHandle>() const
	{
		return TConstArrayView<FMetaStoryStateHandle>(&States[0], Num());
	}

	/** Returns a specified state in the array, or FMetaStoryStateHandle::Invalid if Index is out of array bounds. */
	FMetaStoryStateHandle GetStateSafe(const int32 Index) const
	{
		return (Index >= 0 && Index < (int32)NumStates) ? States[Index] : FMetaStoryStateHandle::Invalid;
	}

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	inline FMetaStoryStateHandle* begin() { return &States[0]; }
	inline FMetaStoryStateHandle* end  () { return &States[0] + Num(); }
	inline const FMetaStoryStateHandle* begin() const { return &States[0]; }
	inline const FMetaStoryStateHandle* end  () const { return &States[0] + Num(); }

	UE::MetaStory::FActiveStateID StateIDs[MaxStates];

	UPROPERTY(VisibleDefaultsOnly, Category = Default)
	FMetaStoryStateHandle States[MaxStates];

	UPROPERTY(VisibleDefaultsOnly, Category = Default)
	uint8 NumStates = 0;
};


UENUM()
enum class EMetaStoryTransitionSourceType : uint8
{
	Unset,
	Asset,
	ExternalRequest,
	Internal
};

/**
 * Describes the origin of an applied transition.
 */
USTRUCT()
struct FMetaStoryTransitionSource
{
	GENERATED_BODY()

	FMetaStoryTransitionSource() = default;

	UE_DEPRECATED(5.6, "Use the version that requires a pointer to the MetaStory asset.")
	explicit FMetaStoryTransitionSource(const EMetaStoryTransitionSourceType SourceType, const FMetaStoryIndex16 TransitionIndex, const FMetaStoryStateHandle TargetState, const EMetaStoryTransitionPriority Priority)
	: FMetaStoryTransitionSource(nullptr, SourceType, TransitionIndex, TargetState, Priority)
	{
	}

	UE_DEPRECATED(5.6, "Use the version that requires a pointer to the MetaStory asset.")
	explicit FMetaStoryTransitionSource(const FMetaStoryIndex16 TransitionIndex, const FMetaStoryStateHandle TargetState, const EMetaStoryTransitionPriority Priority)
	: FMetaStoryTransitionSource(nullptr, EMetaStoryTransitionSourceType::Asset, TransitionIndex, TargetState, Priority)
	{
	}

	UE_DEPRECATED(5.6, "Use the version that requires a pointer to the MetaStory asset.")
	explicit FMetaStoryTransitionSource(const EMetaStoryTransitionSourceType SourceType, const FMetaStoryStateHandle TargetState, const EMetaStoryTransitionPriority Priority)
	: FMetaStoryTransitionSource(nullptr, SourceType, FMetaStoryIndex16::Invalid, TargetState, Priority)
	{
	}

	UE_API explicit FMetaStoryTransitionSource(const UMetaStory* MetaStory, const EMetaStoryTransitionSourceType SourceType, const FMetaStoryIndex16 TransitionIndex, const FMetaStoryStateHandle TargetState, const EMetaStoryTransitionPriority Priority);

	explicit FMetaStoryTransitionSource(const UMetaStory* MetaStory, const FMetaStoryIndex16 TransitionIndex, const FMetaStoryStateHandle TargetState, const EMetaStoryTransitionPriority Priority)
	: FMetaStoryTransitionSource(MetaStory, EMetaStoryTransitionSourceType::Asset, TransitionIndex, TargetState, Priority)
	{
	}

	explicit FMetaStoryTransitionSource(const UMetaStory* MetaStory, const EMetaStoryTransitionSourceType SourceType, const FMetaStoryStateHandle TargetState, const EMetaStoryTransitionPriority Priority)
	: FMetaStoryTransitionSource(MetaStory, SourceType, FMetaStoryIndex16::Invalid, TargetState, Priority)
	{
	}

	void Reset()
	{
		*this = {};
	}

	/** The MetaStory asset owning the transition and state. */
	TWeakObjectPtr<const UMetaStory> Asset;

	/** Describes where the transition originated. */
	EMetaStoryTransitionSourceType SourceType = EMetaStoryTransitionSourceType::Unset;

	/* Index of the transition if from predefined asset transitions, invalid otherwise */
	FMetaStoryIndex16 TransitionIndex;

	/** Transition target state */
	FMetaStoryStateHandle TargetState = FMetaStoryStateHandle::Invalid;
	
	/** Priority of the transition that caused the state change. */
	EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::None;
};


#if WITH_METASTORY_TRACE
struct FMetaStoryInstanceDebugId
{
	FMetaStoryInstanceDebugId() = default;
	FMetaStoryInstanceDebugId(const uint32 InstanceId, const uint32 SerialNumber)
		: Id(InstanceId)
		, SerialNumber(SerialNumber)
	{
	}
	explicit FMetaStoryInstanceDebugId(const uint64 Id)
		: Id(static_cast<uint32>(Id >> 32))
		, SerialNumber(static_cast<uint32>(Id))
	{
	}
	
	bool IsValid() const { return Id != INDEX_NONE && SerialNumber != INDEX_NONE; }
	bool IsInvalid() const { return !IsValid(); }
	void Reset() { *this = Invalid; }

	bool operator==(const FMetaStoryInstanceDebugId& Other) const
	{
		return Id == Other.Id && SerialNumber == Other.SerialNumber;
	}

	bool operator!=(const FMetaStoryInstanceDebugId& Other) const
	{
		return !(*this == Other);
	}

	uint64 ToUint64() const
	{
		return (static_cast<uint64>(Id) << 32) | static_cast<uint64>(SerialNumber);
	}

	friend uint32 GetTypeHash(const FMetaStoryInstanceDebugId InstanceDebugId)
	{
		return HashCombine(InstanceDebugId.Id, InstanceDebugId.SerialNumber);
	}

	friend FString LexToString(const FMetaStoryInstanceDebugId InstanceDebugId)
	{
		return FString::Printf(TEXT("0x%llx"), InstanceDebugId.ToUint64());
	}

	static UE_API const FMetaStoryInstanceDebugId Invalid;
	
	uint32 Id = INDEX_NONE;
	uint32 SerialNumber = INDEX_NONE;
};
#endif // WITH_METASTORY_TRACE

/** Describes current state of a delayed transition. */
USTRUCT()
struct FMetaStoryTransitionDelayedState
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryTransitionDelayedState() = default;
	FMetaStoryTransitionDelayedState(const FMetaStoryTransitionDelayedState&) = default;
	FMetaStoryTransitionDelayedState(FMetaStoryTransitionDelayedState&&) = default;
	FMetaStoryTransitionDelayedState& operator=(const FMetaStoryTransitionDelayedState&) = default;
	FMetaStoryTransitionDelayedState& operator=(FMetaStoryTransitionDelayedState&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** The state ID that triggers the transition. */
	UE::MetaStory::FActiveStateID StateID;

	UE_DEPRECATED(5.6, "MetaStory is unused. Use StateID instead.")
	UPROPERTY()
	TObjectPtr<const UMetaStory> MetaStory = nullptr;

	UPROPERTY()
	FMetaStorySharedEvent CapturedEvent;

	UPROPERTY()
	float TimeLeft = 0.0f;

	UPROPERTY()
	uint32 CapturedEventHash = 0u;

	UE_DEPRECATED(5.6, "StateHandle is unused. Use StateID instead.")
	UPROPERTY()
	FMetaStoryStateHandle StateHandle;
	
	UPROPERTY()
	FMetaStoryIndex16 TransitionIndex = FMetaStoryIndex16::Invalid;
};


namespace UE::MetaStory
{
	/** Describes a finished task waiting to be processed by an execution context. */
	struct
	UE_DEPRECATED(5.6, "Replaced with FMetaStoryTasksCompletionStatus")
	FFinishedTask
	{
		enum class EReasonType : uint8
		{
			/** A global task finished. The FrameID and TaskIndex are valid. */
			GlobalTask,
			/** A task inside a state finished. The FrameID, StateID and TaskIndex are valid. */
			StateTask,
			/** An internal transition finish the state. The FrameID and StateID are valid. */
			InternalTransition,
		};
		FFinishedTask() = default;
		METASTORYMODULE_API FFinishedTask(FActiveFrameID FrameID, FActiveStateID StateID, FMetaStoryIndex16 TaskIndex, EMetaStoryRunStatus RunStatus, EReasonType Reason, bool bTickProcessed);
		/** Frame ID that identifies 1the active frame. */
		FActiveFrameID FrameID;
		/** State ID that contains the finished task. */
		FActiveStateID StateID;
		/** Task that is finished and needs to be processed. */
		FMetaStoryIndex16 TaskIndex = FMetaStoryIndex16::Invalid;
		/** The result of the finished task. */
		EMetaStoryRunStatus RunStatus = EMetaStoryRunStatus::Failed;
		/** The reason of the finished task. */
		EReasonType Reason = EReasonType::GlobalTask;
		/**
		 * Set to true if the task is completed before or during the TickTasks.
		 * Used to identify tasks that were completed and had the chance to be processed by the algo.
		 * If not processed, they won't trigger the transition in this frame.
		 */
		bool bTickProcessed = false;
	};

	/** Describes the reason behind MetaStory ticking. */
	UENUM()
	enum class EMetaStoryTickReason : uint8
	{
		None,
		/** A scheduled tick request is active. */
		ScheduledTickRequest,
		/** The tick is forced. The schema doesn't support scheduling. */
		Forced,
		/** An active state requested a custom tick rate. */
		StateCustomTickRate,
		/** A task in an active state request ticking. */
		TaskTicking,
		/** A transition in an active state request ticking. */
		TransitionTicking,
		/** A transition request occurs via RequestTransition. */
		TransitionRequest,
		/** An event need to be cleared. */
		Event,
		/** A state completed async and transition didn't tick yet. */
		CompletedState,
		/** A active delayed transitions is pending. */
		DelayedTransition,
		/** A broadcast delegate occurs via BroadcastDelegate, and a transition is listening to the delegate. */
		Delegate,
	};
}

/*
 * Information on how a MetaStory should tick next.
 */
USTRUCT()
struct FMetaStoryScheduledTick
{
	GENERATED_BODY()

public:
	FMetaStoryScheduledTick() = default;

	bool operator==(const FMetaStoryScheduledTick&) const = default;
	bool operator!=(const FMetaStoryScheduledTick&) const = default;

	/** Make a scheduled tick that returns Sleep. */
	static UE_API FMetaStoryScheduledTick MakeSleep();
	/** Make a scheduled tick that returns EveryFrame. */
	static UE_API FMetaStoryScheduledTick MakeEveryFrames(UE::MetaStory::EMetaStoryTickReason Reason = UE::MetaStory::EMetaStoryTickReason::None);
	/** Make a scheduled tick that returns NextFrame. */
	static UE_API FMetaStoryScheduledTick MakeNextFrame(UE::MetaStory::EMetaStoryTickReason Reason = UE::MetaStory::EMetaStoryTickReason::None);
	/** Make a scheduled tick that returns a tick rate. The value needs to be greater than zero. */
	static UE_API FMetaStoryScheduledTick MakeCustomTickRate(float DeltaTime, UE::MetaStory::EMetaStoryTickReason Reason = UE::MetaStory::EMetaStoryTickReason::None);

public:
	/** @return true if it doesn't need to tick until an event/delegate/transition/... occurs. */
	UE_API bool ShouldSleep() const;
	/** @return true if it the needs to tick every frame. */
	UE_API bool ShouldTickEveryFrames() const;
	/** @return true if it usually doesn't need to tick but it needs to tick once next frame. */
	UE_API bool ShouldTickOnceNextFrame() const;
	/** @return true if it has a custom tick rate. */
	UE_API bool HasCustomTickRate() const;
	/** @return the delay in seconds between ticks. */
	UE_API float GetTickRate() const;

	/** @return the reason why tick is required. */
	UE::MetaStory::EMetaStoryTickReason GetReason() const
	{
		return Reason;
	}

private:
	FMetaStoryScheduledTick(float DeltaTime, UE::MetaStory::EMetaStoryTickReason Reason)
		: NextDeltaTime(DeltaTime)
		, Reason(Reason)
	{}

	UPROPERTY()
	float NextDeltaTime = 0.0f;
	UE::MetaStory::EMetaStoryTickReason Reason = UE::MetaStory::EMetaStoryTickReason::None;
};


namespace UE::MetaStory
{
/*
 * ID of a scheduled tick request.
 */
struct FScheduledTickHandle
{
public:
	FScheduledTickHandle() = default;
	FScheduledTickHandle(const FScheduledTickHandle&) = default;

	METASTORYMODULE_API static FScheduledTickHandle GenerateNewHandle();

	bool IsValid() const
	{
		return Value != 0;
	}

	bool operator==(const FScheduledTickHandle& Other) const = default;
	bool operator!=(const FScheduledTickHandle& Other) const = default;

private:
	explicit FScheduledTickHandle(uint32 InValue)
		: Value(InValue)
	{}

	uint32 Value = 0;
};
} // namespace UE::MetaStory


/** Describes added delegate listeners. */
struct FMetaStoryDelegateActiveListeners
{
	FMetaStoryDelegateActiveListeners() = default;
	METASTORYMODULE_API ~FMetaStoryDelegateActiveListeners();

	/** Adds a delegate bound in the editor to the list. Safe to be called during broadcasting.*/
	METASTORYMODULE_API void Add(const FMetaStoryDelegateListener& Listener, FSimpleDelegate Delegate, UE::MetaStory::FActiveFrameID FrameID, UE::MetaStory::FActiveStateID StateID, FMetaStoryIndex16 OwningNodeIndex);

	/** Removes a delegate bound in the editor from the list. Safe to be called during broadcasting. */
	METASTORYMODULE_API void Remove(const FMetaStoryDelegateListener& Listener);

	/** Removes the listener by predicate. */
	METASTORYMODULE_API void RemoveAll(UE::MetaStory::FActiveFrameID FrameID);

	/** Removes the listener that match. */
	METASTORYMODULE_API void RemoveAll(UE::MetaStory::FActiveStateID StateID);

	/** Broadcasts the listener by predicate. */
	METASTORYMODULE_API void BroadcastDelegate(const FMetaStoryDelegateDispatcher& Dispatcher, const FMetaStoryExecutionState& Exec);

private:
	void RemoveUnbounds();

	struct FActiveListener
	{
		FActiveListener() = default;
		FActiveListener(const FMetaStoryDelegateListener& Listener, FSimpleDelegate InDelegate, UE::MetaStory::FActiveFrameID FrameID, UE::MetaStory::FActiveStateID StateID, FMetaStoryIndex16 OwningNodeIndex);

		bool IsValid() const
		{
			return Listener.IsValid() && Delegate.IsBound();
		}

		FMetaStoryDelegateListener Listener;
		FSimpleDelegate Delegate;
		UE::MetaStory::FActiveFrameID FrameID;
		UE::MetaStory::FActiveStateID StateID;
		FMetaStoryIndex16 OwningNodeIndex = FMetaStoryIndex16::Invalid;
	};

	TArray<FActiveListener> Listeners;
	uint32 BroadcastingLockCount : 31 = 0;
	uint32 bContainsUnboundListeners : 1 = false;
};

namespace UE::MetaStory
{
	/** Helper that identifies an execution frame with the root state and its tree asset. */
	USTRUCT()
	struct FMetaStoryExecutionFrameHandle
	{
		GENERATED_BODY()

		FMetaStoryExecutionFrameHandle() = default;
		FMetaStoryExecutionFrameHandle(TNotNull<const UMetaStory*> InMetaStory, FMetaStoryStateHandle InRootState)
			: MetaStory(InMetaStory)
			, RootState(InRootState)
		{
		}

		bool IsValid() const
		{
			return RootState.IsValid() && MetaStory != nullptr;
		}

		const UMetaStory* GetMetaStory() const
		{
			return MetaStory;
		}

		FMetaStoryStateHandle GetRootState() const
		{
			return RootState;
		}

private:
		UPROPERTY()
		TObjectPtr<const UMetaStory> MetaStory;

		UPROPERTY()
		FMetaStoryStateHandle RootState;
	};
}

/** Describes an active branch of a MetaStory. */
USTRUCT(BlueprintType)
struct FMetaStoryExecutionFrame
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryExecutionFrame() = default;
	FMetaStoryExecutionFrame(const FMetaStoryExecutionFrame&) = default;
	FMetaStoryExecutionFrame(FMetaStoryExecutionFrame&&) = default;
	FMetaStoryExecutionFrame& operator=(const FMetaStoryExecutionFrame&) = default;
	FMetaStoryExecutionFrame& operator=(FMetaStoryExecutionFrame&&) = default;
	UE_DEPRECATED(5.6, "The recorded frame doesn't have all the needed information to properly form an FMetaStoryExecutionFrame.")
	UE_API FMetaStoryExecutionFrame(const FMetaStoryRecordedExecutionFrame& RecordedExecutionFrame);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.7, "Use the other version of IsSameRoot.")
	bool IsSameFrame(const FMetaStoryExecutionFrame& OtherFrame) const
	{
		return MetaStory == OtherFrame.MetaStory && RootState == OtherFrame.RootState;
	}

	/** @return wherever the frame uses the MetaStory and root state. */
	bool HasSameRoot(const FMetaStoryExecutionFrame& OtherFrame) const
	{
		return MetaStory == OtherFrame.MetaStory && RootState == OtherFrame.RootState;
	}

	/** @return wherever the frame uses the MetaStory and root state. */
	bool HasRoot(const UE::MetaStory::FMetaStoryExecutionFrameHandle& FrameHandle) const
	{
		return MetaStory == FrameHandle.GetMetaStory() && RootState == FrameHandle.GetRootState();
	}

	/** @return wherever the frame uses the MetaStory and root state. */
	bool HasRoot(TNotNull<const UMetaStory*> InMetaStory, FMetaStoryStateHandle InRootState) const
	{
		return MetaStory == InMetaStory && RootState == InRootState;
	}
	
	/** The MetaStory used by the frame. */
	UPROPERTY()
	TObjectPtr<const UMetaStory> MetaStory = nullptr;

	/** The root state of the frame (e.g. Root state or a subtree). */
	UPROPERTY()
	FMetaStoryStateHandle RootState = FMetaStoryStateHandle::Root;
	
	/** Active states in this frame */
	UPROPERTY()
	FMetaStoryActiveStates ActiveStates;

	/** Flag to track the completion of a global task or a task from a state in the ActiveStates. */
	UPROPERTY(Transient)
	FMetaStoryTasksCompletionStatus ActiveTasksStatus;

	/** Unique frame ID for this frame. Can be used to identify the frame. */
	UE::MetaStory::FActiveFrameID FrameID;

	/**
	 * The evaluator or task node index that was "entered".
	 * Used during the Enter and Exit phase. A node can fail EnterState.
	 * Nodes after ActiveNodeIndex do not receive ExitState, because they didn't receive EnterState.
	 */
	FMetaStoryIndex16 ActiveNodeIndex = FMetaStoryIndex16::Invalid;

	/** First index of the external data for this frame. */
	UPROPERTY()
	FMetaStoryIndex16 ExternalDataBaseIndex = FMetaStoryIndex16::Invalid;

	/** Index within the instance data to the first global instance data (e.g. global tasks) */
	UPROPERTY()
	FMetaStoryIndex16 GlobalInstanceIndexBase = FMetaStoryIndex16::Invalid;

	/** Index within the instance data to the first active state's instance data (e.g. tasks) */
	UPROPERTY()
	FMetaStoryIndex16 ActiveInstanceIndexBase = FMetaStoryIndex16::Invalid;

	/** Index within the execution runtime data to the first execution runtime's instance data (e.g. tasks). */
	UPROPERTY()
	FMetaStoryIndex16 ExecutionRuntimeIndexBase = FMetaStoryIndex16::Invalid;

	/** Handle to the state parameter data, exists in ParentFrame. */
	UPROPERTY()
	FMetaStoryDataHandle StateParameterDataHandle = FMetaStoryDataHandle::Invalid; 

	/** Handle to the global parameter data, exists in ParentFrame. */
	UPROPERTY()
	FMetaStoryDataHandle GlobalParameterDataHandle = FMetaStoryDataHandle::Invalid;
	
	/** If true, the global tasks of the MetaStory should be handle in this frame. */
	UPROPERTY()
	uint8 bIsGlobalFrame : 1 = false;

	/**
	 * If true, the global tasks/evaluator received the "EnterState".
	 * Can be sustained or added via a linked state.
	 * Only call StateEnter when the state didn't previously receive a state enter.
	 */
	UPROPERTY()
	uint8 bHaveEntered : 1 = false;

#if defined(WITH_EDITORONLY_DATA) && WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.7, "States in ActiveStates are always valid.")
	uint8 NumCurrentlyActiveStates = 0;
#endif
};

/** Describes the execution state of the current MetaStory instance. */
USTRUCT()
struct FMetaStoryExecutionState
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryExecutionState() = default;
	FMetaStoryExecutionState(const FMetaStoryExecutionState&) = default;
	FMetaStoryExecutionState(FMetaStoryExecutionState&&) = default;
	FMetaStoryExecutionState& operator=(const FMetaStoryExecutionState&) = default;
	FMetaStoryExecutionState& operator=(FMetaStoryExecutionState&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
public:
	void Reset()
	{
		*this = FMetaStoryExecutionState();
	}

	/** @return the unique path of all the active states of all the active execution frames. */
	UE_API UE::MetaStory::FActiveStatePath GetActiveStatePath() const;

	UE_DEPRECATED(5.6, "FindAndRemoveExpiredDelayedTransitions is not used anymore.")
	/** Finds all delayed transition states for a specific transition and removes them. Returns their copies. */
	TArray<FMetaStoryTransitionDelayedState, TInlineAllocator<8>> FindAndRemoveExpiredDelayedTransitions(const UMetaStory* OwnerMetaStory, const FMetaStoryIndex16 TransitionIndex)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TArray<FMetaStoryTransitionDelayedState, TInlineAllocator<8>> Result;
		for (TArray<FMetaStoryTransitionDelayedState>::TIterator It = DelayedTransitions.CreateIterator(); It; ++It)
		{
			if (It->TimeLeft <= 0.0f && It->MetaStory == OwnerMetaStory && It->TransitionIndex == TransitionIndex)
			{
				Result.Emplace(MoveTemp(*It));
				It.RemoveCurrentSwap();
			}
		}

		return Result;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** @return the active frame that matches the unique frame ID. */
	UE_API const FMetaStoryExecutionFrame* FindActiveFrame(UE::MetaStory::FActiveFrameID FrameID) const;

	/** @return the active frame that matches the unique frame ID. */
	UE_API FMetaStoryExecutionFrame* FindActiveFrame(UE::MetaStory::FActiveFrameID FrameID);

	/** @return the active frame index that matches the unique frame ID. */
	UE_API int32 IndexOfActiveFrame(UE::MetaStory::FActiveFrameID FrameID) const;

	/** @return whether it contains any scheduled tick requests. */
	bool HasScheduledTickRequests() const
	{
		return ScheduledTickRequests.Num() > 0;
	}

	/** @return the best/smallest scheduled tick request of all the requests. */
	FMetaStoryScheduledTick GetScheduledTickRequest() const
	{
		return HasScheduledTickRequests() ? CachedScheduledTickRequest : FMetaStoryScheduledTick();
	}

	/** Adds a scheduled tick request. */
	UE_API UE::MetaStory::FScheduledTickHandle AddScheduledTickRequest(FMetaStoryScheduledTick ScheduledTick);

	/** Updates the scheduled tick of a previous request. */
	UE_API bool UpdateScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle, FMetaStoryScheduledTick ScheduledTick);

	/** Removes a request. */
	UE_API bool RemoveScheduledTickRequest(UE::MetaStory::FScheduledTickHandle Handle);

private:
	UE_API void CacheScheduledTickRequest();

public:
	/** Currently active frames (and states) */
	UPROPERTY()
	TArray<FMetaStoryExecutionFrame> ActiveFrames;

	/** Pending delayed transitions. */
	UPROPERTY()
	TArray<FMetaStoryTransitionDelayedState> DelayedTransitions;

	/** Used by MetaStory random-based operations. */
	UPROPERTY()
	FRandomStream RandomStream;

	/** Active delegate listeners. */
	FMetaStoryDelegateActiveListeners DelegateActiveListeners;

private:
	/** ScheduledTick */
	struct FScheduledTickRequest
	{
		UE::MetaStory::FScheduledTickHandle Handle;
		FMetaStoryScheduledTick ScheduledTick;
	};
	TArray<FScheduledTickRequest> ScheduledTickRequests;

	/** The current computed value from ScheduledTickRequests. Only valid when ScheduledTickRequests is not empty. */
	FMetaStoryScheduledTick CachedScheduledTickRequest;

public:
#if WITH_METASTORY_TRACE
	/** Id for the active instance used for debugging. */
	mutable FMetaStoryInstanceDebugId InstanceDebugId;
#endif

	/** Optional extension for the execution context. */
	UPROPERTY(Transient)
	TInstancedStruct<FMetaStoryExecutionExtension> ExecutionExtension;

	/** Result of last TickTasks */
	UPROPERTY()
	EMetaStoryRunStatus LastTickStatus = EMetaStoryRunStatus::Failed;

	/** Running status of the instance */
	UPROPERTY()
	EMetaStoryRunStatus TreeRunStatus = EMetaStoryRunStatus::Unset;

	/** Completion status stored if Stop was called during the Tick and needed to be deferred. */
	UPROPERTY()
	EMetaStoryRunStatus RequestedStop = EMetaStoryRunStatus::Unset;

	/** Current update phase used to validate reentrant calls to the main entry points of the execution context (i.e. Start, Stop, Tick). */
	UPROPERTY()
	EMetaStoryUpdatePhase CurrentPhase = EMetaStoryUpdatePhase::Unset;

	/** Number of times a new state has been changed. */
	UPROPERTY()
	uint16 StateChangeCount = 0;

	/** A task that completed a state or a global task that completed a global frame. */
	UPROPERTY()
	bool bHasPendingCompletedState = false;

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Replaced with FMetaStoryTasksCompletionStatus")
	/** Pending finished tasks that need processing. */
	TArray<UE::MetaStory::FFinishedTask> FinishedTasks;

	UE_DEPRECATED(5.6, "Use FinishTask to completed a state.")
	/** Handle of the state that was first to report state completed (success or failure), used to trigger completion transitions. */
	UPROPERTY()
	FMetaStoryIndex16 CompletedFrameIndex = FMetaStoryIndex16::Invalid; 
	
	UE_DEPRECATED(5.6, "Use FinishTask to completed a state.")
	UPROPERTY()
	FMetaStoryStateHandle CompletedStateHandle = FMetaStoryStateHandle::Invalid;

	UE_DEPRECATED(5.6, "CurrentExecutionContext is not needed anymore. Use FrameID and StateID.")
	FMetaStoryExecutionContext* CurrentExecutionContext = nullptr;

	UE_DEPRECATED(5.7, "Use FMetaStoryExecutionFrame::ActiveNodeIndex instead.")
	UPROPERTY()
	FMetaStoryIndex16 EnterStateFailedFrameIndex = FMetaStoryIndex16::Invalid;

	UE_DEPRECATED(5.7, "Use FMetaStoryExecutionFrame::ActiveNodeIndex instead.")
	UPROPERTY()
	FMetaStoryIndex16 EnterStateFailedTaskIndex = FMetaStoryIndex16::Invalid;

	UE_DEPRECATED(5.7, "Use FMetaStoryExecutionFrame::ActiveNodeIndex instead.")
	UPROPERTY()
	FMetaStoryIndex16 LastExitedNodeIndex = FMetaStoryIndex16::Invalid;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif //WITH_EDITORONLY_DATA
};

/** Contains MetaStory events used during State Selection for a single execution frame. */
struct
UE_DEPRECATED(5.7, "Use the MetaStoryExecutionContext.RequestTransitionResult.Selection.SelectionEvents instead.")
FMetaStoryFrameStateSelectionEvents
{
	TStaticArray<FMetaStorySharedEvent, FMetaStoryActiveStates::MaxStates> Events;
};

/**
 * Describes a MetaStory transition. Source is the state where the transition started, Target describes the state where the transition pointed at,
 * and Next describes the selected state. The reason Transition and Next are different is that Transition state can be a selector state,
 * in which case the children will be visited until a leaf state is found, which will be the next state.
 */
USTRUCT(BlueprintType)
struct FMetaStoryTransitionResult
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryTransitionResult() = default;
	FMetaStoryTransitionResult(const FMetaStoryTransitionResult&) = default;
	FMetaStoryTransitionResult(FMetaStoryTransitionResult&&) = default;
	FMetaStoryTransitionResult& operator=(const FMetaStoryTransitionResult&) = default;
	FMetaStoryTransitionResult& operator=(FMetaStoryTransitionResult&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.6, "Use FMetaStoryExecutionContext::MakeTransitionResult to crate a new transition.")
	UE_API FMetaStoryTransitionResult(const FMetaStoryRecordedTransitionResult& RecordedTransition);

	void Reset()
	{
		*this = FMetaStoryTransitionResult();
	}

	/** Frame that was active when the transition was requested. */
	UE::MetaStory::FActiveFrameID SourceFrameID;

	/**
	 * The state the transition was requested from.
	 * It can be invalid if the transition is requested outside the Tick.
	 */
	UE::MetaStory::FActiveStateID SourceStateID;

	/** Transition target state. It can be a completion state. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FMetaStoryStateHandle TargetState = FMetaStoryStateHandle::Invalid;

	/** The current state being executed. On enter/exit callbacks this is the state of the task. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	FMetaStoryStateHandle CurrentState = FMetaStoryStateHandle::Invalid;
	
	/** Current Run status. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	EMetaStoryRunStatus CurrentRunStatus = EMetaStoryRunStatus::Unset;

	/** If the change type is Sustained, then the CurrentState was reselected, or if Changed then the state was just activated. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	EMetaStoryStateChangeType ChangeType = EMetaStoryStateChangeType::Changed; 

	/** Priority of the transition that caused the state change. */
	UPROPERTY(EditDefaultsOnly, Category = "Default", BlueprintReadOnly)
	EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::None;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.7, "Use the MetaStoryExecutionContext.RequestTransitionResult.Selection.SelectedState instead.")
	/** States selected as result of the transition. */
	UPROPERTY(Category = "Default", BlueprintReadOnly, meta = (DeprecatedProperty))
	TArray<FMetaStoryExecutionFrame> NextActiveFrames;

	UE_DEPRECATED(5.7, "Use the MetaStoryExecutionContext.RequestTransitionResult.Selection.SelectionEvents instead.")
	/** Events used in state selection. */
	TArray<FMetaStoryFrameStateSelectionEvents> NextActiveFrameEvents;

	UE_DEPRECATED(5.6, "Use SourceFrameID instead")
	/** MetaStory asset that was active when the transition was requested. */
	UPROPERTY(Category = "Default", BlueprintReadOnly, meta = (DeprecatedProperty, FormerlySerializedAs = "SourceStateTree"))
	TObjectPtr<const UMetaStory> SourceMetaStory = nullptr;

	UE_DEPRECATED(5.6, "Use SourceFrameID instead.")
	/** Root state the execution frame where the transition was requested. */
	UPROPERTY(Category = "Default", BlueprintReadOnly, meta = (DeprecatedProperty))
	FMetaStoryStateHandle SourceRootState = FMetaStoryStateHandle::Invalid;

	UE_DEPRECATED(5.6, "Use SourceStateID instead")
	/** Transition source state. */
	UPROPERTY(Category = "Default", BlueprintReadOnly, meta=(DeprecatedProperty))
	FMetaStoryStateHandle SourceState = FMetaStoryStateHandle::Invalid;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
};

/*
 * Enumeration for the different transition recording types.
 * This is used by the execution context to capture transition snapshots if set to record.
*/
UENUM()
enum class EMetaStoryRecordTransitions : uint8
{
	No,
	Yes
};

/*
 * Captured MetaStory execution frame that can be cached for recording purposes.
 * Held in FMetaStoryRecordedTransitionResult for its NextActiveFrames.
 */
USTRUCT()
struct
UE_DEPRECATED(5.7, "FMetaStoryRecordedExecutionFrame is not used anymore.")
FMetaStoryRecordedExecutionFrame
{
	GENERATED_BODY()

	FMetaStoryRecordedExecutionFrame() = default;
	UE_DEPRECATED(5.6, "Use FMetaStoryExecutionContext::MakeRecordedTransitionResult to create a new recorded transition.")
	UE_API FMetaStoryRecordedExecutionFrame(const FMetaStoryExecutionFrame& ExecutionFrame);

	/** The MetaStory used for ticking this frame. */
	UPROPERTY()
	TObjectPtr<const UMetaStory> MetaStory = nullptr;

	/** The root state of the frame (e.g. Root state or a subtree). */
	UPROPERTY()
	FMetaStoryStateHandle RootState = FMetaStoryStateHandle::Root; 
	
	/** Active states in this frame. */
	UPROPERTY()
	FMetaStoryActiveStates ActiveStates;

	/** If true, the global tasks of the MetaStory should be handle in this frame. */
	UPROPERTY()
	uint8 bIsGlobalFrame : 1 = false;

	/** Captured indices of the events we've recorded. */
	TStaticArray<uint8, FMetaStoryActiveStates::MaxStates> EventIndices;
};

/** Captured state cached for recording purposes. */
USTRUCT()
struct FMetaStoryRecordedActiveState
{
	GENERATED_BODY()

	FMetaStoryRecordedActiveState() = default;

	/** The MetaStory that owns the state handle. */
	UPROPERTY()
	TObjectPtr<const UMetaStory> MetaStory;

	/** The active state. */
	UPROPERTY()
	FMetaStoryStateHandle State;

	/** Captured events from the transition that we've recorded */
	UPROPERTY()
	int32 EventIndex = INDEX_NONE;
};

/*
 * Captured MetaStory transition result that can be cached for recording purposes.
 * when transitions are recorded through this structure, we can replicate them down
 * to clients to keep our MetaStory in sync.
 */
USTRUCT()
struct FMetaStoryRecordedTransitionResult
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryRecordedTransitionResult() = default;
	FMetaStoryRecordedTransitionResult(const FMetaStoryRecordedTransitionResult&) = default;
	FMetaStoryRecordedTransitionResult(FMetaStoryRecordedTransitionResult&&) = default;
	FMetaStoryRecordedTransitionResult& operator=(const FMetaStoryRecordedTransitionResult&) = default;
	FMetaStoryRecordedTransitionResult& operator=(FMetaStoryRecordedTransitionResult&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.6, "Use FMetaStoryExecutionContext::MakeRecordedTransitionResult to create a new recorded transition.")
	UE_API FMetaStoryRecordedTransitionResult(const FMetaStoryTransitionResult& Transition);
	
	/** The selected states. */
	UPROPERTY()
	TArray<FMetaStoryRecordedActiveState> States;

	/** The selected states. */
	UPROPERTY()
	TArray<FMetaStoryEvent> Events;
	
	/** Priority of the transition that caused the state change. */
	UPROPERTY()
	EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::None;

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.7, "The recorded transition doesn't record the frame. ForceTransition will recreated them if needed.")
	/** States selected as result of the transition. */
	UPROPERTY()
	TArray<FMetaStoryRecordedExecutionFrame> NextActiveFrames;

	UE_DEPRECATED(5.7, "The event are recorded in SelectedStates.")
	/** Captured events from the transition that we've recorded */
	UPROPERTY()
	TArray<FMetaStoryEvent> NextActiveFrameEvents;

	UE_DEPRECATED(5.7, "We can't assumed that the recorded has the same active states. The source won't be set.")
	/** Transition source state. */
	UPROPERTY()
	FMetaStoryStateHandle SourceState = FMetaStoryStateHandle::Invalid;

	UE_DEPRECATED(5.7, "The TargetState is always the last item. We can't assumed that the recorded has the same active states.")
	/** Transition target state. */
	UPROPERTY()
	FMetaStoryStateHandle TargetState = FMetaStoryStateHandle::Invalid;

	UE_DEPRECATED(5.7, "We can't assumed that the recorded has the same active states. The source won't be set.")
	/** MetaStory asset that was active when the transition was requested. */
	UPROPERTY(meta = (FormerlySerializedAs = "SourceStateTree"))
	TObjectPtr<const UMetaStory> SourceMetaStory = nullptr;

	UE_DEPRECATED(5.7, "We can't assumed that the recorded has the same active states. The source won't be set.")
	/** Root state the execution frame where the transition was requested. */
	UPROPERTY()
	FMetaStoryStateHandle SourceRootState = FMetaStoryStateHandle::Invalid;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif //WITH_EDITORONLY_DATA
};

#undef UE_API
