#include "Scenario/MetaplotFlowPlacement.h"

#include "Flow/MetaplotFlow.h"

bool MetaplotFlowPlacement::IsValidCellForNodeMove(const UMetaplotFlow* Flow, const FGuid& MovingNodeId, int32 NewStage, int32 NewLayer)
{
	if (!Flow || !MovingNodeId.IsValid())
	{
		return false;
	}

	if (NewStage < 0 || NewLayer < 0)
	{
		return false;
	}

	for (const FMetaplotNode& Node : Flow->Nodes)
	{
		if (Node.NodeId == MovingNodeId)
		{
			continue;
		}
		if (Node.StageIndex == NewStage && Node.LayerIndex == NewLayer)
		{
			return false;
		}
	}

	auto ResolveStage = [&](const FGuid& NodeId) -> int32
	{
		if (NodeId == MovingNodeId)
		{
			return NewStage;
		}
		const FMetaplotNode* Found = Flow->Nodes.FindByPredicate([NodeId](const FMetaplotNode& N)
		{
			return N.NodeId == NodeId;
		});
		return Found ? Found->StageIndex : 0;
	};

	auto ResolveLayer = [&](const FGuid& NodeId) -> int32
	{
		if (NodeId == MovingNodeId)
		{
			return NewLayer;
		}
		const FMetaplotNode* Found = Flow->Nodes.FindByPredicate([NodeId](const FMetaplotNode& N)
		{
			return N.NodeId == NodeId;
		});
		return Found ? Found->LayerIndex : 0;
	};

	for (const FMetaplotTransition& Tr : Flow->Transitions)
	{
		if (!Tr.SourceNodeId.IsValid() || !Tr.TargetNodeId.IsValid() || Tr.SourceNodeId == Tr.TargetNodeId)
		{
			continue;
		}

		const int32 SrcStage = ResolveStage(Tr.SourceNodeId);
		const int32 DstStage = ResolveStage(Tr.TargetNodeId);
		const int32 SrcLayer = ResolveLayer(Tr.SourceNodeId);
		const int32 DstLayer = ResolveLayer(Tr.TargetNodeId);

		if (DstStage <= SrcStage)
		{
			return false;
		}

		if (SrcLayer == DstLayer && DstStage != SrcStage + 1)
		{
			return false;
		}
	}

	return true;
}
