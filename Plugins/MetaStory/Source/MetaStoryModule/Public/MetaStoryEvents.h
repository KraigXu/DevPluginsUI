// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "StructUtils/StructView.h"
#include "MetaStoryIndexTypes.h"
#include "MetaStoryEvents.generated.h"

#define UE_API METASTORYMODULE_API

/** Enum used for flow control during event iteration. */
UENUM()
enum class EMetaStoryLoopEvents : uint8
{
	/** Continues to next event. */
	Next,
	/** Stops the event handling loop. */
	Break,
	/** Consumes and removes the current event. */
	Consume,
};

/**
 * MetaStory event with payload.
 */
USTRUCT(BlueprintType)
struct FMetaStoryEvent
{
	GENERATED_BODY()

	FMetaStoryEvent() = default;

	explicit FMetaStoryEvent(const FGameplayTag InTag)
		: Tag(InTag)
	{
	}
	
	explicit FMetaStoryEvent(const FGameplayTag InTag, const FConstStructView InPayload, const FName InOrigin)
		: Tag(InTag)
		, Payload(InPayload)
		, Origin(InOrigin)
	{
	}

	friend inline uint32 GetTypeHash(const FMetaStoryEvent& Event)
	{
		uint32 Hash = GetTypeHash(Event.Tag);
		
		if (Event.Payload.IsValid())
		{
			Hash = HashCombineFast(Hash, Event.Payload.GetScriptStruct()->GetStructTypeHash(Event.Payload.GetMemory()));
		}

		return Hash;
	}
	
	/** Tag describing the event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default", meta=(Categories="MetaStoryEvent"))
	FGameplayTag Tag;

	/** Optional payload for the event. */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	FInstancedStruct Payload;

	/** Optional info to describe who sent the event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	FName Origin;
};

/**
 * A struct wrapping FMetaStoryEvent in shared struct, used to make it easier to refer to the events during State Tree update.
 */
USTRUCT()
struct FMetaStorySharedEvent
{
	GENERATED_BODY()

	FMetaStorySharedEvent() = default;

	explicit FMetaStorySharedEvent(const FGameplayTag InTag, const FConstStructView InPayload, const FName InOrigin)
		: Event(MakeShared<FMetaStoryEvent>(InTag, InPayload, InOrigin))
	{}

	explicit FMetaStorySharedEvent(const FMetaStoryEvent& InEvent)
		: Event(MakeShared<FMetaStoryEvent>(InEvent))
	{}

	void AddStructReferencedObjects(FReferenceCollector& Collector);

	const FMetaStoryEvent* Get() const
	{
		return Event.Get();
	}

	FMetaStoryEvent* GetMutable()
	{
		return Event.Get();
	}

	const FMetaStoryEvent* operator->() const
	{
		return Event.Get();
	}

	FMetaStoryEvent* operator->()
	{
		return Event.Get();
	}

	const FMetaStoryEvent& operator*()
	{
		check(Event.IsValid());
		return *Event.Get();
	}

	FMetaStoryEvent& operator*() const
	{
		check(Event.IsValid());
		return *Event.Get();
	}

	bool IsValid() const
	{
		return Event.IsValid();
	}

	bool operator==(const FMetaStorySharedEvent& Other) const
	{
		return Event == Other.Event;
	}

protected:
	TSharedPtr<FMetaStoryEvent> Event;
};

template<>
struct TStructOpsTypeTraits<FMetaStorySharedEvent> : public TStructOpsTypeTraitsBase2<FMetaStorySharedEvent>
{
	enum
	{
		WithAddStructReferencedObjects = true,
	};
};

/**
 * Event queue buffering all the events to be processed by a State Tree.
 */
USTRUCT()
struct FMetaStoryEventQueue
{
	GENERATED_BODY()

	/** Maximum number of events that can be buffered. */
	static constexpr int32 MaxActiveEvents = 64;

	/** @return const view to all the events in the buffer. */
	TConstArrayView<FMetaStorySharedEvent> GetEventsView() const
	{
		return SharedEvents;
	}

	/** @return view to all the events in the buffer. */
	TArrayView<FMetaStorySharedEvent> GetMutableEventsView()
	{
		return SharedEvents;
	}

	/** Resets the events in the event queue */
	void Reset()
	{
		SharedEvents.Reset();
	}

	/** @return true if the queue has any events. */
	bool HasEvents() const
	{
		return !SharedEvents.IsEmpty();
	}
	
	/**
	 * Buffers and event to be sent to the State Tree.
	 * @param Owner Optional pointer to an owner UObject that is used for logging errors.
	 * @param Tag tag identifying the event.
	 * @param Payload Optional reference to the payload struct.
	 * @param Origin Optional name identifying the origin of the event.
	 * @return true if successfully added the event to the events queue.
	 */
	UE_API bool SendEvent(const UObject* Owner, const FGameplayTag& Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName());

	/**
	 * Consumes and removes the specified event from the event queue.
	 * @return true if successfully found and removed the event from the queue.
	 */
	UE_API bool ConsumeEvent(const FMetaStorySharedEvent& Event);

	/**
	 * Iterates over all events.
	 * @param Function a lambda which takes const FMetaStorySharedEvent& Event, and returns EMetaStoryLoopEvents.
	 */
	template<typename TFunc>
	void ForEachEvent(TFunc&& Function)
	{
		for (TArray<FMetaStorySharedEvent>::TIterator It(SharedEvents); It; ++It)
		{
			const EMetaStoryLoopEvents Result = Function(*It);
			if (Result == EMetaStoryLoopEvents::Break)
			{
				break;
			}
			if (Result == EMetaStoryLoopEvents::Consume)
			{
				It.RemoveCurrent();
			}
		}
	}

protected:
	// Used by FMetaStoryExecutionState to implement deprecated functionality.
	TArray<FMetaStorySharedEvent>& GetEventsArray() { return SharedEvents; };

	UPROPERTY()
	TArray<FMetaStorySharedEvent> SharedEvents;

	friend struct FMetaStoryInstanceData;
};

#undef UE_API
