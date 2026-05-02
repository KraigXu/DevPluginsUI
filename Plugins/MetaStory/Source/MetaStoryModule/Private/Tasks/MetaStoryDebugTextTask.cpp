// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryDebugTextTask.h"
#include "MetaStoryExecutionContext.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryDebugTextTask)

#define LOCTEXT_NAMESPACE "MetaStory"

FMetaStoryDebugTextTask::FMetaStoryDebugTextTask()
{
	bShouldCallTick = false;
	// We do not want to change the ReferenceActor if it's bound.
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;

#if WITH_EDITORONLY_DATA
	bConsideredForCompletion = false;
	bCanEditConsideredForCompletion = false;
#endif
}

EMetaStoryRunStatus FMetaStoryDebugTextTask::EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const
{
	if (!bEnabled)
	{
		return EMetaStoryRunStatus::Running;
	}

	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	const UWorld* World = Context.GetWorld();
	if (World == nullptr && InstanceData.ReferenceActor != nullptr)
	{
		World = InstanceData.ReferenceActor->GetWorld();
	}

	// Reference actor is not required (offset will be used as a global world location)
	// but a valid world is required.
	if (World == nullptr)
	{
		return EMetaStoryRunStatus::Failed;
	}

	if (!Text.IsEmpty())
	{
		DrawDebugString(World, Offset, Text, InstanceData.ReferenceActor, TextColor, /*Duration*/-1, /*DrawShadows*/true, FontScale);
	}

	if (!InstanceData.BindableText.IsEmpty())
	{
		DrawDebugString(World, Offset, InstanceData.BindableText, InstanceData.ReferenceActor, TextColor, /*Duration*/-1, /*DrawShadows*/true, FontScale);
	}

	return EMetaStoryRunStatus::Running;
}

void FMetaStoryDebugTextTask::ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const
{
	if (!bEnabled)
	{
		return;
	}

	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	const UWorld* World = Context.GetWorld();
	if (World == nullptr && InstanceData.ReferenceActor != nullptr)
	{
		World = InstanceData.ReferenceActor->GetWorld();
	}

	// Reference actor is not required (offset was used as a global world location)
	// but a valid world is required.
	if (World == nullptr)
	{
		return;
	}

	// Drawing an empty text will remove the HUD DebugText entries associated to the target actor
	DrawDebugString(World, Offset, "",	InstanceData.ReferenceActor);
}

#if WITH_EDITOR
FText FMetaStoryDebugTextTask::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	// Note that FMetaStoryDebugTextTaskInstanceData::BindableText is not added to the formatted string
	//  since the bindings are not copied at this point so there is nothing to display when not at runtime.
	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("DebugTextRich", "<b>Debug Text</> \"{Text}\"")
		: LOCTEXT("DebugText", "Debug Text \"{Text}\"");

	return FText::FormatNamed(Format,
		TEXT("Text"), FText::FromString(Text));
}
#endif

#undef LOCTEXT_NAMESPACE