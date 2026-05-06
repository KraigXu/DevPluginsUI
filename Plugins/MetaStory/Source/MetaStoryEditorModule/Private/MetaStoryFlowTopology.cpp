// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryFlowTopology.h"

#include "Flow/MetaStoryFlow.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorModule.h"
#include "MetaStoryState.h"
#include "MetaStoryTasksStatus.h"
#include "MetaStoryTypes.h"

namespace UE::MetaStory::FlowTopology
{
	namespace Private
	{
		static void Report(FMetaStoryCompilerLog* Log, EMessageSeverity::Type Severity, const FString& Message)
		{
			if (Log)
			{
				Log->Reportf(Severity, TEXT("%s"), *Message);
			}
			else if (Severity == EMessageSeverity::Error)
			{
				UE_LOG(LogMetaStoryEditor, Error, TEXT("%s"), *Message);
			}
			else if (Severity == EMessageSeverity::Warning)
			{
				UE_LOG(LogMetaStoryEditor, Warning, TEXT("%s"), *Message);
			}
		}
	}

	bool RebuildShadowStates(UMetaStoryEditorData& EditorData, FMetaStoryCompilerLog* Log)
	{
		if (!EditorData.bUseMetaStoryFlowTopology)
		{
			return true;
		}

		UMetaStoryFlow* Flow = EditorData.MetaStoryFlow;
		if (!Flow)
		{
			Private::Report(Log, EMessageSeverity::Error, TEXT("Flow topology is enabled but MetaStoryFlow is null."));
			return false;
		}

		Flow->SyncNodeStatesWithNodes();
		Flow->NormalizeEditorTaskNodes();

		if (Flow->Nodes.IsEmpty())
		{
			Private::Report(Log, EMessageSeverity::Error, TEXT("MetaStoryFlow has no nodes."));
			return false;
		}

		if (!Flow->StartNodeId.IsValid())
		{
			Private::Report(Log, EMessageSeverity::Error, TEXT("MetaStoryFlow has an invalid StartNodeId."));
			return false;
		}

		const FMetaStoryFlowNode* StartNode = Flow->Nodes.FindByPredicate([&](const FMetaStoryFlowNode& N)
		{
			return N.NodeId == Flow->StartNodeId;
		});
		if (!StartNode)
		{
			Private::Report(Log, EMessageSeverity::Error, TEXT("StartNodeId does not match any node in MetaStoryFlow."));
			return false;
		}

		auto HasNodeId = [&Flow](const FGuid& Id) -> bool
		{
			return Flow->Nodes.ContainsByPredicate([&Id](const FMetaStoryFlowNode& N)
			{
				return N.NodeId == Id;
			});
		};

		for (const FMetaStoryFlowTransition& Tr : Flow->Transitions)
		{
			if (!HasNodeId(Tr.SourceNodeId) || !HasNodeId(Tr.TargetNodeId))
			{
				Private::Report(Log, EMessageSeverity::Error, FString::Printf(TEXT("Flow transition references unknown node (%s -> %s)."), *Tr.SourceNodeId.ToString(), *Tr.TargetNodeId.ToString()));
				return false;
			}
		}

		TMap<FGuid, int32> InDegree;
		for (const FMetaStoryFlowNode& N : Flow->Nodes)
		{
			InDegree.FindOrAdd(N.NodeId);
		}
		for (const FMetaStoryFlowTransition& Tr : Flow->Transitions)
		{
			if (Tr.TargetNodeId.IsValid())
			{
				InDegree.FindOrAdd(Tr.TargetNodeId)++;
			}
		}

		if (InDegree.FindRef(Flow->StartNodeId) != 0)
		{
			Private::Report(Log, EMessageSeverity::Error, TEXT("Flow start node must have no incoming transitions."));
			return false;
		}

		for (const FMetaStoryFlowNode& N : Flow->Nodes)
		{
			if (N.NodeId == Flow->StartNodeId)
			{
				continue;
			}
			const int32 Inc = InDegree.FindRef(N.NodeId);
			if (Inc == 0)
			{
				Private::Report(Log, EMessageSeverity::Warning, FString::Printf(TEXT("Flow node %s is not reachable (no incoming transitions)."), *N.NodeId.ToString()));
			}
			else if (Inc > 1)
			{
				Private::Report(Log, EMessageSeverity::Error, FString::Printf(TEXT("Flow node %s has %d incoming transitions; merge/join is not supported in the flow→MetaStory bridge yet."), *N.NodeId.ToString(), Inc));
				return false;
			}
		}

		EditorData.SubTrees.Reset();

		UMetaStoryState* Root = NewObject<UMetaStoryState>(&EditorData, TEXT("MetaStoryFlowRoot"), RF_Transactional);
		Root->Name = TEXT("MetaStoryFlowRoot");
		Root->Type = EMetaStoryStateType::Group;
		Root->SelectionBehavior = EMetaStoryStateSelectionBehavior::TrySelectChildrenInOrder;
		Root->ID = FGuid::NewGuid();

		TMap<FGuid, UMetaStoryState*> NodeIdToState;

		TArray<const FMetaStoryFlowNode*> SortedNodes;
		SortedNodes.Reserve(Flow->Nodes.Num());
		for (const FMetaStoryFlowNode& N : Flow->Nodes)
		{
			SortedNodes.Add(&N);
		}
		SortedNodes.Sort([](const FMetaStoryFlowNode& A, const FMetaStoryFlowNode& B)
		{
			if (A.StageIndex != B.StageIndex)
			{
				return A.StageIndex < B.StageIndex;
			}
			if (A.LayerIndex != B.LayerIndex)
			{
				return A.LayerIndex < B.LayerIndex;
			}
			return A.NodeId < B.NodeId;
		});

		for (const FMetaStoryFlowNode* NodePtr : SortedNodes)
		{
			const FMetaStoryFlowNode& PlotNode = *NodePtr;
			UMetaStoryState* S = NewObject<UMetaStoryState>(Root, MakeUniqueObjectName(Root, UMetaStoryState::StaticClass(), FName(*FString::Printf(TEXT("Node_%s"), *PlotNode.NodeId.ToString(EGuidFormats::Short)))), RF_Transactional);
			S->Parent = Root;
			S->ID = PlotNode.NodeId;
			if (!PlotNode.NodeName.IsEmpty())
			{
				S->Name = FName(PlotNode.NodeName.ToString());
			}
			else
			{
				S->Name = FName(*FString::Printf(TEXT("Node_%s"), *PlotNode.NodeId.ToString(EGuidFormats::Short)));
			}
			S->Description = PlotNode.Description.ToString();
			S->Type = EMetaStoryStateType::State;
			S->SelectionBehavior = EMetaStoryStateSelectionBehavior::TryEnterState;
			S->TasksCompletion = (PlotNode.NodeType == EMetaStoryFlowNodeType::Parallel) ? EMetaStoryTaskCompletionType::All : EMetaStoryTaskCompletionType::Any;

			NodeIdToState.Add(PlotNode.NodeId, S);
		}

		auto OrderChildren = [&]() -> TArray<UMetaStoryState*>
		{
			TArray<UMetaStoryState*> Ordered;
			Ordered.Reserve(SortedNodes.Num());
			UMetaStoryState* const* StartPtr = NodeIdToState.Find(Flow->StartNodeId);
			if (StartPtr && *StartPtr)
			{
				Ordered.Add(*StartPtr);
			}
			for (const FMetaStoryFlowNode* NodePtr : SortedNodes)
			{
				UMetaStoryState* const* Ptr = NodeIdToState.Find(NodePtr->NodeId);
				if (!Ptr || !*Ptr)
				{
					continue;
				}
				if (NodePtr->NodeId == Flow->StartNodeId)
				{
					continue;
				}
				Ordered.Add(*Ptr);
			}
			return Ordered;
		};

		Root->Children = OrderChildren();

		for (UMetaStoryState* Child : Root->Children)
		{
			if (Child)
			{
				Child->Parent = Root;
			}
		}

		for (const FMetaStoryFlowTransition& Tr : Flow->Transitions)
		{
			UMetaStoryState* const* SrcPtr = NodeIdToState.Find(Tr.SourceNodeId);
			UMetaStoryState* const* TgtPtr = NodeIdToState.Find(Tr.TargetNodeId);
			if (!SrcPtr || !TgtPtr || !*SrcPtr || !*TgtPtr)
			{
				Private::Report(Log, EMessageSeverity::Warning, FString::Printf(TEXT("Skipping flow transition with missing endpoint (%s -> %s)."), *Tr.SourceNodeId.ToString(), *Tr.TargetNodeId.ToString()));
				continue;
			}

			UMetaStoryState* SrcState = *SrcPtr;
			UMetaStoryState* TgtState = *TgtPtr;

			FMetaStoryTransition& MT = SrcState->Transitions.AddDefaulted_GetRef();
			MT.Trigger = EMetaStoryTransitionTrigger::OnStateCompleted;
			MT.State.Name = TgtState->Name;
			MT.State.ID = TgtState->ID;
			MT.State.LinkType = EMetaStoryTransitionType::GotoState;
			MT.ID = FGuid::NewGuid();

			for (const FMetaStoryFlowCondition& C : Tr.Conditions)
			{
				switch (C.Type)
				{
				case EMetaStoryFlowConditionType::RequiredNodeCompleted:
					Private::Report(Log, EMessageSeverity::Warning, FString::Printf(TEXT("RequiredNodeCompleted is not yet lowered to MetaStory conditions (transition %s -> %s)."), *Tr.SourceNodeId.ToString(), *Tr.TargetNodeId.ToString()));
					break;
				case EMetaStoryFlowConditionType::RandomProbability:
					Private::Report(Log, EMessageSeverity::Warning, FString::Printf(TEXT("RandomProbability is not yet lowered to MetaStory conditions (transition %s -> %s)."), *Tr.SourceNodeId.ToString(), *Tr.TargetNodeId.ToString()));
					break;
				case EMetaStoryFlowConditionType::CustomBehavior:
				default:
					Private::Report(Log, EMessageSeverity::Warning, FString::Printf(TEXT("CustomBehavior flow condition is not supported in the bridge (transition %s -> %s)."), *Tr.SourceNodeId.ToString(), *Tr.TargetNodeId.ToString()));
					break;
				}
			}
		}

		EditorData.SubTrees.Add(Root);
		EditorData.ReparentStates();
		return true;
	}
}
