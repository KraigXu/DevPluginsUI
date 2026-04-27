#include "Flow/MetaplotFlow.h"

#include "Runtime/MetaplotStoryTask.h"

namespace MetaplotFlowPrivate
{
	static bool SyncLegacyFromInstancedData(FMetaplotEditorTaskNode& TaskNode)
	{
		bool bChanged = false;

		if (TaskNode.NodeData.IsValid() && TaskNode.NodeData.GetScriptStruct() == FMetaplotEditorTaskNodeData::StaticStruct())
		{
			const FMetaplotEditorTaskNodeData& NodeData = TaskNode.NodeData.Get<FMetaplotEditorTaskNodeData>();
			if (TaskNode.TaskClass.IsNull() && !NodeData.TaskClass.IsNull())
			{
				TaskNode.TaskClass = NodeData.TaskClass;
				bChanged = true;
			}
			if (TaskNode.bEnabled != NodeData.bEnabled)
			{
				TaskNode.bEnabled = NodeData.bEnabled;
				bChanged = true;
			}
			if (TaskNode.bConsideredForCompletion != NodeData.bConsideredForCompletion)
			{
				TaskNode.bConsideredForCompletion = NodeData.bConsideredForCompletion;
				bChanged = true;
			}
		}

		if (TaskNode.InstanceData.IsValid() && TaskNode.InstanceData.GetScriptStruct() == FMetaplotEditorTaskInstanceData::StaticStruct())
		{
			const FMetaplotEditorTaskInstanceData& InstanceData = TaskNode.InstanceData.Get<FMetaplotEditorTaskInstanceData>();
			if (!TaskNode.InstanceObject && InstanceData.InstanceObject)
			{
				TaskNode.InstanceObject = InstanceData.InstanceObject;
				bChanged = true;
			}
		}

		return bChanged;
	}

	static bool SyncInstancedDataFromLegacy(FMetaplotEditorTaskNode& TaskNode)
	{
		bool bChanged = false;

		if (!TaskNode.NodeData.IsValid() || TaskNode.NodeData.GetScriptStruct() != FMetaplotEditorTaskNodeData::StaticStruct())
		{
			TaskNode.NodeData.InitializeAs<FMetaplotEditorTaskNodeData>();
			bChanged = true;
		}
		FMetaplotEditorTaskNodeData& NodeData = TaskNode.NodeData.GetMutable<FMetaplotEditorTaskNodeData>();
		if (NodeData.TaskClass != TaskNode.TaskClass)
		{
			NodeData.TaskClass = TaskNode.TaskClass;
			bChanged = true;
		}
		if (NodeData.bEnabled != TaskNode.bEnabled)
		{
			NodeData.bEnabled = TaskNode.bEnabled;
			bChanged = true;
		}
		if (NodeData.bConsideredForCompletion != TaskNode.bConsideredForCompletion)
		{
			NodeData.bConsideredForCompletion = TaskNode.bConsideredForCompletion;
			bChanged = true;
		}

		if (!TaskNode.InstanceData.IsValid() || TaskNode.InstanceData.GetScriptStruct() != FMetaplotEditorTaskInstanceData::StaticStruct())
		{
			TaskNode.InstanceData.InitializeAs<FMetaplotEditorTaskInstanceData>();
			bChanged = true;
		}
		FMetaplotEditorTaskInstanceData& InstanceData = TaskNode.InstanceData.GetMutable<FMetaplotEditorTaskInstanceData>();
		if (InstanceData.InstanceObject != TaskNode.InstanceObject)
		{
			InstanceData.InstanceObject = TaskNode.InstanceObject;
			bChanged = true;
		}

		return bChanged;
	}
}

void UMetaplotFlow::PostLoad()
{
	Super::PostLoad();
	MigrateStoryTaskSpecsToEditorTaskNodes();
	NormalizeEditorTaskNodes();
	SyncNodeEditorTaskSetsWithNodes();
}

