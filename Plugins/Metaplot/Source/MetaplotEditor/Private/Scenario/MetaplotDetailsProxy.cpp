#include "Scenario/MetaplotDetailsProxy.h"

#include "Flow/MetaplotFlow.h"

void UMetaplotNodeDetailsProxy::Initialize(UMetaplotFlow* InFlowAsset, const FGuid& InNodeId)
{
	FlowAsset = InFlowAsset;
	NodeId = InNodeId;
	PullFromFlow();
}

void UMetaplotNodeDetailsProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	PushToFlow();
}

FMetaplotNode* UMetaplotNodeDetailsProxy::FindNodeMutable() const
{
	return FlowAsset ? FlowAsset->Nodes.FindByPredicate([this](const FMetaplotNode& Node)
	{
		return Node.NodeId == NodeId;
	}) : nullptr;
}

const FMetaplotNode* UMetaplotNodeDetailsProxy::FindNode() const
{
	return FlowAsset ? FlowAsset->Nodes.FindByPredicate([this](const FMetaplotNode& Node)
	{
		return Node.NodeId == NodeId;
	}) : nullptr;
}

void UMetaplotNodeDetailsProxy::PullFromFlow()
{
	const FMetaplotNode* Node = FindNode();
	if (!Node)
	{
		return;
	}

	NodeName = Node->NodeName;
	Description = Node->Description;
	NodeType = Node->NodeType;
	StageIndex = Node->StageIndex;
	LayerIndex = Node->LayerIndex;
	CompletionPolicy = Node->CompletionPolicy;
	ResultPolicy = Node->ResultPolicy;
	RuntimeResult = Node->RuntimeResult;
}

void UMetaplotNodeDetailsProxy::PushToFlow()
{
	FMetaplotNode* Node = FindNodeMutable();
	if (!Node || !FlowAsset)
	{
		return;
	}

	FlowAsset->Modify();
	Node->NodeName = NodeName;
	Node->Description = Description;
	Node->NodeType = NodeType;
	const int32 ClampedStage = FMath::Max(0, StageIndex);
	const int32 ClampedLayer = FMath::Max(0, LayerIndex);
	Node->StageIndex = ClampedStage;
	Node->LayerIndex = ClampedLayer;
	StageIndex = ClampedStage;
	LayerIndex = ClampedLayer;
	Node->CompletionPolicy = CompletionPolicy;
	Node->ResultPolicy = ResultPolicy;
	FlowAsset->MarkPackageDirty();
}

void UMetaplotTransitionDetailsProxy::Initialize(UMetaplotFlow* InFlowAsset, const FGuid& InSourceNodeId, const FGuid& InTargetNodeId)
{
	FlowAsset = InFlowAsset;
	SourceNodeId = InSourceNodeId;
	TargetNodeId = InTargetNodeId;
	PullFromFlow();
}

bool UMetaplotTransitionDetailsProxy::ResolveBlackboardType(const FName& KeyName, EMetaplotBlackboardType& OutType) const
{
	if (!FlowAsset || KeyName.IsNone())
	{
		return false;
	}

	const FMetaplotBlackboardEntry* Entry = FlowAsset->DefaultBlackboard.FindByPredicate([KeyName](const FMetaplotBlackboardEntry& Candidate)
	{
		return Candidate.Name == KeyName;
	});
	if (!Entry)
	{
		return false;
	}

	OutType = Entry->Type;
	return true;
}

void UMetaplotTransitionDetailsProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	PushToFlow();
}

FMetaplotTransition* UMetaplotTransitionDetailsProxy::FindTransitionMutable() const
{
	return FlowAsset ? FlowAsset->Transitions.FindByPredicate([this](const FMetaplotTransition& Transition)
	{
		return Transition.SourceNodeId == SourceNodeId && Transition.TargetNodeId == TargetNodeId;
	}) : nullptr;
}

const FMetaplotTransition* UMetaplotTransitionDetailsProxy::FindTransition() const
{
	return FlowAsset ? FlowAsset->Transitions.FindByPredicate([this](const FMetaplotTransition& Transition)
	{
		return Transition.SourceNodeId == SourceNodeId && Transition.TargetNodeId == TargetNodeId;
	}) : nullptr;
}

void UMetaplotTransitionDetailsProxy::PullFromFlow()
{
	const FMetaplotTransition* Transition = FindTransition();
	if (!Transition)
	{
		return;
	}

	Conditions = Transition->Conditions;
}

void UMetaplotTransitionDetailsProxy::PushToFlow()
{
	FMetaplotTransition* Transition = FindTransitionMutable();
	if (!Transition || !FlowAsset)
	{
		return;
	}

	FlowAsset->Modify();
	Transition->Conditions = Conditions;
	FlowAsset->MarkPackageDirty();
}
