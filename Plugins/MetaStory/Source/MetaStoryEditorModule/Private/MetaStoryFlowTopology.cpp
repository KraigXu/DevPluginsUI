// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryFlowTopology.h"

#include "Flow/MetaStoryFlow.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorModule.h"
#include "MetaStoryEditorNode.h"
#include "MetaStoryNodeBase.h"
#include "MetaStoryState.h"
#include "MetaStoryTasksStatus.h"
#include "MetaStoryTypes.h"
#include "Customizations/MetaStoryEditorNodeUtils.h"

#include "UObject/UObjectGlobals.h"

namespace UE::MetaStory::FlowTopology
{
	namespace Private
	{
		static const FName GMetaStoryFlowRootStateName(TEXT("MetaStoryFlowRoot"));

		/**
		 * Per-flow-node editor state that lives on shadow UMetaStoryState objects but is not rebuilt from UMetaStoryFlow alone.
		 * Must be snapshotted before SubTrees.Reset(); otherwise PostLoad / compile rebuild drops deserialized user edits (Color, Tag, tasks, conditions, …).
		 */
		struct FFlowShadowNodeEditorStash
		{
			TArray<FMetaStoryEditorNode> Tasks;
			TArray<FMetaStoryEditorNode> EnterConditions;
			TArray<FMetaStoryEditorNode> Considerations;
			FMetaStoryEditorNode SingleTask;

			FMetaStoryEditorColorRef ColorRef;
			FGameplayTag Tag;
			EMetaStoryTaskCompletionType TasksCompletion = EMetaStoryTaskCompletionType::Any;
			EMetaStoryStateSelectionBehavior SelectionBehavior = EMetaStoryStateSelectionBehavior::TryEnterState;
			bool bEnabled = true;
			float Weight = 1.f;
			bool bHasCustomTickRate = false;
			float CustomTickRate = 0.f;
			bool bCheckPrerequisitesWhenActivatingChildDirectly = true;
			bool bHasRequiredEventToEnter = false;
			FMetaStoryEventDesc RequiredEventToEnter;
			FMetaStoryStateParameters Parameters;
			FMetaStoryStateLink LinkedSubtree;
			TObjectPtr<UMetaStory> LinkedAsset = nullptr;
		};

		static void ReparentEditorNodesForOwner(TArray<FMetaStoryEditorNode>& Nodes, UMetaStoryState* Owner)
		{
			for (FMetaStoryEditorNode& Ed : Nodes)
			{
				if (Ed.InstanceObject)
				{
					Ed.InstanceObject = DuplicateObject(Ed.InstanceObject, Owner);
				}
				if (Ed.ExecutionRuntimeDataObject)
				{
					Ed.ExecutionRuntimeDataObject = DuplicateObject(Ed.ExecutionRuntimeDataObject, Owner);
				}
			}
		}

		static void RunPostLoadOnEditorNodes(UMetaStoryState* Owner, TArray<FMetaStoryEditorNode>& Nodes)
		{
			for (FMetaStoryEditorNode& Ed : Nodes)
			{
				if (FMetaStoryNodeBase* Node = Ed.Node.GetMutablePtr<FMetaStoryNodeBase>())
				{
					UE::MetaStoryEditor::EditorNodeUtils::ConditionalUpdateNodeInstanceData(Ed, *Owner);
					Node->PostLoad(Ed.GetInstance());
				}
			}
		}

		static void RestoreSingleEditorNode(UMetaStoryState* Owner, FMetaStoryEditorNode& Ed)
		{
			if (Ed.InstanceObject)
			{
				Ed.InstanceObject = DuplicateObject(Ed.InstanceObject, Owner);
			}
			if (Ed.ExecutionRuntimeDataObject)
			{
				Ed.ExecutionRuntimeDataObject = DuplicateObject(Ed.ExecutionRuntimeDataObject, Owner);
			}
			if (FMetaStoryNodeBase* Node = Ed.Node.GetMutablePtr<FMetaStoryNodeBase>())
			{
				UE::MetaStoryEditor::EditorNodeUtils::ConditionalUpdateNodeInstanceData(Ed, *Owner);
				Node->PostLoad(Ed.GetInstance());
			}
		}