bool UMetaplotFlow::MigrateStoryTaskSpecsToEditorTaskNodes()
{
	if (NodeTaskSets.IsEmpty())
	{
		return false;
	}

	bool bMigrated = false;
	for (const FMetaplotNodeStoryTasks& LegacyTaskSet : NodeTaskSets)
	{
		FMetaplotNodeEditorTasks* ExistingTaskSet = NodeEditorTaskSets.FindByPredicate(
			[&LegacyTaskSet](const FMetaplotNodeEditorTasks& EditorTaskSet)
			{
				return EditorTaskSet.NodeId == LegacyTaskSet.NodeId;
			});

		if (!ExistingTaskSet)
		{
			ExistingTaskSet = &NodeEditorTaskSets.AddDefaulted_GetRef();
			ExistingTaskSet->NodeId = LegacyTaskSet.NodeId;
		}

		ExistingTaskSet->Tasks.Reserve(ExistingTaskSet->Tasks.Num() + LegacyTaskSet.StoryTasks.Num());
		for (const FMetaplotStoryTaskSpec& LegacyTask : LegacyTaskSet.StoryTasks)
		{
			FMetaplotEditorTaskNode& NewTaskNode = ExistingTaskSet->Tasks.AddDefaulted_GetRef();
			NewTaskNode.ID = FGuid::NewGuid();
			NewTaskNode.InstanceObject = LegacyTask.Task;
			NewTaskNode.TaskClass = LegacyTask.TaskClass;
			NewTaskNode.bEnabled = true;
			NewTaskNode.bConsideredForCompletion = LegacyTask.bRequired;

			if (LegacyTask.Task)
			{
				if (NewTaskNode.TaskClass.IsNull())
				{
					NewTaskNode.TaskClass = LegacyTask.Task->GetClass();
				}

				if (LegacyTask.Task->GetOuter() != this)
				{
					LegacyTask.Task->Rename(nullptr, this, REN_DontCreateRedirectors | REN_NonTransactional);
				}
			}
		}

		bMigrated = true;
	}

	NodeTaskSets.Reset();
	return bMigrated;
}

bool UMetaplotFlow::NormalizeEditorTaskNodes()
{
	bool bChanged = false;

	for (FMetaplotNodeEditorTasks& EditorTaskSet : NodeEditorTaskSets)
	{
		for (FMetaplotEditorTaskNode& TaskNode : EditorTaskSet.Tasks)
		{
			bChanged |= MetaplotFlowPrivate::SyncLegacyFromInstancedData(TaskNode);

			if (!TaskNode.ID.IsValid())
			{
				TaskNode.ID = FGuid::NewGuid();
				bChanged = true;
			}

			if (TaskNode.InstanceObject)
			{
				if (TaskNode.TaskClass.IsNull())
				{
					TaskNode.TaskClass = TaskNode.InstanceObject->GetClass();
					bChanged = true;
				}

				if (TaskNode.InstanceObject->GetOuter() != this)
				{
					TaskNode.InstanceObject->Rename(nullptr, this, REN_DontCreateRedirectors | REN_NonTransactional);
					bChanged = true;
				}
			}

			if (TaskNode.TaskClass.IsNull() && TaskNode.InstanceObject == nullptr && TaskNode.bEnabled)
			{
				TaskNode.bEnabled = false;
				bChanged = true;
			}

			bChanged |= MetaplotFlowPrivate::SyncInstancedDataFromLegacy(TaskNode);
		}
	}

	return bChanged;
}

bool UMetaplotFlow::SyncNodeEditorTaskSetsWithNodes()
{
	TSet<FGuid> ValidNodeIds;
	ValidNodeIds.Reserve(Nodes.Num());
	for (const FMetaplotNode& Node : Nodes)
	{
		if (Node.NodeId.IsValid())
		{
			ValidNodeIds.Add(Node.NodeId);
		}
	}

	bool bChanged = false;

	NodeEditorTaskSets.RemoveAll([&](const FMetaplotNodeEditorTasks& Entry)
	{
		const bool bRemove = !Entry.NodeId.IsValid() || !ValidNodeIds.Contains(Entry.NodeId);
		bChanged |= bRemove;
		return bRemove;
	});

	TSet<FGuid> SeenTaskSetNodeIds;
	SeenTaskSetNodeIds.Reserve(NodeEditorTaskSets.Num());
	for (int32 Index = NodeEditorTaskSets.Num() - 1; Index >= 0; --Index)
	{
		const FGuid& NodeId = NodeEditorTaskSets[Index].NodeId;
		if (SeenTaskSetNodeIds.Contains(NodeId))
		{
			NodeEditorTaskSets.RemoveAt(Index);
			bChanged = true;
			continue;
		}
		SeenTaskSetNodeIds.Add(NodeId);
	}

	for (const FMetaplotNode& Node : Nodes)
	{
		if (!Node.NodeId.IsValid())
		{
			continue;
		}

		const bool bExists = NodeEditorTaskSets.ContainsByPredicate([&](const FMetaplotNodeEditorTasks& Entry)
		{
			return Entry.NodeId == Node.NodeId;
		});
		if (!bExists)
		{
			FMetaplotNodeEditorTasks& NewSet = NodeEditorTaskSets.AddDefaulted_GetRef();
			NewSet.NodeId = Node.NodeId;
			bChanged = true;
		}
	}

	return bChanged;
}
