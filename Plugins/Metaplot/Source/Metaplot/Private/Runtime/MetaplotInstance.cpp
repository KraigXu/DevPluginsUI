#include "Runtime/MetaplotInstance.h"

#include "Runtime/MetaplotStoryTask.h"

namespace MetaplotRuntimeTaskPrivate
{
	static TSoftClassPtr<UMetaplotStoryTask> ResolveTaskClass(const FMetaplotEditorTaskNode& TaskNode)
	{
		if (TaskNode.InstanceObject)
		{
			return TaskNode.InstanceObject->GetClass();
		}
		if (TaskNode.NodeData.IsValid() && TaskNode.NodeData.GetScriptStruct() == FMetaplotEditorTaskNodeData::StaticStruct())
		{
			const FMetaplotEditorTaskNodeData& NodeData = TaskNode.NodeData.Get<FMetaplotEditorTaskNodeData>();
			if (!NodeData.TaskClass.IsNull())
			{
				return NodeData.TaskClass;
			}
		}
		return nullptr;
	}

	static bool ResolveTaskEnabled(const FMetaplotEditorTaskNode& TaskNode)
	{
		if (TaskNode.NodeData.IsValid() && TaskNode.NodeData.GetScriptStruct() == FMetaplotEditorTaskNodeData::StaticStruct())
		{
			return TaskNode.NodeData.Get<FMetaplotEditorTaskNodeData>().bEnabled;
		}
		return TaskNode.bEnabled;
	}

	static bool ResolveTaskRequiredForCompletion(const FMetaplotEditorTaskNode& TaskNode)
	{
		if (TaskNode.NodeData.IsValid() && TaskNode.NodeData.GetScriptStruct() == FMetaplotEditorTaskNodeData::StaticStruct())
		{
			return TaskNode.NodeData.Get<FMetaplotEditorTaskNodeData>().bConsideredForCompletion;
		}
		return TaskNode.bConsideredForCompletion;
	}

	static UMetaplotStoryTask* ResolveTaskTemplate(const FMetaplotEditorTaskNode& TaskNode)
	{
		if (TaskNode.InstanceData.IsValid() && TaskNode.InstanceData.GetScriptStruct() == FMetaplotEditorTaskInstanceData::StaticStruct())
		{
			if (UMetaplotStoryTask* InstanceTemplate = TaskNode.InstanceData.Get<FMetaplotEditorTaskInstanceData>().InstanceObject)
			{
				return InstanceTemplate;
			}
		}
		return TaskNode.InstanceObject;
	}
}

bool UMetaplotInstance::Initialize(UMetaplotFlow* InFlow)
{
	FlowAsset = InFlow;
	CompletedNodes.Reset();
	CurrentNodeId.Invalidate();
	bIsRunning = false;
	ActiveNodeState = FMetaplotRuntimeNodeState();

	if (!FlowAsset)
	{
		return false;
	}

	return true;
}

bool UMetaplotInstance::Start()
{
	if (!FlowAsset || !FlowAsset->StartNodeId.IsValid())
	{
		return false;
	}

	bIsRunning = ActivateNode(FlowAsset->StartNodeId);
	return bIsRunning;
}

void UMetaplotInstance::TickInstance(float DeltaTime)
{
	if (!bIsRunning || !CurrentNodeId.IsValid())
	{
		return;
	}

	EvaluateActiveNode(DeltaTime);
}

bool UMetaplotInstance::ActivateNode(const FGuid& NodeId)
{
	const FMetaplotNode* Node = FindNode(NodeId);
	if (!Node)
	{
		bIsRunning = false;
		return false;
	}

	CurrentNodeId = NodeId;
	ActiveNodeState = FMetaplotRuntimeNodeState();
	ActiveNodeState.NodeId = NodeId;
	ActiveNodeState.Result = EMetaplotNodeResult::None;
	BuildNodeTasks(NodeId, ActiveNodeState);
	return true;
}

void UMetaplotInstance::BuildNodeTasks(const FGuid& NodeId, FMetaplotRuntimeNodeState& OutNodeState)
{
	if (const FMetaplotNodeState* NodeState = FindNodeState(NodeId))
	{
		for (const FMetaplotEditorTaskNode& TaskNode : NodeState->Tasks)
		{
			if (!MetaplotRuntimeTaskPrivate::ResolveTaskEnabled(TaskNode))
			{
				continue;
			}

			UMetaplotStoryTask* TaskInstance = nullptr;
			if (UMetaplotStoryTask* TemplateTask = MetaplotRuntimeTaskPrivate::ResolveTaskTemplate(TaskNode))
			{
				TaskInstance = DuplicateObject<UMetaplotStoryTask>(TemplateTask, this);
			}
			else
			{
				UClass* TaskClass = MetaplotRuntimeTaskPrivate::ResolveTaskClass(TaskNode).LoadSynchronous();
				if (TaskClass && TaskClass->IsChildOf(UMetaplotStoryTask::StaticClass()))
				{
					TaskInstance = NewObject<UMetaplotStoryTask>(this, TaskClass);
				}
			}
			if (!TaskInstance)
			{
				continue;
			}

			TaskInstance->EnterTask(this, NodeId);

			FMetaplotRuntimeTaskState RuntimeTask;
			RuntimeTask.TaskInstance = TaskInstance;
			RuntimeTask.bRequired = MetaplotRuntimeTaskPrivate::ResolveTaskRequiredForCompletion(TaskNode);
			RuntimeTask.RunState = EMetaplotTaskRunState::Running;
			OutNodeState.Tasks.Add(RuntimeTask);
		}
	}
}

