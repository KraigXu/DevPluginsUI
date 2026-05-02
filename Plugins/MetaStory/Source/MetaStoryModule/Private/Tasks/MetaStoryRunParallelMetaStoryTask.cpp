// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/MetaStoryRunParallelMetaStoryTask.h"
#include "MetaStoryExecutionContext.h"
#include "MetaStoryExecutionTypes.h"
#include "MetaStoryInstanceData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryRunParallelMetaStoryTask)

#define LOCTEXT_NAMESPACE "MetaStory"

void FMetaStoryRunParallelStateTreeExecutionExtension::ScheduleNextTick(const FContextParameters& Context, const FNextTickArguments& Args)
{
	const FMetaStoryMinimalExecutionContext ExecutionContext(&Context.Owner, &Context.MetaStory, Context.InstanceData);
	const FMetaStoryScheduledTick ScheduledTick = ExecutionContext.GetNextScheduledTick();
	WeakExecutionContext.UpdateScheduledTickRequest(ScheduledTickHandle, ScheduledTick);
}

FMetaStoryRunParallelStateTreeTask::FMetaStoryRunParallelStateTreeTask()
{
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
	bShouldAffectTransitions = true;
	bConsideredForScheduling = false;
}

EMetaStoryRunStatus FMetaStoryRunParallelStateTreeTask::EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transitions) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const FMetaStoryReference& MetaStoryToRun = GetStateTreeToRun(Context, InstanceData);
	if (!MetaStoryToRun.IsValid())
	{
		return EMetaStoryRunStatus::Failed;
	}

	// Find if it's a recursive call. The detection is not perfect. For example: MetaStorys with a parallel task that links to each other cannot be detected.
	const bool bInParentContext = Context.GetActiveFrames().ContainsByPredicate([NewTree = MetaStoryToRun.GetStateTree()](const FMetaStoryExecutionFrame& Frame)
		{
			return Frame.MetaStory == NewTree;
		});
	const bool bFromParentProcessedFrame = Context.GetCurrentlyProcessedFrame() != nullptr ? Context.GetCurrentlyProcessedFrame()->MetaStory == MetaStoryToRun.GetStateTree() : false;
	if (bInParentContext || bFromParentProcessedFrame)
	{
		UE_LOG(LogMetaStory, Warning, TEXT("Trying to start a new parallel tree from the same tree '%s'"), *MetaStoryToRun.GetStateTree()->GetName());
		return EMetaStoryRunStatus::Failed;
	}

	if (InstanceData.ScheduledTickHandle.IsValid())
	{
		Context.RemoveScheduledTickRequest(InstanceData.ScheduledTickHandle);
	}

	// Share event queue with parent tree.
	if (FMetaStoryInstanceData* OuterInstanceData = Context.GetMutableInstanceData())
	{
		InstanceData.TreeInstanceData.SetSharedEventQueue(OuterInstanceData->GetSharedMutableEventQueue());
	}
	
	InstanceData.RunningStateTree = MetaStoryToRun.GetStateTree();
	FMetaStoryExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return EMetaStoryRunStatus::Failed;
	}

	FMetaStoryRunParallelStateTreeExecutionExtension Extension;
	Extension.WeakExecutionContext = Context.MakeWeakExecutionContext();
	const EMetaStoryRunStatus RunStatus = ParallelTreeContext.Start(FMetaStoryExecutionContext::FStartParameters
		{
			.GlobalParameters = &MetaStoryToRun.GetParameters(),
			.ExecutionExtension = TInstancedStruct<FMetaStoryRunParallelStateTreeExecutionExtension>::Make(MoveTemp(Extension))
		});

	InstanceData.ScheduledTickHandle = Context.AddScheduledTickRequest(ParallelTreeContext.GetNextScheduledTick());
	InstanceData.TreeInstanceData.GetMutableExecutionState()->ExecutionExtension.GetMutable<FMetaStoryRunParallelStateTreeExecutionExtension>().ScheduledTickHandle = InstanceData.ScheduledTickHandle;

	return RunStatus;
}

