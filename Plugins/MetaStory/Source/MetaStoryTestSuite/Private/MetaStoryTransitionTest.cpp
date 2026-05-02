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

struct FMetaStoryTest_TransitionPriority : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		/*
			- Root
				- State1 : Task1 -> Succeeded
					- State1A : Task1A -> Next
					- State1B : Task1B -> Next
					- State1C : Task1C
		
			Task1A completed first, transitioning to State1B.
			Task1, Task1B, and Task1C complete at the same time, we should take the transition on the first completed state (State1).
		*/
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UMetaStoryState& State1A = State1.AddChildState(FName(TEXT("State1A")));
		UMetaStoryState& State1B = State1.AddChildState(FName(TEXT("State1B")));
		UMetaStoryState& State1C = State1.AddChildState(FName(TEXT("State1C")));

		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 2;
		State1.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);
		
		auto& Task1A = State1A.AddTask<FTestTask_Stand>(FName(TEXT("Task1A")));
		Task1A.GetNode().TicksToCompletion = 1;
		State1A.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::NextState);

		auto& Task1B = State1B.AddTask<FTestTask_Stand>(FName(TEXT("Task1B")));
		Task1B.GetNode().TicksToCompletion = 2;
		State1B.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::NextState);

		auto& Task1C = State1C.AddTask<FTestTask_Stand>(FName(TEXT("Task1C")));
		Task1C.GetNode().TicksToCompletion = 2;
		
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
		AITEST_TRUE(TEXT("MetaStory Task1A should enter state"), Exec.Expect(Task1A.GetName(), EnterStateStr));
		Exec.LogClear();

		// Transition from Task1A to Task1B
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task1A should complete"), Exec.Expect(Task1A.GetName(), StateCompletedStr));
		AITEST_TRUE(TEXT("MetaStory Task1B should enter state"), Exec.Expect(Task1B.GetName(), EnterStateStr));
		Exec.LogClear();

		// Task1 completes, and we should take State1 transition. 
		Status = Exec.Tick(0.1f);
		AITEST_TRUE("MetaStory Task1 should complete", Exec.Expect(Task1.GetName(), StateCompletedStr));
		AITEST_EQUAL(TEXT("Tree execution should stop on success"), Status, EMetaStoryRunStatus::Succeeded);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionPriority, "System.MetaStory.Transition.Priority");

