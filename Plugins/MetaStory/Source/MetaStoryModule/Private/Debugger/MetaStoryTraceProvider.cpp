// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_METASTORY_TRACE_DEBUGGER

#include "Debugger/MetaStoryTraceProvider.h"
#include "Debugger/MetaStoryDebugger.h"

FName FMetaStoryTraceProvider::ProviderName("MetaStoryDebuggerProvider");

#define LOCTEXT_NAMESPACE "MetaStoryDebuggerProvider"

FMetaStoryTraceProvider::FMetaStoryTraceProvider(TraceServices::IAnalysisSession& InSession)
	: Session(InSession)
{
}

bool FMetaStoryTraceProvider::ReadTimelines(const FMetaStoryInstanceDebugId InstanceId, const TFunctionRef<void(const FMetaStoryInstanceDebugId ProcessedInstanceId, const FEventsTimeline&)> Callback) const
{
	Session.ReadAccessCheck();

	// Read specific timeline if specified
	if (InstanceId.IsValid())
	{
		const uint32* IndexPtr = InstanceIdToDebuggerEntryTimelines.Find(InstanceId);
		if (IndexPtr != nullptr && EventsTimelines.IsValidIndex(*IndexPtr))
		{
			Callback(InstanceId, *EventsTimelines[*IndexPtr]);
			return true;
		}
	}
	else
	{
		for(auto It = InstanceIdToDebuggerEntryTimelines.CreateConstIterator(); It; ++It)
		{
			if (EventsTimelines.IsValidIndex(It.Value()))
			{
				Callback(It.Key(), *EventsTimelines[It.Value()]);
			}
		}

		return EventsTimelines.Num() > 0;
	}

	return false;
}

bool FMetaStoryTraceProvider::ReadTimelines(const UMetaStory& MetaStory, TFunctionRef<void(const FMetaStoryInstanceDebugId ProcessedInstanceId, const FEventsTimeline&)> Callback) const
{
	Session.ReadAccessCheck();

	for (auto It = InstanceIdToDebuggerEntryTimelines.CreateConstIterator(); It; ++It)
	{
		check(EventsTimelines.IsValidIndex(It.Value()));
		check(Descriptors.Num() == EventsTimelines.Num());

		if (Descriptors[It.Value()].Get().MetaStory == &MetaStory)
		{
			Callback(Descriptors[It.Value()].Get().Id, *EventsTimelines[It.Value()]);
		}
	}

	return EventsTimelines.Num() > 0;
}

void FMetaStoryTraceProvider::AppendEvent(const FMetaStoryInstanceDebugId InInstanceId, const double InTime, const FMetaStoryTraceEventVariantType& InEvent)
{
	Session.WriteAccessCheck();

	// It is currently possible to receive events from an instance without receiving event `EMetaStoryTraceEventType::Push` first
	// (i.e. traces were activated after the statetree instance execution was started).    
	// We plan to buffer the Instance events (Started/Stopped) to address this but for now we ignore events related to that instance.
	if (const uint32* IndexPtr = InstanceIdToDebuggerEntryTimelines.Find(InInstanceId))
	{
		EventsTimelines[*IndexPtr]->AppendEvent(InTime, InEvent);
	}
	
	Session.UpdateDurationSeconds(InTime);
}

