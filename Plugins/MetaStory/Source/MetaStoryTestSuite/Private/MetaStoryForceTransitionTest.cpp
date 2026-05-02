// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryTest.h"
#include "MetaStoryTestBase.h"
#include "MetaStoryTestTypes.h"

#include "MetaStoryCompilerLog.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryCompiler.h"
#include "Conditions/MetaStoryCommonConditions.h"

#define LOCTEXT_NAMESPACE "AITestSuite_MetaStoryTask"

UE_DISABLE_OPTIMIZATION_SHIP

namespace UE::MetaStory::Tests
{

struct FMetaStoryTest_ForceTransition_All : FMetaStoryTestBase
{
	int32 AddStateTaskIndex = 0;
	int32 AddMetaStoryIndex = 0;

	int32 TransitionIfTaskIndex = INDEX_NONE;
	int32 TransitionIfTreeIndex = INDEX_NONE;
	FMetaStoryStateHandle TransitionTo;

	UMetaStoryState& AddSubTree(UMetaStoryEditorData& EditorData, FName StateName, EMetaStoryStateType StateType)
	{
		UMetaStoryState& State = EditorData.AddSubTree(StateName);
		State.Type = StateType;
		State.SelectionBehavior = EMetaStoryStateSelectionBehavior::TryEnterState;
		TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = State.AddTask<FTestTask_PrintValue>(StateName);
		Task.GetInstanceData().Value = AddStateTaskIndex + (AddMetaStoryIndex*100);
		++AddStateTaskIndex;
		Task.GetNode().CustomTickFunc = [this](FMetaStoryExecutionContext& Context, const FTestTask_PrintValue* Task)
			{
				FTestTask_PrintValue::FInstanceDataType& InstanceData = Context.GetInstanceData(*Task);
				if ((InstanceData.Value % 100) == TransitionIfTaskIndex
					&& (InstanceData.Value / 100) == TransitionIfTreeIndex)
				{
					Context.RequestTransition(TransitionTo);
				}
			};
		return State;
	}

	UMetaStoryState& AddChildState(UMetaStoryState& ParentState, FName StateName, EMetaStoryStateType StateType)
	{
		UMetaStoryState& State = ParentState.AddChildState(StateName, StateType);
		State.SelectionBehavior = EMetaStoryStateSelectionBehavior::TryEnterState;
		TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = State.AddTask<FTestTask_PrintValue>(StateName);
		Task.GetInstanceData().Value = AddStateTaskIndex++;
		Task.GetNode().CustomTickFunc = [this](FMetaStoryExecutionContext& Context, const FTestTask_PrintValue* Task)
			{
				FTestTask_PrintValue::FInstanceDataType& InstanceData = Context.GetInstanceData(*Task);
				if ((InstanceData.Value % 100) == TransitionIfTaskIndex
					&& (InstanceData.Value / 100) == TransitionIfTreeIndex)
				{
					Context.RequestTransition(TransitionTo);
				}
			};
		return State;
	}