struct FMetaStoryTest_TransitionPriorityEnterState : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		
		UMetaStoryState& Root =	EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State0 = Root.AddChildState(FName(TEXT("State0")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UMetaStoryState& State1A = State1.AddChildState(FName(TEXT("State1A")));
		UMetaStoryState& State2 = Root.AddChildState(FName(TEXT("State2")));
		UMetaStoryState& State3 = Root.AddChildState(FName(TEXT("State3")));

		auto& Task0 = State0.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		State0.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &State1);

		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().EnterStateResult = EMetaStoryRunStatus::Failed;
		State1.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &State2);
		
		auto& Task1A = State1A.AddTask<FTestTask_Stand>(FName(TEXT("Task1A")));
		State1A.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &State3);

		auto& Task2 = State2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		State2.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);

		auto& Task3 = State3.AddTask<FTestTask_Stand>(FName(TEXT("Task3")));
		State3.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);

		
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
		AITEST_TRUE(TEXT("MetaStory Task0 should enter state"), Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// Transition from State0 to State1, it should fail (Task1), and the transition on State1->State2 (and not State1A->State3)
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task0 should complete"), Exec.Expect(Task0.GetName(), StateCompletedStr));
		AITEST_TRUE(TEXT("MetaStory Task2 should enter state"), Exec.Expect(Task2.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory Task3 should not enter state"), Exec.Expect(Task3.GetName(), EnterStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionPriorityEnterState, "System.MetaStory.Transition.PriorityEnterState");

struct FMetaStoryTest_TransitionNextSelectableState : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		// Root
		//  State0: OnCompleted->Nextselectable
		//  State1: EnterCondition fails
		//  State2: EnterCondition fails
		//  State3: EnterCondition successed. OnCompleted->Succeeded
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root =	EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State0 = Root.AddChildState(FName(TEXT("State0")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UMetaStoryState& State2 = Root.AddChildState(FName(TEXT("State2")));
		UMetaStoryState& State3 = Root.AddChildState(FName(TEXT("State3")));

		auto& EvalA = EditorData.AddEvaluator<FTestEval_A>();
		EvalA.GetInstanceData().bBoolA = true;

		auto& Task0 = State0.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		State0.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::NextSelectableState);

		// Add Task 1 with Condition that will always fail
		auto& Task1 = State1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		auto& BoolCond1 = State1.AddEnterCondition<FMetaStoryCompareBoolCondition>();

		EditorData.AddPropertyBinding(EvalA, TEXT("bBoolA"), BoolCond1, TEXT("bLeft"));
		BoolCond1.GetInstanceData().bRight = !EvalA.GetInstanceData().bBoolA;

		// Add Task 2 with Condition that will always fail
		auto& Task2 = State2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		auto& BoolCond2 = State2.AddEnterCondition<FMetaStoryCompareBoolCondition>();

		EditorData.AddPropertyBinding(EvalA, TEXT("bBoolA"), BoolCond2, TEXT("bLeft"));
		BoolCond2.GetInstanceData().bRight = !EvalA.GetInstanceData().bBoolA;

		// Add Task 3 with Condition that will always succeed
		auto& Task3 = State3.AddTask<FTestTask_Stand>(FName(TEXT("Task3")));
		auto& BoolCond3 = State3.AddEnterCondition<FMetaStoryCompareBoolCondition>();
		State3.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);

		EditorData.AddPropertyBinding(EvalA, TEXT("bBoolA"), BoolCond3, TEXT("bLeft"));
		BoolCond3.GetInstanceData().bRight = EvalA.GetInstanceData().bBoolA;

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr = TEXT("Tick");
		const TCHAR* EnterStateStr = TEXT("EnterState");
		const TCHAR* ExitStateStr = TEXT("ExitState");
		const TCHAR* StateCompletedStr = TEXT("StateCompleted");

		// Start and enter state
		Exec.Start();
		AITEST_TRUE(TEXT("Should be in active state"), Exec.ExpectInActiveStates("Root", "State0"));
		AITEST_TRUE(TEXT("MetaStory Task0 should enter state"), Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// Transition from State0 and tries to select State1. It should fail (Task1 and Tasks2) and because transition is set to "Next Selectable", it should now select Task 3 and Enter State
		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("Should be in active state"), Exec.ExpectInActiveStates("Root", "State3"));
		AITEST_TRUE(TEXT("MetaStory Task0 should complete"), Exec.Expect(Task0.GetName(), StateCompletedStr));
		AITEST_FALSE(TEXT("MetaStory Task1 should not enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory Task2 should not enter state"), Exec.Expect(Task2.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory Task3 should enter state"), Exec.Expect(Task3.GetName(), EnterStateStr));
		Exec.LogClear();

		// Complete Task3
		EMetaStoryRunStatus Status = Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("Tick should succeed"), Status, EMetaStoryRunStatus::Succeeded);
		AITEST_TRUE(TEXT("Should be in active state"), Exec.GetActiveStateNames().Num() == 0);
		AITEST_TRUE(TEXT("MetaStory Task3 should complete"), Exec.Expect(Task3.GetName(), StateCompletedStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionNextSelectableState, "System.MetaStory.Transition.NextSelectableState");

struct FMetaStoryTest_TransitionNextWithParentData : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root =	EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State0 = Root.AddChildState(FName(TEXT("State0")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UMetaStoryState& State1A = State1.AddChildState(FName(TEXT("State1A")));

		auto& RootTask = Root.AddTask<FTestTask_B>(FName(TEXT("RootTask")));
		RootTask.GetInstanceData().bBoolB = true;

		auto& Task0 = State0.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		State0.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::NextState);

		auto& Task1A = State1A.AddTask<FTestTask_Stand>(FName(TEXT("Task1A")));
		auto& BoolCond1 = State1A.AddEnterCondition<FMetaStoryCompareBoolCondition>();

		EditorData.AddPropertyBinding(RootTask, TEXT("bBoolB"), BoolCond1, TEXT("bLeft"));
		BoolCond1.GetInstanceData().bRight = true;

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

		// Start and enter state
		Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Task0 should enter state"), Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// Transition from State0 and tries to select State1.
		// This tests that data from current shared active states (Root) is available during state selection.
		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task0 should complete"), Exec.Expect(Task0.GetName(), StateCompletedStr));
		AITEST_TRUE(TEXT("MetaStory Task1A should enter state"), Exec.Expect(Task1A.GetName(), EnterStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionNextWithParentData, "System.MetaStory.Transition.NextWithParentData");

struct FMetaStoryTest_TransitionGlobalDataView : FMetaStoryTestBase
{
	// Tests that the global eval and task dataviews are kept up to date when transitioning from  
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		UMetaStoryState& StateB = Root.AddChildState(FName(TEXT("B")));

		auto& EvalA = EditorData.AddEvaluator<FTestEval_A>(FName(TEXT("Eval")));
		EvalA.GetInstanceData().IntA = 42;
		auto& GlobalTask = EditorData.AddGlobalTask<FTestTask_PrintValue>(FName(TEXT("Global")));
		GlobalTask.GetInstanceData().Value = 123;
		
		// State A
		auto& Task0 = StateA.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		StateA.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &StateB);

		// State B
		auto& Task1 = StateB.AddTask<FTestTask_PrintValue>(FName(TEXT("Task1")));
		EditorData.AddPropertyBinding(EvalA, TEXT("IntA"), Task1, TEXT("Value"));
		auto& Task2 = StateB.AddTask<FTestTask_PrintValue>(FName(TEXT("Task2")));
		EditorData.AddPropertyBinding(GlobalTask, TEXT("Value"), Task2, TEXT("Value"));

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* EnterState42Str(TEXT("EnterState42"));
		const TCHAR* EnterState123Str(TEXT("EnterState123"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Task0 should enter state"), Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// Transition from StateA to StateB, Task0 should enter state with evaluator value copied.
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task0 should enter state with value 42"), Exec.Expect(Task1.GetName(), EnterState42Str));
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state with value 123"), Exec.Expect(Task2.GetName(), EnterState123Str));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionGlobalDataView, "System.MetaStory.Transition.GlobalDataView");

struct FMetaStoryTest_TransitionDelay : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		const FGameplayTag Tag = GetTestTag1();

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		UMetaStoryState& StateB = Root.AddChildState(FName(TEXT("B")));

		// State A
		auto& Task0 = StateA.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		Task0.GetNode().TicksToCompletion = 100;
		
		FMetaStoryTransition& Transition = StateA.AddTransition(EMetaStoryTransitionTrigger::OnEvent, EMetaStoryTransitionType::GotoState, &StateB);
		Transition.bDelayTransition = true;
		Transition.DelayDuration = 0.15f;
		Transition.DelayRandomVariance = 0.0f;
		Transition.RequiredEvent.Tag = Tag;

		// State B
		auto& Task1 = StateB.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 100;

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
		AITEST_TRUE(TEXT("MetaStory Task0 should enter state"), Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// This should cause delayed transition.
		Exec.SendEvent(Tag);
		
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task0 should tick"), Exec.Expect(Task0.GetName(), TickStr));
		Exec.LogClear();

		// Should have execution frames
		AITEST_TRUE(TEXT("Should have active frames"), InstanceData.GetExecutionState()->ActiveFrames.Num() > 0);

		// Should have delayed transitions
		const int32 NumDelayedTransitions0 = InstanceData.GetExecutionState()->DelayedTransitions.Num();
		AITEST_EQUAL(TEXT("Should have a delayed transition"), NumDelayedTransitions0, 1);

		// Tick and expect a delayed transition. 
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task0 should tick"), Exec.Expect(Task0.GetName(), TickStr));
		Exec.LogClear();

		const int32 NumDelayedTransitions1 = InstanceData.GetExecutionState()->DelayedTransitions.Num();
		AITEST_EQUAL(TEXT("Should have a delayed transition"), NumDelayedTransitions1, 1);

		// Should complete delayed transition.
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task0 should exit state"), Exec.Expect(Task0.GetName(), ExitStateStr));
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionDelay, "System.MetaStory.Transition.Delay");

struct FMetaStoryTest_TransitionDelayZero : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		const FGameplayTag Tag = GetTestTag1();

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		UMetaStoryState& StateB = Root.AddChildState(FName(TEXT("B")));

		// State A
		auto& Task0 = StateA.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		Task0.GetNode().TicksToCompletion = 100;
		
		FMetaStoryTransition& Transition = StateA.AddTransition(EMetaStoryTransitionTrigger::OnEvent, EMetaStoryTransitionType::GotoState, &StateB);
		Transition.bDelayTransition = true;
		Transition.DelayDuration = 0.0f;
		Transition.DelayRandomVariance = 0.0f;
		Transition.RequiredEvent.Tag = Tag;

		// State B
		auto& Task1 = StateB.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 100;

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

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Task0 should enter state"), Exec.Expect(Task0.GetName(), EnterStateStr));
		Exec.LogClear();

		// This should cause delayed transition. Because the time is 0, it should happen immediately.
		Exec.SendEvent(Tag);
		
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Task0 should exit state"), Exec.Expect(Task0.GetName(), ExitStateStr));
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionDelayZero, "System.MetaStory.Transition.DelayZero");

struct FMetaStoryTest_PassingTransitionEventToStateSelection : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		FPropertyBindingPath PathToPayloadMember;
		{
			const bool bParseResult = PathToPayloadMember.FromString(TEXT("Payload.A"));

			AITEST_TRUE(TEXT("Parsing path should succeeed"), bParseResult);

			FMetaStoryEvent EventWithPayload;
			EventWithPayload.Payload = FInstancedStruct::Make<FMetaStoryTest_PropertyStructA>();
			const bool bUpdateSegments = PathToPayloadMember.UpdateSegmentsFromValue(FMetaStoryDataView(FStructView::Make(EventWithPayload)));
			AITEST_TRUE(TEXT("Updating segments should succeeed"), bUpdateSegments);
		}

		// This state shouldn't be selected, because transition's condition and state's enter condition exlude each other.
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		StateA.bHasRequiredEventToEnter  = true;
		StateA.RequiredEventToEnter.PayloadStruct = FMetaStoryTest_PropertyStructA::StaticStruct();
		auto& TaskA = StateA.AddTask<FTestTask_Stand>(FName(TEXT("TaskA")));
		TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& AIntCond = StateA.AddEnterCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Equal);
		AIntCond.GetInstanceData().Right = 0;
		EditorData.AddPropertyBinding(
			FPropertyBindingPath(StateA.GetEventID(), PathToPayloadMember.GetSegments()),
			FPropertyBindingPath(AIntCond.ID, TEXT("Left")));

		// This state should be selected as the sent event fullfils both transition's condition and state's enter condition.
		UMetaStoryState& StateB = Root.AddChildState(FName(TEXT("B")));
		StateB.bHasRequiredEventToEnter  = true;
		StateB.RequiredEventToEnter.PayloadStruct = FMetaStoryTest_PropertyStructA::StaticStruct();
		auto& TaskB = StateB.AddTask<FTestTask_PrintValue>(FName(TEXT("TaskB")));
		// Test copying data from the state event. The condition properties are copied from temp instance data during selection, this gets copied from active instance data.
		TaskB.GetInstanceData().Value = -1; // Initially -1, expected to be overridden by property binding below. 
		EditorData.AddPropertyBinding(
			FPropertyBindingPath(StateB.GetEventID(), PathToPayloadMember.GetSegments()),
			FPropertyBindingPath(TaskB.ID, TEXT("Value")));
		
		TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& BIntCond = StateB.AddEnterCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Equal);
		BIntCond.GetInstanceData().Right = 1;
		EditorData.AddPropertyBinding(
			FPropertyBindingPath(StateB.GetEventID(), PathToPayloadMember.GetSegments()),
			FPropertyBindingPath(BIntCond.ID, TEXT("Left")));

		// This state should be selected only initially when there's not event in the queue.
		UMetaStoryState& StateInitial = Root.AddChildState(FName(TEXT("Initial")));
		auto& TaskInitial = StateInitial.AddTask<FTestTask_Stand>(FName(TEXT("TaskInitial")));
		// Transition from Initial -> StateA
		FMetaStoryTransition& TransA = StateInitial.AddTransition(EMetaStoryTransitionTrigger::OnEvent, FGameplayTag(), EMetaStoryTransitionType::GotoState, &StateA);
		TransA.RequiredEvent.PayloadStruct = FMetaStoryTest_PropertyStructA::StaticStruct();
		TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& TransAIntCond = TransA.AddCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Equal);
		TransAIntCond.GetInstanceData().Right = 1;
		EditorData.AddPropertyBinding(
			FPropertyBindingPath(TransA.GetEventID(), PathToPayloadMember.GetSegments()),
			FPropertyBindingPath(TransAIntCond.ID, TEXT("Left")));
		// Transition from Initial -> StateB
		FMetaStoryTransition& TransB = StateInitial.AddTransition(EMetaStoryTransitionTrigger::OnEvent, FGameplayTag(), EMetaStoryTransitionType::GotoState, &StateB);
		TransB.RequiredEvent.PayloadStruct = FMetaStoryTest_PropertyStructA::StaticStruct();
		TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& TransBIntCond = TransB.AddCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Equal);
		TransBIntCond.GetInstanceData().Right = 1;
		EditorData.AddPropertyBinding(
			FPropertyBindingPath(TransB.GetEventID(), PathToPayloadMember.GetSegments()),
			FPropertyBindingPath(TransBIntCond.ID, TEXT("Left")));

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* EnterStateStr(TEXT("EnterState"));

		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory TaskInitial should enter state"), Exec.Expect(TaskInitial.GetName(), EnterStateStr));
		Exec.LogClear();

		// The conditions test for payload Value=1, the first event should not trigger transition. 
		Exec.SendEvent(GetTestTag1(), FConstStructView::Make(FMetaStoryTest_PropertyStructA{0}));
		Exec.SendEvent(GetTestTag1(), FConstStructView::Make(FMetaStoryTest_PropertyStructA{1}));
		Status = Exec.Tick(0.1f);

		AITEST_FALSE(TEXT("MetaStory TaskA should not enter state"), Exec.Expect(TaskA.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory TaskB should enter state"), Exec.Expect(TaskB.GetName(), TEXT("EnterState1"))); // TaskB decorates "EnterState" with value from the payload.
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_PassingTransitionEventToStateSelection, "System.MetaStory.Transition.PassingTransitionEventToStateSelection");

struct FMetaStoryTest_FollowTransitions : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Root
		// Trans
		// A
		// B
		// C
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		FInstancedPropertyBag& RootPropertyBag = GetRootPropertyBag(EditorData);
		RootPropertyBag.AddProperty(FName(TEXT("Int")), EPropertyBagPropertyType::Int32);
		RootPropertyBag.SetValueInt32(FName(TEXT("Int")), 1);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateTrans = Root.AddChildState(FName(TEXT("Trans")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		UMetaStoryState& StateB = Root.AddChildState(FName(TEXT("B")));
		UMetaStoryState& StateC = Root.AddChildState(FName(TEXT("C")));

		// Root

		// Trans
		{
			StateTrans.SelectionBehavior = EMetaStoryStateSelectionBehavior::TryFollowTransitions;

			{
				// This transition should be skipped due to the condition
				FMetaStoryTransition& TransA = StateTrans.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &StateA);
				TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& TransIntCond = TransA.AddCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Equal);
				TransIntCond.GetInstanceData().Right = 0;
				EditorData.AddPropertyBinding(
					FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Int")),
					FPropertyBindingPath(TransIntCond.ID, TEXT("Left")));
			}

			{
				// This transition leads to selection, but will be overridden.
				FMetaStoryTransition& TransB = StateTrans.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &StateB);
				TransB.Priority = EMetaStoryTransitionPriority::Normal;
				TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& TransIntCond = TransB.AddCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Equal);
				TransIntCond.GetInstanceData().Right = 1;
				EditorData.AddPropertyBinding(
					FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Int")),
					FPropertyBindingPath(TransIntCond.ID, TEXT("Left")));
			}

			{
				// This transition is selected, should override previous one due to priority.
				FMetaStoryTransition& TransC = StateTrans.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &StateC);
				TransC.Priority = EMetaStoryTransitionPriority::High;
				TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& TransIntCond = TransC.AddCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Equal);
				TransIntCond.GetInstanceData().Right = 1;
				EditorData.AddPropertyBinding(
					FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Int")),
					FPropertyBindingPath(TransIntCond.ID, TEXT("Left")));
			}
		}

		auto& TaskA = StateA.AddTask<FTestTask_Stand>(FName(TEXT("TaskA")));
		auto& TaskB = StateB.AddTask<FTestTask_Stand>(FName(TEXT("TaskB")));
		auto& TaskC = StateC.AddTask<FTestTask_Stand>(FName(TEXT("TaskC")));

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr = TEXT("Tick");
		const TCHAR* EnterStateStr = TEXT("EnterState");
		const TCHAR* ExitStateStr = TEXT("ExitState");

		Status = Exec.Start();
		AITEST_FALSE(TEXT("MetaStory TaskA should not enter state"), Exec.Expect(TaskA.GetName(), EnterStateStr));
		AITEST_FALSE(TEXT("MetaStory TaskB should not enter state"), Exec.Expect(TaskB.GetName(), EnterStateStr));
		AITEST_TRUE(TEXT("MetaStory TaskC should enter state"), Exec.Expect(TaskC.GetName(), EnterStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_FollowTransitions, "System.MetaStory.Transition.FollowTransitions");

struct FMetaStoryTest_InfiniteLoop : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		FInstancedPropertyBag& RootPropertyBag = GetRootPropertyBag(EditorData);
		RootPropertyBag.AddProperty(FName(TEXT("Int")), EPropertyBagPropertyType::Int32);
		RootPropertyBag.SetValueInt32(FName(TEXT("Int")), 1);
		
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateA = Root.AddChildState(FName(TEXT("A")));
		UMetaStoryState& StateB = StateA.AddChildState(FName(TEXT("B")));

		// Root

		// State A
		{
			StateA.SelectionBehavior = EMetaStoryStateSelectionBehavior::TryFollowTransitions;
			{
				// A -> B
				FMetaStoryTransition& Trans = StateA.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &StateB);
				TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& TransIntCond = Trans.AddCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Equal);
				TransIntCond.GetInstanceData().Right = 1;
				EditorData.AddPropertyBinding(
					FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Int")),
					FPropertyBindingPath(TransIntCond.ID, TEXT("Left")));
			}
		}

		// State B
		{
			StateB.SelectionBehavior = EMetaStoryStateSelectionBehavior::TryFollowTransitions;
			{
				// B -> A
				FMetaStoryTransition& Trans = StateB.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &StateA);
				TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& TransIntCond = Trans.AddCondition<FMetaStoryCompareIntCondition>(EGenericAICheck::Equal);
				TransIntCond.GetInstanceData().Right = 1;
				EditorData.AddPropertyBinding(
					FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Int")),
					FPropertyBindingPath(TransIntCond.ID, TEXT("Left")));
			}
		}
		
		auto& TaskA = StateA.AddTask<FTestTask_Stand>(FName(TEXT("TaskA")));
		auto& TaskB = StateB.AddTask<FTestTask_Stand>(FName(TEXT("TaskB")));

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);

		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		const TCHAR* TickStr = TEXT("Tick");
		const TCHAR* EnterStateStr = TEXT("EnterState");
		const TCHAR* ExitStateStr = TEXT("ExitState");

		GetTestRunner().AddExpectedMessage(TEXT("Loop detected when trying to select state"), ELogVerbosity::Warning, EAutomationExpectedErrorFlags::Contains, 1);
		GetTestRunner().AddExpectedError(TEXT("Failed to select initial state"), EAutomationExpectedErrorFlags::Contains, 1);
		
		Status = Exec.Start();
		AITEST_EQUAL(TEXT("Start should fail"), Status, EMetaStoryRunStatus::Failed);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMetaStoryTest_InfiniteLoop, "System.MetaStory.Transition.InfiniteLoop");

