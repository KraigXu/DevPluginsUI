#include "Flow/MetaplotFlow.h"

#include "Runtime/MetaplotStoryTask.h"

namespace MetaplotFlowPrivate
{
	static bool NormalizeTaskNode(FMetaplotEditorTaskNode& TaskNode, UObject* InstanceOuter)
	{
		bool bChanged = false;

		if (!TaskNode.ID.IsValid())
		{
			TaskNode.ID = FGuid::NewGuid();
			bChanged = true;
		}

		if (TaskNode.InstanceObject)
		{
			if (InstanceOuter && TaskNode.InstanceObject->GetOuter() != InstanceOuter)
			{
				TaskNode.InstanceObject->Rename(nullptr, InstanceOuter, REN_DontCreateRedirectors | REN_NonTransactional);
				bChanged = true;
			}
		}

		if (TaskNode.InstanceObject == nullptr && TaskNode.bEnabled)
		{
			TaskNode.bEnabled = false;
			bChanged = true;
		}

		if (!TaskNode.NodeData.IsValid() || TaskNode.NodeData.GetScriptStruct() != FMetaplotEditorTaskNodeData::StaticStruct())
		{
			TaskNode.NodeData.InitializeAs<FMetaplotEditorTaskNodeData>();
			bChanged = true;
		}
		FMetaplotEditorTaskNodeData& NodeData = TaskNode.NodeData.GetMutable<FMetaplotEditorTaskNodeData>();
		const TSoftClassPtr<UMetaplotStoryTask> DerivedTaskClass = TaskNode.InstanceObject ? TaskNode.InstanceObject->GetClass() : nullptr;
		if (NodeData.TaskClass != DerivedTaskClass)
		{
			NodeData.TaskClass = DerivedTaskClass;
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
	NormalizeEditorTaskNodes();
	SyncNodeStatesWithNodes();
}

bool UMetaplotFlow::NormalizeEditorTaskNodes()
{
	bool bChanged = false;

	for (FMetaplotNodeState& NodeState : NodeStates)
	{
		for (FMetaplotEditorTaskNode& TaskNode : NodeState.Tasks)
		{
			bChanged |= MetaplotFlowPrivate::NormalizeTaskNode(TaskNode, this);
		}
	}

	return bChanged;
}

bool UMetaplotFlow::SyncNodeStatesWithNodes()
{
	// Kept for API compatibility while the project migrates to NodeStates-only task storage.
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

	NodeStates.RemoveAll([&](const FMetaplotNodeState& Entry)
	{
		const bool bRemove = !Entry.ID.IsValid() || !ValidNodeIds.Contains(Entry.ID);
		bChanged |= bRemove;
		return bRemove;
	});

	TSet<FGuid> SeenTaskSetNodeIds;
	SeenTaskSetNodeIds.Reserve(NodeStates.Num());
	for (int32 Index = NodeStates.Num() - 1; Index >= 0; --Index)
	{
		const FGuid& NodeId = NodeStates[Index].ID;
		if (SeenTaskSetNodeIds.Contains(NodeId))
		{
			NodeStates.RemoveAt(Index);
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

		const bool bExists = NodeStates.ContainsByPredicate([&](const FMetaplotNodeState& Entry)
		{
			return Entry.ID == Node.NodeId;
		});
		if (!bExists)
		{
			FMetaplotNodeState& NewState = NodeStates.AddDefaulted_GetRef();
			NewState.ID = Node.NodeId;
			NewState.Name = Node.NodeName;
			NewState.Description = Node.Description;
			NewState.Type = Node.NodeType;
			NewState.bEnabled = true;
			bChanged = true;
		}
	}

	return bChanged;
}
