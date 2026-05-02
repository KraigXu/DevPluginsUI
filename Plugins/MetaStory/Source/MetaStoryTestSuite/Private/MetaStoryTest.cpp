// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryTest.h"
#include "MetaStoryTestBase.h"
#include "AITestsCommon.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryCompiler.h"
#include "Conditions/MetaStoryCommonConditions.h"
#include "Tasks/MetaStoryRunParallelMetaStoryTask.h"
#include "MetaStoryTestTypes.h"
#include "AutoRTFM.h"
#include "Engine/World.h"
#include "Async/ParallelFor.h"
#include "GameplayTagsManager.h"
#include "MetaStoryReference.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryTest)

#define LOCTEXT_NAMESPACE "AITestSuite_MetaStoryTest"

UE_DISABLE_OPTIMIZATION_SHIP

std::atomic<int32> FMetaStoryTestConditionInstanceData::GlobalCounter = 0;

namespace UE::MetaStory::Tests
{


struct FMetaStoryTest_MakeAndBakeMetaStory : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		UMetaStoryState& StateB = Root.AddChildState(FName(TEXT("B")));

		// Root
		auto& EvalA = EditorData.AddEvaluator<FTestEval_A>();
		
		// State A
		auto& TaskB1 = StateA.AddTask<FTestTask_B>();
		EditorData.AddPropertyBinding(EvalA, TEXT("IntA"), TaskB1, TEXT("IntB"));

		auto& IntCond = StateA.AddEnterCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Less);
		IntCond.GetInstanceData().Right = 2;

		EditorData.AddPropertyBinding(EvalA, TEXT("IntA"), IntCond, TEXT("Left"));

		StateA.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &StateB);

		// State B
		auto& TaskB2 = StateB.AddTask<FTestTask_B>();
		EditorData.AddPropertyBinding(EvalA, TEXT("bBoolA"), TaskB2, TEXT("bBoolB"));

		FMetaStoryTransition& Trans = StateB.AddTransition({}, EMetaStoryTransitionType::GotoState, &Root);
		auto& TransFloatCond = Trans.AddCondition<FMetaStoryCompareFloatCondition>(EGenericAICheck::Less);
		TransFloatCond.GetInstanceData().Right = 13.0f;
		EditorData.AddPropertyBinding(EvalA, TEXT("FloatA"), TransFloatCond, TEXT("Left"));

		StateB.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
		AITEST_TRUE(TEXT("MetaStory should be ready to run"), MetaStory.IsReadyToRun());

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_MakeAndBakeMetaStory, "System.MetaStory.MakeAndBakeMetaStory");


struct FMetaStoryTest_EmptyMetaStory : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		Root.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory should be completed"), Status == EMetaStoryRunStatus::Succeeded);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_EmptyMetaStory, "System.MetaStory.Empty");

struct FMetaStoryTest_Sequence : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UMetaStoryState& State2 = Root.AddChildState(FName(TEXT("State2")));

		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		State1.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::NextState);

		auto& Task2 = State2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		State2.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));
		
		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory Task1 should not tick"), Exec.Expect(Task1.GetName(), TickStr));
		Exec.LogClear();

		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task1 should tick, and exit state"), Exec.Expect(Task1.GetName(), TickStr).Then(Task1.GetName(), ExitStateStr));
		AITEST_TRUE(TEXT("MetaStory Task2 should enter state"), Exec.Expect(Task2.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory Task2 should not tick"), Exec.Expect(Task2.GetName(), TickStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();
		
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task2 should tick, and exit state"), Exec.Expect(Task2.GetName(), TickStr).Then(Task2.GetName(), ExitStateStr));
		AITEST_FALSE(TEXT("MetaStory Task1 should not tick"), Exec.Expect(Task1.GetName(), TickStr));
		AITEST_TRUE(TEXT("MetaStory should be completed"), Status == EMetaStoryRunStatus::Succeeded);
		Exec.LogClear();

		Status = Exec.Tick(0.1f);
		AITEST_FALSE(TEXT("MetaStory Task1 should not tick"), Exec.Expect(Task1.GetName(), TickStr));
		AITEST_FALSE(TEXT("MetaStory Task2 should not tick"), Exec.Expect(Task2.GetName(), TickStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Sequence, "System.MetaStory.Sequence");

struct FMetaStoryTest_Select : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UMetaStoryState& State1A = State1.AddChildState(FName(TEXT("State1A")));

		auto& TaskRoot = Root.AddTask<FTestTask_Stand>(FName(TEXT("TaskRoot")));
		TaskRoot.GetNode().TicksToCompletion = 3;  // let Task1A to complete first

		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 3; // let Task1A to complete first

		auto& Task1A = State1A.AddTask<FTestTask_Stand>(FName(TEXT("Task1A")));
		Task1A.GetNode().TicksToCompletion = 2;
		State1A.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &State1);

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory TaskRoot should enter state"), Exec.Expect(TaskRoot.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task1A should enter state"), Exec.Expect(Task1A.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory TaskRoot should not tick"), Exec.Expect(TaskRoot.GetName(), TickStr));
		AITEST_FALSE(TEXT("MetaStory Task1 should not tick"), Exec.Expect(Task1.GetName(), TickStr));
		AITEST_FALSE(TEXT("MetaStory Task1A should not tick"), Exec.Expect(Task1A.GetName(), TickStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Regular tick, no state selection at all.
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory tasks should update in order"), Exec.Expect(TaskRoot.GetName(), TickStr).Then(Task1.GetName(), TickStr).Then(Task1A.GetName(), TickStr));
		AITEST_FALSE(TEXT("MetaStory TaskRoot should not EnterState"), Exec.Expect(TaskRoot.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory Task1 should not EnterState"), Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory Task1A should not EnterState"), Exec.Expect(Task1A.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory TaskRoot should not ExitState"), Exec.Expect(TaskRoot.GetName(), ExitStateStr));
		AITEST_FALSE(TEXT("MetaStory Task1 should not ExitState"), Exec.Expect(Task1.GetName(), ExitStateStr));
		AITEST_FALSE(TEXT("MetaStory Task1A should not ExitState"), Exec.Expect(Task1A.GetName(), ExitStateStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Partial reselect, Root should not get EnterState
		Status = Exec.Tick(0.1f);
		AITEST_FALSE(TEXT("MetaStory TaskRoot should not enter state"), Exec.Expect(TaskRoot.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task1 should tick, exit state, and enter state"), Exec.Expect(Task1.GetName(), TickStr).Then(Task1.GetName(), ExitStateStr).Then(Task1.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task1A should tick, exit state, and enter state"), Exec.Expect(Task1A.GetName(), TickStr).Then(Task1A.GetName(), ExitStateStr).Then(Task1A.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Select, "System.MetaStory.Select");


struct FMetaStoryTest_FailEnterState : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UMetaStoryState& State1A = State1.AddChildState(FName(TEXT("State1A")));

		auto& TaskRoot = Root.AddTask<FTestTask_Stand>(FName(TEXT("TaskRoot")));

		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		auto& Task2 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		Task2.GetNode().EnterStateResult = EMetaStoryRunStatus::Failed;
		auto& Task3 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task3")));

		auto& Task1A = State1A.AddTask<FTestTask_Stand>(FName(TEXT("Task1A")));
		State1A.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &State1);

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));
		const TCHAR* StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory TaskRoot should enter state"), Exec.Expect(TaskRoot.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task2 should enter state"), Exec.Expect(Task2.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory Task3 should not enter state"), Exec.Expect(Task3.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Should execute StateCompleted in reverse order"), Exec.Expect(Task2.GetName(), StateCompletedStr).Then(Task1.GetName(), StateCompletedStr).Then(TaskRoot.GetName(), StateCompletedStr));
		AITEST_FALSE(TEXT("MetaStory Task3 should not state complete"), Exec.Expect(Task3.GetName(), StateCompletedStr));
		AITEST_TRUE(TEXT("MetaStory exec status should be failed"), Exec.GetLastTickStatus() == EMetaStoryRunStatus::Failed);
		Exec.LogClear();

		// It will try 5 times to reenter the same states.
		Exec.Tick(0.01f);
		AITEST_TRUE(TEXT("MetaStory TaskRoot should tick"), Exec.Expect(TaskRoot.GetName(), TickStr));
		AITEST_TRUE(TEXT("MetaStory Task1 should tick"), Exec.Expect(Task1.GetName(), TickStr));
		AITEST_FALSE(TEXT("MetaStory Task2 should not tick because it completed"), Exec.Expect(Task2.GetName(), TickStr));
		AITEST_FALSE(TEXT("MetaStory Task3 should not tick because it didn't enter state"), Exec.Expect(Task3.GetName(), TickStr));
		AITEST_FALSE(TEXT("MetaStory Task3 should not exit state"), Exec.Expect(Task3.GetName(), ExitStateStr));
		Exec.LogClear();

		// Stop and exit state
		Status = Exec.Stop();
		AITEST_FALSE(TEXT("MetaStory Task1 should not state complete"), Exec.Expect(Task1.GetName(), StateCompletedStr));
		AITEST_FALSE(TEXT("MetaStory Task2 should not state complete"), Exec.Expect(Task2.GetName(), StateCompletedStr));
		AITEST_FALSE(TEXT("MetaStory Task3 should not state complete"), Exec.Expect(Task3.GetName(), StateCompletedStr));
		AITEST_FALSE(TEXT("MetaStory TaskRoot should not state complete"), Exec.Expect(TaskRoot.GetName(), StateCompletedStr));
		AITEST_TRUE(TEXT("MetaStory TaskRoot should exit state"), Exec.Expect(TaskRoot.GetName(), ExitStateStr));
		AITEST_TRUE(TEXT("MetaStory Task1 should exit state"), Exec.Expect(Task1.GetName(), ExitStateStr));
		AITEST_TRUE(TEXT("MetaStory Task2 should exit state"), Exec.Expect(Task2.GetName(), ExitStateStr));
		AITEST_FALSE(TEXT("MetaStory Task3 should not exit state"), Exec.Expect(Task3.GetName(), ExitStateStr));
		AITEST_TRUE(TEXT("MetaStory status should be stopped"), Status == EMetaStoryRunStatus::Stopped);
		Exec.LogClear();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_FailEnterState, "System.MetaStory.FailEnterState");


struct FMetaStoryTest_Restart : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));

		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 2;

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE("MetaStory should get compiled", bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("MetaStory should init", bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));
		const TCHAR* StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory exec status should be running"), Exec.GetLastTickStatus() == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Tick
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory exec status should be running"), Exec.GetLastTickStatus() == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Call Start again, should stop and start the tree.
		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Task1 should exit state"), Exec.Expect(Task1.GetName(), ExitStateStr));
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory exec status should be running"), Exec.GetLastTickStatus() == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Restart, "System.MetaStory.Restart");

struct FMetaStoryTest_SubTree_ActiveTasks : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")), EMetaStoryStateType::Linked);
		UMetaStoryState& State2 = Root.AddChildState(FName(TEXT("State2")));
		UMetaStoryState& State3 = Root.AddChildState(FName(TEXT("State3")), EMetaStoryStateType::Subtree);
		UMetaStoryState& State3A = State3.AddChildState(FName(TEXT("State3A")));
		UMetaStoryState& State3B = State3.AddChildState(FName(TEXT("State3B")));

		State1.SetLinkedState(State3.GetLinkToState());

		State1.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &State2);

		auto& Task2 = State2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		State2.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);

		auto& Task3A = State3A.AddTask<FTestTask_Stand>(FName(TEXT("Task3A")));
		State3A.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &State3B);

		auto& Task3B = State3B.AddTask<FTestTask_Stand>(FName(TEXT("Task3B")));
		State3B.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));
		const TCHAR* StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Status = Exec.Start();

		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/State1/State3/State3A"), Exec.ExpectInActiveStates(Root.Name, State1.Name, State3.Name, State3A.Name));
		AITEST_FALSE(TEXT("MetaStory Task2 should not enter state"), Exec.Expect(Task2.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task3A should enter state"), Exec.Expect(Task3A.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Transition within subtree
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/State1/State3/State3B"), Exec.ExpectInActiveStates(Root.Name, State1.Name, State3.Name, State3B.Name));
		AITEST_TRUE(TEXT("MetaStory Task3B should enter state"), Exec.Expect(Task3B.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Complete subtree
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/State2"), Exec.ExpectInActiveStates(Root.Name, State2.Name));
		AITEST_TRUE(TEXT("MetaStory Task2 should enter state"), Exec.Expect(Task2.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Complete the whole tree
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory should complete in succeeded"), Status == EMetaStoryRunStatus::Succeeded);
		Exec.LogClear();

		Exec.Stop();
		
		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_SubTree_ActiveTasks, "System.MetaStory.SubTree.ActiveTasks");

struct FMetaStoryTest_SubTree_NoActiveTasks : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		/*
		 - RootA
			- StateA : SubTree -> StateB
			- StateB
		 - RootB -> StateB
		 - SubTree[DisabledTask] -> StateB
			- StateC -> RootB
		 */
		UMetaStoryState& RootA = EditorData.AddSubTree(FName(TEXT("RootA")));
		UMetaStoryState& StateA = RootA.AddChildState(FName(TEXT("StateA")));
		UMetaStoryState& StateB = RootA.AddChildState(FName(TEXT("StateB")));

		UMetaStoryState& RootB = EditorData.AddSubTree(FName(TEXT("RootB")));

		UMetaStoryState& SubTree = EditorData.AddSubTree(FName(TEXT("SubTree")));
		UMetaStoryState& StateC = SubTree.AddChildState(FName(TEXT("StateC")));

		SubTree.Type = EMetaStoryStateType::Subtree;
		StateA.Type = EMetaStoryStateType::Linked;
		StateA.SetLinkedState(SubTree.GetLinkToState());

		StateA.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &StateB);
		RootB.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &StateB);
		SubTree.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &StateB);
		StateC.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &RootB);

		auto& TaskNode = SubTree.AddTask<FTestTask_Stand>(FName(TEXT("DisabledTask")));
		TaskNode.GetNode().bTaskEnabled = false;

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		// Start and enter state
		Status = Exec.Start();

		AITEST_TRUE(TEXT("MetaStory Active States should be in RootA/StateA/SubTree/StateC"),
					Exec.ExpectInActiveStates(RootA.Name, StateA.Name, SubTree.Name, StateC.Name));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Transition from the subtree frame. The parent frame and the disabled task should be ignored.
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Active States should be in RootB"),
					Exec.ExpectInActiveStates(RootB.Name));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Active States should be in RootA/StateB"),
					Exec.ExpectInActiveStates(RootA.Name, StateB.Name));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Active States should be in RootA/StateA/SubTree/StateC"),
					Exec.ExpectInActiveStates(RootA.Name, StateA.Name, SubTree.Name, StateC.Name));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_SubTree_NoActiveTasks, "System.MetaStory.SubTree.NoActiveTasks");