		static void StashFlowShadowNodeEditorsBeforeSubtreeReset(
			UMetaStoryEditorData& EditorData,
			UMetaStoryFlow* Flow,
			TMap<FGuid, FFlowShadowNodeEditorStash>& OutStashByNodeId)
		{
			OutStashByNodeId.Reset();
			if (!Flow || EditorData.SubTrees.Num() != 1)
			{
				return;
			}

			UMetaStoryState* Root = EditorData.SubTrees[0].Get();
			if (!Root || Root->Name != GMetaStoryFlowRootStateName)
			{
				return;
			}

			auto CopyRows = [Flow](const TArray<FMetaStoryEditorNode>& Src) -> TArray<FMetaStoryEditorNode>
			{
				TArray<FMetaStoryEditorNode> Out;
				Out.Reserve(Src.Num());
				for (const FMetaStoryEditorNode& Row : Src)
				{
					FMetaStoryEditorNode Copy = Row;
					if (Copy.InstanceObject)
					{
						Copy.InstanceObject = DuplicateObject(Copy.InstanceObject, Flow);
					}
					if (Copy.ExecutionRuntimeDataObject)
					{
						Copy.ExecutionRuntimeDataObject = DuplicateObject(Copy.ExecutionRuntimeDataObject, Flow);
					}
					Out.Add(MoveTemp(Copy));
				}
				return Out;
			};

			auto CopySingleRow = [Flow](const FMetaStoryEditorNode& Src) -> FMetaStoryEditorNode
			{
				FMetaStoryEditorNode Copy = Src;
				if (Copy.InstanceObject)
				{
					Copy.InstanceObject = DuplicateObject(Copy.InstanceObject, Flow);
				}
				if (Copy.ExecutionRuntimeDataObject)
				{
					Copy.ExecutionRuntimeDataObject = DuplicateObject(Copy.ExecutionRuntimeDataObject, Flow);
				}
				return Copy;
			};

			for (UMetaStoryState* Child : Root->Children)
			{
				if (!Child || !Child->ID.IsValid())
				{
					continue;
				}

				FFlowShadowNodeEditorStash Stash;
				Stash.Tasks = CopyRows(Child->Tasks);
				Stash.EnterConditions = CopyRows(Child->EnterConditions);
				Stash.Considerations = CopyRows(Child->Considerations);
				Stash.SingleTask = CopySingleRow(Child->SingleTask);

				Stash.ColorRef = Child->ColorRef;
				Stash.Tag = Child->Tag;
				Stash.TasksCompletion = Child->TasksCompletion;
				Stash.SelectionBehavior = Child->SelectionBehavior;
				Stash.bEnabled = Child->bEnabled;
				Stash.Weight = Child->Weight;
				Stash.bHasCustomTickRate = Child->bHasCustomTickRate;
				Stash.CustomTickRate = Child->CustomTickRate;
				Stash.bCheckPrerequisitesWhenActivatingChildDirectly = Child->bCheckPrerequisitesWhenActivatingChildDirectly;
				Stash.bHasRequiredEventToEnter = Child->bHasRequiredEventToEnter;
				Stash.RequiredEventToEnter = Child->RequiredEventToEnter;
				Stash.Parameters = Child->Parameters;
				Stash.LinkedSubtree = Child->LinkedSubtree;
				Stash.LinkedAsset = Child->LinkedAsset;

				OutStashByNodeId.Add(Child->ID, MoveTemp(Stash));
			}
		}

