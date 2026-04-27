#include "Runtime/MetaplotInstance.h"

#include "Runtime/MetaplotStoryTask.h"

bool UMetaplotInstance::Initialize(UMetaplotFlow* InFlow)
{
	FlowAsset = InFlow;
	RuntimeBlackboard.Reset();
	CompletedNodes.Reset();
	CurrentNodeId.Invalidate();
	bIsRunning = false;
	ActiveNodeState = FMetaplotRuntimeNodeState();

	if (!FlowAsset)
	{
		return false;
	}

	RuntimeBlackboard = FlowAsset->DefaultBlackboard;
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

bool UMetaplotInstance::SetBlackboardInt(FName Key, int32 Value)
{
	const int32 Index = FindBlackboardEntryIndex(Key);
	if (Index == INDEX_NONE || RuntimeBlackboard[Index].Type != EMetaplotBlackboardType::Int)
	{
		return false;
	}

	RuntimeBlackboard[Index].IntValue = Value;
	return true;
}

bool UMetaplotInstance::GetBlackboardInt(FName Key, int32& OutValue) const
{
	const int32 Index = FindBlackboardEntryIndex(Key);
	if (Index == INDEX_NONE || RuntimeBlackboard[Index].Type != EMetaplotBlackboardType::Int)
	{
		return false;
	}

	OutValue = RuntimeBlackboard[Index].IntValue;
	return true;
}

bool UMetaplotInstance::SetBlackboardBool(FName Key, bool Value)
{
	const int32 Index = FindBlackboardEntryIndex(Key);
	if (Index == INDEX_NONE || RuntimeBlackboard[Index].Type != EMetaplotBlackboardType::Bool)
	{
		return false;
	}

	RuntimeBlackboard[Index].BoolValue = Value;
	return true;
}

bool UMetaplotInstance::GetBlackboardBool(FName Key, bool& OutValue) const
{
	const int32 Index = FindBlackboardEntryIndex(Key);
	if (Index == INDEX_NONE || RuntimeBlackboard[Index].Type != EMetaplotBlackboardType::Bool)
	{
		return false;
	}

	OutValue = RuntimeBlackboard[Index].BoolValue;
	return true;
}

bool UMetaplotInstance::SetBlackboardFloat(FName Key, float Value)
{
	const int32 Index = FindBlackboardEntryIndex(Key);
	if (Index == INDEX_NONE || RuntimeBlackboard[Index].Type != EMetaplotBlackboardType::Float)
	{
		return false;
	}

	RuntimeBlackboard[Index].FloatValue = Value;
	return true;
}

bool UMetaplotInstance::GetBlackboardFloat(FName Key, float& OutValue) const
{
	const int32 Index = FindBlackboardEntryIndex(Key);
	if (Index == INDEX_NONE || RuntimeBlackboard[Index].Type != EMetaplotBlackboardType::Float)
	{
		return false;
	}

	OutValue = RuntimeBlackboard[Index].FloatValue;
	return true;
}

bool UMetaplotInstance::SetBlackboardString(FName Key, const FString& Value)
{
	const int32 Index = FindBlackboardEntryIndex(Key);
	if (Index == INDEX_NONE || RuntimeBlackboard[Index].Type != EMetaplotBlackboardType::String)
	{
		return false;
	}

	RuntimeBlackboard[Index].StringValue = Value;
	return true;
}

bool UMetaplotInstance::GetBlackboardString(FName Key, FString& OutValue) const
{
	const int32 Index = FindBlackboardEntryIndex(Key);
	if (Index == INDEX_NONE || RuntimeBlackboard[Index].Type != EMetaplotBlackboardType::String)
	{
		return false;
	}

	OutValue = RuntimeBlackboard[Index].StringValue;
	return true;
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
	if (const FMetaplotNodeEditorTasks* EditorTaskSet = FindEditorTaskSet(NodeId))
	{
		for (const FMetaplotEditorTaskNode& TaskNode : EditorTaskSet->Tasks)
		{
			if (!TaskNode.bEnabled)
			{
				continue;
			}

			UMetaplotStoryTask* TaskInstance = nullptr;
			if (TaskNode.InstanceObject)
			{
				TaskInstance = DuplicateObject<UMetaplotStoryTask>(TaskNode.InstanceObject, this);
			}
			else
			{
				UClass* TaskClass = TaskNode.TaskClass.LoadSynchronous();
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
			RuntimeTask.bRequired = TaskNode.bConsideredForCompletion;
			RuntimeTask.RunState = EMetaplotTaskRunState::Running;
			OutNodeState.Tasks.Add(RuntimeTask);
		}
		return;
	}

	const FMetaplotNodeStoryTasks* LegacyTaskSet = FindLegacyTaskSet(NodeId);
	if (!LegacyTaskSet)
	{
		return;
	}

	for (const FMetaplotStoryTaskSpec& TaskSpec : LegacyTaskSet->StoryTasks)
	{
		UMetaplotStoryTask* TaskInstance = nullptr;
		if (TaskSpec.Task)
		{
			TaskInstance = DuplicateObject<UMetaplotStoryTask>(TaskSpec.Task, this);
		}
		else
		{
			// Backward compatibility for old assets that only store TaskClass.
			UClass* TaskClass = TaskSpec.TaskClass.LoadSynchronous();
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
		RuntimeTask.bRequired = TaskSpec.bRequired;
		RuntimeTask.RunState = EMetaplotTaskRunState::Running;
		OutNodeState.Tasks.Add(RuntimeTask);
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
		case EMetaplotConditionType::BlackboardCompare:
		{
			const int32 BlackboardIndex = FindBlackboardEntryIndex(Condition.BlackboardKey);
			if (BlackboardIndex == INDEX_NONE)
			{
				return false;
			}

			const FMetaplotBlackboardEntry& Entry = RuntimeBlackboard[BlackboardIndex];
			if (Entry.Type == EMetaplotBlackboardType::Int)
			{
				if (!CompareBlackboardInt(Entry.IntValue, Condition.ComparisonOp, Condition.IntValue))
				{
					return false;
				}
			}
			else if (Entry.Type == EMetaplotBlackboardType::Bool)
			{
				const bool bMatches = (Condition.ComparisonOp == EMetaplotComparisonOp::Equal && Entry.BoolValue == Condition.BoolValue)
					|| (Condition.ComparisonOp == EMetaplotComparisonOp::NotEqual && Entry.BoolValue != Condition.BoolValue);
				if (!bMatches)
				{
					return false;
				}
			}
			else
			{
				return false;
			}
			break;
		}
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

const FMetaplotNodeEditorTasks* UMetaplotInstance::FindEditorTaskSet(const FGuid& NodeId) const
{
	if (!FlowAsset)
	{
		return nullptr;
	}

	for (const FMetaplotNodeEditorTasks& NodeTasks : FlowAsset->NodeEditorTaskSets)
	{
		if (NodeTasks.NodeId == NodeId)
		{
			return &NodeTasks;
		}
	}

	return nullptr;
}

const FMetaplotNodeStoryTasks* UMetaplotInstance::FindLegacyTaskSet(const FGuid& NodeId) const
{
	if (!FlowAsset)
	{
		return nullptr;
	}

	for (const FMetaplotNodeStoryTasks& NodeTasks : FlowAsset->NodeTaskSets)
	{
		if (NodeTasks.NodeId == NodeId)
		{
			return &NodeTasks;
		}
	}

	return nullptr;
}

int32 UMetaplotInstance::FindBlackboardEntryIndex(FName Key) const
{
	for (int32 i = 0; i < RuntimeBlackboard.Num(); ++i)
	{
		if (RuntimeBlackboard[i].Name == Key)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

bool UMetaplotInstance::CompareBlackboardInt(int32 CurrentValue, EMetaplotComparisonOp Op, int32 ExpectedValue) const
{
	switch (Op)
	{
	case EMetaplotComparisonOp::Equal:
		return CurrentValue == ExpectedValue;
	case EMetaplotComparisonOp::NotEqual:
		return CurrentValue != ExpectedValue;
	case EMetaplotComparisonOp::Greater:
		return CurrentValue > ExpectedValue;
	case EMetaplotComparisonOp::Less:
		return CurrentValue < ExpectedValue;
	case EMetaplotComparisonOp::GreaterOrEqual:
		return CurrentValue >= ExpectedValue;
	case EMetaplotComparisonOp::LessOrEqual:
		return CurrentValue <= ExpectedValue;
	default:
		return false;
	}
}