struct FMetaStoryTest_SubTree_Condition : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		/*
		- Root
			- Linked : Subtree -> Root
			- SubTree : Task1
				- ? State1 : Task2 -> Succeeded // condition linked to Task1
				- State2 : Task3
		*/
		
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& Linked = Root.AddChildState(FName(TEXT("Linked")), EMetaStoryStateType::Linked);
		
		UMetaStoryState& SubTree = Root.AddChildState(FName(TEXT("SubTree")), EMetaStoryStateType::Subtree);
		UMetaStoryState& State1 = SubTree.AddChildState(FName(TEXT("State1")));
		UMetaStoryState& State2 = SubTree.AddChildState(FName(TEXT("State2")));

		Linked.SetLinkedState(SubTree.GetLinkToState());

		Linked.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &Linked);

		// SubTask should not complete during the test.
		TMetaStoryTypedEditorNode<FTestTask_Stand>& SubTask = SubTree.AddTask<FTestTask_Stand>(FName(TEXT("SubTask")));
		SubTask.GetNode().TicksToCompletion = 100;

		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 1;

		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task2 = State2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		Task2.GetNode().TicksToCompletion = 1;
		
		// Allow to enter State1 if Task1 instance data TicksToCompletion > 0.
		TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& IntCond1 = State1.AddEnterCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Greater);
		EditorData.AddPropertyBinding(SubTask, TEXT("CurrentTick"), IntCond1, TEXT("Left"));
		IntCond1.GetInstanceData().Right = 0;

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE("MetaStory should get compiled", bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));
		const TCHAR* StateCompletedStr(TEXT("StateCompleted"));

		GetTestRunner().AddExpectedMessage(TEXT("Evaluation forced to false"), ELogVerbosity::Warning, EAutomationExpectedErrorFlags::Contains, 1);

		// Start and enter state
		Status = Exec.Start();

		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/Linked/SubTree/State2"), Exec.ExpectInActiveStates(Root.Name, Linked.Name, SubTree.Name, State2.Name));
		AITEST_FALSE(TEXT("MetaStory State1 should not be active"), Exec.ExpectInActiveStates(State1.Name)); // Enter condition should prevent to enter State1
		AITEST_TRUE(TEXT("MetaStory SubTask should enter state"), Exec.Expect(SubTask.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task2 should enter state"), Exec.Expect(Task2.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Task1 completes, and we should enter State1 since the enter condition now passes.
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/Linked/SubTree/State1"), Exec.ExpectInActiveStates(Root.Name, Linked.Name, SubTree.Name, State1.Name));
		AITEST_FALSE(TEXT("MetaStory State2 should not be active"), Exec.ExpectInActiveStates(State2.Name));
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_SubTree_Condition, "System.MetaStory.SubTree.Condition");

struct FMetaStoryTest_SubTree_CascadedSucceeded : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		//	- Root [TaskA]
		//		- LinkedState>SubTreeState -> (F)Failed
		//		- SubTreeState [TaskB]
		//			- SubLinkedState>SubSubTreeState -> (S)Failed
		//		- SubSubTreeState
		//			- SubSubLeaf [TaskC] -> (S)Succeeded
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& LinkedState = Root.AddChildState(FName(TEXT("Linked")), EMetaStoryStateType::Linked);
		
		UMetaStoryState& SubTreeState = Root.AddChildState(FName(TEXT("SubTreeState")), EMetaStoryStateType::Subtree);
		UMetaStoryState& SubLinkedState = SubTreeState.AddChildState(FName(TEXT("SubLinkedState")), EMetaStoryStateType::Linked);
		
		UMetaStoryState& SubSubTreeState = Root.AddChildState(FName(TEXT("SubSubTreeState")), EMetaStoryStateType::Subtree);
		UMetaStoryState& SubSubLeaf = SubSubTreeState.AddChildState(FName(TEXT("SubSubLeaf")));

		LinkedState.SetLinkedState(SubTreeState.GetLinkToState());
		SubLinkedState.SetLinkedState(SubSubTreeState.GetLinkToState());

		LinkedState.AddTransition(EMetaStoryTransitionTrigger::OnStateFailed, EMetaStoryTransitionType::Failed);
		SubLinkedState.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::Failed);
		SubSubLeaf.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::Succeeded);

		TMetaStoryTypedEditorNode<FTestTask_Stand>& TaskA = Root.AddTask<FTestTask_Stand>(FName(TEXT("TaskA")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& TaskB = SubTreeState.AddTask<FTestTask_Stand>(FName(TEXT("TaskB")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& TaskC = SubSubLeaf.AddTask<FTestTask_Stand>(FName(TEXT("TaskC")));

		TaskA.GetNode().TicksToCompletion = 2;
		TaskB.GetNode().TicksToCompletion = 2;
		TaskC.GetNode().TicksToCompletion = 1; // The deepest task completes first.
		
		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));
		const TCHAR* StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/Linked/SubTreeState"), Exec.ExpectInActiveStates(Root.Name, LinkedState.Name, SubTreeState.Name, SubLinkedState.Name, SubSubTreeState.Name, SubSubLeaf.Name));
		AITEST_TRUE(TEXT("TaskA,B,C should enter state"), Exec.Expect(TaskA.GetName(), EnterStateStr).Then(TaskB.GetName(), EnterStateStr).Then(TaskC.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Subtrees completes, and it completes the whole tree too.
		// There's no good way to observe this externally. We switch the return along the way to make sure the transition does not happen directly from the leaf to failed.
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory should be Failed"), Status == EMetaStoryRunStatus::Failed);
		Exec.LogClear();

		Exec.Stop();
		
		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_SubTree_CascadedSucceeded, "System.MetaStory.SubTree.CascadedSucceeded");


struct FMetaStoryTest_SharedInstanceData : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		auto& IntCond = Root.AddEnterCondition<FMetaStoryTestCondition>();
		IntCond.GetInstanceData().Count = 1;

		auto& Task = Root.AddTask<FTestTask_Stand>(FName(TEXT("Task")));
		Task.GetNode().TicksToCompletion = 2;

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		// Init, nothing should access the shared data.
		constexpr int32 NumConcurrent = 100;
		UE_AUTORTFM_OPEN
		{
			FMetaStoryTestConditionInstanceData::GlobalCounter = 0;
		};

		bool bInitSucceeded = true;
		TArray<FMetaStoryInstanceData> InstanceDatas;

		InstanceDatas.SetNum(NumConcurrent);
		for (int32 Index = 0; Index < NumConcurrent; Index++)
		{
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceDatas[Index]);
			bInitSucceeded &= Exec.IsValid();
		}
		AITEST_TRUE(TEXT("All MetaStory contexts should init"), bInitSucceeded);

		int32 GlobalCounterValue;
		UE_AUTORTFM_OPEN
		{
			GlobalCounterValue = FMetaStoryTestConditionInstanceData::GlobalCounter;
		};
		AITEST_EQUAL(TEXT("Test condition global counter should be 0"), GlobalCounterValue, 0);
		
		// Start in parallel
		// This should create shared data per thread.
		// We expect that ParallelForWithTaskContext() creates a context per thread.
		TArray<FMetaStoryTestRunContext> RunContexts;
		
		ParallelForWithTaskContext(
			RunContexts,
			InstanceDatas.Num(),
			[&InstanceDatas, &MetaStory](FMetaStoryTestRunContext& RunContext, int32 Index)
			{
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceDatas[Index]);
				const EMetaStoryRunStatus Status = Exec.Start();
				if (Status == EMetaStoryRunStatus::Running)
				{
					RunContext.Count++;
				}
			}
		);

		int32 StartTotalRunning = 0;
		for (FMetaStoryTestRunContext RunContext : RunContexts)
		{
			StartTotalRunning += RunContext.Count;
		}
		AITEST_EQUAL(TEXT("All MetaStory contexts should be running after Start"), StartTotalRunning, NumConcurrent);

		UE_AUTORTFM_OPEN
		{
			GlobalCounterValue = FMetaStoryTestConditionInstanceData::GlobalCounter;
		};
		AITEST_EQUAL(TEXT("Test condition global counter should equal context count after Start"), GlobalCounterValue, InstanceDatas.Num());
		
		// Tick in parallel
		// This should not recreate the data, so FMetaStoryTestConditionInstanceData::GlobalCounter should stay as is.
		for (FMetaStoryTestRunContext RunContext : RunContexts)
		{
			RunContext.Count = 0;
		}

		ParallelForWithTaskContext(
			RunContexts,
			InstanceDatas.Num(),
			[&InstanceDatas, &MetaStory](FMetaStoryTestRunContext& RunContext, int32 Index)
			{
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceDatas[Index]);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				if (Status == EMetaStoryRunStatus::Running)
				{
					RunContext.Count++;
				}
			}
		);

		int32 TickTotalRunning = 0;
		for (FMetaStoryTestRunContext RunContext : RunContexts)
		{
			TickTotalRunning += RunContext.Count;
		}
		AITEST_EQUAL(TEXT("All MetaStory contexts should be running after Tick"), TickTotalRunning, NumConcurrent);

		UE_AUTORTFM_OPEN
		{
			GlobalCounterValue = FMetaStoryTestConditionInstanceData::GlobalCounter;
		};
		AITEST_EQUAL(TEXT("Test condition global counter should equal context count after Tick"), GlobalCounterValue, InstanceDatas.Num());

		ParallelForWithTaskContext(
			RunContexts,
			InstanceDatas.Num(),
			[&InstanceDatas, &MetaStory](FMetaStoryTestRunContext& RunContext, int32 Index)
			{
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceDatas[Index]);
				Exec.Stop();
			}
		);

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_SharedInstanceData, "System.MetaStory.SharedInstanceData");