struct FMetaStoryTest_RegularTransitions : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//  Global task and parameter
		//	RootA
		//		StateB -> Next
		//		StateC -> Next
		//		StateD -> Next
		//		StateE -> Next
		//		StateF -> Next
		//		StateG -> Succeeded

		FMetaStoryCompilerLog Log;

		// Main asset
		UMetaStory& MetaStory = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
			FGuid RootParameter_ValueID;
			{
				// Parameters
				FInstancedPropertyBag& RootPropertyBag = GetRootPropertyBag(EditorData);
				RootPropertyBag.AddProperty("Value", EPropertyBagPropertyType::Int32);
				RootPropertyBag.SetValueInt32("Value", -111);
				RootParameter_ValueID = RootPropertyBag.FindPropertyDescByName("Value")->ID;

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask = EditorData.AddGlobalTask<FTestTask_PrintValue>("GlobalTask");
				GlobalTask.GetInstanceData().Value = -1;
				EditorData.AddPropertyBinding(FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Value")), FPropertyBindingPath(GlobalTask.ID, TEXT("Value")));
			}

			UMetaStoryState& Root = EditorData.AddSubTree("RootA");
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = Root.AddTask<FTestTask_PrintValue>("TaskA");
				Task.GetInstanceData().Value = -1;
				EditorData.AddPropertyBinding(FPropertyBindingPath(EditorData.GetRootParametersGuid(), TEXT("Value")), FPropertyBindingPath(Task.ID, TEXT("Value")));
			}
			{
				UMetaStoryState& StateB = Root.AddChildState("StateB", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = StateB.AddTask<FTestTask_PrintValue>("TaskB");
				Task.GetInstanceData().Value = 1;
				FMetaStoryTransition& Transition = StateB.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::NextState);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}
			{
				UMetaStoryState& StateB = Root.AddChildState("StateC", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = StateB.AddTask<FTestTask_PrintValue>("TaskC");
				Task.GetInstanceData().Value = 2;
				FMetaStoryTransition& Transition = StateB.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::NextState);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}
			{
				UMetaStoryState& StateD = Root.AddChildState("StateD", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = StateD.AddTask<FTestTask_PrintValue>("TaskD");
				Task.GetInstanceData().Value = 3;
				FMetaStoryTransition& Transition = StateD.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &Root);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}

			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory);
			AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
		}

		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		{
			EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

			FInstancedPropertyBag Parameters;
			Parameters.MigrateToNewBagInstance(MetaStory.GetDefaultParameters());
			Parameters.SetValueInt32("Value", 111);

			Status = Exec.Start(&Parameters);
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("Start should enter Global tasks"), Exec.Expect("GlobalTask", TEXT("EnterState111")));
			AITEST_TRUE(TEXT("Start should enter StateA"), Exec.Expect("TaskA", TEXT("EnterState111")));
			AITEST_TRUE(TEXT("Start should enter StateB"), Exec.Expect("TaskB", TEXT("EnterState1")));
			Exec.LogClear();

			Status = Exec.Tick(1.5f); // over tick, should trigger
			AITEST_EQUAL(TEXT("1st Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("1st Tick should tick Global tasks"), Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE(TEXT("1st Tick should tick StateA"), Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE(TEXT("1st Tick should tick StateB"), Exec.Expect("TaskB", TEXT("Tick1")));
			Exec.LogClear();

			// B go to C
			Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("2nd Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("2nd Tick should tick Global tasks"), Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE(TEXT("2nd Tick should tick StateA"), Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE(TEXT("2nd Tick should tick the StateB"), Exec.Expect("TaskB", TEXT("Tick1")));
			AITEST_TRUE(TEXT("2nd Tick should exit the StateB"), Exec.Expect("TaskB", TEXT("ExitState1")));
			AITEST_TRUE(TEXT("2nd Tick should enter the StateC"), Exec.Expect("TaskC", TEXT("EnterState2")));
			AITEST_FALSE(TEXT("2nd Tick should not exit the StateA"), Exec.Expect("TaskA", TEXT("ExitState111")));
			AITEST_FALSE(TEXT("2nd Tick should not exit the Global tasks"), Exec.Expect("GlobalTask", TEXT("ExitState111")));
			Exec.LogClear();

			Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("3rd Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("3rd Tick should tick Global tasks"), Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE(TEXT("3rd Tick should tick StateA"), Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE(TEXT("3rd Tick should tick StateC"), Exec.Expect("TaskC", TEXT("Tick2")));
			AITEST_FALSE(TEXT("3th Tick should not exit StateC"), Exec.Expect("TaskD", TEXT("ExitState2")));
			Exec.LogClear();

			// C go to D
			Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("4th Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("4th Tick should tick Global tasks"), Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE(TEXT("4th Tick should tick StateA"), Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE(TEXT("4th Tick should tick the StateC"), Exec.Expect("TaskC", TEXT("Tick2")));
			AITEST_TRUE(TEXT("4th Tick should exit the StateC"), Exec.Expect("TaskC", TEXT("ExitState2")));
			AITEST_TRUE(TEXT("4th Tick should enter the StateD"), Exec.Expect("TaskD", TEXT("EnterState3")));
			AITEST_FALSE(TEXT("4th Tick should not exit the StateA"), Exec.Expect("TaskA", TEXT("ExitState111")));
			AITEST_FALSE(TEXT("4th Tick should not exit the Global tasks"), Exec.Expect("GlobalTask", TEXT("ExitState111")));
			Exec.LogClear();

			Status = Exec.Tick(0.001f);
			AITEST_EQUAL(TEXT("5th Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("5th Tick should tick Global tasks"), Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE(TEXT("5th Tick should tick StateA"), Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE(TEXT("5th Tick should tick StateD"), Exec.Expect("TaskD", TEXT("Tick3")));
			AITEST_FALSE(TEXT("5th Tick should not exit StateD"), Exec.Expect("TaskD", TEXT("ExitState3")));
			Exec.LogClear();

			// D go to root
			Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("6th Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("6th Tick should tick Global tasks"), Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE(TEXT("6th Tick should tick StateA"), Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE(TEXT("6th Tick should tick StateD"), Exec.Expect("TaskD", TEXT("Tick3")));
			AITEST_TRUE(TEXT("6th Tick should exit the StateD"), Exec.Expect("TaskD", TEXT("ExitState3")));
			AITEST_FALSE(TEXT("6th Tick should not exit the Global tasks"), Exec.Expect("GlobalTask", TEXT("ExitState111")));
			AITEST_FALSE(TEXT("6th Tick should not enter the Global tasks"), Exec.Expect("GlobalTask", TEXT("EnterState111")));
			AITEST_TRUE(TEXT("6th Tick should exit the StateA"), Exec.Expect("TaskA", TEXT("ExitState=Sustained")));
			AITEST_TRUE(TEXT("6th Tick should enter the StateA"), Exec.Expect("TaskA", TEXT("EnterState=Sustained")));
			AITEST_TRUE(TEXT("6th Tick should enter the StateB"), Exec.Expect("TaskB", TEXT("EnterState1")));
			AITEST_TRUE(TEXT("6th Tick should enter the StateB"), Exec.Expect("TaskB", TEXT("EnterState=Changed")));
			Exec.LogClear();

			Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("7th Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("7th Tick should tick Global tasks"), Exec.Expect("GlobalTask", TEXT("Tick111")));
			AITEST_TRUE(TEXT("7th Tick should tick StateA"), Exec.Expect("TaskA", TEXT("Tick111")));
			AITEST_TRUE(TEXT("7th Tick should tick StateB"), Exec.Expect("TaskB", TEXT("Tick1")));
			Exec.LogClear();

			Exec.Stop();
			AITEST_TRUE(TEXT("Stop Tick should exit the StateB"), Exec.Expect("TaskB", TEXT("ExitState1")));
			AITEST_TRUE(TEXT("Stop Tick should exit the StateA"), Exec.Expect("TaskA", TEXT("ExitState111")));
			AITEST_TRUE(TEXT("Stop should tick Global tasks"), Exec.Expect("GlobalTask", TEXT("ExitState111")));
			Exec.LogClear();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_RegularTransitions, "System.MetaStory.Transition.RegularTransitions");


struct FMetaStoryTest_RequestTransition : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//RootA
		// StateB
		//  StateC
		//   StateD
		//    StateE
		//     StateF
		//  StateI
		//   StateJ
		// StateX
		//  StateY
		//RootO
		// StateP

		FMetaStoryCompilerLog Log;

		enum class ECustomFunctionToRun
		{
			None,
			TransitionD_To_E,
			TransitionB_To_I,
			TransitionB_To_C,
			TransitionC_To_B,
			TransitionB_To_X,
			TransitionB_To_B,
			TransitionB_To_P,
		} TransitionToExecute = ECustomFunctionToRun::None;
		struct FStateHandle
		{
			FMetaStoryStateHandle B;
			FMetaStoryStateHandle C;
			FMetaStoryStateHandle E;
			FMetaStoryStateHandle I;
			FMetaStoryStateHandle X;
			FMetaStoryStateHandle P;
			FGuid B_ID;
			FGuid C_ID;
			FGuid E_ID;
			FGuid I_ID;
			FGuid X_ID;
			FGuid P_ID;
			bool bFinishTasksB = false;
			bool bFinishTasksC = false;
			bool bFinishTasksD = false;
		} AllStateHandle;

		// Main asset
		UMetaStory& MetaStory = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
			UMetaStoryState& StateA = EditorData.AddSubTree("StateA");
			StateA.AddTask<FTestTask_PrintValue>("StateATask");
			
			UMetaStoryState& StateB = StateA.AddChildState("StateB", EMetaStoryStateType::State);
			FTestTask_PrintValue& TaskB = StateB.AddTask<FTestTask_PrintValue>("StateBTask").GetNode();
			AllStateHandle.B_ID = StateB.ID;

			UMetaStoryState& StateC = StateB.AddChildState("StateC", EMetaStoryStateType::State);
			FTestTask_PrintValue& TaskC = StateC.AddTask<FTestTask_PrintValue>("StateCTask").GetNode();
			AllStateHandle.C_ID = StateC.ID;

			UMetaStoryState& StateD = StateC.AddChildState("StateD", EMetaStoryStateType::State);
			FTestTask_PrintValue& TaskD = StateD.AddTask<FTestTask_PrintValue>("StateDTask").GetNode();
			StateD.SelectionBehavior = EMetaStoryStateSelectionBehavior::TryEnterState;
			
			UMetaStoryState& StateE = StateD.AddChildState("StateE", EMetaStoryStateType::State);
			StateE.AddTask<FTestTask_PrintValue>("StateETask");
			AllStateHandle.E_ID = StateE.ID;
			
			UMetaStoryState& StateF = StateE.AddChildState("StateF", EMetaStoryStateType::State);
			StateF.AddTask<FTestTask_PrintValue>("StateFTask");

			UMetaStoryState& StateI = StateB.AddChildState("StateI", EMetaStoryStateType::State);
			StateI.AddTask<FTestTask_PrintValue>("StateITask");
			AllStateHandle.I_ID = StateI.ID;
			
			UMetaStoryState& StateJ = StateI.AddChildState("StateJ", EMetaStoryStateType::State);
			StateJ.AddTask<FTestTask_PrintValue>("StateJTask");

			UMetaStoryState& StateX = StateA.AddChildState("StateX", EMetaStoryStateType::State);
			StateX.AddTask<FTestTask_PrintValue>("StateXTask");
			AllStateHandle.X_ID = StateX.ID;

			UMetaStoryState& StateY = StateX.AddChildState("StateY", EMetaStoryStateType::State);
			StateY.AddTask<FTestTask_PrintValue>("StateYTask");

			UMetaStoryState& StateO = EditorData.AddSubTree("StateO");
			StateO.AddTask<FTestTask_PrintValue>("StateOTask");

			UMetaStoryState& StateP = StateO.AddChildState("StateP", EMetaStoryStateType::State);
			StateP.AddTask<FTestTask_PrintValue>("StatePTask");
			AllStateHandle.P_ID = StateP.ID;

			TaskB.CustomTickFunc = [&TransitionToExecute, &AllStateHandle]
				(FMetaStoryExecutionContext& Context, const FTestTask_PrintValue* Task)
				{
					if (TransitionToExecute == ECustomFunctionToRun::TransitionB_To_I)
					{
						Context.RequestTransition(AllStateHandle.I);
					}
					if (TransitionToExecute == ECustomFunctionToRun::TransitionB_To_C)
					{
						Context.RequestTransition(AllStateHandle.C);
					}
					if (TransitionToExecute == ECustomFunctionToRun::TransitionB_To_X)
					{
						Context.RequestTransition(AllStateHandle.X);
					}
					if (TransitionToExecute == ECustomFunctionToRun::TransitionB_To_B)
					{
						Context.RequestTransition(AllStateHandle.B);
					}
					if (TransitionToExecute == ECustomFunctionToRun::TransitionB_To_P)
					{
						Context.RequestTransition(AllStateHandle.P);
					}
					if (AllStateHandle.bFinishTasksB)
					{
						Context.FinishTask(*Task, EMetaStoryFinishTaskType::Succeeded);
					}
				};

			TaskC.CustomTickFunc = [&TransitionToExecute, &AllStateHandle]
				(FMetaStoryExecutionContext& Context, const FTestTask_PrintValue* Task)
				{
					if (TransitionToExecute == ECustomFunctionToRun::TransitionC_To_B)
					{
						Context.RequestTransition(AllStateHandle.B);
					}
					if (AllStateHandle.bFinishTasksC)
					{
						Context.FinishTask(*Task, EMetaStoryFinishTaskType::Succeeded);
					}
				};

			TaskD.CustomTickFunc = [&TransitionToExecute, &AllStateHandle]
				(FMetaStoryExecutionContext& Context, const FTestTask_PrintValue* Task)
				{
					if (TransitionToExecute == ECustomFunctionToRun::TransitionD_To_E)
					{
						Context.RequestTransition(AllStateHandle.E);
					}

					if (AllStateHandle.bFinishTasksD)
					{
						Context.FinishTask(*Task, EMetaStoryFinishTaskType::Succeeded);
					}
				};

			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory);
			AITEST_TRUE("MetaStory should get compiled", bResult);

			AllStateHandle.B = MetaStory.GetStateHandleFromId(AllStateHandle.B_ID);
			AllStateHandle.C = MetaStory.GetStateHandleFromId(AllStateHandle.C_ID);
			AllStateHandle.E = MetaStory.GetStateHandleFromId(AllStateHandle.E_ID);
			AllStateHandle.I = MetaStory.GetStateHandleFromId(AllStateHandle.I_ID);
			AllStateHandle.X = MetaStory.GetStateHandleFromId(AllStateHandle.X_ID);
			AllStateHandle.P = MetaStory.GetStateHandleFromId(AllStateHandle.P_ID);
		}

		FMetaStoryInstanceData InstanceData;
		{
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("MetaStory should init", bInitSucceeded);
		}

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

		auto ResetInstanceData = [this, &MetaStory, &InstanceData]()
			{
				{
					FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
					EMetaStoryRunStatus Status = Exec.Stop();
					AITEST_EQUAL(TEXT("Should stop"), Status, EMetaStoryRunStatus::Stopped);
				}
				{
					FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
					const EMetaStoryRunStatus Status = Exec.Start();
					AITEST_EQUAL(TEXT("State should complete with Running"), Status, EMetaStoryRunStatus::Running);
					AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD"));
				}
				return true;
			};

		const TCHAR* EnterStateChangedStr = TEXT("EnterState=Changed");
		const TCHAR* EnterStateSustainedStr = TEXT("EnterState=Sustained");
		const TCHAR* ExitStateChangedStr = TEXT("ExitState=Changed");;
		const TCHAR* ExitStateSustainedStr = TEXT("ExitState=Sustained");

		for (int32 Index = 0; Index < MaxRules; ++Index)
		{
			const EMetaStoryStateSelectionRules StateSelectionRules = MakeStateSelectionRule(Index);
			InstanceData = FMetaStoryInstanceData();
			if (MetaStory.GetStateSelectionRules() != StateSelectionRules)
			{
				MetaStory.ResetCompiled();
				UMetaStoryEditorData* EditorData = CastChecked<UMetaStoryEditorData>(MetaStory.EditorData);
				UMetaStoryTestSchema* Schema = CastChecked<UMetaStoryTestSchema>(EditorData->Schema);
				Schema->SetStateSelectionRules(StateSelectionRules);

				FMetaStoryCompiler Compiler(Log);
				const bool bResult = Compiler.Compile(MetaStory);
				AITEST_TRUE("MetaStory should get compiled", bResult);
			}

			// Normal Start and tick. Make sure we are in ABCD before testing the transitions
			{
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL("Start should complete with Running", Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE("Start should enter Global tasks", Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Start StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Start StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Start StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Start StateD"), LogOrder);
				AITEST_FALSE(TEXT("Tick StateA"), Exec.Expect("StateATask", TEXT("Tick0")));
				AITEST_FALSE(TEXT("Tick StateB"), Exec.Expect("StateBTask", TEXT("Tick0")));
				AITEST_FALSE(TEXT("Tick StateC"), Exec.Expect("StateCTask", TEXT("Tick0")));
				AITEST_FALSE(TEXT("Tick StateD"), Exec.Expect("StateDTask", TEXT("Tick0")));
				AITEST_FALSE(TEXT("Start StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Start StateB"), Exec.Expect("StateBTask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Start StateC"), Exec.Expect("StateCTask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Start StateD"), Exec.Expect("StateDTask", TEXT("ExitState0")));
				Exec.LogClear();
			}
			{
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL("1st Tick should complete with Running", Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE("1st Tick no transition", Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateB"), Exec.Expect("StateBTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateC"), Exec.Expect("StateCTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateD"), Exec.Expect("StateDTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateB"), Exec.Expect("StateBTask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateC"), Exec.Expect("StateCTask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateD"), Exec.Expect("StateDTask", TEXT("ExitState0")));
				Exec.LogClear();
			}
			// Select new child
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionD_To_E;

				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("2nd Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("2nd tick should be in new state"), Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD", "StateE", "StateF"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateETask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Enter StateE"), LogOrder);
				LogOrder = LogOrder.Then("StateFTask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Enter StateF"), LogOrder);
				AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateB"), Exec.Expect("StateBTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateC"), Exec.Expect("StateCTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateD"), Exec.Expect("StateDTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateB"), Exec.Expect("StateBTask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateC"), Exec.Expect("StateCTask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateD"), Exec.Expect("StateDTask", TEXT("ExitState0")));
				Exec.LogClear();
				TransitionToExecute = ECustomFunctionToRun::None;
			}

			// Select a new child but with a completed D
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionD_To_E;
				AllStateHandle.bFinishTasksD = true;
				if (!ResetInstanceData())
				{
					return false;
				}

				const bool bUseCompletedRule = EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates);

				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD", "StateE", "StateF"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				if (bUseCompletedRule)
				{
					LogOrder = LogOrder.Then("StateDTask", ExitStateChangedStr);
					AITEST_TRUE(TEXT("Exit StateD"), LogOrder);
					LogOrder = LogOrder.Then("StateDTask", EnterStateChangedStr);
					AITEST_TRUE(TEXT("Enter StateD"), LogOrder);
				}
				LogOrder = LogOrder.Then("StateETask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Enter StateE"), LogOrder);
				LogOrder = LogOrder.Then("StateFTask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Enter StateF"), LogOrder);
				AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateB"), Exec.Expect("StateBTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateC"), Exec.Expect("StateCTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateB"), Exec.Expect("StateBTask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateC"), Exec.Expect("StateCTask", TEXT("ExitState0")));
				Exec.LogClear();
				TransitionToExecute = ECustomFunctionToRun::None;
				AllStateHandle.bFinishTasksD = false;
			}
			// Select a new child but B is completed (before the source/target)
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionD_To_E;
				AllStateHandle.bFinishTasksB = true;
				if (!ResetInstanceData())
				{
					return false;
				}

				const bool bUseCompletedRule = EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::CompletedStateBeforeTransitionSourceFailsTransition);
				const bool bUseReselectedRule = EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::ReselectedStateCreatesNewStates);

				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				if (bUseCompletedRule)
				{
					AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD"));
				}
				else
				{
					AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD", "StateE", "StateF"));
				}
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				if (bUseCompletedRule)
				{
					const TCHAR* SchemaEnterStateStr = bUseReselectedRule ? EnterStateChangedStr : EnterStateSustainedStr;
					const TCHAR* SchemaExitStateStr = bUseReselectedRule ? ExitStateChangedStr : ExitStateSustainedStr;

					LogOrder = LogOrder.Then("StateDTask", ExitStateChangedStr);
					AITEST_TRUE(TEXT("Enter StateD"), LogOrder);
					LogOrder = LogOrder.Then("StateCTask", ExitStateChangedStr);
					AITEST_TRUE(TEXT("Enter StateC"), LogOrder);
					LogOrder = LogOrder.Then("StateBTask", ExitStateChangedStr);
					AITEST_TRUE(TEXT("Enter StateB"), LogOrder);
					LogOrder = LogOrder.Then("StateATask", SchemaExitStateStr);
					AITEST_TRUE(TEXT("Enter StateA"), LogOrder);
					LogOrder = LogOrder.Then("StateATask", SchemaEnterStateStr);
					AITEST_TRUE(TEXT("Enter StateA"), LogOrder);
					LogOrder = LogOrder.Then("StateBTask", EnterStateChangedStr);
					AITEST_TRUE(TEXT("Enter StateB"), LogOrder);
					LogOrder = LogOrder.Then("StateCTask", EnterStateChangedStr);
					AITEST_TRUE(TEXT("Enter StateC"), LogOrder);
					LogOrder = LogOrder.Then("StateDTask", EnterStateChangedStr);
					AITEST_TRUE(TEXT("Enter StateD"), LogOrder);

					// Only one transition
					LogOrder = LogOrder.Then("StateBTask", TEXT("EnterState0"));
					AITEST_FALSE(TEXT("Enter StateD"), LogOrder);

					AITEST_FALSE(TEXT("Enter StateE"), Exec.Expect("StateETask", TEXT("EnterState0")));
					AITEST_FALSE(TEXT("Enter StateF"), Exec.Expect("StateFTask", TEXT("EnterState0")));
					AITEST_FALSE(TEXT("Exit StateE"), Exec.Expect("StateETask", TEXT("ExitState0")));
					AITEST_FALSE(TEXT("Exit StateF"), Exec.Expect("StateFTask", TEXT("ExitState0")));
				}
				else
				{
					LogOrder = LogOrder.Then("StateETask", EnterStateChangedStr);
					AITEST_TRUE(TEXT("Enter StateE"), LogOrder);
					LogOrder = LogOrder.Then("StateFTask", EnterStateChangedStr);
					AITEST_TRUE(TEXT("Enter StateF"), LogOrder);
					AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
					AITEST_FALSE(TEXT("Enter StateB"), Exec.Expect("StateBTask", TEXT("EnterState0")));
					AITEST_FALSE(TEXT("Enter StateC"), Exec.Expect("StateCTask", TEXT("EnterState0")));
					AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));
					AITEST_FALSE(TEXT("Exit StateB"), Exec.Expect("StateBTask", TEXT("ExitState0")));
					AITEST_FALSE(TEXT("Exit StateC"), Exec.Expect("StateCTask", TEXT("ExitState0")));
				}
				Exec.LogClear();
				TransitionToExecute = ECustomFunctionToRun::None;
				AllStateHandle.bFinishTasksB = false;
			}
			// Select a sibling
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionB_To_I;
				if (!ResetInstanceData())
				{
					return false;
				}

				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateB", "StateI", "StateJ"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", ExitStateChangedStr);
				AITEST_TRUE(TEXT("Exit StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", ExitStateChangedStr);
				AITEST_TRUE(TEXT("Exit StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateITask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Enter StateI"), LogOrder);
				LogOrder = LogOrder.Then("StateJTask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Enter StateJ"), LogOrder);
				AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateB"), Exec.Expect("StateBTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateB"), Exec.Expect("StateBTask", TEXT("ExitState0")));
				Exec.LogClear();
				TransitionToExecute = ECustomFunctionToRun::None;
			}
			// No state completed. Reselect child.
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionB_To_C;
				if (!ResetInstanceData())
				{
					return false;
				}

				const bool bUseReselectedRule = EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::ReselectedStateCreatesNewStates);
				const TCHAR* SchemaEnterStateStr = bUseReselectedRule ? EnterStateChangedStr : EnterStateSustainedStr;
				const TCHAR* SchemaExitStateStr = bUseReselectedRule ? ExitStateChangedStr : ExitStateSustainedStr;
				 
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
	
				LogOrder = LogOrder.Then("StateDTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateD"), LogOrder);

				AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateB"), Exec.Expect("StateBTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateB"), Exec.Expect("StateBTask", TEXT("ExitState0")));
				Exec.LogClear();
				TransitionToExecute = ECustomFunctionToRun::None;
			}
			// State completed. Reselect child.
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionB_To_C;
				AllStateHandle.bFinishTasksC = true;
				if (!ResetInstanceData())
				{
					return false;
				}

				const bool bUseCompletedRule = EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates);
				const bool bUseReselectedRule = EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::ReselectedStateCreatesNewStates);
				const TCHAR* SchemaEnterStateStr = bUseCompletedRule || bUseReselectedRule ? EnterStateChangedStr : EnterStateSustainedStr;
				const TCHAR* SchemaExitStateStr = bUseCompletedRule || bUseReselectedRule ? ExitStateChangedStr : ExitStateSustainedStr;

				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateD"), LogOrder);
				AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Enter StateB"), Exec.Expect("StateBTask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));
				AITEST_FALSE(TEXT("Exit StateB"), Exec.Expect("StateBTask", TEXT("ExitState0")));
				Exec.LogClear();
				TransitionToExecute = ECustomFunctionToRun::None;
				AllStateHandle.bFinishTasksC = false;
			}
			// No state completed. Reselect parent.
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionC_To_B;
				if (!ResetInstanceData())
				{
					return false;
				}

				const bool bUseReselectedRule = EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::ReselectedStateCreatesNewStates);
				const TCHAR* SchemaEnterStateStr = bUseReselectedRule ? EnterStateChangedStr : EnterStateSustainedStr;
				const TCHAR* SchemaExitStateStr = bUseReselectedRule ? ExitStateChangedStr : ExitStateSustainedStr;

				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateD"), LogOrder);
				AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));

				Exec.LogClear();
				TransitionToExecute = ECustomFunctionToRun::None;
			}
			// State completed. Reselect parent.
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionC_To_B;
				AllStateHandle.bFinishTasksC = true;
				if (!ResetInstanceData())
				{
					return false;
				}

				const bool bUseCompletedRule = EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates);
				const bool bUseReselectedRule = EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::ReselectedStateCreatesNewStates);
				const TCHAR* SchemaEnterStateStr = bUseCompletedRule || bUseReselectedRule ? EnterStateChangedStr : EnterStateSustainedStr;
				const TCHAR* SchemaExitStateStr = bUseCompletedRule || bUseReselectedRule ? ExitStateChangedStr : ExitStateSustainedStr;

				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", SchemaExitStateStr);
				if (bUseReselectedRule)
				{
					AITEST_TRUE(TEXT("Exit StateC"), LogOrder);
					LogOrder = LogOrder.Then("StateBTask", ExitStateChangedStr);
					AITEST_TRUE(TEXT("Exit StateB"), LogOrder);
					LogOrder = LogOrder.Then("StateBTask", EnterStateChangedStr);
				}
				else
				{
					AITEST_TRUE(TEXT("Exit StateC"), LogOrder);
					LogOrder = LogOrder.Then("StateBTask", ExitStateSustainedStr);
					AITEST_TRUE(TEXT("Exit StateB"), LogOrder);
					LogOrder = LogOrder.Then("StateBTask", EnterStateSustainedStr);
				}
				AITEST_TRUE(TEXT("Enter StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateD"), LogOrder);
				AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));

				Exec.LogClear();
				AllStateHandle.bFinishTasksC = false;
				TransitionToExecute = ECustomFunctionToRun::None;
			}
			// linked state
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionB_To_X;
				if (!ResetInstanceData())
				{
					return false;
				}

				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateX", "StateY"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", ExitStateChangedStr);
				AITEST_TRUE(TEXT("Exit StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", ExitStateChangedStr);
				AITEST_TRUE(TEXT("Exit StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", ExitStateChangedStr);
				AITEST_TRUE(TEXT("Exit StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateXTask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Enter StateX"), LogOrder);
				LogOrder = LogOrder.Then("StateYTask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Enter StateY"), LogOrder);
				AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));
				Exec.LogClear();
				TransitionToExecute = ECustomFunctionToRun::None;
			}
			// Reselection same state
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionB_To_B;
				if (!ResetInstanceData())
				{
					return false;
				}
				const bool bUseReselectedRule = EnumHasAllFlags(StateSelectionRules, EMetaStoryStateSelectionRules::ReselectedStateCreatesNewStates);
				const TCHAR* SchemaEnterStateStr = bUseReselectedRule ? EnterStateChangedStr : EnterStateSustainedStr;
				const TCHAR* SchemaExitStateStr = bUseReselectedRule ? ExitStateChangedStr : ExitStateSustainedStr;

				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateA", "StateB", "StateC", "StateD"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", SchemaExitStateStr);
				AITEST_TRUE(TEXT("Exit StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", SchemaEnterStateStr);
				AITEST_TRUE(TEXT("Enter StateD"), LogOrder);
				AITEST_FALSE(TEXT("Enter StateA"), Exec.Expect("StateATask", TEXT("EnterState0")));
				AITEST_FALSE(TEXT("Exit StateA"), Exec.Expect("StateATask", TEXT("ExitState0")));
				Exec.LogClear();
				TransitionToExecute = ECustomFunctionToRun::None;
			}
			// Select another root.
			{
				TransitionToExecute = ECustomFunctionToRun::TransitionB_To_P;
				if (!ResetInstanceData())
				{
					return false;
				}

				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Start should"), Exec.ExpectInActiveStates("StateO", "StateP"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("StateATask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", TEXT("Tick0"));
				AITEST_TRUE(TEXT("Tick StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateDTask", ExitStateChangedStr);
				AITEST_TRUE(TEXT("Exit StateD"), LogOrder);
				LogOrder = LogOrder.Then("StateCTask", ExitStateChangedStr);
				AITEST_TRUE(TEXT("Exit StateC"), LogOrder);
				LogOrder = LogOrder.Then("StateBTask", ExitStateChangedStr);
				AITEST_TRUE(TEXT("Exit StateB"), LogOrder);
				LogOrder = LogOrder.Then("StateATask", ExitStateChangedStr);
				AITEST_TRUE(TEXT("Exit StateA"), LogOrder);
				LogOrder = LogOrder.Then("StateOTask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Enter StateO"), LogOrder);
				LogOrder = LogOrder.Then("StatePTask", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Enter StateP"), LogOrder);
				Exec.LogClear();
				TransitionToExecute = ECustomFunctionToRun::None;
			}
			{
				FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
				EMetaStoryRunStatus Status = Exec.Stop();
				AITEST_EQUAL(TEXT("Should stop"), Status, EMetaStoryRunStatus::Stopped);
			}
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMetaStoryTest_RequestTransition, "System.MetaStory.Transition.RequestTransition");