EMetaStoryRunStatus FMetaStoryRunParallelStateTreeTask::Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.RunningStateTree)
	{
		return EMetaStoryRunStatus::Failed;
	}

	FMetaStoryExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return EMetaStoryRunStatus::Failed;
	}

	const EMetaStoryRunStatus RunStatus = ParallelTreeContext.TickUpdateTasks(DeltaTime);
	Context.UpdateScheduledTickRequest(InstanceData.ScheduledTickHandle, ParallelTreeContext.GetNextScheduledTick());
	return RunStatus;
}

void FMetaStoryRunParallelStateTreeTask::TriggerTransitions(FMetaStoryExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.RunningStateTree)
	{
		return;
	}

	FMetaStoryExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return;
	}

	const EMetaStoryRunStatus LastTreeRunStatus = InstanceData.TreeInstanceData.GetStorage().GetExecutionState().TreeRunStatus;
	ParallelTreeContext.TickTriggerTransitions();

	const EMetaStoryRunStatus NewTreeRunStatus = InstanceData.TreeInstanceData.GetStorage().GetExecutionState().TreeRunStatus;
	if (LastTreeRunStatus != NewTreeRunStatus)
	{
		ensure(NewTreeRunStatus != EMetaStoryRunStatus::Running);
		Context.FinishTask(*this, NewTreeRunStatus == EMetaStoryRunStatus::Succeeded ? EMetaStoryFinishTaskType::Succeeded : EMetaStoryFinishTaskType::Failed);
	}
	Context.UpdateScheduledTickRequest(InstanceData.ScheduledTickHandle, ParallelTreeContext.GetNextScheduledTick());
}

void FMetaStoryRunParallelStateTreeTask::ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.RunningStateTree)
	{
		return;
	}

	FMetaStoryExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return;
	}

	ParallelTreeContext.Stop();
	Context.RemoveScheduledTickRequest(InstanceData.ScheduledTickHandle);
}

const FMetaStoryReference& FMetaStoryRunParallelStateTreeTask::GetStateTreeToRun(FMetaStoryExecutionContext& Context, FInstanceDataType& InstanceData) const
{
	if (MetaStoryOverrideTag.IsValid())
	{
		if (const FMetaStoryReference* Override = Context.GetLinkedStateTreeOverrideForTag(MetaStoryOverrideTag))
		{
			return *Override;
		}
	}

	return InstanceData.MetaStory;
}

#if WITH_EDITOR
EDataValidationResult FMetaStoryRunParallelStateTreeTask::Compile(UE::MetaStory::ICompileNodeContext& Context)
{
	TransitionHandlingPriority = EventHandlingPriority;

	return EDataValidationResult::Valid;
}

void FMetaStoryRunParallelStateTreeTask::PostEditInstanceDataChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FMetaStoryDataView InstanceDataView)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaStoryRunParallelStateTreeTaskInstanceData, MetaStory))
	{
		InstanceDataView.GetMutable<FInstanceDataType>().MetaStory.SyncParameters();
	}
}

void FMetaStoryRunParallelStateTreeTask::PostLoad(FMetaStoryDataView InstanceDataView)
{
	if (FInstanceDataType* DataType = InstanceDataView.GetMutablePtr<FInstanceDataType>())
	{
		DataType->MetaStory.SyncParameters();
	}
}

FText FMetaStoryRunParallelStateTreeTask::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText MetaStoryValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, MetaStory)), Formatting);
	if (MetaStoryValue.IsEmpty())
	{
		MetaStoryValue = FText::FromString(GetNameSafe(InstanceData->MetaStory.GetStateTree()));
	}

	const FText Format = (Formatting == EMetaStoryNodeFormatting::RichText)
		? LOCTEXT("RunParallelRich", "<b>Run Parallel</> {Asset}")
		: LOCTEXT("RunParallel", "Run Parallel {Asset}");

	return FText::FormatNamed(Format,
		TEXT("Asset"), MetaStoryValue);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
