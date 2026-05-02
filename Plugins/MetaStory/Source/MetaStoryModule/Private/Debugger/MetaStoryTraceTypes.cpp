// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debugger/MetaStoryTraceTypes.h"

#include "MetaStory.h"
#include "MetaStoryNodeBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryTraceTypes)

#if WITH_METASTORY_TRACE_DEBUGGER

namespace UE::MetaStoryTrace
{
	FString GetStateName(const UMetaStory& MetaStory, const FMetaStoryCompactState* CompactState)
	{
		check(CompactState);

		if (const UMetaStory* LinkedAsset = CompactState->LinkedAsset.Get())
		{
			return FString::Printf(TEXT("%s > [%s]"), *CompactState->Name.ToString(), *LinkedAsset->GetName());
		}

		if (const FMetaStoryCompactState* LinkedState = MetaStory.GetStateFromHandle(CompactState->LinkedState))
		{
			return FString::Printf(TEXT("%s > %s"), *CompactState->Name.ToString(), *LinkedState->Name.ToString());
		}

		return CompactState->Name.ToString();
	}
}

//----------------------------------------------------------------------//
// FMetaStoryTracePhaseEvent
//----------------------------------------------------------------------//
FString FMetaStoryTracePhaseEvent::ToFullString(const UMetaStory& MetaStory) const
{
	return GetValueString(MetaStory);
}

FString FMetaStoryTracePhaseEvent::GetValueString(const UMetaStory& MetaStory) const
{
	FStringBuilderBase StrBuilder;

	// Override to display only selection behavior
	if (Phase == EMetaStoryUpdatePhase::TrySelectBehavior
		&& StateHandle.IsValid())
	{
		if (const FMetaStoryCompactState* CompactState = MetaStory.GetStateFromHandle(StateHandle))
		{
			return UEnum::GetDisplayValueAsText(CompactState->SelectionBehavior).ToString();
		}

		return FString::Printf(TEXT("Invalid State Index %s for '%s'"), *StateHandle.Describe(), *MetaStory.GetFullName());
	}

	// Otherwise build either: "<PhaseDescription> '<StateName>'" or "<StateName>"
	if (Phase != EMetaStoryUpdatePhase::Unset)
	{
		StrBuilder.Append(UEnum::GetDisplayValueAsText(Phase).ToString());
	}

	const FMetaStoryCompactState* CompactState = MetaStory.GetStateFromHandle(StateHandle);
	if (CompactState != nullptr || StateHandle.IsValid())
	{
		if (StrBuilder.Len())
		{
			StrBuilder.Appendf(TEXT(" '%s'"),
				CompactState != nullptr ? *UE::MetaStoryTrace::GetStateName(MetaStory, CompactState) : *StateHandle.Describe());
		}
		else
		{
			StrBuilder.Appendf(TEXT("%s"),
				CompactState != nullptr ? *UE::MetaStoryTrace::GetStateName(MetaStory, CompactState) : *StateHandle.Describe());
		}
	}

	return StrBuilder.ToString();
}

FString FMetaStoryTracePhaseEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	return *UEnum::GetDisplayValueAsText(EventType).ToString();
}


//----------------------------------------------------------------------//
// FMetaStoryTraceLogEvent
//----------------------------------------------------------------------//
FString FMetaStoryTraceLogEvent::ToFullString(const UMetaStory& MetaStory) const
{
	return FString::Printf(TEXT("%s: %s"), *GetTypeString(MetaStory), *GetValueString(MetaStory));
}

FString FMetaStoryTraceLogEvent::GetValueString(const UMetaStory& MetaStory) const
{
	return (*Message);
}

FString FMetaStoryTraceLogEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	return TEXT("Log");
}


//----------------------------------------------------------------------//
// FMetaStoryTracePropertyEvent
//----------------------------------------------------------------------//
FString FMetaStoryTracePropertyEvent::ToFullString(const UMetaStory& MetaStory) const
{
	return FString::Printf(TEXT("%s: %s"), *GetTypeString(MetaStory), *GetValueString(MetaStory));
}

FString FMetaStoryTracePropertyEvent::GetValueString(const UMetaStory& MetaStory) const
{
	return (*Message);
}

FString FMetaStoryTracePropertyEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	return TEXT("Property value");
}

