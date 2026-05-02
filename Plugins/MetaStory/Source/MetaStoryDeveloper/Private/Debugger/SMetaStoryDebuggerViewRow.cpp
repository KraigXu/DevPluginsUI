// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_METASTORY_TRACE_DEBUGGER

#include "SMetaStoryDebuggerViewRow.h"
#include "MetaStoryStyle.h"
#include "MetaStory.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

namespace UE::MetaStoryDebugger
{

void SFrameEventViewRow::Construct(const FArguments& InArgs,
		const TSharedPtr<STableViewBase>& InOwnerTableView,
		const TSharedPtr<FFrameEventTreeElement>& InElement)
{
	Item = InElement;
	STableRow::Construct(InArgs, InOwnerTableView.ToSharedRef());

	const TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	HorizontalBox->SetToolTipText(GetEventTooltip());

	HorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.AutoWidth()
		[
			SNew(SExpanderArrow, SharedThis(this))
			.ShouldDrawWires(false)
			.IndentAmount(32)
			.BaseIndentLevel(0)
		];

	const TSharedPtr<SWidget> EventImage = CreateImageForEvent();
	if (EventImage.IsValid())
	{
		HorizontalBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.AutoWidth()
		[
			EventImage.ToSharedRef()
		];
	}

	HorizontalBox->AddSlot()
		.Padding(2)
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.TextStyle(&GetEventTextStyle())
			.Text(GetEventDescription())
		];

	this->ChildSlot
		.HAlign(HAlign_Fill)
		[
			HorizontalBox.ToSharedRef()
		];
}

TSharedPtr<SWidget> SFrameEventViewRow::CreateImageForEvent() const
{
	const FMetaStoryStyle& StyleSet = FMetaStoryStyle::Get();

	// Phase events
	if (const FMetaStoryTracePhaseEvent* PhaseEvent = Item->Event.TryGet<FMetaStoryTracePhaseEvent>())
	{
		const FSlateBrush* Image = nullptr;
		switch (PhaseEvent->Phase)
		{
		case EMetaStoryUpdatePhase::EnterStates:	Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.State.Enter");		break;
		case EMetaStoryUpdatePhase::ExitStates:		Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.State.Exit");		break;
		case EMetaStoryUpdatePhase::StateCompleted:	Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.State.Completed");	break;
		default:
			return nullptr;
		}

		return SNew(SImage).Image(Image);
	}

	// Log events
	if (const FMetaStoryTraceLogEvent* LogEvent = Item->Event.TryGet<FMetaStoryTraceLogEvent>())
	{
		const FSlateBrush* Image = nullptr;
		switch (LogEvent->Verbosity)
		{
		case ELogVerbosity::Fatal:
		case ELogVerbosity::Error:		Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Log.Error");	break;
		case ELogVerbosity::Warning:	Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Log.Warning");	break;
		default:
			return nullptr;
		}

		return SNew(SImage).Image(Image);
	}

	// State events
	if (const FMetaStoryTraceStateEvent* StateEvent = Item->Event.TryGet<FMetaStoryTraceStateEvent>())
	{
		const FSlateBrush* Image = nullptr;

		switch (StateEvent->EventType)
		{
		case EMetaStoryTraceEventType::OnStateSelected:		Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.State.Selected");	break;
		default:
			return nullptr;
		}

		return SNew(SImage).Image(Image);
	}

	// Task events
	if (const FMetaStoryTraceTaskEvent* TaskEvent = Item->Event.TryGet<FMetaStoryTraceTaskEvent>())
	{
		const FSlateBrush* Image = nullptr;

		switch (TaskEvent->EventType)
		{
		case EMetaStoryTraceEventType::OnEntered:
		case EMetaStoryTraceEventType::OnTaskCompleted:
		case EMetaStoryTraceEventType::OnTicked:
			switch (TaskEvent->Status)
			{
			case EMetaStoryRunStatus::Failed:		Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Task.Failed");		break;
			case EMetaStoryRunStatus::Succeeded:	Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Task.Succeeded");	break;
			case EMetaStoryRunStatus::Stopped:		Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Task.Stopped");		break;
			default:
				return nullptr;
			}
			break;
		default:
			return nullptr;
		}

		return SNew(SImage).Image(Image);
	}

	// Condition events
	if (Item->Event.IsType<FMetaStoryTraceConditionEvent>())
	{
		const FMetaStoryTraceConditionEvent& ConditionEvent = Item->Event.Get<FMetaStoryTraceConditionEvent>();
		const FSlateBrush* Image = nullptr;

		switch (ConditionEvent.EventType)
		{
		case EMetaStoryTraceEventType::Passed:					Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Condition.Passed");			break;
		case EMetaStoryTraceEventType::ForcedSuccess:			Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Condition.Passed");			break;
		case EMetaStoryTraceEventType::Failed:					Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Condition.Failed");			break;
		case EMetaStoryTraceEventType::ForcedFailure:			Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Condition.Failed");			break;
		case EMetaStoryTraceEventType::InternalForcedFailure:	Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Condition.Failed");			break;
		case EMetaStoryTraceEventType::OnEvaluating:			Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Condition.OnEvaluating");	break;
		case EMetaStoryTraceEventType::OnTransition:			Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Condition.OnTransition");	break;
		default:
			Image = StyleSet.GetBrush("MetaStoryEditor.Debugger.Unset");
		}

		return SNew(SImage).Image(Image);
	}

	return nullptr;
}

