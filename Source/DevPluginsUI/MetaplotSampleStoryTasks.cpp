#include "MetaplotSampleStoryTasks.h"

#include "Runtime/MetaplotInstance.h"

void UMetaplotSampleDelayTask::EnterTask_Implementation(UMetaplotInstance* Instance, FGuid NodeId)
{
	Super::EnterTask_Implementation(Instance, NodeId);
	RemainingSeconds = FMath::Max(0.0f, DurationSeconds);
}

EMetaplotTaskRunState UMetaplotSampleDelayTask::TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime)
{
	Super::TickTask_Implementation(Instance, DeltaTime);

	if (RemainingSeconds <= 0.0f)
	{
		return EMetaplotTaskRunState::Succeeded;
	}

	RemainingSeconds -= FMath::Max(0.0f, DeltaTime);
	return RemainingSeconds <= 0.0f ? EMetaplotTaskRunState::Succeeded : EMetaplotTaskRunState::Running;
}

EMetaplotTaskRunState UMetaplotSampleSetStringTask::TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime)
{
	Super::TickTask_Implementation(Instance, DeltaTime);

	if (!Instance || Key.IsNone())
	{
		return EMetaplotTaskRunState::Failed;
	}

	return Instance->SetBlackboardString(Key, Value) ? EMetaplotTaskRunState::Succeeded : EMetaplotTaskRunState::Failed;
}

EMetaplotTaskRunState UMetaplotSampleIncIntTask::TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime)
{
	Super::TickTask_Implementation(Instance, DeltaTime);

	if (!Instance || Key.IsNone())
	{
		return EMetaplotTaskRunState::Failed;
	}

	int32 CurrentValue = 0;
	if (!Instance->GetBlackboardInt(Key, CurrentValue))
	{
		return EMetaplotTaskRunState::Failed;
	}

	return Instance->SetBlackboardInt(Key, CurrentValue + Delta) ? EMetaplotTaskRunState::Succeeded : EMetaplotTaskRunState::Failed;
}
