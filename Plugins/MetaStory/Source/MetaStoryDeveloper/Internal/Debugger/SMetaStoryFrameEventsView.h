// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_METASTORY_TRACE_DEBUGGER

#include "Debugger/MetaStoryTraceTypes.h"
#include "Widgets/SCompoundWidget.h"

#define UE_API METASTORYDEVELOPER_API

class UMetaStory;

template <typename ItemType> class STreeView;

namespace UE::MetaStoryDebugger
{
struct FFrameEventTreeElement;
struct FInstanceEventCollection;
struct FScrubState;

/**
 * TreeView representing all traced events on a MetaStory instance at a given frame
 */
class SFrameEventsView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFrameEventsView) {}
	SLATE_END_ARGS()

	UE_API void Construct(const FArguments& InArgs, TNotNull<const UMetaStory*> InStateTree);

	/** Selects an element in the list based on a predicate applied on the currently displayed events */
	UE_API void SelectByPredicate(TFunctionRef<bool(const FMetaStoryTraceEventVariantType& Event)> InPredicate);

	/** Rebuilds the view from events for a given frame */
	UE_API void RequestRefresh(const FScrubState& InScrubState);

private:
	/** Recursively sets tree items as expanded. */
	void ExpandAll(const TArray<TSharedPtr<FFrameEventTreeElement>>& Items);

	static void GenerateElementsForProperties(const FMetaStoryTraceEventVariantType& Event, const TSharedRef<FFrameEventTreeElement>& ParentElement);

	TWeakObjectPtr<const UMetaStory> WeakStateTree;

	/** All trace events received for a given instance. */
	TArray<TSharedPtr<FFrameEventTreeElement>> EventsTreeElements;

	/** Tree view displaying the frame events of the instance associated to the selected track. */
	TSharedPtr<STreeView<TSharedPtr<FFrameEventTreeElement>>> EventsTreeView;
};

} // UE::MetaStoryDebugger

#undef UE_API

#endif // WITH_METASTORY_TRACE_DEBUGGER