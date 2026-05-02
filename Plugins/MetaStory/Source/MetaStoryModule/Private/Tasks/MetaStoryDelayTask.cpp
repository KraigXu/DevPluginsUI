// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryDelayTask.h"
#include "MetaStoryExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryDelayTask)

#define LOCTEXT_NAMESPACE "MetaStory"

FMetaStoryDelayTask::FMetaStoryDelayTask()
{
	bConsideredForScheduling = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EMetaStoryRunStatus FMetaStoryDelayTask::EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.bRunForever)
	{
		InstanceData.RemainingTime = FMath::FRandRange(
			FMath::Max(0.0f, InstanceData.Duration - InstanceData.RandomDeviation), (InstanceData.Duration + InstanceData.RandomDeviation));

		InstanceData.ScheduledTickHandle = Context.AddScheduledTickRequest(FMetaStoryScheduledTick::MakeCustomTickRate(InstanceData.RemainingTime));
	}
	
	return EMetaStoryRunStatus::Running;
}

EMetaStoryRunStatus FMetaStoryDelayTask::Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.bRunForever)
	{
		InstanceData.RemainingTime -= DeltaTime;

		if (InstanceData.RemainingTime <= 0.f)
		{
			Context.RemoveScheduledTickRequest(InstanceData.ScheduledTickHandle);
			return EMetaStoryRunStatus::Succeeded;
		}
		Context.UpdateScheduledTickRequest(InstanceData.ScheduledTickHandle, FMetaStoryScheduledTick::MakeCustomTickRate(InstanceData.RemainingTime));
	}
	
	return EMetaStoryRunStatus::Running;
}

void FMetaStoryDelayTask::ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	Context.RemoveScheduledTickRequest(InstanceData.ScheduledTickHandle);
}

#if WITH_EDITOR
FText FMetaStoryDelayTask::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText Value = FText::GetEmpty();

	if (const FPropertyBindingPath* RunForeverSourcePath = BindingLookup.GetPropertyBindingSource(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FMetaStoryDelayTaskInstanceData, bRunForever))))
	{
		Value = FText::Format(LOCTEXT("ForeverBound", "Forever={0}"),
			BindingLookup.GetPropertyPathDisplayName(*RunForeverSourcePath, Formatting));
	}
	else if (InstanceData->bRunForever)
	{
		Value = LOCTEXT("Forever", "Forever");
	}
	else
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 1;
		Options.MaximumFractionalDigits = 3;

		FText DurationText = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, Duration)), Formatting);
		if (DurationText.IsEmpty())
		{
			DurationText = FText::AsNumber(InstanceData->Duration, &Options);
		}

		FText RandomDeviationText = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, RandomDeviation)), Formatting);
		if (RandomDeviationText.IsEmpty()
			&& !FMath::IsNearlyZero(InstanceData->RandomDeviation))
		{
			RandomDeviationText = FText::AsNumber(InstanceData->RandomDeviation, &Options);
		}

		if (RandomDeviationText.IsEmpty())
		{
			Value = DurationText;
		}
		else
		{
			if (Formatting == EMetaStoryNodeFormatting::RichText)
			{
				Value = FText::Format(LOCTEXT("DelayValueRich", "{0} <s>\u00B1{1}</>"), // +-
					DurationText,
					RandomDeviationText);
			}
			else
			{
				Value = FText::Format(LOCTEXT("DelayValue", "{0} \u00B1{1}"), // +-
					DurationText,
					RandomDeviationText);
			}
		}
	}

	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("DelayRich", "<b>Delay</> {Time}")
		: LOCTEXT("Delay", "Delay {Time}");

	return FText::FormatNamed(Format,
		TEXT("Time"), Value);
}
#endif

#undef LOCTEXT_NAMESPACE
