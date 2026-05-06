// Copyright Epic Games, Inc. All Rights Reserved.

#include "Flow/MetaStoryFlow.h"

namespace MetaStoryFlowPrivate
{
	static bool NormalizeTaskNode(FMetaStoryFlowEditorTaskNode& TaskNode, UObject* InstanceOuter)
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

		if (!TaskNode.NodeData.IsValid() || TaskNode.NodeData.GetScriptStruct() != FMetaStoryFlowEditorTaskNodeData::StaticStruct())
		{
			TaskNode.NodeData.InitializeAs<FMetaStoryFlowEditorTaskNodeData>();
			bChanged = true;
		}
		FMetaStoryFlowEditorTaskNodeData& NodeData = TaskNode.NodeData.GetMutable<FMetaStoryFlowEditorTaskNodeData>();
		const TSoftClassPtr<UObject> DerivedTaskClass = TaskNode.InstanceObject ? TaskNode.InstanceObject->GetClass() : nullptr;
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

		if (!TaskNode.InstanceData.IsValid() || TaskNode.InstanceData.GetScriptStruct() != FMetaStoryFlowEditorTaskInstanceData::StaticStruct())
		{
			TaskNode.InstanceData.InitializeAs<FMetaStoryFlowEditorTaskInstanceData>();
			bChanged = true;
		}
		FMetaStoryFlowEditorTaskInstanceData& InstanceData = TaskNode.InstanceData.GetMutable<FMetaStoryFlowEditorTaskInstanceData>();
		if (InstanceData.InstanceObject != TaskNode.InstanceObject)
		{
			InstanceData.InstanceObject = TaskNode.InstanceObject;
			bChanged = true;
		}
		return bChanged;
	}
}

void UMetaStoryFlow::PostLoad()
{
	Super::PostLoad();
	NormalizeEditorTaskNodes();
	SyncNodeStatesWithNodes();
}

bool UMetaStoryFlow::NormalizeEditorTaskNodes()
{
	bool bChanged = false;

	for (FMetaStoryFlowNodeState& NodeState : NodeStates)
	{
		for (FMetaStoryFlowEditorTaskNode& TaskNode : NodeState.Tasks)
		{
			bChanged |= MetaStoryFlowPrivate::NormalizeTaskNode(TaskNode, this);
		}
	}

	return bChanged;
}

bool UMetaStoryFlow::SyncNodeStatesWithNodes()
{
	TSet<FGuid> ValidNodeIds;
	ValidNodeIds.Reserve(Nodes.Num());
	for (const FMetaStoryFlowNode& Node : Nodes)
	{
		if (Node.NodeId.IsValid())
		{
			ValidNodeIds.Add(Node.NodeId);
		}
	}

	bool bChanged = false;

	NodeStates.RemoveAll([&](const FMetaStoryFlowNodeState& Entry)
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

	for (const FMetaStoryFlowNode& Node : Nodes)
	{
		if (!Node.NodeId.IsValid())
		{
			continue;
		}

		const bool bExists = NodeStates.ContainsByPredicate([&](const FMetaStoryFlowNodeState& Entry)
		{
			return Entry.ID == Node.NodeId;
		});
		if (!bExists)
		{
			FMetaStoryFlowNodeState& NewState = NodeStates.AddDefaulted_GetRef();
			NewState.ID = Node.NodeId;
			NewState.Name = Node.NodeName;
			NewState.Description = Node.Description;
			NewState.bEnabled = true;
			bChanged = true;
		}
	}

	return bChanged;
}
