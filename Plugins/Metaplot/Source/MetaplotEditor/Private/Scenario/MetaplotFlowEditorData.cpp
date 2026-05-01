#include "Scenario/MetaplotFlowEditorData.h"

#include "Flow/MetaplotFlow.h"

void UMetaplotFlowEditorData::Initialize(UMetaplotFlow* InFlowAsset)
{
	FlowAsset = InFlowAsset;
	ResetSelection();
	SyncAuxiliaryData();
}

void UMetaplotFlowEditorData::ResetSelection()
{
	SelectedNodeId.Invalidate();
	SelectedTransitionIndex = INDEX_NONE;
}

void UMetaplotFlowEditorData::SetSelectedNode(const FGuid& InNodeId)
{
	SelectedNodeId = InNodeId;
	SelectedTransitionIndex = INDEX_NONE;
}

void UMetaplotFlowEditorData::SetSelectedTransitionIndex(const int32 InTransitionIndex)
{
	SelectedTransitionIndex = InTransitionIndex;
	if (InTransitionIndex != INDEX_NONE)
	{
		SelectedNodeId.Invalidate();
	}
}

const FMetaplotNode* UMetaplotFlowEditorData::FindSelectedNode() const
{
	if (!FlowAsset || !SelectedNodeId.IsValid())
	{
		return nullptr;
	}

	return FlowAsset->Nodes.FindByPredicate([this](const FMetaplotNode& Node)
	{
		return Node.NodeId == SelectedNodeId;
	});
}

FMetaplotNode* UMetaplotFlowEditorData::FindMutableSelectedNode()
{
	return const_cast<FMetaplotNode*>(static_cast<const UMetaplotFlowEditorData*>(this)->FindSelectedNode());
}

const FMetaplotNodeState* UMetaplotFlowEditorData::FindSelectedNodeState() const
{
	if (!FlowAsset || !SelectedNodeId.IsValid())
	{
		return nullptr;
	}

	return FlowAsset->NodeStates.FindByPredicate([this](const FMetaplotNodeState& State)
	{
		return State.ID == SelectedNodeId;
	});
}

FMetaplotNodeState* UMetaplotFlowEditorData::FindMutableSelectedNodeState()
{
	return const_cast<FMetaplotNodeState*>(static_cast<const UMetaplotFlowEditorData*>(this)->FindSelectedNodeState());
}

const FMetaplotTransition* UMetaplotFlowEditorData::FindSelectedTransition() const
{
	if (!FlowAsset || !FlowAsset->Transitions.IsValidIndex(SelectedTransitionIndex))
	{
		return nullptr;
	}

	return &FlowAsset->Transitions[SelectedTransitionIndex];
}

FMetaplotTransition* UMetaplotFlowEditorData::FindMutableSelectedTransition()
{
	return const_cast<FMetaplotTransition*>(static_cast<const UMetaplotFlowEditorData*>(this)->FindSelectedTransition());
}

void UMetaplotFlowEditorData::SyncAuxiliaryData()
{
	if (!FlowAsset)
	{
		return;
	}

	FlowAsset->SyncNodeStatesWithNodes();

	// Keep NodeStates one-to-one with Nodes by NodeId.
	for (const FMetaplotNode& Node : FlowAsset->Nodes)
	{
		FMetaplotNodeState* NodeState = FlowAsset->NodeStates.FindByPredicate([&Node](const FMetaplotNodeState& State)
		{
			return State.ID == Node.NodeId;
		});

		if (!NodeState)
		{
			FMetaplotNodeState NewState;
			NewState.ID = Node.NodeId;
			NewState.Name = Node.NodeName;
			NewState.Description = Node.Description;
			NewState.Type = Node.NodeType;
			NewState.bEnabled = true;
			NewState.Transitions = FlowAsset->Transitions.FilterByPredicate([&Node](const FMetaplotTransition& Transition)
			{
				return Transition.SourceNodeId == Node.NodeId;
			});

			FlowAsset->NodeStates.Add(MoveTemp(NewState));
		}
		else
		{
			NodeState->Name = Node.NodeName;
			NodeState->Description = Node.Description;
			NodeState->Type = Node.NodeType;
			NodeState->Transitions = FlowAsset->Transitions.FilterByPredicate([&Node](const FMetaplotTransition& Transition)
			{
				return Transition.SourceNodeId == Node.NodeId;
			});

		}
	}

	FlowAsset->NodeStates.RemoveAll([this](const FMetaplotNodeState& State)
	{
		return !FlowAsset->Nodes.ContainsByPredicate([&State](const FMetaplotNode& Node)
		{
			return Node.NodeId == State.ID;
		});
	});

	if (SelectedTransitionIndex != INDEX_NONE && !FlowAsset->Transitions.IsValidIndex(SelectedTransitionIndex))
	{
		SelectedTransitionIndex = INDEX_NONE;
	}
	if (SelectedNodeId.IsValid())
	{
		const bool bNodeStillExists = FlowAsset->Nodes.ContainsByPredicate([this](const FMetaplotNode& Node)
		{
			return Node.NodeId == SelectedNodeId;
		});
		if (!bNodeStillExists)
		{
			SelectedNodeId.Invalidate();
		}
	}
}

bool UMetaplotFlowEditorData::EnsureSelectedNodeState()
{
	if (!FlowAsset || !SelectedNodeId.IsValid())
	{
		return false;
	}

	if (FindSelectedNodeState())
	{
		return true;
	}

	const FMetaplotNode* Node = FindSelectedNode();
	if (!Node)
	{
		return false;
	}

	FMetaplotNodeState NewState;
	NewState.ID = Node->NodeId;
	NewState.Name = Node->NodeName;
	NewState.Description = Node->Description;
	NewState.Type = Node->NodeType;
	NewState.Transitions = FlowAsset->Transitions.FilterByPredicate([this](const FMetaplotTransition& Transition)
	{
		return Transition.SourceNodeId == SelectedNodeId;
	});

	FlowAsset->NodeStates.Add(MoveTemp(NewState));
	return true;
}