const FTextBlockStyle& SFrameEventViewRow::GetEventTextStyle() const
{
	const FMetaStoryStyle& StyleSet = FMetaStoryStyle::Get();

	if (Item->Event.IsType<FMetaStoryTracePhaseEvent>())
	{
		return StyleSet.GetWidgetStyle<FTextBlockStyle>("MetaStoryDebugger.Element.Bold");
	}

	if (Item->Event.IsType<FMetaStoryTracePropertyEvent>())
	{
		return StyleSet.GetWidgetStyle<FTextBlockStyle>("MetaStoryDebugger.Element.Subdued");
	}

	if (const FMetaStoryTraceLogEvent* LogEvent = Item->Event.TryGet<FMetaStoryTraceLogEvent>())
	{
		// Make verbose logs more subtle
		if (LogEvent->Verbosity >= ELogVerbosity::Verbose)
		{
			return StyleSet.GetWidgetStyle<FTextBlockStyle>("MetaStoryDebugger.Element.Subdued");
		}
	}

	return StyleSet.GetWidgetStyle<FTextBlockStyle>("MetaStoryDebugger.Element.Normal");
}

FText SFrameEventViewRow::GetEventDescription() const
{
	FString EventDescription;
	if (Item->Description.IsEmpty())
	{
		if (const UMetaStory* MetaStory = Item->WeakStateTree.Get())
		{
			// Some types have some custom representations so we want to use a more minimal description.
			if (Item->Event.IsType<FMetaStoryTraceStateEvent>()
				|| Item->Event.IsType<FMetaStoryTraceTaskEvent>()
				|| Item->Event.IsType<FMetaStoryTraceEvaluatorEvent>()
				|| Item->Event.IsType<FMetaStoryTracePropertyEvent>()
				|| Item->Event.IsType<FMetaStoryTraceConditionEvent>()
				|| Item->Event.IsType<FMetaStoryTraceLogEvent>())
			{
				Visit([&EventDescription, MetaStory](auto& TypedEvent)
					{
						EventDescription = TypedEvent.GetValueString(*MetaStory);
					}, Item->Event);
			}
			else
			{
				Visit([&EventDescription, MetaStory](auto& TypedEvent)
					{
						EventDescription = TypedEvent.ToFullString(*MetaStory);
					}, Item->Event);
			}
		}
	}
	else
	{
		EventDescription = Item->Description;
	}
	
	return FText::FromString(EventDescription);
}

FText SFrameEventViewRow::GetEventTooltip() const
{
	FString Tooltip;
	if (const UMetaStory* MetaStory = Item->WeakStateTree.Get())
	{
		Visit([&Tooltip, MetaStory](auto& TypedEvent)
			{
				Tooltip = TypedEvent.ToFullString(*MetaStory);
			}, Item->Event);
	}

	return FText::FromString(Tooltip);
}

} // UE::MetaStoryDebugger

#endif // WITH_METASTORY_TRACE_DEBUGGER