struct FMetaStoryTest_LastConditionWithIndent : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));

		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		State1.AddEnterCondition<FMetaStoryTestCondition>();
		auto& LastCondition = State1.AddEnterCondition<FMetaStoryTestCondition>();

		// Last condition has Indent
		LastCondition.ExpressionIndent = 1;
		
		State1.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory Task1 should not tick"), Exec.Expect(Task1.GetName(), TickStr));
		Exec.LogClear();

		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task1 should tick, and exit state"), Exec.Expect(Task1.GetName(), TickStr).Then(Task1.GetName(), ExitStateStr));
		AITEST_TRUE(TEXT("MetaStory should be completed"), Status == EMetaStoryRunStatus::Succeeded);
		Exec.LogClear();

		Status = Exec.Tick(0.1f);
		AITEST_FALSE(TEXT("MetaStory Task1 should not tick"), Exec.Expect(Task1.GetName(), TickStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_LastConditionWithIndent, "System.MetaStory.LastConditionWithIndent");

struct FMetaStoryTest_StateRequiringEvent : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		
		FGameplayTag ValidTag = GetTestTag1();
		FGameplayTag InvalidTag = GetTestTag2();

		using FValidPayload = FMetaStoryTest_PropertyStructA;
		using FInvalidPayload = FMetaStoryTest_PropertyStructB;

		// This state shouldn't be selected as it requires different tag.
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		StateA.bHasRequiredEventToEnter  = true;
		StateA.RequiredEventToEnter.Tag = InvalidTag;
		auto& TaskA = StateA.AddTask<FTestTask_Stand>(FName(TEXT("TaskA")));

		// This state shouldn't be selected as it requires different payload.
		UMetaStoryState& StateB = Root.AddChildState(FName(TEXT("B")));
		StateB.bHasRequiredEventToEnter  = true;
		StateB.RequiredEventToEnter.PayloadStruct = FInvalidPayload::StaticStruct();
		auto& TaskB = StateB.AddTask<FTestTask_Stand>(FName(TEXT("TaskB")));

		// This state shouldn't be selected as it requires the same tag, but different payload.
		UMetaStoryState& StateC = Root.AddChildState(FName(TEXT("C")));
		StateC.bHasRequiredEventToEnter  = true;
		StateC.RequiredEventToEnter.Tag = ValidTag;
		StateC.RequiredEventToEnter.PayloadStruct = FInvalidPayload::StaticStruct();
		auto& TaskC = StateC.AddTask<FTestTask_Stand>(FName(TEXT("TaskC")));

		// This state shouldn't be selected as it requires the same payload, but different tag.
		UMetaStoryState& StateD = Root.AddChildState(FName(TEXT("D")));
		StateD.bHasRequiredEventToEnter  = true;
		StateD.RequiredEventToEnter.Tag = InvalidTag;
		StateD.RequiredEventToEnter.PayloadStruct = FValidPayload::StaticStruct();
		auto& TaskD = StateD.AddTask<FTestTask_Stand>(FName(TEXT("TaskD")));

		// This state should be selected as the arrived event matches the requirement.
		UMetaStoryState& StateE = Root.AddChildState(FName(TEXT("E")));
		StateE.bHasRequiredEventToEnter  = true;
		StateE.RequiredEventToEnter.Tag = ValidTag;
		StateE.RequiredEventToEnter.PayloadStruct = FValidPayload::StaticStruct();
		auto& TaskE = StateE.AddTask<FTestTask_Stand>(FName(TEXT("TaskE")));

		// This state should be selected only initially when there's not event in the queue.
		UMetaStoryState& StateInitial = Root.AddChildState(FName(TEXT("Initial")));
		auto& TaskInitial = StateInitial.AddTask<FTestTask_Stand>(FName(TEXT("TaskInitial")));
		StateInitial.AddTransition(EMetaStoryTransitionTrigger::OnEvent, ValidTag, EMetaStoryTransitionType::GotoState, &Root);

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;

		const TCHAR* EnterStateStr(TEXT("EnterState"));

		auto Test=[&](FTestMetaStoryExecutionContext& Exec)
			{
				AITEST_FALSE(TEXT("MetaStory TaskA should not enter state"), Exec.Expect(TaskA.GetName(), EnterStateStr));
				AITEST_FALSE(TEXT("MetaStory TaskB should not enter state"), Exec.Expect(TaskB.GetName(), EnterStateStr));
				AITEST_FALSE(TEXT("MetaStory TaskC should not enter state"), Exec.Expect(TaskC.GetName(), EnterStateStr));
				AITEST_FALSE(TEXT("MetaStory TaskD should not enter state"), Exec.Expect(TaskD.GetName(), EnterStateStr));
				AITEST_TRUE(TEXT("MetaStory TaskE should enter state"), Exec.Expect(TaskE.GetName(), EnterStateStr));
				return true;
			};

		{
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("MetaStory should init", bInitSucceeded);

			Status = Exec.Start();
			AITEST_TRUE(TEXT("MetaStory TaskInitial should enter state"), Exec.Expect(TaskInitial.GetName(), EnterStateStr));
			Exec.LogClear();

			Exec.SendEvent(ValidTag, FConstStructView::Make(FValidPayload()));
			Status = Exec.Tick(0.1f);

			if (!Test(Exec))
			{
				return false;
			}
			Exec.LogClear();

			Exec.Stop();
		}
		// Same test but event sent with weak context while the FTestMetaStoryExecutionContext still exist
		{
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

			Status = Exec.Start();
			AITEST_TRUE(TEXT("MetaStory TaskInitial should enter state"), Exec.Expect(TaskInitial.GetName(), EnterStateStr));
			Exec.LogClear();

			FMetaStoryWeakExecutionContext WeakExec = Exec.MakeWeakExecutionContext();
			WeakExec.SendEvent(ValidTag, FConstStructView::Make(FValidPayload()));
			Status = Exec.Tick(0.1f);

			if (!Test(Exec))
			{
				return false;
			}
			Exec.LogClear();

			Exec.Stop();
		}
		// Same test but event sent with weak context
		{
			FMetaStoryWeakExecutionContext WeakExec;
			{
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

				Status = Exec.Start();
				AITEST_TRUE(TEXT("MetaStory TaskInitial should enter state"), Exec.Expect(TaskInitial.GetName(), EnterStateStr));
				Exec.LogClear();

				WeakExec = Exec.MakeWeakExecutionContext();
			}
			{
				WeakExec.SendEvent(ValidTag, FConstStructView::Make(FValidPayload()));
			}
			{
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				Status = Exec.Tick(0.1f);

				if (!Test(Exec))
				{
					return false;
				}
				Exec.LogClear();

				Exec.Stop();
			}
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_StateRequiringEvent, "System.MetaStory.StateRequiringEvent");

struct FMetaStoryTest_Start : FMetaStoryTestBase
{
	virtual UMetaStory& SetupTree()
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		UMetaStoryState& StateB = Root.AddChildState(FName(TEXT("B")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& TaskA = StateA.AddTask<FTestTask_Stand>(TaskAName);
		TMetaStoryTypedEditorNode<FTestTask_Stand>& TaskB = StateB.AddTask<FTestTask_Stand>(TaskBName);

		// Transition success 
		StateA.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::GotoState, &StateB);
		TaskA.GetNode().EnterStateResult = EMetaStoryRunStatus::Succeeded;

		return MetaStory;
	}

	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = SetupTree();

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE("MetaStory should get compiled", bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("MetaStory should init", bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));
		const TCHAR* StateCompletedStr(TEXT("StateCompleted"));

		{
			EMetaStoryRunStatus Status = Exec.Start();
			AITEST_EQUAL("Start should be running", Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE("MetaStory Active States should be in states.", Exec.ExpectInActiveStates("Root", "A"));
			AITEST_TRUE("MetaStory TaskA should enter state", Exec.Expect(TaskAName, EnterStateStr));
			AITEST_TRUE("MetaStory TaskA should state complete", Exec.Expect(TaskAName, StateCompletedStr));
			AITEST_TRUE("MetaStory execution should not sleep", !Exec.GetNextScheduledTick().ShouldSleep());
			Exec.LogClear();
		}
		{
			EMetaStoryRunStatus Status = Exec.Tick(0.1f);
			AITEST_TRUE("MetaStory Active States should be in states.", Exec.ExpectInActiveStates("Root", "B"));
			//@TODO Only one StateComplete
			//AITEST_FALSE("MetaStory TaskA should state complete", Exec.Expect(TaskAName, StateCompletedStr));
			AITEST_TRUE("MetaStory TaskA should get exit state expectedly", Exec.Expect(TaskAName, ExitStateStr));
			AITEST_TRUE("MetaStory TaskB should get enter state expectedly", Exec.Expect(TaskBName, EnterStateStr));
			Exec.LogClear();
		}

		Exec.Stop();

		return true;
	}

protected:
	const FName TaskAName = FName("TaskA");
	const FName TaskBName = FName("TaskB");
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Start, "System.MetaStory.Start.FirstStateSucceededImmediately");

struct FMetaStoryTest_StartScheduledTick : FMetaStoryTest_Start
{
	virtual UMetaStory& SetupTree()
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		UMetaStoryState& StateB = Root.AddChildState(FName(TEXT("B")));
		TMetaStoryTypedEditorNode<FTestTask_StandNoTick>& TaskA = StateA.AddTask<FTestTask_StandNoTick>(TaskAName);
		TMetaStoryTypedEditorNode<FTestTask_StandNoTick>& TaskB = StateB.AddTask<FTestTask_StandNoTick>(TaskBName);

		// Transition success 
		StateA.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::GotoState, &StateB);
		TaskA.GetNode().EnterStateResult = EMetaStoryRunStatus::Succeeded;

		return MetaStory;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_StartScheduledTick, "System.MetaStory.Start.FirstStateSucceededImmediatelyWithScheduledTick");

