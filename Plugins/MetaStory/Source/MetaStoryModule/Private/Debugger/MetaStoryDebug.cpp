// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debugger/MetaStoryDebug.h"
#include "Debugger/MetaStoryRuntimeValidationInstanceData.h"
#include "MetaStoryExecutionContext.h"
#include "MetaStoryInstanceData.h"

#if WITH_METASTORY_DEBUG

namespace UE::MetaStory::Debug
{
FPhaseDelegate OnBeginUpdatePhase_AnyThread;
FPhaseDelegate OnEndUpdatePhase_AnyThread;

FStateDelegate OnStateEvent_AnyThread;

FTransitionDelegate OnTransitionEvent_AnyThread;

FNodeDelegate OnConditionEnterState_AnyThread;
FNodeDelegate OnTestCondition_AnyThread;
FNodeDelegate OnConditionExitState_AnyThread;
FNodeDelegate OnEvaluatorEnterTree_AnyThread;
FNodeDelegate OnTickEvaluator_AnyThread;
FNodeDelegate OnEvaluatorExitTree_AnyThread;
FNodeDelegate OnTaskEnterState_AnyThread;
FNodeDelegate OnTickTask_AnyThread;
FNodeDelegate OnTaskExitState_AnyThread;
FEventSentDelegate OnEventSent_AnyThread;
FEventConsumedDelegate OnEventConsumed_AnyThread;

FNodeReference::FNodeReference(TNotNull<const UMetaStory*> InStateTree, const FMetaStoryIndex16 InNodeIndex)
	: MetaStory(InStateTree)
	, Index(InNodeIndex)
{
}

namespace Private
{
void UpdatePhaseEnter(const FMetaStoryExecutionContext& ExecutionContext, const EMetaStoryUpdatePhase Phase, const FMetaStoryStateHandle StateHandle, const FPhaseDelegate& Delegate)
{
	Delegate.Broadcast(ExecutionContext, Phase, StateHandle);
}

void UpdatePhaseExit(const FMetaStoryExecutionContext& ExecutionContext, const EMetaStoryUpdatePhase Phase, const FMetaStoryStateHandle StateHandle, const FPhaseDelegate& Delegate)
{
	Delegate.Broadcast(ExecutionContext, Phase, StateHandle);
}

void NodeEnter(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node, const FNodeDelegate& Delegate)
{
	FMetaStoryInstanceData* InstanceData = ExecutionContext.GetMutableInstanceData();
	FGuid NodeId = Node.MetaStory->GetNodeIdFromIndex(Node.Index);
	if (ensure(NodeId.IsValid()))
	{
		if (FRuntimeValidationInstanceData* RuntimeValidation = InstanceData->GetRuntimeValidation().GetInstanceData())
		{
			const FActiveFrameID FrameID = ExecutionContext.GetCurrentlyProcessedFrame() ? ExecutionContext.GetCurrentlyProcessedFrame()->FrameID : FActiveFrameID();
			RuntimeValidation->NodeEnterState(NodeId, FrameID);
		}
		Delegate.Broadcast(ExecutionContext, FNodeDelegateArgs{ .Node = Node, .NodeId = NodeId });
	}
}

void NodeExit(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node, const FNodeDelegate& Delegate)
{
	FMetaStoryInstanceData* InstanceData = ExecutionContext.GetMutableInstanceData();
	FGuid NodeId = Node.MetaStory->GetNodeIdFromIndex(Node.Index);
	if (ensure(NodeId.IsValid()))
	{
		if (FRuntimeValidationInstanceData* RuntimeValidation = InstanceData->GetRuntimeValidation().GetInstanceData())
		{
			const FActiveFrameID FrameID = ExecutionContext.GetCurrentlyProcessedFrame() ? ExecutionContext.GetCurrentlyProcessedFrame()->FrameID : FActiveFrameID();
			RuntimeValidation->NodeExitState(NodeId, FrameID);
		}
		Delegate.Broadcast(ExecutionContext, FNodeDelegateArgs{ .Node = Node, .NodeId = NodeId });
	}
}

void NodeTick(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node, const FNodeDelegate& Delegate)
{
	const FGuid NodeId = Node.MetaStory->GetNodeIdFromIndex(Node.Index);
	if (ensure(NodeId.IsValid()))
	{
		Delegate.Broadcast(ExecutionContext, FNodeDelegateArgs{ .Node = Node, .NodeId = NodeId });
	}
}
} //namespace Private

void EnterPhase(const FMetaStoryExecutionContext& ExecutionContext, const EMetaStoryUpdatePhase Phase, const FMetaStoryStateHandle StateHandle)
{
	Private::UpdatePhaseEnter(ExecutionContext, Phase, StateHandle, OnBeginUpdatePhase_AnyThread);
}

void ExitPhase(const FMetaStoryExecutionContext& ExecutionContext, const EMetaStoryUpdatePhase Phase, const FMetaStoryStateHandle StateHandle)
{
	Private::UpdatePhaseExit(ExecutionContext, Phase, StateHandle, OnEndUpdatePhase_AnyThread);
}

void StateEvent(const FMetaStoryExecutionContext& ExecutionContext, const FMetaStoryStateHandle StateHandle, const EMetaStoryTraceEventType EventType)
{
	OnStateEvent_AnyThread.Broadcast(ExecutionContext, StateHandle, EventType);
}

void TransitionEvent(const FMetaStoryExecutionContext& ExecutionContext, const FMetaStoryTransitionSource& TransitionSource, const EMetaStoryTraceEventType EventType)
{
	OnTransitionEvent_AnyThread.Broadcast(ExecutionContext, TransitionSource, EventType);
}

void ConditionEnterState(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node)
{
	Private::NodeEnter(ExecutionContext, Node, OnConditionEnterState_AnyThread);
}

void ConditionTest(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node)
{
	Private::NodeTick(ExecutionContext, Node, OnTestCondition_AnyThread);
}

void ConditionExitState(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node)
{
	Private::NodeExit(ExecutionContext, Node, OnConditionExitState_AnyThread);
}

void EvaluatorEnterTree(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node)
{
	Private::NodeEnter(ExecutionContext, Node, OnEvaluatorEnterTree_AnyThread);
}

void EvaluatorTick(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node)
{
	Private::NodeTick(ExecutionContext, Node, OnTickEvaluator_AnyThread);
}

void EvaluatorExitTree(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node)
{
	Private::NodeExit(ExecutionContext, Node, OnEvaluatorExitTree_AnyThread);
}

void TaskEnterState(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node)
{
	Private::NodeEnter(ExecutionContext, Node, OnTaskEnterState_AnyThread);
}

void TaskTick(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node)
{
	Private::NodeTick(ExecutionContext, Node, OnTickTask_AnyThread);
}

void TaskExitState(const FMetaStoryExecutionContext& ExecutionContext, const FNodeReference Node)
{
	Private::NodeExit(ExecutionContext, Node, OnTaskExitState_AnyThread);
}

void EventSent(const FMetaStoryMinimalExecutionContext& ExecutionContext, TNotNull<const UMetaStory*> MetaStory, const FGameplayTag Tag, const FConstStructView Payload, const FName Origin)
{
	OnEventSent_AnyThread.Broadcast(ExecutionContext, FEventSentDelegateArgs{ .MetaStory = MetaStory, .Tag = Tag, .Payload = Payload, .Origin = Origin });
}

void EventConsumed(const FMetaStoryExecutionContext& ExecutionContext, const FMetaStorySharedEvent& Event)
{
	OnEventConsumed_AnyThread.Broadcast(ExecutionContext, Event);
}

}//namespace UE::MetaStory::Debug

#endif // WITH_METASTORY_DEBUG
