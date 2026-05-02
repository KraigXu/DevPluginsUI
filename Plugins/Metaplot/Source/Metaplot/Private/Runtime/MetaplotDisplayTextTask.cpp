#include "Runtime/MetaplotDisplayTextTask.h"

#include "Runtime/MetaplotInstance.h"

void UMetaplotDisplayTextTask::EnterTask_Implementation(UMetaplotInstance* Instance, FGuid NodeId)
{
	Super::EnterTask_Implementation(Instance, NodeId);
	bHasEntered = true;
}

EMetaplotTaskRunState UMetaplotDisplayTextTask::TickTask_Implementation(UMetaplotInstance* Instance, float DeltaTime)
{
	Super::TickTask_Implementation(Instance, DeltaTime);

	// Baseline behavior: this task only exposes editable/bindable parameters.
	// Runtime systems can consume these values from the instantiated task object.
	if (!bEnabled)
	{
		return EMetaplotTaskRunState::Succeeded;
	}

	if (!bHasEntered)
	{
		return EMetaplotTaskRunState::Running;
	}

	return EMetaplotTaskRunState::Succeeded;
}