//
// The stop tests test how the combinations of execution path to stop the tree are reported on ExitState() transition.  
//
struct FMetaStoryTest_Stop : FMetaStoryTestBase
{
	UMetaStory& SetupTree()
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& TaskA = StateA.AddTask<FTestTask_Stand>(TaskAName);
		TMetaStoryTypedEditorNode<FTestTask_Stand>& GlobalTask = EditorData.AddGlobalTask<FTestTask_Stand>(GlobalTaskName);

		// Transition success 
		StateA.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::Succeeded);
		StateA.AddTransition(EMetaStoryTransitionTrigger::OnStateFailed, EMetaStoryTransitionType::Failed);

		GlobalTask.GetNode().TicksToCompletion = GlobalTaskTicks;
		GlobalTask.GetNode().TickCompletionResult = GlobalTaskStatus;
		GlobalTask.GetNode().EnterStateResult = GlobalTaskEnterStatus;

		TaskA.GetNode().TicksToCompletion = NormalTaskTicks;
		TaskA.GetNode().TickCompletionResult = NormalTaskStatus;
		TaskA.GetNode().EnterStateResult = NormalTaskEnterStatus;

		return MetaStory;
	}
	
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = SetupTree();
		
		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Start should be running"), Status, EMetaStoryRunStatus::Running);
		AITEST_TRUE(TEXT("MetaStory GlobalTask should enter state"), Exec.Expect(GlobalTaskName, EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory TaskA should enter state"), Exec.Expect(TaskAName, EnterStateStr));
		Exec.LogClear();

		Status = Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("Tree should end expectedly"), Status, ExpectedStatusAfterTick);
		AITEST_TRUE(TEXT("MetaStory GlobalTask should get exit state expectedly"), Exec.Expect(GlobalTaskName, ExpectedExitStatusStr));
		AITEST_TRUE(TEXT("MetaStory TaskA should get exit state expectedly"), Exec.Expect(TaskAName, ExpectedExitStatusStr));
		Exec.LogClear();
		
		Exec.Stop();

		return true;
	}

protected:

	const FName GlobalTaskName = FName(TEXT("GlobalTask"));
	const FName TaskAName = FName(TEXT("TaskA"));
	
	EMetaStoryRunStatus NormalTaskStatus = EMetaStoryRunStatus::Succeeded;
	EMetaStoryRunStatus NormalTaskEnterStatus = EMetaStoryRunStatus::Running;
	int32 NormalTaskTicks = 1;

	EMetaStoryRunStatus GlobalTaskStatus = EMetaStoryRunStatus::Succeeded;
	EMetaStoryRunStatus GlobalTaskEnterStatus = EMetaStoryRunStatus::Running;
	int32 GlobalTaskTicks = 1;

	EMetaStoryRunStatus ExpectedStatusAfterTick = EMetaStoryRunStatus::Succeeded;

	FString ExpectedExitStatusStr = TEXT("ExitSucceeded");
};

struct FMetaStoryTest_Stop_NormalSucceeded : FMetaStoryTest_Stop
{
	virtual bool SetUp() override
	{
		// Normal task completes as succeeded.
		NormalTaskStatus = EMetaStoryRunStatus::Succeeded;
		NormalTaskTicks = 1;

		// Global task completes later
		GlobalTaskTicks = 2;

		// Tree should complete as succeeded.
		ExpectedStatusAfterTick = EMetaStoryRunStatus::Succeeded;
		
		// Tasks should have Transition.CurrentRunStatus as succeeded 
		ExpectedExitStatusStr = TEXT("ExitSucceeded");

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Stop_NormalSucceeded, "System.MetaStory.Stop.NormalSucceeded");

struct FMetaStoryTest_Stop_NormalFailed : FMetaStoryTest_Stop
{
	virtual bool SetUp() override
	{
		// Normal task completes as failed.
		NormalTaskStatus = EMetaStoryRunStatus::Failed;
		NormalTaskTicks = 1;

		// Global task completes later.
		GlobalTaskTicks = 2;

		// Tree should complete as failed.
		ExpectedStatusAfterTick = EMetaStoryRunStatus::Failed;
		
		// Tasks should have Transition.CurrentRunStatus as failed.
		ExpectedExitStatusStr = TEXT("ExitFailed");

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Stop_NormalFailed, "System.MetaStory.Stop.NormalFailed");


struct FMetaStoryTest_Stop_GlobalSucceeded : FMetaStoryTest_Stop
{
	virtual bool SetUp() override
	{
		// Normal task completes later.
		NormalTaskTicks = 2;

		// Global task completes as succeeded.
		GlobalTaskStatus = EMetaStoryRunStatus::Succeeded;
		GlobalTaskTicks = 1;

		// Tree should complete as succeeded.
		ExpectedStatusAfterTick = EMetaStoryRunStatus::Succeeded;
		
		// Tasks should have Transition.CurrentRunStatus as succeeded.
		ExpectedExitStatusStr = TEXT("ExitSucceeded");

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Stop_GlobalSucceeded, "System.MetaStory.Stop.GlobalSucceeded");

struct FMetaStoryTest_Stop_GlobalFailed : FMetaStoryTest_Stop
{
	virtual bool SetUp() override
	{
		// Normal task completes later
		NormalTaskTicks = 2;

		// Global task completes as failed.
		GlobalTaskStatus = EMetaStoryRunStatus::Failed;
		GlobalTaskTicks = 1;

		// Tree should complete as failed.
		ExpectedStatusAfterTick = EMetaStoryRunStatus::Failed;
		
		// Tasks should have Transition.CurrentRunStatus as failed.
		ExpectedExitStatusStr = TEXT("ExitFailed");

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Stop_GlobalFailed, "System.MetaStory.Stop.GlobalFailed");


//
// Tests combinations of completing the tree on EnterState.
//
struct FMetaStoryTest_StopEnterNormal : FMetaStoryTest_Stop
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = SetupTree();
		
		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		// If a normal task fails at start, the last tick status will be failed, but transition handling (and final execution status) will take place next tick. 
		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Tree should be running after start"), Status, EMetaStoryRunStatus::Running);
		AITEST_EQUAL(TEXT("Last execution status should be expected value"), Exec.GetLastTickStatus(), ExpectedStatusAfterStart);

