// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_METASTORY_TRACE_DEBUGGER

#include "MetaStoryTraceTypes.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Containers/Timelines.h"

namespace UE::MetaStoryDebugger { struct FInstanceDescriptor; }
struct FMetaStoryInstanceDebugId;
class UMetaStory;

class IMetaStoryTraceProvider : public TraceServices::IProvider
{
public:
	typedef TraceServices::ITimeline<FMetaStoryTraceEventVariantType> FEventsTimeline;

	/**
	 * Return instance descriptor associated to the given instance id.
	 * @param InstanceId Id of a specific instance to get the descriptor for.
	 * @return Shared pointer to the descriptor if the specified instance was found.
	 */
	virtual TSharedPtr<const UE::MetaStoryDebugger::FInstanceDescriptor> GetInstanceDescriptor(const FMetaStoryInstanceDebugId InstanceId) const = 0;

	/**
	 * Return all instances with events in the traces.
	 * @param OutInstances List of all instances with events in the traces regardless if they are active or not.
	 */
	virtual void GetInstances(TArray<const TSharedRef<const UE::MetaStoryDebugger::FInstanceDescriptor>>& OutInstances) const = 0;

	UE_DEPRECATED(5.7, "Use the version with TSharedRef array instead")
	virtual void GetInstances(TArray<UE::MetaStoryDebugger::FInstanceDescriptor>& OutInstances) const final
	{
	}

	/**
	 * Execute given function receiving an event timeline for a given instance or all timelines if instance not specified.  
	 * @param InstanceId Id of a specific instance to get the timeline for; could be an FMetaStoryInstanceDebugId::Invalid to go through all timelines
	 * @param Callback Function called for timeline(s) matching the provided Id 
	 * @return True if the specified instance was found for a given Id or at least one timeline was found when no specific Id is provided. 
	 */
	virtual bool ReadTimelines(const FMetaStoryInstanceDebugId InstanceId, TFunctionRef<void(const FMetaStoryInstanceDebugId ProcessedInstanceId, const FEventsTimeline&)> Callback) const = 0;

	/**
	 * Execute given function receiving an event timeline for all timelines matching a given asset.  
	 * @param MetaStory Asset that must be used by the instance to be processed
	 * @param Callback Function called for timeline(s) matching the provided asset 
	 * @return True if the specified instance was found for a given Id or at least one timeline was found when no specific Id is provided. 
	 */
	virtual bool ReadTimelines(const UMetaStory& MetaStory, TFunctionRef<void(const FMetaStoryInstanceDebugId ProcessedInstanceId, const FEventsTimeline&)> Callback) const = 0;
};

#endif // WITH_METASTORY_TRACE_DEBUGGER
