#include "Scenario/MetaplotTransitionDetailsProxy.h"

#include "Scenario/MetaplotDetailsContext.h"
#include "Flow/MetaplotFlow.h"

void UMetaplotTransitionDetailsProxy::Initialize(UMetaplotFlow* InFlowAsset, const FGuid& InSourceNodeId, const FGuid& InTargetNodeId)
{
	FlowAsset = InFlowAsset;
	SourceNodeId = InSourceNodeId;
	TargetNodeId = InTargetNodeId;
	PullFromFlow();
}

void UMetaplotTransitionDetailsProxy::SetDetailsContext(UMetaplotDetailsContext* InDetailsContext)
{
	DetailsContext = InDetailsContext;
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