		// Handles any transitions from failed transition
		Status = Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("Start should be expected value"), Status, ExpectedStatusAfterStart);
		AITEST_TRUE(TEXT("MetaStory GlobalTask should get exit state expectedly"), Exec.Expect(GlobalTaskName, ExpectedExitStatusStr));

		AITEST_TRUE(TEXT("MetaStory TaskA should enter state"), Exec.Expect(TaskAName, EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory TaskA should report exit status"), Exec.Expect(TaskAName, ExpectedExitStatusStr));

		Exec.Stop();
		
		return true;
	}

	EMetaStoryRunStatus ExpectedStatusAfterStart = EMetaStoryRunStatus::Succeeded;
	FString ExpectedExitStatusStr = TEXT("ExitSucceeded");
	bool bExpectNormalTaskToRun = true; 
};

struct FMetaStoryTest_Stop_NormalEnterSucceeded : FMetaStoryTest_StopEnterNormal
{
	virtual bool SetUp() override
	{
		// Tasks should complete later.
		NormalTaskTicks = 2;
		GlobalTaskTicks = 2;

		// Normal task EnterState as succeeded, completion is handled using completion transitions.
		NormalTaskEnterStatus = EMetaStoryRunStatus::Succeeded;

		// Tree should complete as succeeded.
		ExpectedStatusAfterStart = EMetaStoryRunStatus::Succeeded;
		
		// Tasks should have Transition.CurrentRunStatus as succeeded.
		ExpectedExitStatusStr = TEXT("ExitSucceeded");

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Stop_NormalEnterSucceeded, "System.MetaStory.Stop.NormalEnterSucceeded");

struct FMetaStoryTest_Stop_NormalEnterFailed : FMetaStoryTest_StopEnterNormal
{
	virtual bool SetUp() override
	{
		// Tasks should complete later.
		NormalTaskTicks = 2;
		GlobalTaskTicks = 2;

		// Normal task EnterState as failed, completion is handled using completion transitions.
		NormalTaskEnterStatus = EMetaStoryRunStatus::Failed;

		// Tree should complete as failed.
		ExpectedStatusAfterStart = EMetaStoryRunStatus::Failed;
		
		// Tasks should have Transition.CurrentRunStatus as failed.
		ExpectedExitStatusStr = TEXT("ExitFailed");

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Stop_NormalEnterFailed, "System.MetaStory.Stop.NormalEnterFailed");




struct FMetaStoryTest_StopEnterGlobal : FMetaStoryTest_Stop
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = SetupTree();
		
		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Start should be expected value"), Status, ExpectedStatusAfterStart);
		AITEST_TRUE(TEXT("MetaStory GlobalTask should get exit state expectedly"), Exec.Expect(GlobalTaskName, ExpectedExitStatusStr));

		// Normal tasks should not run
		AITEST_FALSE(TEXT("MetaStory TaskA should not enter state"), Exec.Expect(TaskAName, EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory TaskA should not report exit status"), Exec.Expect(TaskAName, ExpectedExitStatusStr));
		Exec.LogClear();

		Exec.Stop();
		
		return true;
	}

	EMetaStoryRunStatus ExpectedStatusAfterStart = EMetaStoryRunStatus::Succeeded;
	FString ExpectedExitStatusStr = TEXT("ExitSucceeded");
};

struct FMetaStoryTest_Stop_GlobalEnterSucceeded : FMetaStoryTest_StopEnterGlobal
{
	virtual bool SetUp() override
	{
		// Tasks should complete later.
		NormalTaskTicks = 2;
		GlobalTaskTicks = 2;

		// Global task EnterState as succeeded, completion is handled directly based on the global task status.
		GlobalTaskEnterStatus = EMetaStoryRunStatus::Succeeded;

		// Tree should complete as succeeded.
		ExpectedStatusAfterStart = EMetaStoryRunStatus::Succeeded;
		
		// Tasks should have Transition.CurrentRunStatus as Succeeded.
		ExpectedExitStatusStr = TEXT("ExitSucceeded");

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Stop_GlobalEnterSucceeded, "System.MetaStory.Stop.GlobalEnterSucceeded");

struct FMetaStoryTest_Stop_GlobalEnterFailed : FMetaStoryTest_StopEnterGlobal
{
	virtual bool SetUp() override
	{
		// Tasks should complete later.
		NormalTaskTicks = 2;
		GlobalTaskTicks = 2;

		// Global task EnterState as failed, completion is handled directly based on the global task status.
		GlobalTaskEnterStatus = EMetaStoryRunStatus::Failed;

		// Tree should complete as failed.
		ExpectedStatusAfterStart = EMetaStoryRunStatus::Failed;
		
		// Tasks should have Transition.CurrentRunStatus as failed.
		ExpectedExitStatusStr = TEXT("ExitFailed");

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Stop_GlobalEnterFailed, "System.MetaStory.Stop.GlobalEnterFailed");

struct FMetaStoryTest_Stop_ExternalStop : FMetaStoryTest_Stop
{
	virtual bool SetUp() override
	{
		// Tasks should complete later.
		NormalTaskTicks = 2;
		GlobalTaskTicks = 2;

		// Tree should tick and keep on running.
		ExpectedStatusAfterTick = EMetaStoryRunStatus::Running;

		// Tree should stop as stopped.
		ExpectedStatusAfterStop = EMetaStoryRunStatus::Stopped;
		
		// Tasks should have Transition.CurrentRunStatus as stopped. 
		ExpectedExitStatusStr = TEXT("ExitStopped");

		return true;
	}
	
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = SetupTree();
		
		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Start should be running"), Status, EMetaStoryRunStatus::Running);
		AITEST_TRUE(TEXT("MetaStory GlobalTask should enter state"), Exec.Expect(GlobalTaskName, EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory TaskA should enter state"), Exec.Expect(TaskAName, EnterStateStr));
		Exec.LogClear();

		Status = Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("Tree should end expectedly"), Status, ExpectedStatusAfterTick);
		Exec.LogClear();

		Status = Exec.Stop(EMetaStoryRunStatus::Stopped);
		AITEST_EQUAL(TEXT("Start should be running"), Status, ExpectedStatusAfterStop);
		if (!ExpectedExitStatusStr.IsEmpty())
		{
			AITEST_TRUE(TEXT("MetaStory GlobalTask should get exit state expectedly"), Exec.Expect(GlobalTaskName, ExpectedExitStatusStr));
			AITEST_TRUE(TEXT("MetaStory TaskA should get exit state expectedly"), Exec.Expect(TaskAName, ExpectedExitStatusStr));
		}
		
		return true;
	}

	EMetaStoryRunStatus ExpectedStatusAfterStop = EMetaStoryRunStatus::Stopped;
	
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Stop_ExternalStop, "System.MetaStory.Stop.ExternalStop");

struct FMetaStoryTest_Stop_AlreadyStopped : FMetaStoryTest_Stop_ExternalStop
{
	virtual bool SetUp() override
	{
		// Normal task completes before stop.
		NormalTaskTicks = 1;
		NormalTaskStatus = EMetaStoryRunStatus::Succeeded;

		// Global task completes later
		GlobalTaskTicks = 2;

		// Tree should tick stop as succeeded.
		ExpectedStatusAfterTick = EMetaStoryRunStatus::Succeeded;

		// Tree is already stopped, should keep the status (not Stopped).
		ExpectedStatusAfterStop = EMetaStoryRunStatus::Succeeded;
		
		// Skip exit status check.
		ExpectedExitStatusStr = TEXT("");

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Stop_AlreadyStopped, "System.MetaStory.Stop.AlreadyStopped");

//
// The deferred stop tests validates that the tree can be properly stopped if requested in the main entry points (Start, Tick, Stop).  
//
struct FMetaStoryTest_DeferredStop : FMetaStoryTestBase
{
	UMetaStory& SetupTree() const
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		TMetaStoryTypedEditorNode<FTestTask_StopTree>& TaskA = StateA.AddTask<FTestTask_StopTree>(TEXT("Task"));
		TMetaStoryTypedEditorNode<FTestTask_StopTree>& GlobalTask = EditorData.AddGlobalTask<FTestTask_StopTree>(TEXT("GlobalTask"));

		StateA.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::Succeeded);
		StateA.AddTransition(EMetaStoryTransitionTrigger::OnStateFailed, EMetaStoryTransitionType::Failed);

