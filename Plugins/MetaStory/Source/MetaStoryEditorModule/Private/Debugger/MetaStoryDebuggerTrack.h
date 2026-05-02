// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_METASTORY_TRACE_DEBUGGER

#include "RewindDebuggerTrack.h"
#include "SMetaStoryDebuggerEventTimelineView.h"
#include "Debugger/MetaStoryDebuggerTypes.h"

class SMetaStoryDebuggerView;
struct FMetaStoryDebugger;

namespace UE::MetaStoryDebugger
{

/**
 * Helper struct for debuggers relying on MetaStory instance tracks
 */
struct FInstanceTrackHelper : public TSharedFromThis<FInstanceTrackHelper>
{
	explicit FInstanceTrackHelper(const FMetaStoryInstanceDebugId InInstanceId, const TRange<double>& InViewRange);
	
	TSharedPtr<SMetaStoryDebuggerEventTimelineView::FTimelineEventData> GetTimelineData() const
	{
		return TimelineData;
	}

	/**
	 * Rebuild the timeline data from the provided Event collection
	 * @return Whether the number of points, or windows, in the timeline data
	 * changed since the last update.
	 * */
	bool RebuildEventData(
		TNotNull<const UMetaStory*> InMetaStory
		, const FInstanceEventCollection& InEventCollection
		, const double InRecordingDuration
		, const double InScrubTime
		, FMetaStoryTraceActiveStates* OutLastActiveStates = nullptr
		, const bool bInIsStaleTrack = false) const;

	FMetaStoryInstanceDebugId GetInstanceId() const
	{
		return InstanceId;
	}

	const FMetaStoryTraceActiveStates& GetActiveStates() const
	{
		return ActiveStates;
	}

	void SetActiveStates(const FMetaStoryTraceActiveStates& InActiveStates)
	{
		ActiveStates = InActiveStates;
	}

	TSharedPtr<SWidget> CreateTimelineView()
	{
		return SNew(SMetaStoryDebuggerEventTimelineView)
			.ViewRange_Lambda([WeakHelper = AsWeak()]
				{
					if (const TSharedPtr<FInstanceTrackHelper> Helper = WeakHelper.Pin())
					{
						return Helper->ViewRange;
					}
					return TRange<double>{};
				})
			.EventData_Lambda([WeakHelper = AsWeak()]()
				{
					if (const TSharedPtr<FInstanceTrackHelper> Helper = WeakHelper.Pin())
					{
						return Helper->TimelineData;
					}
					return TSharedPtr<SMetaStoryDebuggerEventTimelineView::FTimelineEventData>{};
				});
	}

protected:

	FMetaStoryInstanceDebugId InstanceId;
	TSharedPtr<SMetaStoryDebuggerEventTimelineView::FTimelineEventData> TimelineData;
	const TRange<double>& ViewRange;
	FMetaStoryTraceActiveStates ActiveStates;
};

} // UE::MetaStoryDebugger


/** Base struct for Debugger tracks to append some functionalities not available in RewindDebuggerTrack */
struct FMetaStoryDebuggerBaseTrack : RewindDebugger::FRewindDebuggerTrack
{
	explicit FMetaStoryDebuggerBaseTrack(const FSlateIcon& Icon, const FText& TrackName)
		: Icon(Icon)
		, TrackName(TrackName)
	{
	}

	virtual bool IsStale() const
	{
		return false;
	}

	virtual void MarkAsStale(const double InStaleTime)
	{
	}

	virtual void OnSelected()
	{
	}

protected:
	virtual FSlateIcon GetIconInternal() override
	{
		return Icon;
	}

	virtual FName GetNameInternal() const override
	{
		return FName(TrackName.ToString());
	}

	virtual FText GetDisplayNameInternal() const override
	{
		return TrackName;
	}

	FSlateIcon Icon;
	FText TrackName;
};

/** Track used to represent timeline events for a single MetaStory instance. */
struct FMetaStoryDebuggerInstanceTrack : FMetaStoryDebuggerBaseTrack
{
	explicit FMetaStoryDebuggerInstanceTrack(
		const TSharedRef<SMetaStoryDebuggerView>& InDebuggerView,
		const TSharedRef<FMetaStoryDebugger>& InDebugger,
		const FMetaStoryInstanceDebugId InInstanceId,
		const FText& InName,
		const TRange<double>& InViewRange);

	virtual void OnSelected() override;

	void MarkAsActive()
	{
		StaleTime = UE::MetaStoryDebugger::FInstanceDescriptor::ActiveInstanceEndTime;
	}

	virtual void MarkAsStale(const double InStaleTime) override
	{
		// Do not update timestamp for already stale tracks
		if (!IsStale())
		{
			StaleTime = InStaleTime;
		}
	}

	virtual bool IsStale() const override
	{
		return StaleTime != UE::MetaStoryDebugger::FInstanceDescriptor::ActiveInstanceEndTime;
	}

protected:
	virtual bool UpdateInternal() override;
	virtual TSharedPtr<SWidget> GetTimelineViewInternal() override;

private:
	
	TWeakPtr<SMetaStoryDebuggerView> MetaStoryDebuggerView;
	TWeakPtr<FMetaStoryDebugger> MetaStoryDebugger;
	TSharedRef<UE::MetaStoryDebugger::FInstanceTrackHelper> InstanceTrackHelper;
	TSharedPtr<const UE::MetaStoryDebugger::FInstanceDescriptor> Descriptor;
	double StaleTime = UE::MetaStoryDebugger::FInstanceDescriptor::ActiveInstanceEndTime;
};


/** Parent track of all the MetaStory instance tracks sharing the same execution context owner */
struct FMetaStoryDebuggerOwnerTrack : FMetaStoryDebuggerBaseTrack
{
	explicit FMetaStoryDebuggerOwnerTrack(const FText& InInstanceName);

	void AddSubTrack(const TSharedPtr<FMetaStoryDebuggerInstanceTrack>& InSubTrack)
	{
		SubTracks.Emplace(InSubTrack);
	}

	int32 NumSubTracks() const
	{
		return SubTracks.Num();
	}

	virtual void MarkAsStale(double InStaleTime) override;
	virtual bool IsStale() const override;

protected:
	virtual bool UpdateInternal() override;
	virtual TConstArrayView<TSharedPtr<FRewindDebuggerTrack>> GetChildrenInternal(TArray<TSharedPtr<FRewindDebuggerTrack>>& OutTracks) const override;

private:
	TArray<TSharedPtr<FMetaStoryDebuggerInstanceTrack>> SubTracks;
};

#endif // WITH_METASTORY_TRACE_DEBUGGER