struct FMetaStoryTest_TransitionToNone : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		const FGameplayTag Tag = GetTestTag1();

		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));
		UMetaStoryState& State2 = Root.AddChildState(FName(TEXT("State2")));

		FMetaStoryTransition& TransitionRoot = Root.AddTransition(EMetaStoryTransitionTrigger::OnEvent, EMetaStoryTransitionType::GotoState);
		TransitionRoot.State = State1.GetLinkToState();
		TransitionRoot.RequiredEvent.Tag = Tag;

		TMetaStoryTypedEditorNode<FTestTask_StandNoTick>& Task1 = State1.AddTask<FTestTask_StandNoTick>(FName(TEXT("Task1")));
		FMetaStoryTransition& TransitionState1 = State1.AddTransition(EMetaStoryTransitionTrigger::OnEvent, EMetaStoryTransitionType::None);
		TransitionState1.RequiredEvent.Tag = Tag;

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE("MetaStory should get compiled", bResult);

		EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("MetaStory should init", bInitSucceeded);

		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		// Start and enter state
		Status = Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Task1 should enter state"), Exec.Expect(Task1.GetName(), EnterStateStr));
		Exec.LogClear();

		// Send event with Tag
		Exec.SendEvent(Tag);

		// Transition from Root to State2 should not be triggered
		Status = Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("Active states should stay the same"), Exec.ExpectInActiveStates("Root", "State1"));
		AITEST_FALSE(TEXT("MetaStory Task1 should not exit state"), Exec.Expect(Task1.GetName(), ExitStateStr));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionToNone, "System.MetaStory.Transition.ToNone");