//----------------------------------------------------------------------//
// FMetaStoryTraceTransitionEvent
//----------------------------------------------------------------------//
FString FMetaStoryTraceTransitionEvent::ToFullString(const UMetaStory& MetaStory) const
{
	return FString::Printf(TEXT("%s '%s': %s"), *UEnum::GetDisplayValueAsText(EventType).ToString(), *GetTypeString(MetaStory), *GetValueString(MetaStory));
}

FString FMetaStoryTraceTransitionEvent::GetValueString(const UMetaStory& MetaStory) const
{
	const FMetaStoryCompactState* CompactState = MetaStory.GetStateFromHandle(TransitionSource.TargetState);
	FStringBuilderBase StrBuilder;

	if (TransitionSource.SourceType == EMetaStoryTransitionSourceType::Asset)
	{
		if (const FMetaStoryCompactStateTransition* Transition = MetaStory.GetTransitionFromIndex(TransitionSource.TransitionIndex))
		{
			ensureAlways(Transition->Priority == TransitionSource.Priority);
			ensureAlways(Transition->State == TransitionSource.TargetState);

			const bool bHasTag = Transition->RequiredEvent.Tag.IsValid();
			const bool bHasPayload = Transition->RequiredEvent.PayloadStruct != nullptr;
			
			if (bHasTag || bHasPayload)
			{
				StrBuilder.Appendf(TEXT("("));
				
				if (bHasTag)
				{
					StrBuilder.Appendf(TEXT("Tag: '%s'"), *Transition->RequiredEvent.Tag.ToString()); 
				}
				if (bHasTag && bHasPayload)
				{
					StrBuilder.Appendf(TEXT(", "));
				}
				if (bHasPayload)
				{
					StrBuilder.Appendf(TEXT(" Payload: '%s'"), *Transition->RequiredEvent.PayloadStruct->GetName()); 
				}
				StrBuilder.Appendf(TEXT(") ")); 
			}
		}
		else
		{
			StrBuilder.Appendf(TEXT("[Invalid Transition Index %s for '%s']"), *LexToString(TransitionSource.TransitionIndex.Get()), *MetaStory.GetFullName());
		}
	}
	
	StrBuilder.Appendf(TEXT("go to State '%s'"), CompactState != nullptr ? *UE::MetaStoryTrace::GetStateName(MetaStory, CompactState) : *TransitionSource.TargetState.Describe());

	if (TransitionSource.Priority != EMetaStoryTransitionPriority::None)
	{
		StrBuilder.Appendf(TEXT(" (Priority: %s)"), *UEnum::GetDisplayValueAsText(TransitionSource.Priority).ToString()); 
	}


	return StrBuilder.ToString();
}

FString FMetaStoryTraceTransitionEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	if (TransitionSource.SourceType == EMetaStoryTransitionSourceType::Asset)
	{
		if (const FMetaStoryCompactStateTransition* Transition = MetaStory.GetTransitionFromIndex(TransitionSource.TransitionIndex))
		{
			return *UEnum::GetDisplayValueAsText(Transition->Trigger).ToString();
		}

		return FString::Printf(TEXT("Invalid Transition Index %s for '%s'"), *LexToString(TransitionSource.TransitionIndex.Get()), *MetaStory.GetFullName());
	}

	return *UEnum::GetDisplayValueAsText(TransitionSource.SourceType).ToString();
}


//----------------------------------------------------------------------//
// FMetaStoryTraceNodeEvent
//----------------------------------------------------------------------//
FString FMetaStoryTraceNodeEvent::ToFullString(const UMetaStory& MetaStory) const
{
	const FConstStructView NodeView = Index.IsValid() ? MetaStory.GetNode(Index.Get()) : FConstStructView();
	const FMetaStoryNodeBase* Node = NodeView.GetPtr<const FMetaStoryNodeBase>();

	return FString::Printf(TEXT("%s '%s (%s)'"),
			*UEnum::GetDisplayValueAsText(EventType).ToString(),
			Node != nullptr ? *Node->Name.ToString() : *LexToString(Index.Get()),
			NodeView.IsValid() ? *NodeView.GetScriptStruct()->GetName() : TEXT("Invalid Node"));
}

FString FMetaStoryTraceNodeEvent::GetValueString(const UMetaStory& MetaStory) const
{
	const FConstStructView NodeView = Index.IsValid() ? MetaStory.GetNode(Index.Get()) : FConstStructView();
	const FMetaStoryNodeBase* Node = NodeView.GetPtr<const FMetaStoryNodeBase>();

	return FString::Printf(TEXT("%s '%s'"),
			*UEnum::GetDisplayValueAsText(EventType).ToString(),
			Node != nullptr ? *Node->Name.ToString() : *LexToString(Index.Get()));
}

FString FMetaStoryTraceNodeEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	const FConstStructView NodeView = Index.IsValid() ? MetaStory.GetNode(Index.Get()) : FConstStructView();
	return FString::Printf(TEXT("%s"), NodeView.IsValid() ? *NodeView.GetScriptStruct()->GetName() : TEXT("Invalid Node"));
}


//----------------------------------------------------------------------//
// FMetaStoryTraceStateEvent
//----------------------------------------------------------------------//
FMetaStoryStateHandle FMetaStoryTraceStateEvent::GetStateHandle() const
{
	return FMetaStoryStateHandle(Index.Get());
}

FString FMetaStoryTraceStateEvent::ToFullString(const UMetaStory& MetaStory) const
{
	const FMetaStoryStateHandle StateHandle(Index.Get());
	if (const FMetaStoryCompactState* CompactState = MetaStory.GetStateFromHandle(StateHandle))
	{
		if (CompactState->SelectionBehavior != EMetaStoryStateSelectionBehavior::None)
		{
			return FString::Printf(TEXT("%s '%s' (%s)"),
				*UEnum::GetDisplayValueAsText(EventType).ToString(),
				*GetValueString(MetaStory),
				*UEnum::GetDisplayValueAsText(CompactState->SelectionBehavior).ToString());
		}
	}

	return FString::Printf(TEXT("%s '%s'"),
		*UEnum::GetDisplayValueAsText(EventType).ToString(),
		*GetValueString(MetaStory));
}

FString FMetaStoryTraceStateEvent::GetValueString(const UMetaStory& MetaStory) const
{
	const FMetaStoryStateHandle StateHandle(Index.Get());
	if (const FMetaStoryCompactState* CompactState = MetaStory.GetStateFromHandle(StateHandle))
	{
		TArray<FString, TInlineAllocator<FMetaStoryActiveStates::MaxStates>> StateHierarchy;
		StateHierarchy.Push(UE::MetaStoryTrace::GetStateName(MetaStory, CompactState));

		// Climb hierarchy
		for (FMetaStoryStateHandle ParentHandle = CompactState->Parent; ParentHandle.IsValid(); ParentHandle = CompactState->Parent)
		{
			if (CompactState = MetaStory.GetStateFromHandle(ParentHandle); CompactState != nullptr)
			{
				StateHierarchy.Push(UE::MetaStoryTrace::GetStateName(MetaStory, CompactState));
			}
			else
			{
				break;
			}
		}

		// Reverse to output from the root
		Algo::Reverse(StateHierarchy);

		return FString::Join(StateHierarchy, TEXT("."));
	}

	return FString::Printf(TEXT("Invalid State Index %s for '%s'"), *StateHandle.Describe(), *MetaStory.GetFullName());
}

FString FMetaStoryTraceStateEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	return TEXT("State");
}


//----------------------------------------------------------------------//
// FMetaStoryTraceTaskEvent
//----------------------------------------------------------------------//
FString FMetaStoryTraceTaskEvent::ToFullString(const UMetaStory& MetaStory) const
{
	return FString::Printf(TEXT("%s -> %s"), *FMetaStoryTraceNodeEvent::ToFullString(MetaStory), *UEnum::GetDisplayValueAsText(Status).ToString());
}

FString FMetaStoryTraceTaskEvent::GetValueString(const UMetaStory& MetaStory) const
{
	const FConstStructView NodeView = Index.IsValid() ? MetaStory.GetNode(Index.Get()) : FConstStructView();
	const FMetaStoryNodeBase* Node = NodeView.GetPtr<const FMetaStoryNodeBase>();
	return FString::Printf(TEXT("%s"), Node != nullptr ? *Node->Name.ToString() : *LexToString(Index.Get()));
}

FString FMetaStoryTraceTaskEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	return FMetaStoryTraceNodeEvent::GetTypeString(MetaStory);
}


//----------------------------------------------------------------------//
// FMetaStoryTraceEvaluatorEvent
//----------------------------------------------------------------------//
FString FMetaStoryTraceEvaluatorEvent::ToFullString(const UMetaStory& MetaStory) const
{
	return FMetaStoryTraceNodeEvent::ToFullString(MetaStory);
}

