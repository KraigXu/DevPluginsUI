// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEvents.h"
#include "MetaStoryTypes.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEvents)

//----------------------------------------------------------------//
// FMetaStorySharedEvent
//----------------------------------------------------------------//

void FMetaStorySharedEvent::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	if (IsValid())
	{
		Collector.AddPropertyReferencesWithStructARO(FMetaStoryEvent::StaticStruct(), Event.Get());
	}
}


//----------------------------------------------------------------//
// FMetaStoryEventQueue
//----------------------------------------------------------------//

bool FMetaStoryEventQueue::SendEvent(const UObject* Owner, const FGameplayTag& Tag, const FConstStructView Payload, const FName Origin)
{
	if (!Tag.IsValid() && !Payload.IsValid())
	{
		UE_VLOG_UELOG(Owner, LogMetaStory, Error, TEXT("%s: An event with an invalid tag and payload has been sent to '%s'. This is not allowed."), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Owner));
		return false;
	}

	if (SharedEvents.Num() >= MaxActiveEvents)
	{
		UE_VLOG_UELOG(Owner, LogMetaStory, Error, TEXT("%s: Too many events send on '%s'. Dropping event %s"), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Owner), *Tag.ToString());
		return false;
	}

	SharedEvents.Emplace(Tag, Payload, Origin);
	return true;
}

bool FMetaStoryEventQueue::ConsumeEvent(const FMetaStorySharedEvent& Event)
{
	const int32 Count = SharedEvents.RemoveAllSwap([&EventToRemove = Event](const FMetaStorySharedEvent& Event)
	{
		return Event == EventToRemove;
	});
	return Count > 0;
}