struct FMetaStoryTest_TransitionTwoEvents : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

			UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
			UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));
			UMetaStoryState& State2 = Root.AddChildState(FName(TEXT("State2")));
			UMetaStoryState& State3 = State2.AddChildState(FName(TEXT("State3")));
			UMetaStoryState& State4 = State2.AddChildState(FName(TEXT("State4")));

			State2.bHasRequiredEventToEnter = true;
			State2.RequiredEventToEnter.Tag = GetTestTag1();

			State3.bHasRequiredEventToEnter = true;
			State3.RequiredEventToEnter.Tag = GetTestTag2();

			FMetaStoryTransition& TransitionA = State1.AddTransition(EMetaStoryTransitionTrigger::OnEvent, EMetaStoryTransitionType::GotoState);
			TransitionA.State = State2.GetLinkToState();
			TransitionA.RequiredEvent.Tag = GetTestTag1();

			TMetaStoryTypedEditorNode<FTestTask_StandNoTick>& State3Task1 = State3.AddTask<FTestTask_StandNoTick>(FName(TEXT("State3Task1")));
		}
		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory);
			AITEST_TRUE("MetaStory should get compiled", bResult);
		}

		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		FMetaStoryInstanceData InstanceData;
		TArray<FMetaStoryRecordedTransitionResult> RecordedTransitions;
		{
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData, {}, EMetaStoryRecordTransitions::Yes);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("MetaStory should init", bInitSucceeded);

			// Start and enter state
			EMetaStoryRunStatus Status = Exec.Start();
			AITEST_TRUE(TEXT("Active states should stay the same"), Exec.ExpectInActiveStates("Root", "State1"));
			Exec.LogClear();

			// Transition from State1 to State2. State3 fails (missing event), enter State4.
			Exec.SendEvent(GetTestTag1());
			Exec.Tick(0.1f);
			AITEST_TRUE(TEXT("Active states should stay the same"), Exec.ExpectInActiveStates("Root", "State2", "State4"));
			Exec.LogClear();

			Exec.Stop();
			Exec.Start();
			AITEST_TRUE(TEXT("Active states should stay the same"), Exec.ExpectInActiveStates("Root", "State1"));

			// Transition from State1 to State2 failed.
			Exec.SendEvent(GetTestTag2());
			Status = Exec.Tick(0.1f);
			AITEST_TRUE(TEXT("Active states should stay the same"), Exec.ExpectInActiveStates("Root", "State1"));
			Exec.LogClear();

			// Transition from State1 to State2. State3 succeed.
			Exec.SendEvent(GetTestTag2());
			Exec.SendEvent(GetTestTag1());
			Status = Exec.Tick(0.1f);
			AITEST_TRUE(TEXT("Active states should stay the same"), Exec.ExpectInActiveStates("Root", "State2", "State3"));
			Exec.LogClear();

			RecordedTransitions = Exec.GetRecordedTransitions();
			Exec.Stop();
		}

		{
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("MetaStory should init", bInitSucceeded);

			// Start and enter state
			EMetaStoryRunStatus Status = Exec.Start();
			AITEST_TRUE(TEXT("Active states should stay the same"), Exec.ExpectInActiveStates("Root", "State1"));
			Exec.LogClear();

			AITEST_TRUE(TEXT("Wrong number of transitions"), RecordedTransitions.Num() == 5);

			Exec.ForceTransition(RecordedTransitions[0]); // start
			AITEST_TRUE(TEXT("Start."), Exec.ExpectInActiveStates("Root", "State1"));

			Exec.ForceTransition(RecordedTransitions[1]); // state 3 doesn't have the event
			AITEST_TRUE(TEXT("State3 failed."), Exec.ExpectInActiveStates("Root", "State2", "State4"));
			
			Exec.ForceTransition(RecordedTransitions[2]); // start
			AITEST_TRUE(TEXT("Start and failed transition."), Exec.ExpectInActiveStates("Root", "State1"));

			Exec.ForceTransition(RecordedTransitions[3]); // failed
			AITEST_TRUE(TEXT("Start and failed transition."), Exec.ExpectInActiveStates("Root", "State1"));

			Exec.ForceTransition(RecordedTransitions[4]); // succeed
			AITEST_TRUE(TEXT("State3 succeed."), Exec.ExpectInActiveStates("Root", "State2", "State3"));

			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionTwoEvents, "System.MetaStory.Transition.TwoEvents");