FString FMetaStoryTraceEvaluatorEvent::GetValueString(const UMetaStory& MetaStory) const
{
	const FConstStructView NodeView = Index.IsValid() ? MetaStory.GetNode(Index.Get()) : FConstStructView();
	const FMetaStoryNodeBase* Node = NodeView.GetPtr<const FMetaStoryNodeBase>();
	return FString::Printf(TEXT("%s"), Node != nullptr ? *Node->Name.ToString() : *LexToString(Index.Get()));
}

FString FMetaStoryTraceEvaluatorEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	return FMetaStoryTraceNodeEvent::GetTypeString(MetaStory);
}


//----------------------------------------------------------------------//
// FMetaStoryTraceConditionEvent
//----------------------------------------------------------------------//
FString FMetaStoryTraceConditionEvent::ToFullString(const UMetaStory& MetaStory) const
{
	return FMetaStoryTraceNodeEvent::ToFullString(MetaStory);
}

FString FMetaStoryTraceConditionEvent::GetValueString(const UMetaStory& MetaStory) const
{
	const FConstStructView NodeView = Index.IsValid() ? MetaStory.GetNode(Index.Get()) : FConstStructView();
	const FMetaStoryNodeBase* Node = NodeView.GetPtr<const FMetaStoryNodeBase>();

	return FString::Printf(TEXT("%s"), Node != nullptr ? *Node->Name.ToString() : *LexToString(Index.Get()));
}

FString FMetaStoryTraceConditionEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	return FMetaStoryTraceNodeEvent::GetTypeString(MetaStory);
}


//----------------------------------------------------------------------//
// FMetaStoryTraceActiveStatesEvent
//----------------------------------------------------------------------//
FMetaStoryTraceActiveStatesEvent::FMetaStoryTraceActiveStatesEvent(const double RecordingWorldTime)
	: FMetaStoryTraceBaseEvent(RecordingWorldTime, EMetaStoryTraceEventType::Unset)
{
}

FString FMetaStoryTraceActiveStatesEvent::ToFullString(const UMetaStory& MetaStory) const
{
	if (ActiveStates.PerAssetStates.Num() > 0)
	{
		return FString::Printf(TEXT("%s: %s"), *GetTypeString(MetaStory), *GetValueString(MetaStory));
	}
	return TEXT("No active states");
}

FString FMetaStoryTraceActiveStatesEvent::GetValueString(const UMetaStory&) const
{
	FStringBuilderBase StatePath;
	for (const FMetaStoryTraceActiveStates::FAssetActiveStates& PerAssetStates : ActiveStates.PerAssetStates)
	{
		if (const UMetaStory* MetaStory = PerAssetStates.WeakStateTree.Get())
		{
			for (const FMetaStoryStateHandle Handle : PerAssetStates.ActiveStates)
			{
				const FMetaStoryCompactState* State = MetaStory->GetStateFromHandle(Handle);
				StatePath.Appendf(TEXT("%s%s"),
					StatePath.Len() == 0 ? TEXT("") : TEXT("."),
					State == nullptr ? TEXT("Invalid") : *UE::MetaStoryTrace::GetStateName(*MetaStory, State));
			}
		}
	}
	return StatePath.ToString();
}

FString FMetaStoryTraceActiveStatesEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	return TEXT("New active states");
}


//----------------------------------------------------------------------//
// FMetaStoryTraceInstanceFrameEvent
//----------------------------------------------------------------------//
FMetaStoryTraceInstanceFrameEvent::FMetaStoryTraceInstanceFrameEvent(const double RecordingWorldTime, const EMetaStoryTraceEventType EventType, const UMetaStory* MetaStory)
	: FMetaStoryTraceBaseEvent(RecordingWorldTime, EventType)
	, WeakStateTree(MetaStory)
{
}

FString FMetaStoryTraceInstanceFrameEvent::ToFullString(const UMetaStory& MetaStory) const
{
	return FString::Printf(TEXT("%s: %s"), *GetTypeString(MetaStory), *GetValueString(MetaStory));
}

FString FMetaStoryTraceInstanceFrameEvent::GetValueString(const UMetaStory& MetaStory) const
{
	return GetNameSafe(WeakStateTree.Get());
}

FString FMetaStoryTraceInstanceFrameEvent::GetTypeString(const UMetaStory& MetaStory) const
{
	return TEXT("New instance frame");
}

#endif // WITH_METASTORY_TRACE_DEBUGGER