		static void RestoreFlowShadowNodeFromStash(
			UMetaStoryState* S,
			UMetaStoryFlow* Flow,
			TMap<FGuid, FFlowShadowNodeEditorStash>& InOutStash,
			const FGuid& NodeId)
		{
			if (!S || !Flow || !NodeId.IsValid())
			{
				return;
			}

			FFlowShadowNodeEditorStash* Entry = InOutStash.Find(NodeId);
			if (!Entry)
			{
				return;
			}

			{
				TArray<FMetaStoryEditorNode> Nodes = MoveTemp(Entry->Tasks);
				ReparentEditorNodesForOwner(Nodes, S);
				S->Tasks = MoveTemp(Nodes);
				RunPostLoadOnEditorNodes(S, S->Tasks);
			}
			{
				TArray<FMetaStoryEditorNode> Nodes = MoveTemp(Entry->EnterConditions);
				ReparentEditorNodesForOwner(Nodes, S);
				S->EnterConditions = MoveTemp(Nodes);
				RunPostLoadOnEditorNodes(S, S->EnterConditions);
			}
			{
				TArray<FMetaStoryEditorNode> Nodes = MoveTemp(Entry->Considerations);
				ReparentEditorNodesForOwner(Nodes, S);
				S->Considerations = MoveTemp(Nodes);
				RunPostLoadOnEditorNodes(S, S->Considerations);
			}
			S->SingleTask = MoveTemp(Entry->SingleTask);
			RestoreSingleEditorNode(S, S->SingleTask);

			S->ColorRef = Entry->ColorRef;
			S->Tag = Entry->Tag;
			S->TasksCompletion = Entry->TasksCompletion;
			S->SelectionBehavior = Entry->SelectionBehavior;
			S->bEnabled = Entry->bEnabled;
			S->Weight = Entry->Weight;
			S->bHasCustomTickRate = Entry->bHasCustomTickRate;
			S->CustomTickRate = Entry->CustomTickRate;
			S->bCheckPrerequisitesWhenActivatingChildDirectly = Entry->bCheckPrerequisitesWhenActivatingChildDirectly;
			S->bHasRequiredEventToEnter = Entry->bHasRequiredEventToEnter;
			S->RequiredEventToEnter = Entry->RequiredEventToEnter;
			S->Parameters = Entry->Parameters;
			S->LinkedSubtree = Entry->LinkedSubtree;
			S->LinkedAsset = Entry->LinkedAsset;

			InOutStash.Remove(NodeId);
		}

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
		UMetaStoryFlow* Flow = EditorData.MetaStoryFlow;
		if (!Flow)
		{
			return true;
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
				// Isolated / draft nodes are normal while editing; avoid Warning spam on PostLoad and editor rebuilds.
				// Keep Warning in compile log when a compiler is active.
				const FString Msg = FString::Printf(
					TEXT("Flow node %s is not reachable (no incoming transitions)."),
					*N.NodeId.ToString());
				if (Log)
				{
					Private::Report(Log, EMessageSeverity::Warning, Msg);
				}
				else
				{
					UE_LOG(LogMetaStoryEditor, Verbose, TEXT("%s"), *Msg);
				}
			}
			else if (Inc > 1)
			{
				// Merge/join: multiple Flow transitions targeting this node each become an OnStateCompleted→Goto on their
				// respective source shadow states. Runtime semantics follow MetaStory transition evaluation order (not deferred).
				const FString Msg = FString::Printf(
					TEXT("Flow node %s has %d incoming transitions (merge); lowered as separate transitions from each predecessor."),
					*N.NodeId.ToString(),
					Inc);
				if (Log)
				{
					Private::Report(Log, EMessageSeverity::Warning, Msg);
				}
				else
				{
					UE_LOG(LogMetaStoryEditor, Verbose, TEXT("%s"), *Msg);
				}
			}
		}

		TMap<FGuid, Private::FFlowShadowNodeEditorStash> StashedShadowEditors;

		// Finish UObject loading on deserialized SubTrees before discard/rebuild. During asset PostLoad, children may
		// still have RF_NeedPostLoad; tearing down / allocating replacement objects with fixed names can otherwise hit
		// UObjectGlobals StaticReplaceObject asserts.
		EditorData.VisitHierarchy([](UMetaStoryState& State, UMetaStoryState* /*ParentState*/) mutable
		{
			State.ConditionalPostLoad();
			return EMetaStoryVisitor::Continue;
		});

		Private::StashFlowShadowNodeEditorsBeforeSubtreeReset(EditorData, Flow, StashedShadowEditors);

		EditorData.SubTrees.Reset();

		const FName RootUObjectName = MakeUniqueObjectName(&EditorData, UMetaStoryState::StaticClass(), TEXT("MetaStoryFlowRoot"));
		UMetaStoryState* Root = NewObject<UMetaStoryState>(&EditorData, RootUObjectName, RF_Transactional);
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
			S->TasksCompletion = EMetaStoryTaskCompletionType::Any;

			Private::RestoreFlowShadowNodeFromStash(S, Flow, StashedShadowEditors, PlotNode.NodeId);

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
