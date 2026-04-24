#include "Runtime/MetaplotStoryTask.h"

void UMetaplotStoryTask::EnterTask_Implementation(UMetaplotInstance* Instance, FGuid NodeId)
{
}

EMetaplotTaskRunState UMetaplotStoryTask::TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime)
{
	return EMetaplotTaskRunState::Succeeded;
}

void UMetaplotStoryTask::ExitTask_Implementation(UMetaplotInstance* Instance, FGuid NodeId)
{
}