void UMetaplotInstance::EvaluateActiveNode(float DeltaTime)
{
	const FMetaplotNode* Node = FindNode(CurrentNodeId);
	if (!Node)
	{
		bIsRunning = false;
		return;
	}

	bool bAnyRunning = false;
	for (FMetaplotRuntimeTaskState& TaskState : ActiveNodeState.Tasks)
	{
		if (!TaskState.TaskInstance || TaskState.RunState != EMetaplotTaskRunState::Running)
		{
			continue;
		}

		const EMetaplotTaskRunState NewState = TaskState.TaskInstance->TickTask(this, DeltaTime);
		TaskState.RunState = NewState;
		if (NewState == EMetaplotTaskRunState::Running)
		{
			bAnyRunning = true;
		}
	}

	if (bAnyRunning)
	{
		return;
	}

	for (FMetaplotRuntimeTaskState& TaskState : ActiveNodeState.Tasks)
	{
		if (TaskState.TaskInstance)
		{
			TaskState.TaskInstance->ExitTask(this, CurrentNodeId);
		}
	}

	ActiveNodeState.Result = ComputeNodeResult(*Node, ActiveNodeState);
	CompletedNodes.Add(CurrentNodeId);

	const FGuid FinishedNodeId = CurrentNodeId;
	for (const FMetaplotTransition& Transition : FlowAsset->Transitions)
	{
		if (Transition.SourceNodeId != FinishedNodeId)
		{
			continue;
		}

		if (!EvaluateTransitionConditions(Transition))
		{
			continue;
		}

		ActivateNode(Transition.TargetNodeId);
		return;
	}

	bIsRunning = false;
}

EMetaplotNodeResult UMetaplotInstance::ComputeNodeResult(const FMetaplotNode& Node, const FMetaplotRuntimeNodeState& NodeState) const
{
	bool bAnyFailed = false;
	bool bAllSucceeded = true;
	for (const FMetaplotRuntimeTaskState& TaskState : NodeState.Tasks)
	{
		if (!TaskState.bRequired)
		{
			continue;
		}

		if (TaskState.RunState == EMetaplotTaskRunState::Failed)
		{
			bAnyFailed = true;
			bAllSucceeded = false;
		}
		else if (TaskState.RunState != EMetaplotTaskRunState::Succeeded)
		{
			bAllSucceeded = false;
		}
	}

	if (Node.ResultPolicy == EMetaplotNodeResultPolicy::AnyFailedIsFailed)
	{
		return bAnyFailed ? EMetaplotNodeResult::Failed : EMetaplotNodeResult::Succeeded;
	}

	if (Node.ResultPolicy == EMetaplotNodeResultPolicy::AllSucceeded)
	{
		return bAllSucceeded ? EMetaplotNodeResult::Succeeded : EMetaplotNodeResult::Failed;
	}

	return EMetaplotNodeResult::None;
}

bool UMetaplotInstance::EvaluateTransitionConditions(const FMetaplotTransition& Transition) const
{
	for (const FMetaplotCondition& Condition : Transition.Conditions)
	{
		switch (Condition.Type)
		{
		case EMetaplotConditionType::RequiredNodeCompleted:
			if (!CompletedNodes.Contains(Condition.RequiredNodeId))
			{
				return false;
			}
			break;
		case EMetaplotConditionType::RandomProbability:
			if (FMath::FRand() > Condition.Probability)
			{
				return false;
			}
			break;
		case EMetaplotConditionType::CustomBehavior:
		default:
			return false;
		}
	}

	return true;
}

const FMetaplotNode* UMetaplotInstance::FindNode(const FGuid& NodeId) const
{
	if (!FlowAsset)
	{
		return nullptr;
	}

	for (const FMetaplotNode& Node : FlowAsset->Nodes)
	{
		if (Node.NodeId == NodeId)
		{
			return &Node;
		}
	}

	return nullptr;
}

const FMetaplotNodeState* UMetaplotInstance::FindNodeState(const FGuid& NodeId) const
{
	if (!FlowAsset)
	{
		return nullptr;
	}

	for (const FMetaplotNodeState& NodeState : FlowAsset->NodeStates)
	{
		if (NodeState.ID == NodeId)
		{
			return &NodeState;
		}
	}

	return nullptr;
}