		GlobalTask.GetNode().Phase = GlobalTaskPhase;
		TaskA.GetNode().Phase = TaskPhase;

		return MetaStory;
	}

	virtual bool RunDerivedTest(FTestMetaStoryExecutionContext& Exec) = 0;

	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = SetupTree();

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		return RunDerivedTest(Exec);
	}

protected:

	EMetaStoryUpdatePhase GlobalTaskPhase = EMetaStoryUpdatePhase::Unset;
	EMetaStoryUpdatePhase TaskPhase = EMetaStoryUpdatePhase::Unset;
};

struct FMetaStoryTest_DeferredStop_EnterGlobalTask : FMetaStoryTest_DeferredStop
{
	FMetaStoryTest_DeferredStop_EnterGlobalTask() { GlobalTaskPhase = EMetaStoryUpdatePhase::EnterStates; }
	virtual bool RunDerivedTest(FTestMetaStoryExecutionContext& Exec) override
	{
		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;

		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Tree should be stopped"), Status, EMetaStoryRunStatus::Stopped);

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_DeferredStop_EnterGlobalTask, "System.MetaStory.DeferredStop.EnterGlobalTask");

struct FMetaStoryTest_DeferredStop_TickGlobalTask : FMetaStoryTest_DeferredStop
{
	FMetaStoryTest_DeferredStop_TickGlobalTask() { GlobalTaskPhase = EMetaStoryUpdatePhase::TickMetaStory; }
	virtual bool RunDerivedTest(FTestMetaStoryExecutionContext& Exec) override
	{
		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;

		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Tree should be running"), Status, EMetaStoryRunStatus::Running);

		Status = Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("Tree should be stopped"), Status, EMetaStoryRunStatus::Stopped);

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_DeferredStop_TickGlobalTask, "System.MetaStory.DeferredStop.TickGlobalTask");

struct FMetaStoryTest_DeferredStop_ExitGlobalTask : FMetaStoryTest_DeferredStop
{
	FMetaStoryTest_DeferredStop_ExitGlobalTask() { GlobalTaskPhase = EMetaStoryUpdatePhase::ExitStates; }
	virtual bool RunDerivedTest(FTestMetaStoryExecutionContext& Exec) override
	{
		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;

		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Tree should be running"), Status, EMetaStoryRunStatus::Running);

		Status = Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("Tree should be running"), Status, EMetaStoryRunStatus::Running);

		Status = Exec.Stop();
		AITEST_EQUAL(TEXT("Tree should be stopped"), Status, EMetaStoryRunStatus::Stopped);

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_DeferredStop_ExitGlobalTask, "System.MetaStory.DeferredStop.ExitGlobalTask");

struct FMetaStoryTest_DeferredStop_EnterTask : FMetaStoryTest_DeferredStop
{
	FMetaStoryTest_DeferredStop_EnterTask() { TaskPhase = EMetaStoryUpdatePhase::EnterStates; }
	virtual bool RunDerivedTest(FTestMetaStoryExecutionContext& Exec) override
	{
		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;

		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Tree should be running"), Status, EMetaStoryRunStatus::Stopped);

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_DeferredStop_EnterTask, "System.MetaStory.DeferredStop.EnterTask");

struct FMetaStoryTest_DeferredStop_TickTask : FMetaStoryTest_DeferredStop
{
	FMetaStoryTest_DeferredStop_TickTask() { TaskPhase = EMetaStoryUpdatePhase::TickMetaStory; }
	virtual bool RunDerivedTest(FTestMetaStoryExecutionContext& Exec) override
	{
		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;

		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Tree should be running"), Status, EMetaStoryRunStatus::Running);

		Status = Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("Tree should be stopped"), Status, EMetaStoryRunStatus::Stopped);

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_DeferredStop_TickTask, "System.MetaStory.DeferredStop.TickTask");

struct FMetaStoryTest_DeferredStop_ExitTask : FMetaStoryTest_DeferredStop
{
	FMetaStoryTest_DeferredStop_ExitTask() { TaskPhase = EMetaStoryUpdatePhase::ExitStates; }
	virtual bool RunDerivedTest(FTestMetaStoryExecutionContext& Exec) override
	{
		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;

		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Tree should be running"), Status, EMetaStoryRunStatus::Running);

		Status = Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("Tree should be running"), Status, EMetaStoryRunStatus::Running);

		Status = Exec.Stop();
		AITEST_EQUAL(TEXT("Tree should be stopped"), Status, EMetaStoryRunStatus::Stopped);
		
		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_DeferredStop_ExitTask, "System.MetaStory.DeferredStop.ExitTask");

struct FMetaStoryTest_FinishTasks : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		/*
		 - RootA
			- StateA -> StateB
			- StateB -> StateA
		 */
		UMetaStoryState& RootA = EditorData.AddSubTree(FName(TEXT("RootA")));
		UMetaStoryState& StateA = RootA.AddChildState(FName(TEXT("StateA")));
		UMetaStoryState& StateB = RootA.AddChildState(FName(TEXT("StateB")));

		StateA.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &StateB);
		StateB.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &StateA);
		
		TMetaStoryTypedEditorNode<FMetaStoryTestCondition>& BoolCondB = StateB.AddEnterCondition<FMetaStoryTestCondition>();
		BoolCondB.GetNode().bTestConditionResult = false;

		TMetaStoryTypedEditorNode<FTestTask_PrintValue>& StateATask = StateA.AddTask<FTestTask_PrintValue>("StateATaskA");
		StateATask.GetInstanceData().Value = 101;
		StateATask.GetNode().CustomTickFunc = [](FMetaStoryExecutionContext& Context, const FTestTask_PrintValue* Task)
			{
				Context.FinishTask(*Task, EMetaStoryFinishTaskType::Succeeded);
				FTestTask_PrintValue::FInstanceDataType& InstanceData = Context.GetInstanceData(*Task);
				++InstanceData.Value;
			};

		// Test one finish
		{
			{
				FMetaStoryCompilerLog Log;
				FMetaStoryCompiler Compiler(Log);
				const bool bResult = Compiler.Compile(MetaStory);
				AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
			}
			{
				EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
				FMetaStoryInstanceData InstanceData;
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

				// Start and enter state
				Status = Exec.Start();

				AITEST_TRUE(TEXT("MetaStory Active States should be in RootA/StateA"),
					Exec.ExpectInActiveStates(RootA.Name, StateA.Name));
				AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
				Exec.LogClear();

				// On FinishTask, go to StateB but the condition will fail. Reselect a new StateA.
				Status = Exec.Tick(0.1f);
				AITEST_TRUE(TEXT("MetaStory Active States should be in RootA/StateA"),
					Exec.ExpectInActiveStates(RootA.Name, StateA.Name));
				AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Expect the output tasks"), Exec.Expect("StateATaskA", TEXT("Tick101"))
					.Then("StateATaskA", TEXT("Exitstate102"))
					.Then("StateATaskA", TEXT("EnterState101"))
					);
				Exec.LogClear();

				Exec.Stop();
			}
		}
		// Test two finish
		{
			{
				StateATask.GetNode().CustomTickFunc = [](FMetaStoryExecutionContext& Context, const FTestTask_PrintValue* Task)
					{
						Context.FinishTask(*Task, EMetaStoryFinishTaskType::Succeeded);
						Context.FinishTask(*Task, EMetaStoryFinishTaskType::Succeeded);
						FTestTask_PrintValue::FInstanceDataType& InstanceData = Context.GetInstanceData(*Task);
						++InstanceData.Value;
					};

				FMetaStoryCompilerLog Log;
				FMetaStoryCompiler Compiler(Log);
				const bool bResult = Compiler.Compile(MetaStory);
				AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
			}
			{
				EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
				FMetaStoryInstanceData InstanceData;
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

				// Start and enter state
				Status = Exec.Start();

				AITEST_TRUE(TEXT("MetaStory Active States should be in RootA/StateA"),
					Exec.ExpectInActiveStates(RootA.Name, StateA.Name));
				AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
				Exec.LogClear();

				// On FinishTask, go to StateB but the condition will fail. Reselect a new StateA.
				Status = Exec.Tick(0.1f);
				AITEST_TRUE(TEXT("MetaStory Active States should be in RootA/StateA"),
					Exec.ExpectInActiveStates(RootA.Name, StateA.Name));
				AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Expect the output tasks"), Exec.Expect("StateATaskA", TEXT("Tick101"))
					.Then("StateATaskA", TEXT("Exitstate102"))
					.Then("StateATaskA", TEXT("EnterState101"))
				);
				Exec.LogClear();

				Exec.Stop();
			}
		}
		// Test finish in exit state
		{
			{
				StateATask.GetNode().CustomTickFunc = [](FMetaStoryExecutionContext& Context, const FTestTask_PrintValue* Task)
					{
						Context.FinishTask(*Task, EMetaStoryFinishTaskType::Succeeded);
						FTestTask_PrintValue::FInstanceDataType& InstanceData = Context.GetInstanceData(*Task);
						++InstanceData.Value;
					};
				StateATask.GetNode().CustomExitStateFunc = [](FMetaStoryExecutionContext& Context, const FTestTask_PrintValue* Task)
					{
						Context.FinishTask(*Task, EMetaStoryFinishTaskType::Succeeded);
						FTestTask_PrintValue::FInstanceDataType& InstanceData = Context.GetInstanceData(*Task);
						++InstanceData.Value;
					};

				FMetaStoryCompilerLog Log;
				FMetaStoryCompiler Compiler(Log);
				const bool bResult = Compiler.Compile(MetaStory);
				AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
			}
			{
				EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
				FMetaStoryInstanceData InstanceData;
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

				// Start and enter state
				Status = Exec.Start();

				AITEST_TRUE(TEXT("MetaStory Active States should be in RootA/StateA"),
					Exec.ExpectInActiveStates(RootA.Name, StateA.Name));
				AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
				Exec.LogClear();

				// One FinishTask in exit but should not close StateA again. It should loop to StateA
				Status = Exec.Tick(0.1f);
				AITEST_TRUE(TEXT("MetaStory Active States should be in RootA/StateA"),
					Exec.ExpectInActiveStates(RootA.Name, StateA.Name));
				AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Expect the output tasks"), Exec.Expect("StateATaskA", TEXT("Tick101"))
					.Then("StateATaskA", TEXT("ExitState102"))
					.Then("StateATaskA", TEXT("EnterState101"))
				);
				AITEST_FALSE(TEXT("Expect the output tasks"), Exec.Expect("StateATaskA", TEXT("ExitState103")));
				Exec.LogClear();

				Exec.Stop();
			}
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_FinishTasks, "System.MetaStory.FinishTask");

// Test nested tree overrides
struct FMetaStoryTest_NestedOverride : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FMetaStoryCompilerLog Log;
		