struct FMetaStoryTest_TransitionLinkedAssetFromCompletedParent : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		/*
		- Tree1
			- Root1
				- State1: TryEnter, Task w/ 1 frame to complete
					- State2 : LinkedAsset (On Complete -> State 3)
				- State3
		- Tree2
			- Root1
				- State1: Task w/ 2 frames to complete
		*/

		UMetaStory& MetaStory1 = NewMetaStory();
		UMetaStory& MetaStory2 = NewMetaStory();

		// Setup linked asset
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
			UMetaStoryState& Root1 = EditorData.AddSubTree(FName("Tree2_Root1"));
			UMetaStoryState& State1 = Root1.AddChildState(FName("Tree2_State1"));

			State1.AddTask<FTestTask_PrintValue>("Tree2_State1_TaskPrint")
				.GetInstanceData().Value = 21;
			State1.AddTask<FTestTask_Stand>(FName("Tree2_State1_TaskStand"))
				.GetNode().TicksToCompletion = 2;
			State1.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &Root1);

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory2);
			AITEST_TRUE("MetaStory should get compiled", bResult);
		}

		// Setup main asset
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);
			UMetaStoryState& Root1 = EditorData.AddSubTree(FName("Tree1_Root1"));
			UMetaStoryState& State1 = Root1.AddChildState(FName("Tree1_State1"));

			UMetaStoryState& State2 = State1.AddChildState(FName("Tree1_State2"), EMetaStoryStateType::LinkedAsset);
			UMetaStoryState& State3 = Root1.AddChildState(FName("Tree1_State3"));

			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& PrintTask = State1.AddTask<FTestTask_PrintValue>("Tree1_State1_TaskPrint");
				PrintTask.GetInstanceData().Value = 11;
				TMetaStoryTypedEditorNode<FTestTask_Stand>& StandTask = State1.AddTask<FTestTask_Stand>(FName("Tree1_State1_TaskStand"));
				StandTask.GetNode().TicksToCompletion = 1;
				StandTask.GetInstanceData().Value = 11;
				State1.SelectionBehavior = EMetaStoryStateSelectionBehavior::TryEnterState;
				State1.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &State2);
			}

			{
				State2.SetLinkedStateAsset(&MetaStory2);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& PrintTask = State2.AddTask<FTestTask_PrintValue>("Tree1_State2_TaskPrint");
				PrintTask.GetInstanceData().Value = 12;
			}

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory1);
			AITEST_TRUE("MetaStory should get compiled", bResult);
		}

		FMetaStoryInstanceData InstanceData;
		{
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("MetaStory should init", bInitSucceeded);
		}

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

		const TCHAR* EnterStateChangedStr = TEXT("EnterState=Changed");
		const TCHAR* EnterStateSustainedStr = TEXT("EnterState=Sustained");
		const TCHAR* ExitStateChangedStr = TEXT("ExitState=Changed");
		const TCHAR* ExitStateSustainedStr = TEXT("ExitState=Sustained");

		for (int32 Index = 0; Index < MaxRules; ++Index)
		{
			const EMetaStoryStateSelectionRules StateSelectionRules = MakeStateSelectionRule(Index);
			InstanceData = FMetaStoryInstanceData();
			if (MetaStory1.GetStateSelectionRules() != StateSelectionRules)
			{
				MetaStory1.ResetCompiled();
				UMetaStoryEditorData* EditorData = CastChecked<UMetaStoryEditorData>(MetaStory1.EditorData);
				UMetaStoryTestSchema* Schema = CastChecked<UMetaStoryTestSchema>(EditorData->Schema);
				Schema->SetStateSelectionRules(StateSelectionRules);

				FMetaStoryCompilerLog Log;
				FMetaStoryCompiler Compiler(Log);
				const bool bResult = Compiler.Compile(MetaStory1);
				AITEST_TRUE("MetaStory should get compiled", bResult);
			}

			{
				FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE("MetaStory should init", bInitSucceeded);

				// Start and enter state
				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_TRUE(TEXT("Valid states"), Exec.ExpectInActiveStates("Tree1_Root1", "Tree1_State1"));
				AITEST_TRUE(TEXT("Expected amount of frames."), InstanceData.GetExecutionState()->ActiveFrames.Num() == 1);

				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1_State1_TaskPrint", TEXT("EnterState11"));
				AITEST_TRUE(TEXT("Start enters in the correct order Tree1_State1_TaskPrint"), LogOrder);
				LogOrder = LogOrder.Then("Tree1_State1_TaskPrint", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Start enters in the correct order Tree1_State1_TaskPrint"), LogOrder);

				LogOrder = LogOrder.Then("Tree1_State1_TaskStand", TEXT("EnterState"));
				AITEST_TRUE(TEXT("Start enters in the correct order Tree1_State1_TaskStand"), LogOrder);
				LogOrder = LogOrder.Then("Tree1_State1_TaskStand", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Start enters in the correct order Tree1_State1_TaskStand"), LogOrder);
				Exec.LogClear();
			}

			{
				FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
				EMetaStoryRunStatus Status = Exec.Tick(0.f);
				AITEST_TRUE(TEXT("Active states should now include linked asset states"), Exec.ExpectInActiveStates("Tree1_Root1", "Tree1_State1", "Tree1_State2", "Tree2_Root1", "Tree2_State1"));
				AITEST_TRUE(TEXT("Expected amount of frames."), InstanceData.GetExecutionState()->ActiveFrames.Num() == 2);

				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1_State1_TaskPrint", TEXT("Tick11"));
				AITEST_TRUE(TEXT("Tree1_State1_TaskPrint ticked"), LogOrder);
				LogOrder = LogOrder.Then("Tree1_State1_TaskStand", TEXT("Tick"));
				AITEST_TRUE(TEXT("Tree1_State1_TaskStand ticked"), LogOrder);
				LogOrder = LogOrder.Then("Tree1_State1_TaskStand", TEXT("StateCompleted"));
				AITEST_TRUE(TEXT("Completed State1 in Tree1"), LogOrder);
				LogOrder = LogOrder.Then("Tree1_State2_TaskPrint", TEXT("EnterState12"));
				AITEST_TRUE(TEXT("Entered State2 in Tree1"), LogOrder);
				LogOrder = LogOrder.Then("Tree1_State2_TaskPrint", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Entered State2 in Tree1"), LogOrder);
				LogOrder = LogOrder.Then("Tree2_State1_TaskPrint", TEXT("EnterState21"));
				AITEST_TRUE(TEXT("Entered State1 in Tree2"), LogOrder);
				LogOrder = LogOrder.Then("Tree2_State1_TaskStand", TEXT("EnterState"));
				AITEST_TRUE(TEXT("Entered State1 in Tree2"), LogOrder);
				LogOrder = LogOrder.Then("Tree2_State1_TaskStand", EnterStateChangedStr);
				AITEST_TRUE(TEXT("Entered State1 in Tree2"), LogOrder);
				Exec.LogClear();
			}

			{
				FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
				EMetaStoryRunStatus Status = Exec.Tick(0.f);
				AITEST_TRUE(TEXT("Active states should stay the same after Tree1_State1 completion"), Exec.ExpectInActiveStates("Tree1_Root1", "Tree1_State1", "Tree1_State2", "Tree2_Root1", "Tree2_State1"));
				AITEST_TRUE(TEXT("Expected amount of frames."), InstanceData.GetExecutionState()->ActiveFrames.Num() == 2);

				const bool bCompletedStatedCreateNewStates = EnumHasAnyFlags(StateSelectionRules, EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates);
				const bool bReselectCreateNewStates = EnumHasAnyFlags(StateSelectionRules, EMetaStoryStateSelectionRules::ReselectedStateCreatesNewStates);
				const TCHAR* EnterStateStr = (bCompletedStatedCreateNewStates || bReselectCreateNewStates) ? EnterStateChangedStr: EnterStateSustainedStr;
				const TCHAR* ExitStateStr = (bCompletedStatedCreateNewStates || bReselectCreateNewStates) ? ExitStateChangedStr: ExitStateSustainedStr;

				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1_State1_TaskPrint", TEXT("Tick11"));
				AITEST_TRUE(TEXT("Tree1_State1_TaskPrint ticked"), LogOrder);

				if (bCompletedStatedCreateNewStates)
				{
					LogOrder = LogOrder.Then("Tree1_State1_TaskStand", TEXT("Tick"));
					AITEST_TRUE(TEXT("Tree1_State1_TaskStand ticked"), LogOrder);
				}

				LogOrder = LogOrder.Then("Tree1_State2_TaskPrint", TEXT("Tick12"));
				AITEST_TRUE(TEXT("Tree1_State2_TaskPrint ticked"), LogOrder);
				LogOrder = LogOrder.Then("Tree2_State1_TaskPrint", TEXT("Tick21"));
				AITEST_TRUE(TEXT("Tree2_State1_TaskPrint ticked"), LogOrder);
				LogOrder = LogOrder.Then("Tree2_State1_TaskStand", TEXT("Tick"));
				AITEST_TRUE(TEXT("Tree2_State1_TaskStand ticked"), LogOrder);
				LogOrder = LogOrder.Then("Tree2_State1_TaskStand", TEXT("StateCompleted"));
				AITEST_TRUE(TEXT("Completed State1 in Tree2"), LogOrder);
				LogOrder = LogOrder.Then("Tree1_State1_TaskStand", TEXT("StateCompleted"));
				AITEST_TRUE(TEXT("Completed State1 in Tree1"), LogOrder);
				LogOrder = LogOrder.Then("Tree2_State1_TaskStand", ExitStateStr);
				AITEST_TRUE(TEXT("Exited State1 in Tree2"), LogOrder);
				LogOrder = Exec.Expect("Tree2_State1_TaskPrint", TEXT("ExitState21"));
				AITEST_TRUE(TEXT("Exited State1 in Tree2"), LogOrder);
				LogOrder = Exec.Expect("Tree2_State1_TaskPrint", ExitStateStr);
				AITEST_TRUE(TEXT("Exited State1 in Tree2"), LogOrder);
				LogOrder = Exec.Expect("Tree1_State2_TaskPrint", TEXT("ExitState12"));
				AITEST_TRUE(TEXT("Exited State2 in Tree1"), LogOrder);
				LogOrder = Exec.Expect("Tree1_State2_TaskPrint", ExitStateStr);
				AITEST_TRUE(TEXT("Exited State2 in Tree1"), LogOrder);

				if (bCompletedStatedCreateNewStates)
				{
					LogOrder = LogOrder.Then("Tree1_State1_TaskStand", ExitStateStr);
					AITEST_TRUE(TEXT("Exited State1 in Tree1"), LogOrder);
					LogOrder = Exec.Expect("Tree1_State1_TaskPrint", TEXT("ExitState11"));
					AITEST_TRUE(TEXT("Exited State1 in Tree1"), LogOrder);
					LogOrder = Exec.Expect("Tree1_State1_TaskPrint", ExitStateStr);
					AITEST_TRUE(TEXT("Exited State1 in Tree1"), LogOrder);
					LogOrder = Exec.Expect("Tree1_State1_TaskPrint", TEXT("EnterState11"));
					AITEST_TRUE(TEXT("Entered State1 in Tree1"), LogOrder);
					LogOrder = LogOrder.Then("Tree1_State1_TaskStand", EnterStateStr);
					AITEST_TRUE(TEXT("Entered State1 in Tree1"), LogOrder);
				}

				LogOrder = Exec.Expect("Tree1_State2_TaskPrint", TEXT("EnterState12"));
				AITEST_TRUE(TEXT("Entered State2 in Tree1"), LogOrder);
				LogOrder = LogOrder.Then("Tree1_State2_TaskPrint", EnterStateStr);
				AITEST_TRUE(TEXT("Entered State2 in Tree1"), LogOrder);
				LogOrder = Exec.Expect("Tree2_State1_TaskPrint", TEXT("EnterState21"));
				AITEST_TRUE(TEXT("Entered State1 in Tree2"), LogOrder);
				LogOrder = LogOrder.Then("Tree2_State1_TaskStand", EnterStateStr);
				AITEST_TRUE(TEXT("Entered State1 in Tree2"), LogOrder);
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
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionLinkedAssetFromCompletedParent, "System.MetaStory.Transition.LinkedAssetFromCompletedParent");

struct FMetaStoryTest_TransitionLinkedAssetWith2Root : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		/*
		- Tree1
			- Root1
				- State1 : LinkedAsset
		- Tree2
			- Root1
				- State1: Transition To Root3
			- Root2
				- State2
		*/

		UMetaStory& MetaStory1 = NewMetaStory();
		UMetaStory& MetaStory2 = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
			UMetaStoryState& Root1 = EditorData.AddSubTree(FName("Root1"));
			UMetaStoryState& State1 = Root1.AddChildState(FName("State1"));
			UMetaStoryState& Root2 = EditorData.AddSubTree(FName("Root2"));
			UMetaStoryState& State2 = Root2.AddChildState(FName("State2"));

			auto& Task1 = State1.AddTask<FTestTask_Stand>(FName("Tree2State1Task1"));
			Task1.GetNode().TicksToCompletion = 1;
			State1.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::GotoState, &State2);

			auto& Task2 = State2.AddTask<FTestTask_Stand>(FName(TEXT("Task1A")));
			Task2.GetNode().TicksToCompletion = 1;
			State2.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::Succeeded);

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory2);
			AITEST_TRUE("MetaStory should get compiled", bResult);
		}
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);
			UMetaStoryState& Root1 = EditorData.AddSubTree(FName("Root"));
			UMetaStoryState& State1 = Root1.AddChildState(FName("State1"), EMetaStoryStateType::LinkedAsset);

			State1.SetLinkedStateAsset(&MetaStory2);

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory1);
			AITEST_TRUE("MetaStory should get compiled", bResult);
		}

		FMetaStoryInstanceData InstanceData;
		{
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("MetaStory should init", bInitSucceeded);

			// Start and enter state
			EMetaStoryRunStatus Status = Exec.Start();
			AITEST_TRUE(TEXT("Valid states"), Exec.ExpectInActiveStates("Root", "State1", "Root1", "State1"));
			AITEST_TRUE(TEXT("Expected amount of frames."), InstanceData.GetExecutionState()->ActiveFrames.Num() == 2);
			Exec.LogClear();
		}
			{
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			EMetaStoryRunStatus Status = Exec.Tick(0.f);
			AITEST_TRUE(TEXT("Active states should stay the same"), Exec.ExpectInActiveStates("Root", "State1", "Root2", "State2"));
			AITEST_TRUE(TEXT("Expected amount of frames."), InstanceData.GetExecutionState()->ActiveFrames.Num() == 2);
			Exec.LogClear();
		}
		{
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_TransitionLinkedAssetWith2Root, "System.MetaStory.Transition.LinkedAssetWith2Root");

struct FMetaStoryTest_MultipleTransition_TemporaryFrame_GlobalTask : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		/*
		- Tree1
			- Root
				- State1 Transition1 -> Goto State2; Transition2 -> Goto State3
				- State2 : Tree2
				- State3

		- Tree2 : Global Tasks
			- Root : Tree3

		- Tree3 : Global Tasks
		*/

		UMetaStory& MetaStory1 = NewMetaStory();
		UMetaStory& MetaStory2 = NewMetaStory();
		UMetaStory& MetaStory3 = NewMetaStory();

		{
			UMetaStoryEditorData& Tree1EditorData = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);
			UMetaStoryState& Root = Tree1EditorData.AddSubTree(FName("Tree1Root"));
			UMetaStoryState& State1 = Root.AddChildState(FName("Tree1State1"));
			UMetaStoryState& State2 = Root.AddChildState(FName("Tree1State2"));
			UMetaStoryState& State3 = Root.AddChildState(FName("Tree1State3"));

			FMetaStoryTransition& LowPriorityTrans = State1.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &State2);
			LowPriorityTrans.Priority = EMetaStoryTransitionPriority::Low;
			FMetaStoryTransition& HighPriorityTrans = State1.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &State3);
			HighPriorityTrans.Priority = EMetaStoryTransitionPriority::High;

			State2.Type = EMetaStoryStateType::LinkedAsset;
			State2.SetLinkedStateAsset(&MetaStory2);
		}

		{
			UMetaStoryEditorData& Tree2EditorData = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
			TMetaStoryTypedEditorNode<FTestTask_StandNoTick>& TaskEditorNode = Tree2EditorData.AddGlobalTask<FTestTask_StandNoTick>(FName("Tree2Stand"));
			UMetaStoryState& Root = Tree2EditorData.AddSubTree(FName("Tree2Root"));
			Root.Type = EMetaStoryStateType::LinkedAsset;
			Root.SetLinkedStateAsset(&MetaStory3);
		}

		{
			UMetaStoryEditorData& Tree3EditorData = *Cast<UMetaStoryEditorData>(MetaStory3.EditorData);
			Tree3EditorData.AddGlobalTask<FTestTask_StandNoTick>(FName("Tree3Stand"));

			UMetaStoryState& Root = Tree3EditorData.AddSubTree(FName("Tree3Root"));
		}

		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory3);
			AITEST_TRUE("MetaStory should get compiled", bResult);
		}
		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory2);
			AITEST_TRUE("MetaStory should get compiled", bResult);
		}
		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory1);
			AITEST_TRUE("MetaStory should get compiled", bResult)
		}

		{
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("MetaStory should init", bInitSucceeded);

			EMetaStoryRunStatus Status = Exec.Start();
			AITEST_TRUE(TEXT("Valid states"), Exec.ExpectInActiveStates("Tree1Root", "Tree1State1"));
			Exec.LogClear();

			Exec.Tick(0.1f);

			FTestMetaStoryExecutionContext::FLogOrder LogOrder = FTestMetaStoryExecutionContext::FLogOrder(Exec, 0);
			LogOrder = LogOrder.Then("Tree2Stand", TEXT("EnterState=Changed")).Then("Tree3Stand", TEXT("EnterState=Changed"));
			AITEST_TRUE(TEXT("Enter Global tasks on temporary frames correctly"), LogOrder);

			LogOrder = LogOrder.Then("Tree3Stand", TEXT("ExitStopped"))
				.Then("Tree3Stand", TEXT("ExitState=Changed"))
				.Then("Tree2Stand", TEXT("ExitStopped"))
				.Then("Tree2Stand", TEXT("ExitState=Changed"));

			AITEST_TRUE(TEXT("Exit Global tasks on temporary frames correctly"), LogOrder);

			AITEST_TRUE(TEXT("Valid states"), Exec.ExpectInActiveStates("Tree1Root", "Tree1State3"));

			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_MultipleTransition_TemporaryFrame_GlobalTask, "System.MetaStory.Transition.MultipleTransition.TemporaryFrame.GlobalTask");

} // namespace UE::MetaStory::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
