// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Debugger/MetaStoryDebugger.h"
#include "Debugger/MetaStoryDebuggerTypes.h"
#include "IRewindDebuggerTrackCreator.h"
#include "RewindDebuggerTrack.h"
#include "MetaStoryDebuggerTrack.h"

namespace UE::MetaStory
{
class SCompactTreeDebuggerView;
}

namespace UE::MetaStoryDebugger
{
class SFrameEventsView;

/**
 * RewindDebugger track creator for MetaStory instances
 */
struct FRewindDebuggerTrackCreator final : RewindDebugger::IRewindDebuggerTrackCreator
{
protected:
	virtual FName GetTargetTypeNameInternal() const override;
	virtual FName GetNameInternal() const override
	{
		return "MetaStoryInstanceTrack";
	}
	virtual void GetTrackTypesInternal(TArray<RewindDebugger::FRewindDebuggerTrackType>& Types) const override;
	virtual TSharedPtr<RewindDebugger::FRewindDebuggerTrack> CreateTrackInternal(const RewindDebugger::FObjectId& InObjectId) const override;
	virtual bool HasDebugInfoInternal(const RewindDebugger::FObjectId& InObjectId) const override;
	virtual bool IsCreatingPrimaryChildTrackInternal() const override;
};

/**
 * RewindDebugger track representing a single MetaStory instance
 */
struct FRewindDebuggerTrack : RewindDebugger::FRewindDebuggerTrack, ITraceReader
{
	explicit FRewindDebuggerTrack(const RewindDebugger::FObjectId& InObjectId);

private:
	//~ Begin UE::MetaStoryDebugger::ITraceReader interface
	virtual FInstanceEventCollection* GetOrCreateEventCollection(FMetaStoryInstanceDebugId InstanceId) override;
	//~ End UE::MetaStoryDebugger::ITraceReader interface

	//~ Begin RewindDebugger::FRewindDebuggerTrack interface
	virtual bool UpdateInternal() override;
	virtual TSharedPtr<SWidget> GetTimelineViewInternal() override;
	virtual TSharedPtr<SWidget> GetDetailsViewInternal() override;
	virtual FText GetDisplayNameInternal() const override;
	virtual FSlateIcon GetIconInternal() override;
	virtual FName GetNameInternal() const override;
	virtual uint64 GetObjectIdInternal() const override;
	virtual bool HandleDoubleClickInternal() override;
	virtual FText GetStepCommandTooltipInternal(RewindDebugger::EStepMode) const override;
	virtual TOptional<double> GetStepFrameTimeInternal(RewindDebugger::EStepMode, double) const override;
	//~ End RewindDebugger::FRewindDebuggerTrack interface

	FInstanceEventCollection EventCollection;
	TSharedRef<FInstanceTrackHelper> InstanceTrackHelper;
	TSharedPtr<const FInstanceDescriptor> Descriptor;
	TSharedPtr<MetaStory::SCompactTreeDebuggerView> CompactTreeView;
	TSharedPtr<SFrameEventsView> EventsView;
	FSlateIcon Icon;
	RewindDebugger::FObjectId ObjectId;

	/** Last time in the recording that we used to fetch events, and that we will use for the next read. */
	double LastTraceReadTime = 0;

	/** Last scrub time used to rebuild the event data. */
	double LastUpdateScrubTime = 0;
};

} // UE::MetaStoryDebugger