		const FGameplayTag Tag = GetTestTag1();
		const FGameplayTag Tag2 = GetTestTag2();

		// Asset 2
		UMetaStory& MetaStory2 = NewMetaStory();
		UMetaStoryEditorData& EditorData2 = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
		FInstancedPropertyBag& RootPropertyBag2 = GetRootPropertyBag(EditorData2);
		RootPropertyBag2.AddProperty(FName(TEXT("Int")), EPropertyBagPropertyType::Int32);
		UMetaStoryState& Root2 = EditorData2.AddSubTree(FName(TEXT("Root2")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& TaskRoot2 = Root2.AddTask<FTestTask_Stand>(FName(TEXT("TaskRoot2")));
		{
			FMetaStoryCompiler Compiler2(Log);
			const bool bResult2 = Compiler2.Compile(MetaStory2);
			AITEST_TRUE("MetaStory2 should get compiled", bResult2);
		}
		
		// Asset 3
		UMetaStory& MetaStory3 = NewMetaStory();
		UMetaStoryEditorData& EditorData3 = *Cast<UMetaStoryEditorData>(MetaStory3.EditorData);
		FInstancedPropertyBag& RootPropertyBag3 = GetRootPropertyBag(EditorData3);
		RootPropertyBag3.AddProperty(FName(TEXT("Float")), EPropertyBagPropertyType::Float); // Different parameters
		UMetaStoryState& Root3 = EditorData3.AddSubTree(FName(TEXT("Root3")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& TaskRoot3 = Root3.AddTask<FTestTask_Stand>(FName(TEXT("TaskRoot3")));
		{
			FMetaStoryCompiler Compiler3(Log);
			const bool bResult3 = Compiler3.Compile(MetaStory3);
			AITEST_TRUE(TEXT("MetaStory3 should get compiled"), bResult3);
		}
		// Wrong Asset 4
		UMetaStory* MetaStory4 = NewObject<UMetaStory>(&GetWorld());;
		{
			UMetaStoryEditorData* EditorData = NewObject<UMetaStoryEditorData>(MetaStory4);
			check(EditorData);
			MetaStory4->EditorData = EditorData;
			EditorData->Schema = NewObject<UMetaStoryTestSchema2>();

			FInstancedPropertyBag& RootPropertyBag = GetRootPropertyBag(*EditorData);
			RootPropertyBag.AddProperty(FName(TEXT("Float")), EPropertyBagPropertyType::Float); // Different parameters
			UMetaStoryState& Root4 = EditorData->AddSubTree(FName(TEXT("Root4")));
			TMetaStoryTypedEditorNode<FTestTask_Stand>& TaskRoot4 = Root3.AddTask<FTestTask_Stand>(FName(TEXT("TaskRoot4")));

			FMetaStoryCompiler Compiler4(Log);
			const bool bResult4 = Compiler4.Compile(*MetaStory4);
			AITEST_TRUE(TEXT("MetaStory4 should get compiled"), bResult4);
		}

		// Main asset
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		FInstancedPropertyBag& RootPropertyBag = GetRootPropertyBag(EditorData);

		RootPropertyBag.AddProperty(FName(TEXT("Int")), EPropertyBagPropertyType::Int32);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root1")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A1")), EMetaStoryStateType::LinkedAsset);
		StateA.Tag = Tag;
		StateA.SetLinkedStateAsset(&MetaStory2);

		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		// Without overrides
		{
			EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

			Status = Exec.Start();
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("MetaStory should enter TaskRoot2"), Exec.Expect(TaskRoot2.GetName(), EnterStateStr));

			Exec.Stop();
		}

		// With overrides
		{
			EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
			FMetaStoryInstanceData InstanceData;

			FMetaStoryReferenceOverrides Overrides;
			FMetaStoryReference OverrideRef;
			OverrideRef.SetMetaStory(&MetaStory3);
			Overrides.AddOverride(Tag, OverrideRef);
			
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			Exec.SetLinkedMetaStoryOverrides(Overrides);
			
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("MetaStory should init", bInitSucceeded);
			
			Status = Exec.Start();
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("MetaStory should enter TaskRoot3"), Exec.Expect(TaskRoot3.GetName(), EnterStateStr));
			AITEST_FALSE(TEXT("MetaStory should not enter TaskRoot2"), Exec.Expect(TaskRoot2.GetName(), EnterStateStr));

			Exec.Stop();
		}

		// With wrong overrides
		{
			EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
			FMetaStoryInstanceData InstanceData;

			FMetaStoryReferenceOverrides Overrides;
			FMetaStoryReference OverrideRef3;
			OverrideRef3.SetMetaStory(&MetaStory3);
			Overrides.AddOverride(Tag, OverrideRef3);
			FMetaStoryReference OverrideRef4;
			OverrideRef4.SetMetaStory(MetaStory4);
			Overrides.AddOverride(Tag2, OverrideRef4);

			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);

			GetTestRunner().AddExpectedMessage(TEXT("their schemas don't match"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1, false);
			Exec.SetLinkedMetaStoryOverrides(Overrides);
			AITEST_TRUE(TEXT("Start should complete with Running"), GetTestRunner().HasMetExpectedErrors());

			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

			Status = Exec.Start();
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("MetaStory should enter TaskRoot2"), Exec.Expect(TaskRoot2.GetName(), EnterStateStr));
			AITEST_FALSE(TEXT("MetaStory should not enter TaskRoot3"), Exec.Expect(TaskRoot3.GetName(), EnterStateStr));

			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_NestedOverride, "System.MetaStory.NestedOverride");

// Test parallel tree event priority handling.
struct FMetaStoryTest_RecursiveParallelTask : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//	Root (with task that runs Tree 1)

		UMetaStory& MetaStory1 = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData1 = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);
			UMetaStoryState* Root1 = &EditorData1.AddSubTree("Tree1StateRoot");

			TMetaStoryTypedEditorNode<FMetaStoryRunParallelMetaStoryTask>& GlobalTask = EditorData1.AddGlobalTask<FMetaStoryRunParallelMetaStoryTask>();
			GlobalTask.GetInstanceData().MetaStory.SetMetaStory(&MetaStory1);

			TMetaStoryTypedEditorNode<FTestTask_PrintValue>& RootTask = Root1->AddTask<FTestTask_PrintValue>();
			RootTask.GetInstanceData().Value = 101;
		}
		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory1);
			AITEST_TRUE(TEXT("MetaStoryPar should get compiled"), bResult);
		}
		{
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

			{
 				GetTestRunner().AddExpectedErrorPlain(TEXT("Trying to start a new parallel tree from the same tree"), EAutomationExpectedErrorFlags::Contains, 1);

				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL(TEXT("Start should complete with failed"), Status, EMetaStoryRunStatus::Failed);
				AITEST_TRUE(TEXT(""), GetTestRunner().HasMetExpectedMessages());
			}
		}
		return true;
	}
};

IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_RecursiveParallelTask, "System.MetaStory.RecursiveParallelTask");

// Test parallel tree event priority handling.
struct FMetaStoryTest_ParallelEventPriority : FMetaStoryTestBase
{
	EMetaStoryTransitionPriority ParallelTreePriority = EMetaStoryTransitionPriority::Normal;
	
	virtual bool InstantTest() override
	{
		FMetaStoryCompilerLog Log;
		
		const FGameplayTag EventTag = GetTestTag1();

		// Parallel tree
		// - Root
		//   - State1 ?-> State2
		//   - State2
		UMetaStory& MetaStoryPar = NewMetaStory();
		UMetaStoryEditorData& EditorDataPar = *Cast<UMetaStoryEditorData>(MetaStoryPar.EditorData);

		UMetaStoryState& RootPar = EditorDataPar.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State1 = RootPar.AddChildState(FName(TEXT("State1")));
		UMetaStoryState& State2 = RootPar.AddChildState(FName(TEXT("State2")));

		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 100;
		State1.AddTransition(EMetaStoryTransitionTrigger::OnEvent, EventTag, EMetaStoryTransitionType::NextState);

		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task2 = State2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		Task2.GetNode().TicksToCompletion = 100;

		{
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStoryPar);
			AITEST_TRUE(TEXT("MetaStoryPar should get compiled"), bResult);
		}

		// Main asset
		// - Root [MetaStoryPar]
		//   - State3 ?-> State4
		//   - State4
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State3 = Root.AddChildState(FName(TEXT("State3")));
		UMetaStoryState& State4 = Root.AddChildState(FName(TEXT("State4")));

		TMetaStoryTypedEditorNode<FMetaStoryRunParallelMetaStoryTask>& TaskPar = Root.AddTask<FMetaStoryRunParallelMetaStoryTask>();
		TaskPar.GetNode().SetEventHandlingPriority(ParallelTreePriority);
		
