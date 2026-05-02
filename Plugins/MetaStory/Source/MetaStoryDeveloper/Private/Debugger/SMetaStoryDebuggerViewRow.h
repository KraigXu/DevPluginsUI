// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_METASTORY_TRACE_DEBUGGER

#include "Debugger/MetaStoryTraceTypes.h"
#include "MetaStory.h"
#include "Templates/SharedPointer.h"
#include "TraceServices/Model/Frames.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class FMetaStoryViewModel;

namespace UE::MetaStoryDebugger
{

/** An item in the MetaStoryDebugger trace event tree */
struct FFrameEventTreeElement : TSharedFromThis<FFrameEventTreeElement>
{
	explicit FFrameEventTreeElement(const TraceServices::FFrame& Frame, const FMetaStoryTraceEventVariantType& Event, const UMetaStory* MetaStory)
		: Frame(Frame), Event(Event), WeakStateTree(MetaStory)
	{
	}

	TraceServices::FFrame Frame;
	FMetaStoryTraceEventVariantType Event;
	TArray<TSharedPtr<FFrameEventTreeElement>> Children;
	FString Description;
	TWeakObjectPtr<const UMetaStory> WeakStateTree;
};


/**
 * Widget for row inside the MetaStoryDebugger TreeView.
 */
class SFrameEventViewRow : public STableRow<TSharedPtr<FFrameEventTreeElement>>
{
public:
	void Construct(const FArguments& InArgs,
		const TSharedPtr<STableViewBase>& InOwnerTableView,
		const TSharedPtr<FFrameEventTreeElement>& InElement);

private:
	TSharedPtr<SWidget> CreateImageForEvent() const;
	const FTextBlockStyle& GetEventTextStyle() const;
	FText GetEventDescription() const;
	FText GetEventTooltip() const;

	TSharedPtr<FFrameEventTreeElement> Item;
};

} // UE::MetaStoryDebugger

#endif // WITH_METASTORY_TRACE_DEBUGGER