	UMetaStory* BuildTree1(TNotNull<UMetaStory*> Tree2)
	{
		//Tree1
		// StateA
		//  StateB
		//   StateC
		//  StateD
		//   StateLinkedE -> X
		//  StateF
		//   StateLinkedAssetG -> Tree2
		//  StateLinkedH -> X
		//  StateLinkedAssetI -> Tree2
		// StateQ (new root)
		//  StateR
		//   StateS
		//  StateLinkedT -> X
		// StateX
		//  StateY
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		AddStateTaskIndex = 0;
		AddMetaStoryIndex = 0;
		UMetaStoryState& StateA = AddSubTree(EditorData, "Tree1StateA", EMetaStoryStateType::State);
		UMetaStoryState& StateB = AddChildState(StateA, "Tree1StateB", EMetaStoryStateType::State);
		UMetaStoryState& StateC = AddChildState(StateB, "Tree1StateC", EMetaStoryStateType::State);
		UMetaStoryState& StateD = AddChildState(StateA, "Tree1StateD", EMetaStoryStateType::State);
		UMetaStoryState& StateE = AddChildState(StateD, "Tree1StateE", EMetaStoryStateType::Linked);
		UMetaStoryState& StateF = AddChildState(StateA, "Tree1StateF", EMetaStoryStateType::State);
		UMetaStoryState& StateG = AddChildState(StateF, "Tree1StateG", EMetaStoryStateType::LinkedAsset);
		UMetaStoryState& StateH = AddChildState(StateA, "Tree1StateH", EMetaStoryStateType::Linked);
		UMetaStoryState& StateI = AddChildState(StateA, "Tree1StateI", EMetaStoryStateType::LinkedAsset);
		UMetaStoryState& StateQ = AddSubTree(EditorData, "Tree1StateQ", EMetaStoryStateType::State);
		UMetaStoryState& StateR = AddChildState(StateQ, "Tree1StateR", EMetaStoryStateType::State);
		UMetaStoryState& StateS = AddChildState(StateR, "Tree1StateS", EMetaStoryStateType::State);
		UMetaStoryState& StateT = AddChildState(StateQ, "Tree1StateT", EMetaStoryStateType::Linked);
		UMetaStoryState& StateX = AddSubTree(EditorData, "Tree1StateX", EMetaStoryStateType::Subtree);
		UMetaStoryState& StateY = AddChildState(StateX, "Tree1StateY", EMetaStoryStateType::State);

		StateE.SetLinkedState(StateX.GetLinkToState());
		StateG.SetLinkedStateAsset(Tree2);
		StateH.SetLinkedState(StateX.GetLinkToState());
		StateI.SetLinkedStateAsset(Tree2);
		StateT.SetLinkedState(StateX.GetLinkToState());

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		return Compiler.Compile(MetaStory) ? &MetaStory : nullptr;
	}

	UMetaStory* BuildTree2(TNotNull<UMetaStory*> Tree3)
	{
		//Tree2
		// StateA
		//  StateB
		//   StateC
		//   StateLinkedD -> X
		//   StateLinkedAssetE -> Tree3
		// StateQ (new root)
		//  StateR
		//   StateS
		//  StateLinkedT -> X
		// StateX
		//  StateY
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		AddStateTaskIndex = 0;
		AddMetaStoryIndex = 1;
		UMetaStoryState& StateA = AddSubTree(EditorData, "Tree2StateA", EMetaStoryStateType::State);
		UMetaStoryState& StateB = AddChildState(StateA, "Tree2StateB", EMetaStoryStateType::State);
		UMetaStoryState& StateC = AddChildState(StateB, "Tree2StateC", EMetaStoryStateType::State);
		UMetaStoryState& StateD = AddChildState(StateB, "Tree2StateD", EMetaStoryStateType::Linked);
		UMetaStoryState& StateE = AddChildState(StateB, "Tree2StateE", EMetaStoryStateType::LinkedAsset);
		UMetaStoryState& StateQ = AddSubTree(EditorData, "Tree2StateQ", EMetaStoryStateType::State);
		UMetaStoryState& StateR = AddChildState(StateQ, "Tree2StateR", EMetaStoryStateType::State);
		UMetaStoryState& StateS = AddChildState(StateR, "Tree2StateS", EMetaStoryStateType::State);
		UMetaStoryState& StateT = AddChildState(StateQ, "Tree2StateT", EMetaStoryStateType::Linked);
		UMetaStoryState& StateX = AddSubTree(EditorData, "Tree2StateX", EMetaStoryStateType::Subtree);
		UMetaStoryState& StateY = AddChildState(StateX, "Tree2StateY", EMetaStoryStateType::State);

		StateD.SetLinkedState(StateX.GetLinkToState());
		StateE.SetLinkedStateAsset(Tree3);
		StateT.SetLinkedState(StateX.GetLinkToState());

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		return Compiler.Compile(MetaStory) ? &MetaStory : nullptr;

	}
	UMetaStory* BuildTree3()
	{
		//Tree3
		// StateA
		//  StateB
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		AddStateTaskIndex = 0;
		AddMetaStoryIndex = 2;
		UMetaStoryState& StateA = AddSubTree(EditorData, "Tree3StateA", EMetaStoryStateType::Subtree);
		UMetaStoryState& StateB = AddChildState(StateA, "Tree3StateB", EMetaStoryStateType::State);

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		return Compiler.Compile(MetaStory) ? &MetaStory : nullptr;
	}

	virtual bool InstantTest() override
	{
		UMetaStory* MetaStory3 = BuildTree3();
		AITEST_TRUE(TEXT("MetaStory3 should get compiled"), MetaStory3 != nullptr);

		UMetaStory* MetaStory2 = BuildTree2(MetaStory3);
		AITEST_TRUE(TEXT("MetaStory2 should get compiled"), MetaStory2 != nullptr);

		UMetaStory* MetaStory1 = BuildTree1(MetaStory2);
		AITEST_TRUE(TEXT("MetaStory2 should get compiled"), MetaStory1 != nullptr);

		// Suppress code analyzer warning C6011
		CA_ASSUME(MetaStory1);
		CA_ASSUME(MetaStory2);
		CA_ASSUME(MetaStory3);

		FMetaStoryInstanceData InstanceData;
		FMetaStoryInstanceData OldForceInstanceData;
		{
			FTestMetaStoryExecutionContext Exec(*MetaStory1, *MetaStory1, InstanceData); //-C6011
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);
		}

		constexpr int32 Tree1StateAIndex = 0;
		constexpr int32 Tree1StateQIndex = 9;
		constexpr int32 Tree1StateXIndex = 13;
		AITEST_TRUE(TEXT("Invalid Tree1StateA index."), MetaStory1->GetStates().IsValidIndex(Tree1StateAIndex) && MetaStory1->GetStates()[Tree1StateAIndex].Name == "Tree1StateA");
		AITEST_TRUE(TEXT("Invalid Tree1StateQ index."), MetaStory1->GetStates().IsValidIndex(Tree1StateQIndex) && MetaStory1->GetStates()[Tree1StateQIndex].Name == "Tree1StateQ");
		AITEST_TRUE(TEXT("Invalid Tree1StateX index."), MetaStory1->GetStates().IsValidIndex(Tree1StateXIndex) && MetaStory1->GetStates()[Tree1StateXIndex].Name == "Tree1StateX");
		constexpr int32 Tree2StateAIndex = 0;
		constexpr int32 Tree2StateQIndex = 5;
		constexpr int32 Tree2StateXIndex = 9;
		AITEST_TRUE(TEXT("Invalid Tree2StateA index."), MetaStory2->GetStates().IsValidIndex(Tree2StateAIndex) && MetaStory2->GetStates()[Tree2StateAIndex].Name == "Tree2StateA");
		AITEST_TRUE(TEXT("Invalid Tree2StateQ index."), MetaStory2->GetStates().IsValidIndex(Tree2StateQIndex) && MetaStory2->GetStates()[Tree2StateQIndex].Name == "Tree2StateQ");
		AITEST_TRUE(TEXT("Invalid Tree2StateX index."), MetaStory2->GetStates().IsValidIndex(Tree2StateXIndex) && MetaStory2->GetStates()[Tree2StateXIndex].Name == "Tree2StateX");
		constexpr int32 Tree3StateAIndex = 0;
		AITEST_TRUE(TEXT("Invalid Tree3StateA index."), MetaStory3->GetStates().IsValidIndex(Tree3StateAIndex) && MetaStory3->GetStates()[Tree3StateAIndex].Name == "Tree3StateA");

		struct FTransition
		{
			FMetaStoryStateHandle TargetState;
			int32 ActiveStateSourceIndex;
			int32 ActiveTreeSourceIndex;
			bool bTestNextTree;
			TArray<FName> ExpectedActiveStateNames;
		};

		const FTransition Tree1TransitionsToTest[] = {
			FTransition{FMetaStoryStateHandle(Tree1StateAIndex + 0), Tree1StateAIndex, 0, false, TArray<FName>{"Tree1StateA"}},
			FTransition{FMetaStoryStateHandle(Tree1StateAIndex + 1), Tree1StateAIndex, 0, false, TArray<FName>{"Tree1StateA", "Tree1StateB"}},
			FTransition{FMetaStoryStateHandle(Tree1StateAIndex + 2), Tree1StateAIndex, 0, false, TArray<FName>{"Tree1StateA", "Tree1StateB", "Tree1StateC"}},
			FTransition{FMetaStoryStateHandle(Tree1StateAIndex + 3), Tree1StateAIndex, 0, false, TArray<FName>{"Tree1StateA", "Tree1StateD"}},
			FTransition{FMetaStoryStateHandle(Tree1StateAIndex + 4), Tree1StateAIndex, 0, false, TArray<FName>{"Tree1StateA", "Tree1StateD", "Tree1StateE", "Tree1StateX"}},
			FTransition{FMetaStoryStateHandle(Tree1StateXIndex + 1), Tree1StateXIndex, 0, false, TArray<FName>{"Tree1StateA", "Tree1StateD", "Tree1StateE", "Tree1StateX", "Tree1StateY"}}, //5
			FTransition{FMetaStoryStateHandle(Tree1StateAIndex + 5), Tree1StateAIndex, 0, false, TArray<FName>{"Tree1StateA", "Tree1StateF"}},
			FTransition{FMetaStoryStateHandle(Tree1StateAIndex + 6), Tree1StateAIndex, 0, true,  TArray<FName>{"Tree1StateA", "Tree1StateF", "Tree1StateG", "Tree2StateA"}},
			FTransition{FMetaStoryStateHandle(Tree1StateAIndex + 7), Tree1StateAIndex, 0, false, TArray<FName>{"Tree1StateA", "Tree1StateH", "Tree1StateX"}},
			FTransition{FMetaStoryStateHandle(Tree1StateXIndex + 1), Tree1StateXIndex, 0, false, TArray<FName>{"Tree1StateA", "Tree1StateH", "Tree1StateX", "Tree1StateY"}},
			FTransition{FMetaStoryStateHandle(Tree1StateAIndex + 8), Tree1StateAIndex, 0, true,  TArray<FName>{"Tree1StateA", "Tree1StateI", "Tree2StateA"}}, //10
			FTransition{FMetaStoryStateHandle(Tree1StateQIndex + 0), Tree1StateAIndex, 0, false, TArray<FName>{"Tree1StateQ"}},
			FTransition{FMetaStoryStateHandle(Tree1StateQIndex + 1), Tree1StateQIndex, 0, false, TArray<FName>{"Tree1StateQ", "Tree1StateR"}},
			FTransition{FMetaStoryStateHandle(Tree1StateQIndex + 2), Tree1StateQIndex, 0, false, TArray<FName>{"Tree1StateQ", "Tree1StateR", "Tree1StateS"}},
			FTransition{FMetaStoryStateHandle(Tree1StateQIndex + 3), Tree1StateQIndex, 0, false, TArray<FName>{"Tree1StateQ", "Tree1StateT", "Tree1StateX"}},
			FTransition{FMetaStoryStateHandle(Tree1StateXIndex + 1), Tree1StateXIndex, 0, false, TArray<FName>{"Tree1StateQ", "Tree1StateT", "Tree1StateX", "Tree1StateY"}}, //15
		};

		const FTransition Tree2TransitionsToTest[] = {
			FTransition{FMetaStoryStateHandle(Tree2StateAIndex + 0), Tree2StateAIndex, 1, false, TArray<FName>{"Tree2StateA"}},
			FTransition{FMetaStoryStateHandle(Tree2StateAIndex + 1), Tree2StateAIndex, 1, false, TArray<FName>{"Tree2StateA", "Tree2StateB"}},
			FTransition{FMetaStoryStateHandle(Tree2StateAIndex + 2), Tree2StateAIndex, 1, false, TArray<FName>{"Tree2StateA", "Tree2StateB", "Tree2StateC"}},
			FTransition{FMetaStoryStateHandle(Tree2StateAIndex + 3), Tree2StateAIndex, 1, false, TArray<FName>{"Tree2StateA", "Tree2StateB", "Tree2StateD", "Tree2StateX"}},
			FTransition{FMetaStoryStateHandle(Tree2StateXIndex + 1), Tree2StateXIndex, 1, false, TArray<FName>{"Tree2StateA", "Tree2StateB", "Tree2StateD", "Tree2StateX", "Tree2StateY"}},
			FTransition{FMetaStoryStateHandle(Tree2StateAIndex + 4), Tree2StateAIndex, 1, true,  TArray<FName>{"Tree2StateA", "Tree2StateB", "Tree2StateE", "Tree3StateA"}}, // 5
			FTransition{FMetaStoryStateHandle(Tree2StateQIndex + 0), Tree2StateAIndex, 1, false, TArray<FName>{"Tree2StateQ"}},
			FTransition{FMetaStoryStateHandle(Tree2StateQIndex + 1), Tree2StateQIndex, 1, false, TArray<FName>{"Tree2StateQ", "Tree2StateR"}},
			FTransition{FMetaStoryStateHandle(Tree2StateQIndex + 2), Tree2StateQIndex, 1, false, TArray<FName>{"Tree2StateQ", "Tree2StateR", "Tree2StateS"}},
			FTransition{FMetaStoryStateHandle(Tree2StateQIndex + 3), Tree2StateQIndex, 1, false, TArray<FName>{"Tree2StateQ", "Tree2StateT", "Tree2StateX"}},
			FTransition{FMetaStoryStateHandle(Tree2StateXIndex + 1), Tree2StateXIndex, 1, false, TArray<FName>{"Tree2StateQ", "Tree2StateT", "Tree2StateX", "Tree2StateY"}}, //10
		};

		const FTransition Tree3TransitionsToTest[] = {
			FTransition{FMetaStoryStateHandle(Tree3StateAIndex + 0), Tree3StateAIndex, 2, false, TArray<FName>{"Tree3StateA"}},
			FTransition{FMetaStoryStateHandle(Tree3StateAIndex + 1), Tree3StateAIndex, 2, false, TArray<FName>{"Tree3StateA", "Tree3StateB"}}
		};

		const TArrayView<const FTransition> AllTranstionsToTest[] = { MakeConstArrayView(Tree1TransitionsToTest), MakeConstArrayView(Tree2TransitionsToTest), MakeConstArrayView(Tree3TransitionsToTest) };
		const int32 AllRootStates[] = { Tree1StateAIndex, Tree2StateAIndex, Tree3StateAIndex };

		auto TestAllTransitions = [&](const auto& RecursiveLambda, int32 TreeIndex, const TArray<FName>& PreviousTreeActiveStateName, bool bUseRoot)
			{
				const auto& TreeTransitionsToTest = AllTranstionsToTest[TreeIndex];
				for (int32 TransitionIndex = 0; TransitionIndex < TreeTransitionsToTest.Num(); ++TransitionIndex)
				{
					TArray<FName> ActiveStateNames;
					TArray<FMetaStoryRecordedTransitionResult> RecordedTransitions;
					TArray<UE::MetaStory::ExecutionContext::FStateHandleContext> ActiveStateHandles;
					{
						FTestMetaStoryExecutionContext Exec(*MetaStory1, *MetaStory1, InstanceData, {}, EMetaStoryRecordTransitions::Yes);

						if (bUseRoot && TreeTransitionsToTest[TransitionIndex].ActiveStateSourceIndex == 0)
						{
							Exec.RequestTransition(TreeTransitionsToTest[TransitionIndex].TargetState);
						}
						else
						{
							TransitionIfTaskIndex = TreeTransitionsToTest[TransitionIndex].ActiveStateSourceIndex;
							TransitionIfTreeIndex = TreeTransitionsToTest[TransitionIndex].ActiveTreeSourceIndex;
							TransitionTo = TreeTransitionsToTest[TransitionIndex].TargetState;
						}

						Exec.Tick(0.01f);

						RecordedTransitions = Exec.GetRecordedTransitions();

						ActiveStateNames = Exec.GetActiveStateNames();
						TArray<FName> ExpectedActiveStateNames = PreviousTreeActiveStateName;
						ExpectedActiveStateNames.Append(TreeTransitionsToTest[TransitionIndex].ExpectedActiveStateNames);
						AITEST_TRUE(FString::Printf(TEXT("Normal transition is not in expected states %d:%d"), TreeIndex, TransitionIndex), ActiveStateNames == ExpectedActiveStateNames);

						// Build force transition
						ActiveStateHandles.Reserve(ActiveStateNames.Num());
						const FMetaStoryExecutionState* ExecState = InstanceData.GetExecutionState();
						for (const FMetaStoryExecutionFrame& CurrentFrame : ExecState->ActiveFrames)
						{
							const UMetaStory* CurrentMetaStory = CurrentFrame.MetaStory;
							for (int32 Index = 0; Index < CurrentFrame.ActiveStates.Num(); Index++)
							{
								const FMetaStoryStateHandle Handle = CurrentFrame.ActiveStates[Index];
								ActiveStateHandles.Emplace(CurrentMetaStory, Handle);
							}
						}

						// Reset
						TransitionIfTaskIndex = INDEX_NONE;
						TransitionIfTreeIndex = INDEX_NONE;
						TransitionTo = FMetaStoryStateHandle();
						Exec.LogClear();
					}
					{
						FTestMetaStoryExecutionContext Exec(*MetaStory1, *MetaStory1, OldForceInstanceData);
						for(const FMetaStoryRecordedTransitionResult& ForcedTransition : RecordedTransitions)
						{
							AITEST_TRUE(FString::Printf(TEXT("Old Force transition %d:%d"), TreeIndex, TransitionIndex), Exec.ForceTransition(ForcedTransition) != EMetaStoryRunStatus::Unset);
						}
						const TArray<FName> NewActiveStateNames = Exec.GetActiveStateNames();
						AITEST_TRUE(FString::Printf(TEXT("Old force transition is not in expected states %d:%d"), TreeIndex, TransitionIndex), ActiveStateNames == NewActiveStateNames);
						Exec.LogClear();
					}

					if (TreeTransitionsToTest[TransitionIndex].bTestNextTree)
					{
						ActiveStateNames.Pop();
						if (!RecursiveLambda(RecursiveLambda, TreeIndex + 1, ActiveStateNames, false))
						{
							return false;
						}
					}
				}
				return true;
			};