		TaskPar.GetInstanceData().MetaStory.SetMetaStory(&MetaStoryPar);

		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task3 = State3.AddTask<FTestTask_Stand>(FName(TEXT("Task3")));
		Task3.GetNode().TicksToCompletion = 100;
		State3.AddTransition(EMetaStoryTransitionTrigger::OnEvent, EventTag, EMetaStoryTransitionType::NextState);

		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task4 = State4.AddTask<FTestTask_Stand>(FName(TEXT("Task4")));
		Task4.GetNode().TicksToCompletion = 100;
		
		{
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory);
			AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
		}

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		// Run MetaStoryPar in parallel with the main tree.
		// Both trees have a transition on same event.
		// Setting the priority to Low, should make the main tree to take the transition.
		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
		AITEST_TRUE(TEXT("MetaStory should enter Task1, Task3"), Exec.Expect(Task1.GetName(), EnterStateStr).Then(Task3.GetName(), EnterStateStr));
		Exec.LogClear();

		Status = Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
		AITEST_TRUE(TEXT("MetaStory should tick Task1, Task3"), Exec.Expect(Task1.GetName(), TickStr).Then(Task3.GetName(), TickStr));
		Exec.LogClear();

		Exec.SendEvent(EventTag);

		// If the parallel tree priority is < Normal, then it should always be handled after the main tree.
		// If the parallel tree priority is Normal, then the state order decides (leaf to root)
		// If the parallel tree priority is > Normal, then it should always be handled before the main tree.
		if (ParallelTreePriority <= EMetaStoryTransitionPriority::Normal)
		{
			// Main tree should do the transition.
			Status = Exec.Tick(0.1f);
			AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("MetaStory should enter Task4"), Exec.Expect(Task4.GetName(), EnterStateStr));
			Exec.LogClear();

			Status = Exec.Tick(0.1f);
			AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("MetaStory should tick Task1, Task4"), Exec.Expect(Task1.GetName(), TickStr).Then(Task4.GetName(), TickStr));
			Exec.LogClear();
		}
		else
		{
			// Parallel tree should do the transition.
			Status = Exec.Tick(0.1f);
			AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("MetaStory should enter Task2"), Exec.Expect(Task2.GetName(), EnterStateStr));
			Exec.LogClear();

			Status = Exec.Tick(0.1f);
			AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("MetaStory should tick Task2, Task3"), Exec.Expect(Task2.GetName(), TickStr).Then(Task3.GetName(), TickStr));
			Exec.LogClear();
		}

		Exec.Stop();
		
		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_ParallelEventPriority, "System.MetaStory.ParallelEventPriority");


struct FMetaStoryTest_ParallelEventPriority_Low : FMetaStoryTest_ParallelEventPriority
{
	FMetaStoryTest_ParallelEventPriority_Low()
	{
		ParallelTreePriority = EMetaStoryTransitionPriority::Low;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_ParallelEventPriority_Low, "System.MetaStory.ParallelEventPriority.Low");

struct FMetaStoryTest_ParallelEventPriority_High : FMetaStoryTest_ParallelEventPriority
{
	FMetaStoryTest_ParallelEventPriority_High()
	{
		ParallelTreePriority = EMetaStoryTransitionPriority::High;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_ParallelEventPriority_High, "System.MetaStory.ParallelEventPriority.High");

struct FMetaStoryTest_SubTreeTransition : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		/*
		- Root
			- PreLastStand [Task1] -> Reinforcements
				- BusinessAsUsual [Task2]
			- LastStand [Task3]
				- Reinforcements>TimeoutChecker
			- (f)TimeoutChecker
				- RemainingCount [Task4]
		*/
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		UMetaStoryState& PreLastStand = Root.AddChildState(FName(TEXT("PreLastStand")));
		UMetaStoryState& BusinessAsUsual = PreLastStand.AddChildState(FName(TEXT("BusinessAsUsual")));

		UMetaStoryState& LastStand = Root.AddChildState(FName(TEXT("LastStand")));
		UMetaStoryState& Reinforcements = LastStand.AddChildState(FName(TEXT("Reinforcements")), EMetaStoryStateType::Linked);
		
		UMetaStoryState& TimeoutChecker = LastStand.AddChildState(FName(TEXT("TimeoutChecker")), EMetaStoryStateType::Subtree);
		UMetaStoryState& RemainingCount = TimeoutChecker.AddChildState(FName(TEXT("RemainingCount")));

		Reinforcements.SetLinkedState(TimeoutChecker.GetLinkToState());


		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task1 = PreLastStand.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		PreLastStand.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &Reinforcements);
		Task1.GetInstanceData().Value = 1; // This should finish before the child state

		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task2 = BusinessAsUsual.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		Task2.GetInstanceData().Value = 2;

		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task3 = LastStand.AddTask<FTestTask_Stand>(FName(TEXT("Task3")));
		Task3.GetInstanceData().Value = 2;

		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task4 = LastStand.AddTask<FTestTask_Stand>(FName(TEXT("Task4")));
		Task4.GetInstanceData().Value = 2;

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr(TEXT("Tick"));
		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));
		const TCHAR* StateCompletedStr(TEXT("StateCompleted"));

		// Start and enter state
		Status = Exec.Start();

		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/PreLastStand/BusinessAsUsual"), Exec.ExpectInActiveStates(Root.Name, PreLastStand.Name, BusinessAsUsual.Name));
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task2 should enter state"), Exec.Expect(Task2.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		// Transition to Reinforcements
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/LastStand/Reinforcements/TimeoutChecker/RemainingCount"), Exec.ExpectInActiveStates(Root.Name, LastStand.Name, Reinforcements.Name, TimeoutChecker.Name, RemainingCount.Name));
		AITEST_TRUE(TEXT("MetaStory Task3 should enter state"), Exec.Expect(Task3.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task4 should enter state"), Exec.Expect(Task4.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory should be running"), Status == EMetaStoryRunStatus::Running);
		Exec.LogClear();

		Exec.Stop();
		
		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_SubTreeTransition, "System.MetaStory.SubTreeTransition");

struct FMetaStoryTest_Reentrant : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree1
		//	-Root
		//		- State1
		//			- State2: OnComplete -> State1

		UMetaStory& MetaStory1 = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);
			UMetaStoryState& Root1 = EditorData.AddSubTree(FName(TEXT("Tree1Root")));
			UMetaStoryState& State1 = Root1.AddChildState(FName(TEXT("Tree1State1")));
			UMetaStoryState& State2 = State1.AddChildState(FName(TEXT("Tree1State2")));

			State2.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &State1);
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& State1Task1 = State1.AddTask<FTestTask_PrintValue>("Tree1State1Task1");
				State1Task1.GetInstanceData().Value = 101;
				State1Task1.GetInstanceData().TickRunStatus = EMetaStoryRunStatus::Running;

			}
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& State2Task1 = State2.AddTask<FTestTask_PrintValue>("Tree1State2Task1");
				State2Task1.GetInstanceData().Value = 201;
				State2Task1.GetInstanceData().TickRunStatus = EMetaStoryRunStatus::Succeeded;
			}
			{
				FMetaStoryCompilerLog Log;
				FMetaStoryCompiler Compiler(Log);
				const bool bResult = Compiler.Compile(MetaStory1);
				AITEST_TRUE("MetaStory should get compiled", bResult);
			}
		}
		{
			FMetaStoryInstanceData InstanceData;
			{
				FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE("MetaStory should init", bInitSucceeded);

				// Start and enter state
				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_TRUE("MetaStory Active States should be in Tree1Root/Tree1State1/Tree1State2",
					Exec.ExpectInActiveStates("Tree1Root", "Tree1State1", "Tree1State2"));
				AITEST_TRUE("MetaStory should be running", Status == EMetaStoryRunStatus::Running);
				Exec.LogClear();
			}
			{
				// On go to State1, reselect State1 and State2. It's a new State2 same State1.
				FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);

				UE::MetaStory::FActiveStatePath BeforeStatePath = InstanceData.GetExecutionState()->GetActiveStatePath();

				EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_TRUE("MetaStory Active States should be in Tree1Root/Tree1State1/Tree1State2",
					Exec.ExpectInActiveStates("Tree1Root", "Tree1State1", "Tree1State2"));
				AITEST_TRUE("MetaStory should be running", Status == EMetaStoryRunStatus::Running);
				AITEST_TRUE("Expect the output tasks", Exec.Expect("Tree1State1Task1", TEXT("Tick101"))
					.Then("Tree1State2Task1", TEXT("Tick201"))
					.Then("Tree1State2Task1", TEXT("ExitState201"))
					.Then("Tree1State2Task1", TEXT("ExitState=Changed"))
					.Then("Tree1State1Task1", TEXT("ExitState101"))
					.Then("Tree1State1Task1", TEXT("ExitState=Sustained"))
					.Then("Tree1State1Task1", TEXT("EnterState101"))
					.Then("Tree1State1Task1", TEXT("EnterState=Sustained"))
					.Then("Tree1State2Task1", TEXT("EnterState201"))
					.Then("Tree1State2Task1", TEXT("EnterState=Changed"))
				);


				UE::MetaStory::FActiveStatePath AfterStatePath = InstanceData.GetExecutionState()->GetActiveStatePath();

				AITEST_TRUE(TEXT("Same length"), AfterStatePath.Num() == BeforeStatePath.Num());
				AITEST_TRUE(TEXT("Length is 3"), AfterStatePath.Num() == 3);
				AITEST_TRUE(TEXT("Element 1 is the same"), AfterStatePath.GetView()[0] == BeforeStatePath.GetView()[0]);
				AITEST_TRUE(TEXT("Element 2 is the same"), AfterStatePath.GetView()[1] == BeforeStatePath.GetView()[1]);
				AITEST_TRUE(TEXT("Element 3 is different"), AfterStatePath.GetView()[2] != BeforeStatePath.GetView()[2]);

				Exec.LogClear();
			}
			{
				FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
				Exec.Stop();
			}
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Reentrant, "System.MetaStory.Reentrant");

} // namespace UE::MetaStory::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE

