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
