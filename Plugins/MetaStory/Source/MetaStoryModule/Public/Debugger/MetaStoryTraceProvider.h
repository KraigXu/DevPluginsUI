// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_METASTORY_TRACE_DEBUGGER

#include "IMetaStoryTraceProvider.h"
#include "Model/PointTimeline.h"
#include "MetaStoryIndexTypes.h"

namespace TraceServices { class IAnalysisSession; }
class UMetaStory;
struct FMetaStoryInstanceDebugId;
namespace UE::MetaStoryDebugger { struct FInstanceDescriptor; }

class FMetaStoryTraceProvider : public IMetaStoryTraceProvider
{
public:
	METASTORYMODULE_API static FName ProviderName;

	explicit FMetaStoryTraceProvider(TraceServices::IAnalysisSession& InSession);
	
	void AppendEvent(FMetaStoryInstanceDebugId InInstanceId, double InTime, const FMetaStoryTraceEventVariantType& InEvent);
	void AppendInstanceEvent(
		const FMetaStoryIndex16 AssetDebugId,
		const FMetaStoryInstanceDebugId InInstanceId,
		const TCHAR* InInstanceName,
		double InTime,
		double InWorldRecordingTime,
		EMetaStoryTraceEventType InEventType);

	void AppendAssetDebugId(const UMetaStory* InStateTree, const FMetaStoryIndex16 AssetDebugId);
	bool GetAssetFromDebugId(const FMetaStoryIndex16 AssetDebugId, TWeakObjectPtr<const UMetaStory>& WeakStateTree) const;
	bool GetAssetFromInstanceId(const FMetaStoryInstanceDebugId InstanceId, TWeakObjectPtr<const UMetaStory>& WeakStateTree) const;

protected:
	//~ IMetaStoryDebuggerProvider interface
	virtual TSharedPtr<const UE::MetaStoryDebugger::FInstanceDescriptor> GetInstanceDescriptor(const FMetaStoryInstanceDebugId InstanceId) const override;
	virtual void GetInstances(TArray<const TSharedRef<const UE::MetaStoryDebugger::FInstanceDescriptor>>& OutInstances) const override;
	virtual bool ReadTimelines(const FMetaStoryInstanceDebugId InstanceId, TFunctionRef<void(const FMetaStoryInstanceDebugId ProcessedInstanceId, const FEventsTimeline&)> Callback) const override;
	virtual bool ReadTimelines(const UMetaStory& MetaStory, TFunctionRef<void(const FMetaStoryInstanceDebugId ProcessedInstanceId, const FEventsTimeline&)> Callback) const override;

private:
	TraceServices::IAnalysisSession& Session;

	TMap<FMetaStoryInstanceDebugId, uint32> InstanceIdToDebuggerEntryTimelines;
	TArray<TSharedRef<UE::MetaStoryDebugger::FInstanceDescriptor>> Descriptors;
	TArray<TSharedRef<TraceServices::TPointTimeline<FMetaStoryTraceEventVariantType>>> EventsTimelines;

	struct FMetaStoryDebugIdPair
	{
		FMetaStoryDebugIdPair(const TWeakObjectPtr<const UMetaStory>& WeakStateTree, const FMetaStoryIndex16 Id)
			: WeakStateTree(WeakStateTree)
			, Id(Id)
		{
		}

		TWeakObjectPtr<const UMetaStory> WeakStateTree;
		FMetaStoryIndex16 Id;
	};

	TArray<FMetaStoryDebugIdPair> MetaStoryAssets;
};
#endif // WITH_METASTORY_TRACE_DEBUGGER