void FMetaStoryTraceProvider::AppendInstanceEvent(
	const FMetaStoryIndex16 AssetDebugId,
	const FMetaStoryInstanceDebugId InInstanceId,
	const TCHAR* InInstanceName,
	const double InTime,
	const double InWorldRecordingTime,
	const EMetaStoryTraceEventType InEventType)
{
	if (InEventType == EMetaStoryTraceEventType::Push)
	{
		TWeakObjectPtr<const UMetaStory> WeakStateTree;
		if (GetAssetFromDebugId(AssetDebugId, WeakStateTree))
		{
			if (const UMetaStory* MetaStory = WeakStateTree.Get())
			{
				const TSharedRef<UE::MetaStoryDebugger::FInstanceDescriptor>* Descriptor = Descriptors.FindByPredicate(
					[InInstanceId](const TSharedRef<const UE::MetaStoryDebugger::FInstanceDescriptor>& Descriptor)
					{
						return Descriptor.Get().Id == InInstanceId;
					});

				// Possible to receive new Push when stopping/starting traces during the same session.
				// In that case we can reuse the existing entries
				if (Descriptor == nullptr)
				{
					Descriptors.Emplace(MakeShared<UE::MetaStoryDebugger::FInstanceDescriptor>(
						MetaStory
						, InInstanceId
						, InInstanceName
						, TRange<double>(InWorldRecordingTime, UE::MetaStoryDebugger::FInstanceDescriptor::ActiveInstanceEndTime)));

					check(InstanceIdToDebuggerEntryTimelines.Find(InInstanceId) == nullptr);
					InstanceIdToDebuggerEntryTimelines.Add(InInstanceId, EventsTimelines.Num());

					EventsTimelines.Emplace(MakeShared<TraceServices::TPointTimeline<FMetaStoryTraceEventVariantType>>(Session.GetLinearAllocator()));
				}
			}
			else
			{
				UE_LOG(LogMetaStory, Error, TEXT("Instance event refers to an unloaded asset."));
			}
		}
		else
		{
			UE_LOG(LogMetaStory, Error, TEXT("Instance event refers to an asset Id that wasn't added previously."));
		}
	}
	else if (InEventType == EMetaStoryTraceEventType::Pop)
	{
		// Process only if timeline can be found. See details in AppendEvent comment.
		if (const uint32* Index = InstanceIdToDebuggerEntryTimelines.Find(InInstanceId))
		{
			check(Descriptors.IsValidIndex(*Index));
			Descriptors[*Index].Get().Lifetime.SetUpperBound(InWorldRecordingTime);
		}
	}

	Session.UpdateDurationSeconds(InTime);
}

void FMetaStoryTraceProvider::AppendAssetDebugId(const UMetaStory* InStateTree, const FMetaStoryIndex16 AssetDebugId)
{
	TWeakObjectPtr<const UMetaStory> WeakStateTree;
	if (ensureMsgf(AssetDebugId.IsValid(), TEXT("Expecting valid asset debug Id."))
		&& !GetAssetFromDebugId(AssetDebugId, WeakStateTree))
	{
		MetaStoryAssets.Emplace(FMetaStoryDebugIdPair(InStateTree, AssetDebugId));
	}
}

bool FMetaStoryTraceProvider::GetAssetFromDebugId(const FMetaStoryIndex16 AssetDebugId, TWeakObjectPtr<const UMetaStory>& WeakStateTree) const
{
	verifyf(AssetDebugId.IsValid(), TEXT("Expecting valid asset debug Id."));
	const FMetaStoryDebugIdPair* ExistingPair = MetaStoryAssets.FindByPredicate([AssetDebugId](const FMetaStoryDebugIdPair& Pair)
	{
		return Pair.Id == AssetDebugId;
	});

	WeakStateTree = ExistingPair ? ExistingPair->WeakStateTree : nullptr; 

	return ExistingPair != nullptr;
}

bool FMetaStoryTraceProvider::GetAssetFromInstanceId(const FMetaStoryInstanceDebugId InstanceId, TWeakObjectPtr<const UMetaStory>& WeakStateTree) const
{
	if (const uint32* IndexPtr = InstanceIdToDebuggerEntryTimelines.Find(InstanceId))
	{
		check(Descriptors.Num() == EventsTimelines.Num());
		WeakStateTree = Descriptors[*IndexPtr].Get().MetaStory;
		return true;
	}

	return false;
}

TSharedPtr<const UE::MetaStoryDebugger::FInstanceDescriptor> FMetaStoryTraceProvider::GetInstanceDescriptor(const FMetaStoryInstanceDebugId InstanceId) const
{
	const TSharedRef<UE::MetaStoryDebugger::FInstanceDescriptor>* Descriptor = Descriptors.FindByPredicate([InstanceId](const TSharedRef<const UE::MetaStoryDebugger::FInstanceDescriptor>& Descriptor)
	{
		return Descriptor.Get().Id == InstanceId;
	});

	return Descriptor ? Descriptor->ToSharedPtr() : TSharedPtr<const UE::MetaStoryDebugger::FInstanceDescriptor>{};
}

void FMetaStoryTraceProvider::GetInstances(TArray<const TSharedRef<const UE::MetaStoryDebugger::FInstanceDescriptor>>& OutInstances) const
{
	OutInstances.Reserve(Descriptors.Num());
	Algo::Transform(Descriptors, OutInstances, [](const TSharedRef<UE::MetaStoryDebugger::FInstanceDescriptor>& Descriptor)
	{
		return Descriptor;
	});
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_METASTORY_TRACE_DEBUGGER
