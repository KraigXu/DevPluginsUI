// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryDescriptionHelpers.h"

#include "MetaStoryEditorData.h"
#include "MetaStoryEditorStyle.h"
#include "MetaStoryState.h"
#include "MetaStoryTypes.h"
#include "MetaStoryNodeBase.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

namespace UE::MetaStory::Editor
{

FText GetStateLinkDesc(const UMetaStoryEditorData* EditorData, const FMetaStoryStateLink& Link, EMetaStoryNodeFormatting Formatting, bool bShowStatePath)
{
	if (!EditorData)
	{
		return FText::GetEmpty();
	}

	if (Link.LinkType == EMetaStoryTransitionType::None)
	{
		return Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionNoneRich", "<i>None</>")
			: LOCTEXT("TransitionNone", "None");
	}
	if (Link.LinkType == EMetaStoryTransitionType::NextState)
	{
		return Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionNextStateRich", "<i>Next State</>")
			: LOCTEXT("TransitionNextState", "Next State");
	}
	if (Link.LinkType == EMetaStoryTransitionType::NextSelectableState)
	{
		return Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionNextSelectableStateRich", "<i>Next Selectable State</>")
			: LOCTEXT("TransitionNextSelectableState", "Next Selectable State");
	}
	if (Link.LinkType == EMetaStoryTransitionType::Succeeded)
	{
		return Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionTreeSucceededRich", "<i>Tree Succeeded</>")
			: LOCTEXT("TransitionTreeSucceeded", "Tree Succeeded");
	}
	if (Link.LinkType == EMetaStoryTransitionType::Failed)
	{
		return Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionTreeFailedRich", "<i>Tree Failed</>")
			: LOCTEXT("TransitionTreeFailed", "Tree Failed");
	}
	if (Link.LinkType == EMetaStoryTransitionType::GotoState)
	{
		if (const UMetaStoryState* State = EditorData->GetStateByID(Link.ID))
		{
			if (bShowStatePath)
			{
				TArray<FText> Path;
				while (State)
				{
					Path.Insert(FText::FromName(State->Name), 0);
					State = State->Parent;
				}
				return FText::Join(FText::FromString(TEXT("/")), Path);
			}
			else
			{
				return FText::FromName(State->Name);
			}
		}
		return FText::FromName(Link.Name);
	}

	return LOCTEXT("TransitionInvalid", "Invalid");
}

const FSlateBrush* GetStateLinkIcon(const UMetaStoryEditorData* EditorData, const FMetaStoryStateLink& Link)
{
	if (!EditorData)
	{
		return nullptr;
	}

	if (Link.LinkType == EMetaStoryTransitionType::None)
	{
		return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.None");
	}
	if (Link.LinkType == EMetaStoryTransitionType::NextState)
	{
		return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Next");
	}
	if (Link.LinkType == EMetaStoryTransitionType::NextSelectableState)
	{
		return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Next");
	}
	if (Link.LinkType == EMetaStoryTransitionType::Succeeded)
	{
		return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Succeeded");
	}
	if (Link.LinkType == EMetaStoryTransitionType::Failed)
	{
		return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Failed");
	}
	if (Link.LinkType == EMetaStoryTransitionType::GotoState)
	{
		if (const UMetaStoryState* State = EditorData->GetStateByID(Link.ID))
		{
			// Figure out icon.
			if (State->SelectionBehavior == EMetaStoryStateSelectionBehavior::None)
			{
				return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.SelectNone");
			}
			else if (State->SelectionBehavior == EMetaStoryStateSelectionBehavior::TryEnterState)
			{
				return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.TryEnterState");			
			}
			else if (State->SelectionBehavior == EMetaStoryStateSelectionBehavior::TrySelectChildrenInOrder)
			{
				if (State->Children.IsEmpty()
					|| State->Type == EMetaStoryStateType::Linked
					|| State->Type == EMetaStoryStateType::LinkedAsset)
				{
					return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.TryEnterState");			
				}
				else
				{
					return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.TrySelectChildrenInOrder");
				}
			}
			else if (State->SelectionBehavior == EMetaStoryStateSelectionBehavior::TryFollowTransitions)
			{
				return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.TryFollowTransitions");
			}
		}
	}
	
	return nullptr;
}

FSlateColor GetStateLinkColor(const UMetaStoryEditorData* EditorData, const FMetaStoryStateLink& Link)
{
	if (Link.LinkType == EMetaStoryTransitionType::GotoState)
	{
		if (const UMetaStoryState* State = EditorData->GetStateByID(Link.ID))
		{
			FLinearColor Color = FColor(31, 151, 167);
			if (const FMetaStoryEditorColor* FoundColor = EditorData->FindColor(State->ColorRef))
			{
				Color = FoundColor->Color;
			}
			return Color;
		}
		
		return FLinearColor(1.f, 1.f, 1.f, 0.25f);
	}
	return FLinearColor::White;
}

FText GetTransitionDesc(const UMetaStoryEditorData* EditorData, const FMetaStoryTransition& Transition, EMetaStoryNodeFormatting Formatting, bool bShowStatePath)
{
	if (!EditorData)
	{
		return FText::GetEmpty();
	}
	
	FText TriggerText;
	if (Transition.Trigger == EMetaStoryTransitionTrigger::OnStateCompleted)
	{
		TriggerText = Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionOnStateCompletedRich", "<b>On State Completed</>")
			: LOCTEXT("TransitionOnStateCompleted", "On State Completed");
	}
	else if (Transition.Trigger == EMetaStoryTransitionTrigger::OnStateSucceeded)
	{
		TriggerText = Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionOnStateSucceededRich", "<b>On State Succeeded</b>")
			: LOCTEXT("TransitionOnStateSucceeded", "On State Succeeded");
	}
	else if (Transition.Trigger == EMetaStoryTransitionTrigger::OnStateFailed)
	{
		TriggerText = Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionOnStateFailedRich", "<b>On State Failed</>")
			: LOCTEXT("TransitionOnStateFailed", "On State Failed");
	}
	else if (Transition.Trigger == EMetaStoryTransitionTrigger::OnTick)
	{
		TriggerText = Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionOnTickRich", "<b>On Tick</b>")
			: LOCTEXT("TransitionOnTick", "On Tick");
	}
	else if (Transition.Trigger == EMetaStoryTransitionTrigger::OnEvent)
	{
		TArray<FText> PayloadItems;
		
		if (Transition.RequiredEvent.IsValid())
		{
			if (Transition.RequiredEvent.Tag.IsValid())
			{
				const FText TagFormat = Formatting == EMetaStoryNodeFormatting::RichText
					? LOCTEXT("TransitionEventTagRich", "<s>Tag:</> '{0}'")
					: LOCTEXT("TransitionEventTag", "Tag: '{0}'");
				PayloadItems.Add(FText::Format(TagFormat, FText::FromName(Transition.RequiredEvent.Tag.GetTagName())));
			}
			
			if (Transition.RequiredEvent.PayloadStruct)
			{
				const FText PayloadFormat = Formatting == EMetaStoryNodeFormatting::RichText
					? LOCTEXT("TransitionEventPayloadRich", "<s>Payload:</> '{0}'")
					: LOCTEXT("TransitionEventPayload", "Payload: '{0}'");
				PayloadItems.Add(FText::Format(PayloadFormat, Transition.RequiredEvent.PayloadStruct->GetDisplayNameText()));
			}
		}
		else
		{
			PayloadItems.Add(LOCTEXT("TransitionInvalidEvent", "Invalid"));
		}

		const FText TransitionFormat = Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionOnEventRich", "<b>On Event</> ({0})")
			: LOCTEXT("TransitionOnEvent", "On Event ({0})");
		
		TriggerText = FText::Format(TransitionFormat, FText::Join(INVTEXT(", "), PayloadItems));
	}
	else if (Transition.Trigger == EMetaStoryTransitionTrigger::OnDelegate)
	{
		FMetaStoryBindingLookup BindingLookup(EditorData);

		const FText BoundDelegateText = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(Transition.ID, GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, DelegateListener)), Formatting);

		const FText TransitionFormat = Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("TransitionOnDelegateRich", "<b>On Delegate</> ({0})")
			: LOCTEXT("TransitionOnDelegate", "On Delegate ({0})");

		TriggerText = FText::Format(TransitionFormat, BoundDelegateText);
	}

	FText ActionText = Formatting == EMetaStoryNodeFormatting::RichText
		? LOCTEXT("ActionGotoRichRich", "<s>go to</>")
		: LOCTEXT("ActionGoto", "go to");
	
	if (Transition.State.LinkType == EMetaStoryTransitionType::Succeeded
		|| Transition.State.LinkType == EMetaStoryTransitionType::Failed)
	{
		ActionText = Formatting == EMetaStoryNodeFormatting::RichText
			? LOCTEXT("ActionReturnRich", "<s>return</>")
			: LOCTEXT("ActionReturn", "return");
	}
	
	return FText::Format(LOCTEXT("TransitionDesc", "{0} {1} {2}"), TriggerText, ActionText, GetStateLinkDesc(EditorData, Transition.State, Formatting, bShowStatePath));
}

} // UE::MetaStory::Editor

#undef LOCTEXT_NAMESPACE
