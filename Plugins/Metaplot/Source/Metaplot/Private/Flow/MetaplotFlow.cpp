#include "Flow/MetaplotFlow.h"

#include "Runtime/MetaplotStoryTask.h"

void UMetaplotFlow::PostLoad()
{
	Super::PostLoad();
	MigrateStoryTaskSpecsToEditorTaskNodes();
}

bool UMetaplotFlow::MigrateStoryTaskSpecsToEditorTaskNodes()
{
	// Migration and cleanup are only needed when legacy data still exists.
	if (NodeTaskSets.IsEmpty())
	{
		return false;
	}

	// If new data already exists, only drop legacy payload to avoid divergence.
	if (!NodeEditorTaskSets.IsEmpty())
	{
		NodeTaskSets.Reset();
		return true;
	}

	NodeEditorTaskSets.Reserve(NodeTaskSets.Num());
	for (const FMetaplotNodeStoryTasks& LegacyTaskSet : NodeTaskSets)
	{
		FMetaplotNodeEditorTasks& NewTaskSet = NodeEditorTaskSets.AddDefaulted_GetRef();
		NewTaskSet.NodeId = LegacyTaskSet.NodeId;
		NewTaskSet.Tasks.Reserve(LegacyTaskSet.StoryTasks.Num());

		for (const FMetaplotStoryTaskSpec& LegacyTask : LegacyTaskSet.StoryTasks)
		{
			FMetaplotEditorTaskNode& NewTaskNode = NewTaskSet.Tasks.AddDefaulted_GetRef();
			NewTaskNode.ID = FGuid::NewGuid();
			NewTaskNode.InstanceObject = LegacyTask.Task;
			NewTaskNode.TaskClass = LegacyTask.TaskClass;
			NewTaskNode.bEnabled = true;
			NewTaskNode.bConsideredForCompletion = true;

			if (LegacyTask.Task)
			{
				if (!NewTaskNode.TaskClass.IsValid())
				{
					NewTaskNode.TaskClass = LegacyTask.Task->GetClass();
				}

				if (LegacyTask.Task->GetOuter() != this)
				{
					LegacyTask.Task->Rename(nullptr, this, REN_DontCreateRedirectors | REN_NonTransactional);
				}
			}
		}
	}

	// Clear legacy storage once converted to avoid dual-source drift.
	NodeTaskSets.Reset();
	return true;
}
