// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryTest.h"
#include "MetaStoryTestBase.h"
#include "MetaStoryTestTypes.h"

#include "MetaStoryCompilerLog.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryCompiler.h"
#include "Conditions/MetaStoryCommonConditions.h"

#define LOCTEXT_NAMESPACE "AITestSuite_MetaStoryTest"

UE_DISABLE_OPTIMIZATION_SHIP

namespace UE::MetaStory::Tests
{
struct FMetaStoryTest_StatePath_LinkStates : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//	Root
		//		RootState1 -> Next
		//			StateLinkedTree (Sub1) -> Next
		//			StateLinkedTree (Sub1) -> Next
		//		RootStateLinkedTree (Sub2) -> Next
		//		RootStateLinkedTree (Sub2) -> Next
		//		RootStateLinkedTree (Tree2) -> Next
		//		RootStateLinkedTree (Tree3) -> Next			# Tree3 fails
		//		RootStateLinkedTree (Tree2) -> Next
		//		RootState2 -> Root
		//	Sub1
		//		StateLinkedTree (Tree2) -> Next
		//		StateLinkedTree (Tree2) -> Next
		//		StateLinkedTree (Sub2) -> Next
		//		StateLinkedTree (Sub2) -> Next
		//		Sub1State1 -> Success
		//	Sub2
		//		Sub2State1 -> Next
		//		Sub2State2 -> Success
		//Tree 2
		//	Root
		//		RootStateLinkedTree (Sub1) -> Next
		//		RootStateLinkedTree (Sub1) -> Next
		//		RootState1 -> Success
		//	Sub1
		//		Sub1State1 -> Next
		//		Sub1State2 -> Success
		//Tree 3
		//	Root
		//		RootStateLinkedTree (Sub1) -> Success
		//	Sub1
		//		Sub1State1 -> cond fail -> success
		UMetaStory& MetaStory1 = NewMetaStory();
		UMetaStory& MetaStory2 = NewMetaStory();
		UMetaStory& MetaStory3 = NewMetaStory();
		// Tree1
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);
			UMetaStoryState& Root = EditorData.AddSubTree("Tree1Root");
			UMetaStoryState& Sub1 = EditorData.AddSubTree("Tree1Sub1");
			Sub1.Type = EMetaStoryStateType::Subtree;
			UMetaStoryState& Sub2 = EditorData.AddSubTree("Tree1Sub2");
			Sub2.Type = EMetaStoryStateType::Subtree;
			// Root
			{
				UMetaStoryState& Tree1RootState1 = Root.AddChildState("Tree1RootState1", EMetaStoryStateType::State);
				UMetaStoryState& Tree1RootStateSub2A = Root.AddChildState("Tree1RootStateSub2A", EMetaStoryStateType::Linked);
				//Tree1RootState1
				{
					{
						UMetaStoryState& ChildState = Tree1RootState1.AddChildState("Tree1State1StateSub1A", EMetaStoryStateType::Linked);
						ChildState.SetLinkedState(Sub1.GetLinkToState());
						ChildState.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
					}
					{
						UMetaStoryState& ChildState = Tree1RootState1.AddChildState("Tree1State1StateSub1B", EMetaStoryStateType::Linked);
						ChildState.SetLinkedState(Sub1.GetLinkToState());
						FMetaStoryTransition& Transition = ChildState.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::GotoState);
						Transition.State = Tree1RootStateSub2A.GetLinkToState();
					}
				}
				//Tree1RootStateSub2A
				{
					Tree1RootStateSub2A.SetLinkedState(Sub2.GetLinkToState());
					Tree1RootStateSub2A.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Root.AddChildState("Tree1RootStateSub2B", EMetaStoryStateType::Linked);
					State.SetLinkedState(Sub2.GetLinkToState());
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Root.AddChildState("Tree1RootStateLinkTree2A", EMetaStoryStateType::LinkedAsset);
					State.SetLinkedStateAsset(&MetaStory2);
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Root.AddChildState("Tree1RootStateLinkTree3", EMetaStoryStateType::LinkedAsset);
					State.SetLinkedStateAsset(&MetaStory3);
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Root.AddChildState("Tree1RootStateLinkTree2B", EMetaStoryStateType::LinkedAsset);
					State.SetLinkedStateAsset(&MetaStory2);
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Root.AddChildState("Tree1RootState1", EMetaStoryStateType::State);
					FMetaStoryTransition& Transition = State.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState);
					Transition.State = Root.GetLinkToState();
					Transition.bDelayTransition = true;
					Transition.DelayDuration = 0.999f;
				}
			}
			//Tree1Sub1
			{
				{
					UMetaStoryState& State = Sub1.AddChildState("Tree1Sub1StateLinkTree2A", EMetaStoryStateType::LinkedAsset);
					State.SetLinkedStateAsset(&MetaStory2);
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Sub1.AddChildState("Tree1Sub1StateLinkTree2B", EMetaStoryStateType::LinkedAsset);
					State.SetLinkedStateAsset(&MetaStory2);
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Sub1.AddChildState("Tree1Sub1StateSub2A", EMetaStoryStateType::Linked);
					State.SetLinkedState(Sub2.GetLinkToState());
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Sub1.AddChildState("Tree1Sub1StateSub2B", EMetaStoryStateType::Linked);
					State.SetLinkedState(Sub2.GetLinkToState());
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Sub1.AddChildState("TreeASub1State1", EMetaStoryStateType::State);
					FMetaStoryTransition& Transition = State.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::Succeeded);
					Transition.bDelayTransition = true;
					Transition.DelayDuration = 0.999f;
				}
			}
			//Tree1Sub2
			{
				{
					UMetaStoryState& State = Sub2.AddChildState("Tree1Sub2State1", EMetaStoryStateType::State);
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Sub2.AddChildState("Tree1Sub2State2", EMetaStoryStateType::State);
					FMetaStoryTransition& Transition = State.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::Succeeded);
					Transition.bDelayTransition = true;
					Transition.DelayDuration = 0.999f;
				}
			}
		}
		//Tree 2
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
			UMetaStoryState& Root = EditorData.AddSubTree("Tree2StateRoot");
			UMetaStoryState& Sub1 = EditorData.AddSubTree("Tree2Sub1");
			Sub1.Type = EMetaStoryStateType::Subtree;
			//Root
			{
				{
					UMetaStoryState& State = Root.AddChildState("Tree2RootStateSub1A", EMetaStoryStateType::Linked);
					State.SetLinkedState(Sub1.GetLinkToState());
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Root.AddChildState("Tree2RootStateSub1B", EMetaStoryStateType::Linked);
					State.SetLinkedState(Sub1.GetLinkToState());
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Root.AddChildState("Tree2RootState1", EMetaStoryStateType::State);
					FMetaStoryTransition& Transition = State.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::Succeeded);
					Transition.bDelayTransition = true;
					Transition.DelayDuration = 0.999f;
				}
			}
			//Tree2Sub1
			{
				{
					UMetaStoryState& State = Sub1.AddChildState("Tree2Sub1State1", EMetaStoryStateType::State);
					State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				}
				{
					UMetaStoryState& State = Sub1.AddChildState("Tree2Sub1State2", EMetaStoryStateType::State);
					FMetaStoryTransition& Transition = State.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::Succeeded);
					Transition.bDelayTransition = true;
					Transition.DelayDuration = 0.999f;
				}
			}
		}
		//Tree 3
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory3.EditorData);
			UMetaStoryState& Root = EditorData.AddSubTree("Tree3StateRoot");
			UMetaStoryState& Sub1 = EditorData.AddSubTree("Tree3Sub1");
			Sub1.Type = EMetaStoryStateType::Subtree;
			//Root
			{
				UMetaStoryState& State = Root.AddChildState("Tree3RootStateSub1A", EMetaStoryStateType::Linked);
				State.SetLinkedState(Sub1.GetLinkToState());
				State.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::Succeeded);
			}
			//Tree3Sub1
			{
				UMetaStoryState& State = Sub1.AddChildState("Tree3Sub1State1", EMetaStoryStateType::State);
				FMetaStoryTransition& Transition = State.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::Succeeded);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 0.999f;

				TMetaStoryTypedEditorNode<FMetaStoryRandomCondition>& Cond = State.AddEnterCondition<FMetaStoryRandomCondition>();
				Cond.GetNode().EvaluationMode = EMetaStoryConditionEvaluationMode::ForcedFalse;
			}
		}

		// Compile tree
		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory3);
			AITEST_TRUE(TEXT("MetaStory3 should get compiled"), bResult);
		}
		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory2);
			AITEST_TRUE(TEXT("MetaStory2 should get compiled"), bResult);
		}
		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory1);
			AITEST_TRUE(TEXT("MetaStory1 should get compiled"), bResult);
		}

		// Create context
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
		{
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);
		}
		{
			UE::MetaStory::FActiveStatePath ExecPath = InstanceData.GetExecutionState()->GetActiveStatePath();
			AITEST_TRUE(TEXT("ExecPath should be empty."), ExecPath.Num() == 0);
		}

		// Test variable and helper functions
		int32 ActiveCounter = 0;
		TArray<UE::MetaStory::FActiveFrameID> PreviousFrameIDs;
		auto PreTestFrameId = [&PreviousFrameIDs, &InstanceData]()
			{
				PreviousFrameIDs.Reset();
				for (int32 Index = 0; Index < InstanceData.GetExecutionState()->ActiveFrames.Num(); ++Index)
				{
					PreviousFrameIDs.Add(InstanceData.GetExecutionState()->ActiveFrames[Index].FrameID);
				}
			};
		auto TestFrameId = [&PreviousFrameIDs, &InstanceData](int32 CorrectAmount)
			{
				int32 Index = 0;
				for (; Index < PreviousFrameIDs.Num() && Index < CorrectAmount; ++Index)
				{
					if (PreviousFrameIDs[Index] != InstanceData.GetExecutionState()->ActiveFrames[Index].FrameID)
					{
						return false;
					}
				}
				for (; Index < InstanceData.GetExecutionState()->ActiveFrames.Num() && Index < PreviousFrameIDs.Num(); ++Index)
				{
					if (PreviousFrameIDs[Index] == InstanceData.GetExecutionState()->ActiveFrames[Index].FrameID)
					{
						return false;
					}
				}
				return true;
			};

		// Start tests
		{
			using namespace UE::MetaStory;

			const EMetaStoryRunStatus Status = Exec.Start();
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateLinkTree2A", "Tree2StateRoot", "Tree2RootStateSub1A", "Tree2Sub1", "Tree2Sub1State1"));

			const FActiveStatePath ExecPath = InstanceData.GetExecutionState()->GetActiveStatePath();
			AITEST_TRUE(TEXT("Has the correct number of path elements"), ExecPath.Num() == 9);
			AITEST_TRUE(TEXT("Has the correct number of active states"), InstanceData.GetExecutionState()->ActiveFrames.Num() == 4);
			{
				const FActiveFrameID FirstFrameID = InstanceData.GetExecutionState()->ActiveFrames[0].FrameID;
				AITEST_TRUE(TEXT("Frame for Tree1Root is active"), FirstFrameID == FActiveFrameID(++ActiveCounter));
				AITEST_TRUE(TEXT("MetaStory1Root is active"), ExecPath.Contains(FActiveStateID(++ActiveCounter)));
				AITEST_TRUE(TEXT("MetaStory1Root is active"), ExecPath.Contains(FActiveState(FirstFrameID, FActiveStateID(ActiveCounter), FMetaStoryStateHandle(0))));
				AITEST_TRUE(TEXT("MetaStory1RootState1 is active"), ExecPath.Contains(FActiveStateID(++ActiveCounter)));
				AITEST_TRUE(TEXT("MetaStory1RootState1 is active"), ExecPath.Contains(FActiveState(FirstFrameID, FActiveStateID(ActiveCounter), FMetaStoryStateHandle(1))));
				AITEST_TRUE(TEXT("MetaStory1State1StateSub1A is active"), ExecPath.Contains(FActiveStateID(++ActiveCounter)));
				AITEST_TRUE(TEXT("MetaStory1State1StateSub1A is active"), ExecPath.Contains(FActiveState(FirstFrameID, FActiveStateID(4), FMetaStoryStateHandle(2))));
			}
			{
				AITEST_TRUE(TEXT("Frame for Tree1Sub1 is active"), InstanceData.GetExecutionState()->ActiveFrames[1].FrameID == FActiveFrameID(++ActiveCounter));
				AITEST_TRUE(TEXT("MetaStory1Sub1 is active"), ExecPath.Contains(FActiveStateID(++ActiveCounter)));
				AITEST_TRUE(TEXT("MetaStory1Sub1StateLinkTree2A is active"), ExecPath.Contains(FActiveStateID(++ActiveCounter)));
			}
			{
				AITEST_TRUE(TEXT("Frame for Tree2StateRoot is active"), InstanceData.GetExecutionState()->ActiveFrames[2].FrameID == FActiveFrameID(++ActiveCounter));
				AITEST_TRUE(TEXT("MetaStory2StateRoot is active"), ExecPath.Contains(FActiveStateID(++ActiveCounter)));
				AITEST_TRUE(TEXT("MetaStory2RootStateSub1A is active"), ExecPath.Contains(FActiveStateID(++ActiveCounter)));
			}
			{
				AITEST_TRUE(TEXT("Frame for Tree2Sub1 is active"), InstanceData.GetExecutionState()->ActiveFrames[3].FrameID == FActiveFrameID(++ActiveCounter));
				AITEST_TRUE(TEXT("MetaStory2Sub1 is active"), ExecPath.Contains(FActiveStateID(++ActiveCounter)));
				AITEST_TRUE(TEXT("MetaStory2Sub1State1 is active"), ExecPath.Contains(FActiveStateID(++ActiveCounter)));
			}
			{
				AITEST_FALSE(TEXT("No accidental increment"), ExecPath.Contains(FActiveStateID(ActiveCounter+1)));
			}
			Exec.LogClear();
		}
		{
			using namespace UE::MetaStory;

			PreTestFrameId();
			EMetaStoryRunStatus Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateLinkTree2A", "Tree2StateRoot", "Tree2RootStateSub1A", "Tree2Sub1", "Tree2Sub1State2"));

			FActiveStatePath ExecPath = InstanceData.GetExecutionState()->GetActiveStatePath();
			AITEST_TRUE(TEXT("Has the correct number of path elements"), ExecPath.Num() == 9);
			AITEST_TRUE(TEXT("Has the correct number of active states"), InstanceData.GetExecutionState()->ActiveFrames.Num() == 4);
			AITEST_TRUE(TEXT("MetaStory2Sub1State2 is active"), ExecPath.Contains(FActiveStateID(++ActiveCounter)));
			AITEST_FALSE(TEXT("No accidental increment"), ExecPath.Contains(FActiveStateID(ActiveCounter + 1)));
			AITEST_TRUE(TEXT("All frames are the same"), TestFrameId(4));
			Exec.LogClear();
		}
		{
			PreTestFrameId();
			EMetaStoryRunStatus Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateLinkTree2A", "Tree2StateRoot", "Tree2RootStateSub1B", "Tree2Sub1", "Tree2Sub1State1"));

			const UE::MetaStory::FActiveStatePath ExecPath = InstanceData.GetExecutionState()->GetActiveStatePath();
			AITEST_TRUE(TEXT("Has the correct number of path elements"), ExecPath.Num() == 9);
			AITEST_TRUE(TEXT("Has the correct number of active states"), InstanceData.GetExecutionState()->ActiveFrames.Num() == 4);
			AITEST_TRUE(TEXT("The last frame changed"), TestFrameId(3));
			Exec.LogClear();
		}
		{
			PreTestFrameId();
			EMetaStoryRunStatus Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateLinkTree2A", "Tree2StateRoot", "Tree2RootStateSub1B", "Tree2Sub1", "Tree2Sub1State2"));

			UE::MetaStory::FActiveStatePath ExecPath = InstanceData.GetExecutionState()->GetActiveStatePath();
			AITEST_TRUE(TEXT("Has the correct number of path elements"), ExecPath.Num() == 9);
			AITEST_TRUE(TEXT("Has the correct number of active states"), InstanceData.GetExecutionState()->ActiveFrames.Num() == 4);
			AITEST_TRUE(TEXT("All frames are the same"), TestFrameId(4));
			Exec.LogClear();
		}
		{
			PreTestFrameId();
			EMetaStoryRunStatus Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateLinkTree2A", "Tree2StateRoot", "Tree2RootState1"));

			UE::MetaStory::FActiveStatePath ExecPath = InstanceData.GetExecutionState()->GetActiveStatePath();
			AITEST_TRUE(TEXT("Has the correct number of path elements"), ExecPath.Num() == 7);
			AITEST_TRUE(TEXT("Has the correct number of active states"), InstanceData.GetExecutionState()->ActiveFrames.Num() == 3);
			AITEST_TRUE(TEXT("All frames are the same"), TestFrameId(3));
			Exec.LogClear();
		}
		{
			PreTestFrameId();
			EMetaStoryRunStatus Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateLinkTree2B", "Tree2StateRoot", "Tree2RootStateSub1A", "Tree2Sub1", "Tree2Sub1State1"));

			UE::MetaStory::FActiveStatePath ExecPath = InstanceData.GetExecutionState()->GetActiveStatePath();
			AITEST_TRUE(TEXT("Has the correct number of path elements"), ExecPath.Num() == 9);
			AITEST_TRUE(TEXT("Has the correct number of active states"), InstanceData.GetExecutionState()->ActiveFrames.Num() == 4);
			AITEST_TRUE(TEXT("All frames are the same"), TestFrameId(2));
			Exec.LogClear();
		}
		{
			PreTestFrameId();
			EMetaStoryRunStatus Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateLinkTree2B", "Tree2StateRoot", "Tree2RootStateSub1A", "Tree2Sub1", "Tree2Sub1State2"));
			AITEST_TRUE(TEXT("All frames are the same"), TestFrameId(4));
			Exec.LogClear();

			PreTestFrameId();
			Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateLinkTree2B", "Tree2StateRoot", "Tree2RootStateSub1B", "Tree2Sub1", "Tree2Sub1State1"));
			AITEST_TRUE(TEXT("All frames are the same"), TestFrameId(3));
			Exec.LogClear();

			PreTestFrameId();
			Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateLinkTree2B", "Tree2StateRoot", "Tree2RootStateSub1B", "Tree2Sub1", "Tree2Sub1State2"));
			AITEST_TRUE(TEXT("Has the correct number of active states"), InstanceData.GetExecutionState()->ActiveFrames.Num() == 4);
			AITEST_TRUE(TEXT("All frames are the same"), TestFrameId(4));
			Exec.LogClear();

			PreTestFrameId();
			Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateLinkTree2B", "Tree2StateRoot", "Tree2RootState1"));
			AITEST_TRUE(TEXT("Has the correct number of active states"), InstanceData.GetExecutionState()->ActiveFrames.Num() == 3);
			AITEST_TRUE(TEXT("All frames are the same"), TestFrameId(3));
			Exec.LogClear();
		}
		{
			PreTestFrameId();
			EMetaStoryRunStatus Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateSub2A", "Tree1Sub2", "Tree1Sub2State1"));
			AITEST_TRUE(TEXT("Has the correct number of active states"), InstanceData.GetExecutionState()->ActiveFrames.Num() == 3);
			AITEST_TRUE(TEXT("All frames are the same"), TestFrameId(2));
			Exec.LogClear();

			PreTestFrameId();
			Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Should be in the correct state"), Exec.ExpectInActiveStates("Tree1Root", "Tree1RootState1", "Tree1State1StateSub1A", "Tree1Sub1", "Tree1Sub1StateSub2A", "Tree1Sub2", "Tree1Sub2State2"));
			AITEST_TRUE(TEXT("Has the correct number of active states"), InstanceData.GetExecutionState()->ActiveFrames.Num() == 3);
			AITEST_TRUE(TEXT("All frames are the same"), TestFrameId(3));
			Exec.LogClear();
		}

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_StatePath_LinkStates, "System.MetaStory.StatePath.LinkStates");

} // namespace UE::MetaStory::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