		constexpr int32 MaxRules = 4;
		auto MakeStateSelectionRule = [](int32 Index)
			{
				EMetaStoryStateSelectionRules Rule = EMetaStoryStateSelectionRules::None;
				if ((Index % 2) == 1)
				{
					Rule |= EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates;
					Rule |= EMetaStoryStateSelectionRules::CompletedStateBeforeTransitionSourceFailsTransition;
				}
				if (Index >= 2)
				{
					Rule |= EMetaStoryStateSelectionRules::ReselectedStateCreatesNewStates;
				}
				return Rule;
			};


		for (int32 Index = 0; Index < MaxRules; ++Index)
		{
			const EMetaStoryStateSelectionRules StateSelectionRules = MakeStateSelectionRule(Index);
			InstanceData = FMetaStoryInstanceData();
			OldForceInstanceData = FMetaStoryInstanceData();

			auto CompileTree = [StateSelectionRules](TNotNull<UMetaStory*> MetaStory)
				{
					MetaStory->ResetCompiled();
					UMetaStoryEditorData* EditorData = CastChecked<UMetaStoryEditorData>(MetaStory->EditorData);
					UMetaStoryTestSchema* Schema = CastChecked<UMetaStoryTestSchema>(EditorData->Schema);
					Schema->SetStateSelectionRules(StateSelectionRules);

					FMetaStoryCompilerLog Log;
					FMetaStoryCompiler Compiler(Log);
					return Compiler.Compile(MetaStory);
				};

			AITEST_TRUE("MetaStory3 should get compiled", CompileTree(MetaStory3));
			AITEST_TRUE("MetaStory2 should get compiled", CompileTree(MetaStory2));
			AITEST_TRUE("MetaStory1 should get compiled", CompileTree(MetaStory1));

			{
				FTestMetaStoryExecutionContext Exec(*MetaStory1, *MetaStory1, InstanceData);
				Exec.Start();
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateA"));
			}
			{
				FTestMetaStoryExecutionContext Exec(*MetaStory1, *MetaStory1, OldForceInstanceData);
				Exec.Start();
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateA"));
			}
			bool bCurrentResult = TestAllTransitions(TestAllTransitions, 0, TArray<FName>(), true);
			{
				FTestMetaStoryExecutionContext Exec(*MetaStory1, *MetaStory1, InstanceData);
				Exec.Stop();
			}
			{
				FTestMetaStoryExecutionContext Exec(*MetaStory1, *MetaStory1, OldForceInstanceData);
				Exec.Stop();
			}

			AITEST_TRUE("Test failed", bCurrentResult);
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_ForceTransition_All, "System.MetaStory.ForceTransition.All");

} // namespace UE::MetaStory::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
