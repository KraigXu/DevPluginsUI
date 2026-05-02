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

namespace Private
{
	struct FScopedCVarBool
	{
		FScopedCVarBool(const TCHAR* VariableName)
		{
			CVar = IConsoleManager::Get().FindConsoleVariable(VariableName);
			check(CVar);
			bPreviousValue = CVar->GetBool();
		}
		~FScopedCVarBool()
		{
			CVar->SetWithCurrentPriority(bPreviousValue);
		}

		FScopedCVarBool(const FScopedCVarBool&) = delete;
		FScopedCVarBool& operator=(const FScopedCVarBool&) = delete;

		void Set(bool NewValue)
		{
			CVar->SetWithCurrentPriority(NewValue);
			check(CVar->GetBool() == NewValue);
		}

	private:
		IConsoleVariable* CVar;
		bool bPreviousValue;
	};
} // namespace Private

struct FMetaStoryTest_FailEnterLinkedAsset : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FMetaStoryCompilerLog Log;

		// Asset 2
		UMetaStory& MetaStory2 = NewMetaStory();
		UMetaStoryEditorData& EditorData2 = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
		UMetaStoryState& Root2 = EditorData2.AddSubTree(FName(TEXT("Root2")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task2 = Root2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& GlobalTask2 = EditorData2.AddGlobalTask<FTestTask_Stand>(FName(TEXT("GlobalTask2")));
		GlobalTask2.GetInstanceData().Value = 123;

		// Always failing enter condition
		TMetaStoryTypedEditorNode<FMetaStoryCompareIntCondition>& IntCond2 = Root2.AddEnterCondition<FMetaStoryCompareIntCondition>();
		EditorData2.AddPropertyBinding(GlobalTask2, TEXT("Value"), IntCond2, TEXT("Left"));
		IntCond2.GetInstanceData().Right = 0;

		FMetaStoryCompiler Compiler2(Log);
		const bool bResult2 = Compiler2.Compile(MetaStory2);
		AITEST_TRUE(TEXT("MetaStory2 should get compiled"), bResult2);

		// Main asset
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root1")));
		UMetaStoryState& A1 = Root.AddChildState(FName(TEXT("A1")), EMetaStoryStateType::LinkedAsset);
		A1.SetLinkedStateAsset(&MetaStory2);

		UMetaStoryState& B1 = Root.AddChildState(FName(TEXT("B1")), EMetaStoryStateType::State);
		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task1 = B1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));

		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		const TCHAR* EnterStateStr(TEXT("EnterState"));
		const TCHAR* ExitStateStr(TEXT("ExitState"));

		{
			EMetaStoryRunStatus Status = EMetaStoryRunStatus::Unset;
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

			Status = Exec.Start();
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("MetaStory should enter GlobalTask2"), Exec.Expect(GlobalTask2.GetName(), EnterStateStr));
			AITEST_TRUE(TEXT("MetaStory should exit GlobalTask2"), Exec.Expect(GlobalTask2.GetName(), ExitStateStr));
			AITEST_FALSE(TEXT("MetaStory should not enter Task2"), Exec.Expect(Task2.GetName(), EnterStateStr));
			AITEST_TRUE(TEXT("MetaStory should enter Task1"), Exec.Expect(Task1.GetName(), EnterStateStr));

			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_FailEnterLinkedAsset, "System.MetaStory.LinkedAsset.FailEnter");

struct FMetaStoryTest_EnterAndExitLinkedAsset : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FMetaStoryCompilerLog Log;

		// Tree1
		//  Root1
		//   A1 (linked to Tree2) OnCompleted->B1
		//   B1 Task1
		// Tree2 GlobalTask2 => 2 ticks
		//  Root2 Task2 => 1 tick

		// Asset 2
		UMetaStory& MetaStory2 = NewMetaStory();
		UMetaStoryEditorData& EditorData2 = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
		UMetaStoryState& Root2 = EditorData2.AddSubTree(FName(TEXT("Root2")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task2 = Root2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		Task2.GetNode().TicksToCompletion = 1;
		TMetaStoryTypedEditorNode<FTestTask_Stand>& GlobalTask2 = EditorData2.AddGlobalTask<FTestTask_Stand>(FName(TEXT("GlobalTask2")));
		GlobalTask2.GetNode().TicksToCompletion = 2;
		{
			FMetaStoryCompiler Compiler2(Log);
			const bool bResult2 = Compiler2.Compile(MetaStory2);
			AITEST_TRUE(TEXT("MetaStory2 should get compiled"), bResult2);
		}

		// Main asset
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root1")));
		UMetaStoryState& A1 = Root.AddChildState(FName(TEXT("A1")), EMetaStoryStateType::LinkedAsset);
		A1.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::NextState);
		A1.SetLinkedStateAsset(&MetaStory2);

		UMetaStoryState& B1 = Root.AddChildState(FName(TEXT("B1")), EMetaStoryStateType::State);
		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task1 = B1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 1;
		{
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory);
			AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
		}

		const TCHAR* EnterStateStr = TEXT("EnterState");
		const TCHAR* SustainedEnterStateStr = TEXT("EnterState=Sustained");
		const TCHAR* ChangedEnterStateStr = TEXT("EnterState=Changed");
		const TCHAR* ExitStateStr = TEXT("ExitState");
		const TCHAR* SustainedExitStateStr = TEXT("ExitState=Sustained");
		const TCHAR* ChangedExitStateStr = TEXT("ExitState=Changed");
		const TCHAR* StateCompletedStateStr = TEXT("StateCompleted");
		const TCHAR* TickStr = TEXT("Tick");

		{
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);
			{
				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL("Start should complete with Running", Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE("MetaStory should enter GlobalTask2", Exec.Expect(GlobalTask2.GetName(), ChangedEnterStateStr));
				AITEST_FALSE("MetaStory should not exit GlobalTask2", Exec.Expect(GlobalTask2.GetName(), ChangedExitStateStr));
				AITEST_FALSE("MetaStory should not exit GlobalTask2", Exec.Expect(GlobalTask2.GetName(), SustainedExitStateStr));
				AITEST_TRUE("MetaStory should enter Task2", Exec.Expect(Task2.GetName(), ChangedEnterStateStr));
				AITEST_FALSE("MetaStory should not exit Task2", Exec.Expect(Task2.GetName(), ChangedExitStateStr));
				AITEST_FALSE("MetaStory should not exit Task2", Exec.Expect(Task2.GetName(), SustainedExitStateStr));
				AITEST_FALSE("MetaStory should not enter Task1", Exec.Expect(Task1.GetName(), EnterStateStr));
				Exec.LogClear();
			}
			{
				// Task2 completes.
				EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL("Tick should complete with Running", Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("MetaStory should exit the root state but not the global"),
					Exec.Expect(GlobalTask2.GetName(), TickStr)
					.Then(Task2.GetName(), TickStr)
					.Then(Task2.GetName(), StateCompletedStateStr)
					.Then(Task2.GetName(), ChangedExitStateStr)
					.Then(GlobalTask2.GetName(), ChangedExitStateStr)
					.Then(Task1.GetName(), ChangedEnterStateStr)
				);
				AITEST_FALSE("MetaStory should not tick Tasks1", Exec.Expect(Task1.GetName(), TickStr));
				AITEST_FALSE("MetaStory should not exit Task1", Exec.Expect(Task1.GetName(), ExitStateStr));
				Exec.LogClear();
			}
			{
				Exec.Stop();
			}
			// Change the order, the global completes before task2. Task2 should not tick
			{
				Task2.GetNode().TicksToCompletion = 1;
				GlobalTask2.GetNode().TicksToCompletion = 1;
				FMetaStoryCompiler Compiler2(Log);
				const bool bResult2 = Compiler2.Compile(MetaStory2);
				AITEST_TRUE(TEXT("MetaStory2 should get compiled"), bResult2);
			}
			{
				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL("Start should complete with Running", Status, EMetaStoryRunStatus::Running);
				Exec.LogClear();
			}
			{
				// GlobalTask2 completes Task2 won't tick and won't complete.
				EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL("Tick should complete with Running", Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("MetaStory should exit the root state and the global"),
					Exec.Expect(GlobalTask2.GetName(), TickStr)
					.Then(Task2.GetName(), ChangedExitStateStr)
					.Then(GlobalTask2.GetName(), ChangedExitStateStr)
					.Then(Task1.GetName(), EnterStateStr)
				);
				AITEST_FALSE("MetaStory should not tick Tasks2", Exec.Expect(Task2.GetName(), TickStr));
				AITEST_FALSE("MetaStory should not tick Tasks1", Exec.Expect(Task1.GetName(), TickStr));
				AITEST_FALSE("MetaStory should not exit Task1", Exec.Expect(Task1.GetName(), ExitStateStr));
				Exec.LogClear();
			}

			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_EnterAndExitLinkedAsset, "System.MetaStory.LinkedAsset.EnterAndExit");

struct FMetaStoryTest_EnterAndExitLinkedAsset2 : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FMetaStoryCompilerLog Log;

		// Tree1
		//  Root1 Task0
		//   A1 (linked to Tree2) no transition
		//   B1 Task1
		// Tree2 GlobalTask2 => 2 ticks
		//  Root2 Task2 => 1 tick

		// Asset 2
		UMetaStory& MetaStory2 = NewMetaStory();
		UMetaStoryEditorData& EditorData2 = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
		UMetaStoryState& Root2 = EditorData2.AddSubTree(FName(TEXT("Root2")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task2 = Root2.AddTask<FTestTask_Stand>(FName(TEXT("Task2")));
		Task2.GetNode().TicksToCompletion = 1;
		TMetaStoryTypedEditorNode<FTestTask_Stand>& GlobalTask2 = EditorData2.AddGlobalTask<FTestTask_Stand>(FName(TEXT("GlobalTask2")));
		GlobalTask2.GetNode().TicksToCompletion = 2;
		{
			FMetaStoryCompiler Compiler2(Log);
			const bool bResult2 = Compiler2.Compile(MetaStory2);
			AITEST_TRUE(TEXT("MetaStory2 should get compiled"), bResult2);
		}

		// Main asset
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root1")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task0 = Root.AddTask<FTestTask_Stand>(FName(TEXT("Task0")));
		Task0.GetNode().TicksToCompletion = 999999;

		UMetaStoryState& A1 = Root.AddChildState(FName(TEXT("A1")), EMetaStoryStateType::LinkedAsset);
		A1.SetLinkedStateAsset(&MetaStory2);

		UMetaStoryState& B1 = Root.AddChildState(FName(TEXT("B1")), EMetaStoryStateType::State);
		TMetaStoryTypedEditorNode<FTestTask_Stand>& Task1 = B1.AddTask<FTestTask_Stand>(FName(TEXT("Task1")));
		Task1.GetNode().TicksToCompletion = 1;
		{
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory);
			AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
		}

		const TCHAR* EnterStateStr = TEXT("EnterState");
		const TCHAR* SustainedEnterStateStr = TEXT("EnterState=Sustained");
		const TCHAR* ChangedEnterStateStr = TEXT("EnterState=Changed");
		const TCHAR* ExitStateStr = TEXT("ExitState");
		const TCHAR* SustainedExitStateStr = TEXT("ExitState=Sustained");
		const TCHAR* ChangedExitStateStr = TEXT("ExitState=Changed");
		const TCHAR* StateCompletedStateStr = TEXT("StateCompleted");
		const TCHAR* TickStr = TEXT("Tick");

		{
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);
			{
				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL("Start should complete with Running", Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE("MetaStory should enter GlobalTask2", Exec.Expect(GlobalTask2.GetName(), ChangedEnterStateStr));
				AITEST_FALSE("MetaStory should not exit GlobalTask2", Exec.Expect(GlobalTask2.GetName(), ChangedExitStateStr));
				AITEST_FALSE("MetaStory should not exit GlobalTask2", Exec.Expect(GlobalTask2.GetName(), SustainedExitStateStr));
				AITEST_TRUE("MetaStory should enter Task2", Exec.Expect(Task2.GetName(), ChangedEnterStateStr));
				AITEST_FALSE("MetaStory should not exit Task2", Exec.Expect(Task2.GetName(), ChangedExitStateStr));
				AITEST_FALSE("MetaStory should not exit Task2", Exec.Expect(Task2.GetName(), SustainedExitStateStr));
				AITEST_FALSE("MetaStory should not enter Task1", Exec.Expect(Task1.GetName(), EnterStateStr));
				Exec.LogClear();
			}
			{
				// Task2 completes. The linked state didn't complete. Global is sustained.
				EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL("Tick should complete with Running", Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("MetaStory should exit the root state but not the global"),
					Exec.Expect(Task0.GetName(), TickStr)
					.Then(GlobalTask2.GetName(), TickStr)
					.Then(Task2.GetName(), TickStr)
					.Then(Task2.GetName(), StateCompletedStateStr)
					.Then(Task0.GetName(), StateCompletedStateStr)
					.Then(Task2.GetName(), ChangedExitStateStr)
					.Then(GlobalTask2.GetName(), SustainedExitStateStr)
					.Then(Task0.GetName(), SustainedExitStateStr)
					.Then(Task0.GetName(), SustainedEnterStateStr)
					.Then(GlobalTask2.GetName(), SustainedEnterStateStr)
					.Then(Task2.GetName(), ChangedEnterStateStr)
				);
				AITEST_FALSE("MetaStory should not tick Task1", Exec.Expect(Task1.GetName(), TickStr));
				AITEST_FALSE("MetaStory should not exit Task1", Exec.Expect(Task1.GetName(), ExitStateStr));
				Exec.LogClear();
			}
			{
				Exec.Stop();
			}
			// Change the order, the global completes before task2. Task2 should not tick.
			{
				Task2.GetNode().TicksToCompletion = 1;
				GlobalTask2.GetNode().TicksToCompletion = 1;
				FMetaStoryCompiler Compiler2(Log);
				const bool bResult2 = Compiler2.Compile(MetaStory2);
				AITEST_TRUE(TEXT("MetaStory2 should get compiled"), bResult2);
			}
			{
				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL("Start should complete with Running", Status, EMetaStoryRunStatus::Running);
				Exec.LogClear();
			}
			{
				// GlobalTask2 completes. The tree completed. Task2 won't tick and won't complete.
				EMetaStoryRunStatus Status = Exec.Tick(0.1f);
				AITEST_EQUAL("Tick should complete with Running", Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("MetaStory should exit the root state and the global"),
					Exec.Expect(GlobalTask2.GetName(), TickStr)
					.Then(Task2.GetName(), ChangedExitStateStr)
					.Then(GlobalTask2.GetName(), ChangedExitStateStr)
					.Then(Task0.GetName(), SustainedExitStateStr)
					.Then(Task0.GetName(), SustainedEnterStateStr)
					.Then(Task2.GetName(), EnterStateStr)
				);
				AITEST_TRUE(TEXT("MetaStory should enter a new global task2"), Exec.Expect(GlobalTask2.GetName(), ChangedEnterStateStr));
				AITEST_FALSE("MetaStory should not tick Tasks2", Exec.Expect(Task2.GetName(), TickStr));
				AITEST_FALSE("MetaStory should not tick Tasks1", Exec.Expect(Task1.GetName(), TickStr));
				AITEST_FALSE("MetaStory should not exit Task1", Exec.Expect(Task1.GetName(), EnterStateStr));
				AITEST_FALSE("MetaStory should not exit Task1", Exec.Expect(Task1.GetName(), ExitStateStr));
				Exec.LogClear();
			}

			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_EnterAndExitLinkedAsset2, "System.MetaStory.LinkedAsset.EnterAndExit2");

struct FMetaStoryTest_MultipleSameLinkedAsset : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//	Root
		//		StateA -> Next
		//		StateB -> Next
		//		StateLinkedTreeA (Tree2) -> Next
		//		StateLinkedTreeB (Tree2) -> Next
		//		StateLinkedTreeC (Tree2) -> Next
		//		StateC -> Succeeded
		//Tree 2
		//  Global task and parameter
		//	RootE
		//		StateA (with transition OnTick to succeeded)

		FMetaStoryCompilerLog Log;

		// Asset 2
		UMetaStory& MetaStory2 = NewMetaStory();
		FGuid RootParameter_ValueID;
		{
			UMetaStoryEditorData& EditorData2 = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
			{
				// Parameters
				FInstancedPropertyBag& RootPropertyBag = GetRootPropertyBag(EditorData2);
				RootPropertyBag.AddProperty("Value", EPropertyBagPropertyType::Int32);
				RootPropertyBag.SetValueInt32("Value", -111);
				RootParameter_ValueID = RootPropertyBag.FindPropertyDescByName("Value")->ID;

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask = EditorData2.AddGlobalTask<FTestTask_PrintValue>("Tree2GlobalTaskA");
				GlobalTask.GetInstanceData().Value = -1;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(EditorData2.GetRootParametersGuid(), TEXT("Value")), FPropertyBindingPath(GlobalTask.ID, TEXT("Value")));
			}

			UMetaStoryState& Root = EditorData2.AddSubTree("Tree2StateRoot");
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = Root.AddTask<FTestTask_PrintValue>("Tree2StateRootTaskA");
				Task1.GetInstanceData().Value = -2;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(EditorData2.GetRootParametersGuid(), TEXT("Value")), FPropertyBindingPath(Task1.ID, TEXT("Value")));
			}
			{
				UMetaStoryState& State = Root.AddChildState("Tree2StateA", EMetaStoryStateType::State);
				FMetaStoryTransition& Transition = State.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::Succeeded);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = State.AddTask<FTestTask_PrintValue>("Tree2StateATaskA");
				Task1.GetInstanceData().Value = -2;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(EditorData2.GetRootParametersGuid(), TEXT("Value")), FPropertyBindingPath(Task1.ID, TEXT("Value")));
			}

			FMetaStoryCompiler Compiler2(Log);
			const bool bResult2 = Compiler2.Compile(MetaStory2);
			AITEST_TRUE(TEXT("MetaStory2 should get compiled"), bResult2);
		}

		// Main asset
		UMetaStory& MetaStory1 = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask = EditorData.AddGlobalTask<FTestTask_PrintValue>("Tree1GlobalTaskA");
				GlobalTask.GetInstanceData().Value = 99;
			}

			UMetaStoryState& Root = EditorData.AddSubTree("Tree1StateRoot");
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = Root.AddTask<FTestTask_PrintValue>("Tree1StateRootTaskA");
				Task1.GetInstanceData().Value = 88;
			}
			{
				UMetaStoryState& StateB = Root.AddChildState("Tree1StateA", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = StateB.AddTask<FTestTask_PrintValue>("Tree1StateATaskA");
				Task.GetInstanceData().Value = 1;
				FMetaStoryTransition& Transition = StateB.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::NextState);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}
			{
				UMetaStoryState& StateB = Root.AddChildState("Tree1StateB", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = StateB.AddTask<FTestTask_PrintValue>("Tree1StateBTaskA");
				Task.GetInstanceData().Value = 2;
				FMetaStoryTransition& Transition = StateB.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::NextState);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}
			{
				UMetaStoryState& C1 = Root.AddChildState("Tree1StateLinkedTreeA", EMetaStoryStateType::LinkedAsset);
				C1.SetLinkedStateAsset(&MetaStory2);
				C1.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				C1.SetParametersPropertyOverridden(RootParameter_ValueID, true);
				C1.Parameters.Parameters.SetValueInt32("Value", 111);
			}
			{
				UMetaStoryState& C2 = Root.AddChildState("Tree1StateLinkedTreeB", EMetaStoryStateType::LinkedAsset);
				C2.SetLinkedStateAsset(&MetaStory2);
				C2.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				C2.SetParametersPropertyOverridden(RootParameter_ValueID, true);
				C2.Parameters.Parameters.SetValueInt32("Value", 222);
			}
			{
				UMetaStoryState& C3 = Root.AddChildState("Tree1StateLinkedTreeC", EMetaStoryStateType::LinkedAsset);
				C3.SetLinkedStateAsset(&MetaStory2);
				C3.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				C3.SetParametersPropertyOverridden(RootParameter_ValueID, true);
				C3.Parameters.Parameters.SetValueInt32("Value", 333);
			}
			{
				UMetaStoryState& StateC = Root.AddChildState("Tree1StateC", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = StateC.AddTask<FTestTask_PrintValue>("Tree1StateCTaskA");
				Task.GetInstanceData().Value = 3;
				FMetaStoryTransition& Transition = StateC.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &Root);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}

			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory1);
			AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
		}

		Private::FScopedCVarBool CVarTickGlobalNodesWithHierarchy = TEXT("MetaStory.TickGlobalNodesFollowingTreeHierarchy");
		for (int32 Counter = 0; Counter < 2; ++Counter)
		{
			const bool bTickGlobalNodesWithHierarchy = Counter == 0;
			CVarTickGlobalNodesWithHierarchy.Set(bTickGlobalNodesWithHierarchy);

			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			{
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);
			}
			{
				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("Start should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateA"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("EnterState99"));
				AITEST_TRUE(TEXT("Start should enter Tree1GlobalTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("EnterState88"));
				AITEST_TRUE(TEXT("Start should enter Tree1StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateATaskA", TEXT("EnterState1"));
				AITEST_TRUE(TEXT("Start should enter Tree1StateATaskA"), LogOrder);
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.5f); // over tick, should trigger
				AITEST_EQUAL(TEXT("1st Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("1st Tick should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateA"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick99"));
				AITEST_TRUE(TEXT("1st should tick tasks Tree1GlobalTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick88"));
				AITEST_TRUE(TEXT("1st should tick tasks Tree1StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateATaskA", TEXT("Tick1"));
				AITEST_TRUE(TEXT("1st should tick tasks Tree1StateATaskA"), LogOrder);
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.0f);
				AITEST_EQUAL(TEXT("2nd Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("2nd Tick should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateB"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick99"));
				AITEST_TRUE(TEXT("2nd Tick should tick Tree1GlobalTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick88"));
				AITEST_TRUE(TEXT("2nd Tick should tick Tree1StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateATaskA", TEXT("Tick1"));
				AITEST_TRUE(TEXT("2nd Tick should tick Tree1StateATaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateATaskA", TEXT("ExitState1"));
				AITEST_TRUE(TEXT("2nd Tick should exit Tree1StateATaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateBTaskA", TEXT("EnterState2"));
				AITEST_TRUE(TEXT("2nd Tick should enter Tree1StateBTaskA"), LogOrder);
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.0f);
				AITEST_EQUAL(TEXT("3rd Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("3rd Tick should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateB"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick99"));
				AITEST_TRUE(TEXT("3rd should tick Tree1GlobalTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick88"));
				AITEST_TRUE(TEXT("3rd should tick Tree1StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateBTaskA", TEXT("Tick2"));
				AITEST_TRUE(TEXT("3rd should tick Tree1StateBTaskA"), LogOrder);
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.0f);
				AITEST_EQUAL(TEXT("4th Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("4th Tick should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateLinkedTreeA", "Tree2StateRoot", "Tree2StateA"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick99"));
				AITEST_TRUE(TEXT("4th Tick should tick Tree1GlobalTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick88"));
				AITEST_TRUE(TEXT("4th Tick should tick Tree1StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateBTaskA", TEXT("Tick2"));
				AITEST_TRUE(TEXT("4th Tick should tick Tree1StateBTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateBTaskA", TEXT("ExitState2"));
				AITEST_TRUE(TEXT("4th Tick should exit Tree1StateBTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("EnterState111"));
				AITEST_TRUE(TEXT("4th Tick should enter Tree2StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree2StateATaskA", TEXT("EnterState111"));
				AITEST_TRUE(TEXT("4th Tick should enter Tree2StateATaskA"), LogOrder);
				AITEST_TRUE(TEXT("4th Tick should tick tasks"), Exec.Expect("Tree2GlobalTaskA", TEXT("EnterState111")));
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(0.001f);
				AITEST_EQUAL(TEXT("5th Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("5th Tick should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateLinkedTreeA", "Tree2StateRoot", "Tree2StateA"));
				if (bTickGlobalNodesWithHierarchy)
				{
					FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick99"));
					AITEST_TRUE(TEXT("5h Tick should tick Tree1GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick88"));
					AITEST_TRUE(TEXT("5h Tick should tick Tree1StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("5h Tick should tick Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("5h Tick should tick Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateATaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("5h Tick should tick Tree2StateATaskA"), LogOrder);
				}
				else
				{
					FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick99"));
					AITEST_TRUE(TEXT("5h Tick should tick Tree1GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("5h Tick should tick Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick88"));
					AITEST_TRUE(TEXT("5h Tick should tick Tree1StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("5h Tick should tick Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateATaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("5h Tick should tick Tree2StateATaskA"), LogOrder);
				}
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.0f);
				AITEST_EQUAL(TEXT("6th Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("6th Tick should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateLinkedTreeB", "Tree2StateRoot", "Tree2StateA"));
				if (bTickGlobalNodesWithHierarchy)
				{
					FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick99"));
					AITEST_TRUE(TEXT("6th Tick should tick Tree1GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick88"));
					AITEST_TRUE(TEXT("6th Tick should tick Tree1StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("6th Tick should tick Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("6th Tick should tick Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateATaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("6th Tick should tick Tree2StateATaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateATaskA", TEXT("ExitState111"));
					AITEST_TRUE(TEXT("6th Tick should exit Tree2StateATaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("ExitState111"));
					AITEST_TRUE(TEXT("6th Tick should exit Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("ExitState111"));
					AITEST_TRUE(TEXT("6th Tick should exit Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("EnterState222"));
					AITEST_TRUE(TEXT("6th Tick should enter Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateATaskA", TEXT("EnterState222"));
					AITEST_TRUE(TEXT("6th Tick should enter Tree2StateATaskA"), LogOrder);
					AITEST_TRUE(TEXT("6th Tick should enter Tree2GlobalTaskA"), Exec.Expect("Tree2GlobalTaskA", TEXT("EnterState222")));
				}
				else
				{
					FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick99"));
					AITEST_TRUE(TEXT("6th Tick should tick Tree1GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("6th Tick should tick Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick88"));
					AITEST_TRUE(TEXT("6th Tick should tick Tree1StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("6th Tick should tick Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateATaskA", TEXT("Tick111"));
					AITEST_TRUE(TEXT("6th Tick should tick Tree2StateATaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateATaskA", TEXT("ExitState111"));
					AITEST_TRUE(TEXT("6th Tick should exit Tree2StateATaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("ExitState111"));
					AITEST_TRUE(TEXT("6th Tick should exit Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("ExitState111"));
					AITEST_TRUE(TEXT("6th Tick should exit Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("EnterState222"));
					AITEST_TRUE(TEXT("6th Tick should enter Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateATaskA", TEXT("EnterState222"));
					AITEST_TRUE(TEXT("6th Tick should enter Tree2StateATaskA"), LogOrder);
					AITEST_TRUE(TEXT("6th Tick should enter Tree2GlobalTaskA"), Exec.Expect("Tree2GlobalTaskA", TEXT("EnterState222")));
				}
				Exec.LogClear();
			}
			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_MultipleSameLinkedAsset, "System.MetaStory.LinkedAsset.MultipleSameTree");

struct FMetaStoryTest_EmptyStateWithTickTransitionLinkedAsset : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//	Root
		//		StateA -> Next
		//		StateLinkedTree (Tree2) -> Next
		//		StateB -> Root
		//Tree 2
		//  Global task and parameter
		//	Root
		//		FailState (condition false)
		//		StateA (condition true and with transition OnTick to succeeded)

		FMetaStoryCompilerLog Log;

		// Asset 2
		UMetaStory& MetaStory2 = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
			TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask = EditorData.AddGlobalTask<FTestTask_PrintValue>("Tree2GlobalTaskA");
			GlobalTask.GetInstanceData().Value = 21;

			UMetaStoryState& Root = EditorData.AddSubTree("Tree2StateRoot");
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = Root.AddTask<FTestTask_PrintValue>("Tree2StateRootTaskA");
				Task.GetInstanceData().Value = 22;
			}
			{
				UMetaStoryState& State = Root.AddChildState("Tree2StateFail", EMetaStoryStateType::State);
				// Add auto fails condition
				auto& Condition = State.AddEnterCondition<FMetaStoryTestBooleanCondition>();
				Condition.GetInstanceData().bSuccess = false;

				// Should never see
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = State.AddTask<FTestTask_PrintValue>("Tree2StateFailTaskA");
				Task.GetInstanceData().Value = 23;
			}
			{
				UMetaStoryState& State = Root.AddChildState("Tree2StateB", EMetaStoryStateType::State);

				// Add auto success condition
				auto& Condition = State.AddEnterCondition<FMetaStoryTestBooleanCondition>();
				Condition.GetInstanceData().bSuccess = true;

				FMetaStoryTransition& Transition = State.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::Succeeded);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}


			FMetaStoryCompiler Compiler2(Log);
			const bool bResult2 = Compiler2.Compile(MetaStory2);
			AITEST_TRUE(TEXT("MetaStory2 should get compiled"), bResult2);
		}

		// Main asset
		UMetaStory& MetaStory1 = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);

			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask = EditorData.AddGlobalTask<FTestTask_PrintValue>("Tree1GlobalTaskA");
				GlobalTask.GetInstanceData().Value = 11;
			}
			UMetaStoryState& Root = EditorData.AddSubTree("Tree1StateRoot");
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = Root.AddTask<FTestTask_PrintValue>("Tree1StateRootTaskA");
				Task.GetInstanceData().Value = 12;
			}
			{
				UMetaStoryState& State = Root.AddChildState("Tree1StateA", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = State.AddTask<FTestTask_PrintValue>("Tree1StateATaskA");
				Task.GetInstanceData().Value = 13;

				FMetaStoryTransition& Transition = State.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::NextState);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}
			{
				UMetaStoryState& C1 = Root.AddChildState("Tree1StateLinkedTree", EMetaStoryStateType::LinkedAsset);
				C1.SetLinkedStateAsset(&MetaStory2);
				C1.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
			}
			{
				UMetaStoryState& State = Root.AddChildState("Tree1StateB", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = State.AddTask<FTestTask_PrintValue>("Tree1StateBTaskA");
				Task.GetInstanceData().Value = 14;
				FMetaStoryTransition& Transition = State.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &Root);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0;
			}

			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory1);
			AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
		}

		Private::FScopedCVarBool CVarTickGlobalNodesWithHierarchy = TEXT("MetaStory.TickGlobalNodesFollowingTreeHierarchy");
		for (int32 Counter = 0; Counter < 2; ++Counter)
		{
			const bool bTickGlobalNodesWithHierarchy = Counter == 0;
			CVarTickGlobalNodesWithHierarchy.Set(bTickGlobalNodesWithHierarchy);

			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			{
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);
			}
			{
				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("EnterState11"));
				AITEST_TRUE(TEXT("Start enters in the correct order Tree1GlobalTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("EnterState12"));
				AITEST_TRUE(TEXT("Start enters in the correct order Tree1StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateATaskA", TEXT("EnterState13"));
				AITEST_TRUE(TEXT("Start enters in the correct order Tree1StateATaskA"), LogOrder);
				AITEST_TRUE(TEXT("Start should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateA"));
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.5f); // over tick, should trigger
				AITEST_EQUAL(TEXT("1st Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick11"));
				AITEST_TRUE(TEXT("1st Tick should tick Tree1GlobalTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick12"));
				AITEST_TRUE(TEXT("1st Tick should tick Tree1StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateATaskA", TEXT("Tick13"));
				AITEST_TRUE(TEXT("1st Tick should tick Tree1StateATaskA"), LogOrder);
				AITEST_TRUE(TEXT("Start should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateA"));
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.0f);
				AITEST_EQUAL(TEXT("2nd Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick11"));
				AITEST_TRUE(TEXT("2nd Tick should tick Tree1GlobalTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick12"));
				AITEST_TRUE(TEXT("2nd Tick should tick Tree1StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateATaskA", TEXT("Tick13"));
				AITEST_TRUE(TEXT("2nd Tick should tick Tree1StateATaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("EnterState21"));
				AITEST_TRUE(TEXT("2nd Tick should enter Tree2GlobalTaskA"), LogOrder);
				LogOrder = LogOrder.Then("MetaStory Test Boolean Condition", TEXT("TestCondition=0"));
				AITEST_TRUE(TEXT("2nd Tick should test Bool"), LogOrder);
				LogOrder = LogOrder.Then("MetaStory Test Boolean Condition", TEXT("TestCondition=1"));
				AITEST_TRUE(TEXT("2nd Tick should test Bool"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateATaskA", TEXT("ExitState13"));
				AITEST_TRUE(TEXT("2nd Tick should exit Tree1StateATaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("EnterState22"));
				AITEST_TRUE(TEXT("2nd Tick should enter Tree2StateRootTaskA"), LogOrder);
				AITEST_FALSE(TEXT("2nd Tick should not enter the fail state."), Exec.Expect("Tree2StateFailTaskA"));
				AITEST_TRUE(TEXT("2nd Tick should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateLinkedTree", "Tree2StateRoot", "Tree2StateB"));
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.0f);
				AITEST_EQUAL(TEXT("3rd Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("3rd Tick should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateLinkedTree", "Tree2StateRoot", "Tree2StateB"));
				if (bTickGlobalNodesWithHierarchy)
				{
					FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick11"));
					AITEST_TRUE(TEXT("3rd Tick should tick Tree1GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick12"));
					AITEST_TRUE(TEXT("3rd Tick should tick Tree1StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("Tick21"));
					AITEST_TRUE(TEXT("3rd Tick should tick Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("Tick22"));
					AITEST_TRUE(TEXT("3rd Tick should tick Tree2StateRootTaskA"), LogOrder);
				}
				else
				{
					FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick11"));
					AITEST_TRUE(TEXT("3rd Tick should tick Tree1GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("Tick21"));
					AITEST_TRUE(TEXT("3rd Tick should tick Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick12"));
					AITEST_TRUE(TEXT("3rd Tick should tick Tree1StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("Tick22"));
					AITEST_TRUE(TEXT("3rd Tick should tick Tree2StateRootTaskA"), LogOrder);
				}
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.0f);
				AITEST_EQUAL(TEXT("4th Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				if (bTickGlobalNodesWithHierarchy)
				{
					FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick11"));
					AITEST_TRUE(TEXT("4th Tick should tick Tree1GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick12"));
					AITEST_TRUE(TEXT("4th Tick should tick Tree1StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("Tick21"));
					AITEST_TRUE(TEXT("4th Tick should tick Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("Tick22"));
					AITEST_TRUE(TEXT("4th Tick should tick Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("ExitState22"));
					AITEST_TRUE(TEXT("4th Tick should exit Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("ExitState21"));
					AITEST_TRUE(TEXT("4th Tick should exit Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree1StateBTaskA", TEXT("EnterState14"));
					AITEST_TRUE(TEXT("4th Tick should enter Tree1StateBTaskA"), LogOrder);
				}
				else
				{
					FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick11"));
					AITEST_TRUE(TEXT("4th Tick should tick Tree1GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("Tick21"));
					AITEST_TRUE(TEXT("4th Tick should tick Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick12"));
					AITEST_TRUE(TEXT("4th Tick should tick Tree1StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("Tick22"));
					AITEST_TRUE(TEXT("4th Tick should tick Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("ExitState22"));
					AITEST_TRUE(TEXT("4th Tick should exit Tree2StateRootTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree2GlobalTaskA", TEXT("ExitState21"));
					AITEST_TRUE(TEXT("4th Tick should exit Tree2GlobalTaskA"), LogOrder);
					LogOrder = LogOrder.Then("Tree1StateBTaskA", TEXT("EnterState14"));
					AITEST_TRUE(TEXT("4th Tick should enter Tree1StateBTaskA"), LogOrder);
				}
				AITEST_TRUE(TEXT("4th Tick should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateB"));
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(0.001f);
				AITEST_EQUAL(TEXT("5th Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1GlobalTaskA", TEXT("Tick11"));
				AITEST_TRUE(TEXT("5th Tick should tick Tree1GlobalTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateRootTaskA", TEXT("Tick12"));
				AITEST_TRUE(TEXT("5th Tick should tick Tree1StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree1StateBTaskA", TEXT("Tick14"));
				AITEST_TRUE(TEXT("5th Tick should tick Tree1StateBTaskA"), LogOrder);
				AITEST_TRUE(TEXT("5th Tick should be in the correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1StateB"));
				Exec.LogClear();
			}
			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_EmptyStateWithTickTransitionLinkedAsset, "System.MetaStory.LinkedAsset.EmptyStateWithTickTransition");

struct FMetaStoryTest_RecursiveLinkedAsset : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//	Root
		//		StateLinkedTree1 (Tree2) -> Next
		//		StateA -> Succeeded
		//Tree 2
		//	Root
		//		StateLinkedTreeA (Tree1) -> Next
		//		StateA -> Succeeded

		UMetaStory& MetaStory1 = NewMetaStory();
		UMetaStoryState* Root1 = nullptr;
		// Asset 1 definition
		{
			UMetaStoryEditorData& EditorData1 = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);
			Root1 = &EditorData1.AddSubTree("Tree1StateRoot");

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory1);
			AITEST_TRUE(TEXT("MetaStory 1 should get compiled"), bResult);
		}

		UMetaStory& MetaStory2 = NewMetaStory();
		UMetaStoryState* Root2 = nullptr;
		// Asset 2 definition
		{
			UMetaStoryEditorData& EditorData2 = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
			Root2 = &EditorData2.AddSubTree("Tree2StateRoot");

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory2);
			AITEST_TRUE(TEXT("MetaStory 2 should get compiled"), bResult);
		}
		// Asset 1 implementation
		{
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = Root1->AddTask<FTestTask_PrintValue>("Tree1StateRootTaskA");
				Task1.GetInstanceData().Value = 101;
			}
			{
				UMetaStoryState& C1 = Root1->AddChildState("Tree1StateLinkedTree1", EMetaStoryStateType::LinkedAsset);
				C1.Tag = GetTestTag1();
				C1.SetLinkedStateAsset(&MetaStory2);
				C1.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
			}
			{
				UMetaStoryState& StateA = Root1->AddChildState("Tree1StateA", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = StateA.AddTask<FTestTask_PrintValue>("Tree1StateA");
				Task.GetInstanceData().Value = 102;
				FMetaStoryTransition& Transition = StateA.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, Root1);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0f;
			}

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory1);
			AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
		}
		// Asset 2 implementation
		UMetaStoryState* Tree2StateLinkedTree1 = nullptr;
		{
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = Root2->AddTask<FTestTask_PrintValue>("Tree2StateRootTaskA");
				Task1.GetInstanceData().Value = 201;
			}
			{
				UMetaStoryState& C1 = Root2->AddChildState("Tree2StateLinkedTree1", EMetaStoryStateType::LinkedAsset);
				C1.Tag = GetTestTag2();
				C1.SetLinkedStateAsset(&MetaStory2);
				C1.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
				Tree2StateLinkedTree1 = &C1;
			}
			{
				UMetaStoryState& StateD = Root2->AddChildState("Tree2StateA", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = StateD.AddTask<FTestTask_PrintValue>("Tree2StateA");
				Task.GetInstanceData().Value = 202;
				FMetaStoryTransition& Transition = StateD.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, Root2);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0f;
			}

			// circular dependency detected
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory2);
			AITEST_FALSE(TEXT("MetaStory should not compiled"), bResult);
		}
		// Fix circular dependency
		{
			Tree2StateLinkedTree1->SetLinkedStateAsset(&MetaStory1);
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory2);
			AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
		}
		// Run test
		{
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			{
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);
			}

			{
				GetTestRunner().AddExpectedErrorPlain(TEXT("Trying to recursively enter subtree"), EAutomationExpectedErrorFlags::Contains, 1);

				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1StateRootTaskA", TEXT("EnterState101"));
				AITEST_TRUE(TEXT("Start should enter Tree1StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree2StateRootTaskA", TEXT("EnterState201"));
				AITEST_TRUE(TEXT("Start should enter Tree2StateRootTaskA"), LogOrder);
				LogOrder = LogOrder.Then("Tree2StateA", TEXT("EnterState202"));
				AITEST_TRUE(TEXT("Start should enter Tree2StateA"), LogOrder);
				Exec.LogClear();
				AITEST_TRUE(TEXT("Doesn't have the expected error message."), GetTestRunner().HasMetExpectedMessages());
			}
			Exec.Stop();
		}

		return true;
	}
};
// This test invokes editor-only methods which aren't AutoRTFM-safe, so we skip it when AutoRTFM is on.
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_RecursiveLinkedAsset, "System.MetaStory.LinkedAsset.RecursiveLinkedAsset");

struct FMetaStoryTest_LinkedAssetTransitionSameTick : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//	Root
		//		State1 -> Delay 1 -> StateLinkedTree1
		//		LinkState2 (Tree2) -> Next
		//		State3 -> Root
		//Tree 2
		//	Root
		//		State1 -> Succeeded

		UMetaStory& MetaStory2 = NewMetaStory();
		UMetaStoryState* Root2 = nullptr;
		// Asset 2
		{
			UMetaStoryEditorData& EditorData2 = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
			Root2 = &EditorData2.AddSubTree("Tree2StateRoot");

			{
				UMetaStoryState& C1 = Root2->AddChildState("Tree2State1", EMetaStoryStateType::State);
				C1.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::Succeeded);
			}
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory2);
			AITEST_TRUE(TEXT("MetaStory should not compiled"), bResult);
		}
		// Asset 1
		UMetaStory& MetaStory1 = NewMetaStory();
		{
			UMetaStoryState* Root1 = nullptr;
			{
				UMetaStoryEditorData& EditorData1 = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);
				Root1 = &EditorData1.AddSubTree("Tree1StateRoot");

				FMetaStoryCompilerLog Log;
				FMetaStoryCompiler Compiler(Log);
				const bool bResult = Compiler.Compile(MetaStory1);
				AITEST_TRUE(TEXT("MetaStory 1 should get compiled"), bResult);
			}
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = Root1->AddTask<FTestTask_PrintValue>("Tree1StateRootTask1");
				Task1.GetInstanceData().Value = 100;
			}
			{
				UMetaStoryState& State1 = Root1->AddChildState("Tree1State1", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = State1.AddTask<FTestTask_PrintValue>("Tree1State1Task1");
				Task.GetInstanceData().Value = 101;
				Task.GetInstanceData().TickRunStatus = EMetaStoryRunStatus::Succeeded;
				State1.AddTransition(EMetaStoryTransitionTrigger::OnStateCompleted, EMetaStoryTransitionType::NextState);
			}
			{
				UMetaStoryState& LinkState2 = Root1->AddChildState("Tree1State2LinkedTree2", EMetaStoryStateType::LinkedAsset);
				LinkState2.SetLinkedStateAsset(&MetaStory2);
				LinkState2.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
			}
			{
				UMetaStoryState& State3 = Root1->AddChildState("Tree1State3", EMetaStoryStateType::State);
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task = State3.AddTask<FTestTask_PrintValue>("Tree1State3Task1");
				Task.GetInstanceData().Value = 103;
				FMetaStoryTransition& Transition = State3.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, Root1);
				Transition.bDelayTransition = true;
				Transition.DelayDuration = 1.0f;
			}

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory1);
			AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);
		}

		// Run test
		{
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			{
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);
			}

			{
				EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State1"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1StateRootTask1", TEXT("EnterState100"));
				AITEST_TRUE(TEXT("Start should enter Tree1StateRootTask1"), LogOrder);
				LogOrder = LogOrder.Then("Tree1State1Task1", TEXT("EnterState101"));
				AITEST_TRUE(TEXT("Start should enter Tree1State1"), LogOrder);
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.01f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State2LinkedTree2", "Tree2StateRoot", "Tree2State1"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1StateRootTask1", TEXT("Tick100"));
				AITEST_TRUE(TEXT("Start should tick Tree1StateRootTask1"), LogOrder);
				LogOrder = LogOrder.Then("Tree1State1Task1", TEXT("Tick101"));
				AITEST_TRUE(TEXT("Start should tick Tree1State1"), LogOrder);
				LogOrder = LogOrder.Then("Tree1State1Task1", TEXT("ExitState101"));
				AITEST_TRUE(TEXT("Start should exit Tree1State1"), LogOrder);
				Exec.LogClear();
			}
			//Tree2State1 -> Succeeded should transition to Tree1State3
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.01f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State3"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1StateRootTask1", TEXT("Tick100"));
				AITEST_TRUE(TEXT("Start should tick Tree1StateRootTask1"), LogOrder);
				LogOrder = LogOrder.Then("Tree1State3Task1", TEXT("EnterState103"));
				AITEST_TRUE(TEXT("Start should enter Tree1State3"), LogOrder);
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.01f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State3"));
				Exec.LogClear();
			}
			{
				EMetaStoryRunStatus Status = Exec.Tick(1.01f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State1"));
				FTestMetaStoryExecutionContext::FLogOrder LogOrder = Exec.Expect("Tree1StateRootTask1", TEXT("Tick100"));
				AITEST_TRUE(TEXT("Start should tick Tree1StateRootTask1"), LogOrder);
				LogOrder = LogOrder.Then("Tree1State3Task1", TEXT("ExitState103"));
				AITEST_TRUE(TEXT("Start should exit Tree1State3"), LogOrder);
				LogOrder = LogOrder.Then("Tree1State1Task1", TEXT("EnterState101"));
				AITEST_TRUE(TEXT("Start should enter Tree1State1"), LogOrder);
				Exec.LogClear();
			}
			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_LinkedAssetTransitionSameTick, "System.MetaStory.LinkedAsset.TransitionSameTick");

struct FMetaStoryTest_Linked_GlobalParameter : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//  Global task and parameter
		//	Root
		//		StateLinkedTree1 (Tree2) -> Next
		//		SubTree2
		//	SubTree
		//		State3 (with transition OnTick to succeeded)
		//Tree 2
		//  Global task and parameter
		//	Root
		//		State1 (with transition OnTick to succeeded)

		auto AddInt = [](FInstancedPropertyBag& PropertyBag, FName VarName)
			{
				PropertyBag.AddProperty(VarName, EPropertyBagPropertyType::Int32);
				PropertyBag.SetValueInt32(VarName, -99);
				return PropertyBag.FindPropertyDescByName(VarName)->ID;
			};
		auto AddDouble = [](FInstancedPropertyBag& PropertyBag, FName VarName)
			{
				PropertyBag.AddProperty(VarName, EPropertyBagPropertyType::Double);
				PropertyBag.SetValueDouble(VarName, -99.0);
				return PropertyBag.FindPropertyDescByName(VarName)->ID;
			};

		FGuid Tree2GlobalParameter_ValueID_Int;

		// Tree 2
		UMetaStory& MetaStory2 = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData2 = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);
			
			// note double before int
			AddDouble(GetRootPropertyBag(EditorData2), "Tree2GlobalDouble");
			Tree2GlobalParameter_ValueID_Int = AddInt(GetRootPropertyBag(EditorData2), "Tree2GlobalInt");

			// Global tasks
			TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask1 = EditorData2.AddGlobalTask<FTestTask_PrintValue>("Tree2GlobalTask1");
			{
				GlobalTask1.GetInstanceData().Value = -1;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(EditorData2.GetRootParametersGuid(), "Tree2GlobalInt"), FPropertyBindingPath(GlobalTask1.ID, "Value"));
			}
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask2 = EditorData2.AddGlobalTask<FTestTask_PrintValue>("Tree2GlobalTask2");
				GlobalTask2.GetInstanceData().Value = -2;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(GlobalTask1.ID, "Value"), FPropertyBindingPath(GlobalTask2.ID, "Value"));
			}

			UMetaStoryState& Root = EditorData2.AddSubTree("Tree2StateRoot");
			{
				AddInt(Root.Parameters.Parameters, "Tree2StateRootParametersInt");
				AddDouble(Root.Parameters.Parameters, "Tree2StateRootParametersDouble");
				EditorData2.AddPropertyBinding(FPropertyBindingPath(EditorData2.GetRootParametersGuid(), "Tree2GlobalInt"), FPropertyBindingPath(Root.Parameters.ID, "Tree2StateRootParametersInt"));
			}
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = Root.AddTask<FTestTask_PrintValue>("Tree2StateRootTask1");
				Task1.GetInstanceData().Value = -1;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(EditorData2.GetRootParametersGuid(), "Tree2GlobalInt"), FPropertyBindingPath(Task1.ID, "Value"));

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task2 = Root.AddTask<FTestTask_PrintValue>("Tree2StateRootTask2");
				Task2.GetInstanceData().Value = -2;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(Root.Parameters.ID, "Tree2StateRootParametersInt"), FPropertyBindingPath(Task2.ID, "Value"));
				
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task3 = Root.AddTask<FTestTask_PrintValue>("Tree2StateRootTask3");
				Task3.GetInstanceData().Value = -3;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(GlobalTask1.ID, "Value"), FPropertyBindingPath(Task3.ID, "Value"));
			}
			{
				UMetaStoryState& State1 = Root.AddChildState("Tree2State1", EMetaStoryStateType::State);
				{
					AddDouble(State1.Parameters.Parameters, "Tree2State1ParametersDouble");
					AddInt(State1.Parameters.Parameters, "Tree2State1ParametersInt");
					EditorData2.AddPropertyBinding(FPropertyBindingPath(Root.Parameters.ID, "Tree2StateRootParametersInt"), FPropertyBindingPath(State1.Parameters.ID, "Tree2State1ParametersInt"));
				}

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = State1.AddTask<FTestTask_PrintValue>("Tree2State1Task1");
				Task1.GetInstanceData().Value = -1;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(EditorData2.GetRootParametersGuid(), "Tree2GlobalInt"), FPropertyBindingPath(Task1.ID, TEXT("Value")));

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task2 = State1.AddTask<FTestTask_PrintValue>("Tree2State1Task2");
				Task2.GetInstanceData().Value = -2;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(Root.Parameters.ID, "Tree2StateRootParametersInt"), FPropertyBindingPath(Task2.ID, TEXT("Value")));

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task3 = State1.AddTask<FTestTask_PrintValue>("Tree2State1Task3");
				Task3.GetInstanceData().Value = -3;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(GlobalTask1.ID, "Value"), FPropertyBindingPath(Task3.ID, "Value"));

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task4 = State1.AddTask<FTestTask_PrintValue>("Tree2State1Task4");
				Task3.GetInstanceData().Value = -4;
				EditorData2.AddPropertyBinding(FPropertyBindingPath(State1.Parameters.ID, "Tree2State1ParametersInt"), FPropertyBindingPath(Task4.ID, "Value"));

				State1.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::Succeeded);
			}

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler2(Log);
			const bool bResult2 = Compiler2.Compile(MetaStory2);
			AITEST_TRUE(TEXT("MetaStory2 should get compiled"), bResult2);
		}

		// Tree 1
		UMetaStory& MetaStory1 = NewMetaStory();
		FGuid Tree1GlobalParameter_ValueID_Int;
		{
			UMetaStoryEditorData& EditorData1 = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);

			Tree1GlobalParameter_ValueID_Int = AddInt(GetRootPropertyBag(EditorData1), "Tree1GlobalInt");
			AddDouble(GetRootPropertyBag(EditorData1), "Tree1GlobalDouble");

			// Global tasks
			TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask1 = EditorData1.AddGlobalTask<FTestTask_PrintValue>("Tree1GlobalTask1");
			{
				GlobalTask1.GetInstanceData().Value = -1;
				EditorData1.AddPropertyBinding(FPropertyBindingPath(EditorData1.GetRootParametersGuid(), "Tree1GlobalInt"), FPropertyBindingPath(GlobalTask1.ID, "Value"));
			}
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask2 = EditorData1.AddGlobalTask<FTestTask_PrintValue>("Tree1GlobalTask2");
				GlobalTask2.GetInstanceData().Value = -2;
				EditorData1.AddPropertyBinding(FPropertyBindingPath(GlobalTask1.ID, "Value"), FPropertyBindingPath(GlobalTask2.ID, "Value"));
			}

			UMetaStoryState& Root = EditorData1.AddSubTree("Tree1StateRoot");
			{
				AddDouble(Root.Parameters.Parameters, "Tree1StateRootParametersDouble");
				AddInt(Root.Parameters.Parameters, "Tree1StateRootParametersInt");
				EditorData1.AddPropertyBinding(FPropertyBindingPath(EditorData1.GetRootParametersGuid(), "Tree1GlobalInt"), FPropertyBindingPath(Root.Parameters.ID, "Tree1StateRootParametersInt"));
			}
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = Root.AddTask<FTestTask_PrintValue>("Tree1StateRootTask1");
				Task1.GetInstanceData().Value = -1;
				EditorData1.AddPropertyBinding(FPropertyBindingPath(EditorData1.GetRootParametersGuid(), "Tree1GlobalInt"), FPropertyBindingPath(Task1.ID, "Value"));

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task2 = Root.AddTask<FTestTask_PrintValue>("Tree1StateRootTask2");
				Task2.GetInstanceData().Value = -2;
				EditorData1.AddPropertyBinding(FPropertyBindingPath(Root.Parameters.ID, "Tree1StateRootParametersInt"), FPropertyBindingPath(Task2.ID, "Value"));

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task3 = Root.AddTask<FTestTask_PrintValue>("Tree1StateRootTask3");
				Task3.GetInstanceData().Value = -3;
				EditorData1.AddPropertyBinding(FPropertyBindingPath(GlobalTask1.ID, "Value"), FPropertyBindingPath(Task3.ID, "Value"));
			}
			{
				UMetaStoryState& State1 = Root.AddChildState("Tree1State1", EMetaStoryStateType::LinkedAsset);
				State1.SetLinkedStateAsset(&MetaStory2);
				State1.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);

				State1.SetParametersPropertyOverridden(Tree2GlobalParameter_ValueID_Int, true);
				const FInstancedPropertyBag* Parameters = State1.GetDefaultParameters();
				AITEST_TRUE(TEXT("Parameter is invalid"), Parameters != nullptr);
				EditorData1.AddPropertyBinding(FPropertyBindingPath(GlobalTask1.ID, "Value"), FPropertyBindingPath(State1.Parameters.ID, "Tree2GlobalInt"));
			}

			FGuid Tree1Sub1Parameter_ValueID_Int;
			UMetaStoryState& Sub1 = EditorData1.AddSubTree("Tree1StateSub1");
			{
				Sub1.Type = EMetaStoryStateType::Subtree;
				Sub1.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::Succeeded);

				AddDouble(Sub1.Parameters.Parameters, "Tree1StateSub1ParametersDouble");
				Tree1Sub1Parameter_ValueID_Int = AddInt(Sub1.Parameters.Parameters, "Tree1StateSub1ParametersInt");

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Sub1Task1 = Sub1.AddTask<FTestTask_PrintValue>("Tree1StateSub1Task1");
				Sub1Task1.GetInstanceData().Value = -1;
				EditorData1.AddPropertyBinding(FPropertyBindingPath(Sub1.Parameters.ID, "Tree1StateSub1ParametersInt"), FPropertyBindingPath(Sub1Task1.ID, "Value"));

				{
					UMetaStoryState& State3 = Sub1.AddChildState("Tree1State3", EMetaStoryStateType::State);
					{
						AddDouble(State3.Parameters.Parameters, "Tree1State3ParametersDouble1");
						AddDouble(State3.Parameters.Parameters, "Tree1State3ParametersDouble2");
						AddInt(State3.Parameters.Parameters, "Tree1State3ParametersInt");
						EditorData1.AddPropertyBinding(FPropertyBindingPath(Sub1.Parameters.ID, "Tree1StateSub1ParametersInt"), FPropertyBindingPath(State3.Parameters.ID, "Tree1State3ParametersInt"));
					}

					TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = State3.AddTask<FTestTask_PrintValue>("Tree1State3Task1");
					Task1.GetInstanceData().Value = -1;
					EditorData1.AddPropertyBinding(FPropertyBindingPath(EditorData1.GetRootParametersGuid(), "Tree1GlobalInt"), FPropertyBindingPath(Task1.ID, TEXT("Value")));

					TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task2 = State3.AddTask<FTestTask_PrintValue>("Tree1State3Task2");
					Task2.GetInstanceData().Value = -2;
					EditorData1.AddPropertyBinding(FPropertyBindingPath(Sub1.Parameters.ID, "Tree1StateSub1ParametersInt"), FPropertyBindingPath(Task2.ID, TEXT("Value")));

					TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task3 = State3.AddTask<FTestTask_PrintValue>("Tree1State3Task3");
					Task3.GetInstanceData().Value = -3;
					EditorData1.AddPropertyBinding(FPropertyBindingPath(GlobalTask1.ID, "Value"), FPropertyBindingPath(Task3.ID, "Value"));

					TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task4 = State3.AddTask<FTestTask_PrintValue>("Tree1State3Task4");
					Task3.GetInstanceData().Value = -4;
					EditorData1.AddPropertyBinding(FPropertyBindingPath(Task1.ID, "Value"), FPropertyBindingPath(Task4.ID, "Value"));

					State3.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::Succeeded);
				}
			}
			{
				UMetaStoryState& State2 = Root.AddChildState("Tree1State2", EMetaStoryStateType::Linked);
				State2.SetLinkedState(Sub1.GetLinkToState());
				State2.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::Succeeded);

				State2.SetParametersPropertyOverridden(Tree1Sub1Parameter_ValueID_Int, true);
				const FInstancedPropertyBag* Parameters = State2.GetDefaultParameters();
				AITEST_TRUE(TEXT("Parameter is invalid"), Parameters != nullptr);
				EditorData1.AddPropertyBinding(FPropertyBindingPath(GlobalTask1.ID, "Value"), FPropertyBindingPath(State2.Parameters.ID, "Tree1StateSub1ParametersInt"));
			}

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler1(Log);
			const bool bResult1 = Compiler1.Compile(MetaStory1);
			AITEST_TRUE(TEXT("MetaStory1 should get compiled"), bResult1);
		}

		FMetaStoryInstanceData InstanceData;
		{
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);
		}
		{
			FMetaStoryReference MetaStoryRef;
			MetaStoryRef.SetMetaStory(&MetaStory1);
			MetaStoryRef.SetPropertyOverridden(Tree1GlobalParameter_ValueID_Int, true);
			MetaStoryRef.GetMutableParameters().SetValueInt32("Tree1GlobalInt", 5);
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			const EMetaStoryRunStatus Status = Exec.Start(FMetaStoryExecutionContext::FStartParameters
				{
					.GlobalParameters = &MetaStoryRef.GetParameters(),
				});
			AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
			AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State1", "Tree2StateRoot", "Tree2State1"));
			AITEST_TRUE(TEXT("Start should enter Tree1GlobalTask1"), Exec.Expect("Tree1GlobalTask1", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree1GlobalTask2"), Exec.Expect("Tree1GlobalTask2", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree1StateRootTask1"), Exec.Expect("Tree1StateRootTask1", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree1StateRootTask2"), Exec.Expect("Tree1StateRootTask2", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree1StateRootTask3"), Exec.Expect("Tree1StateRootTask3", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree2GlobalTask1"), Exec.Expect("Tree2GlobalTask1", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree2GlobalTask2"), Exec.Expect("Tree2GlobalTask2", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree2StateRootTask1"), Exec.Expect("Tree2StateRootTask1", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree2StateRootTask2"), Exec.Expect("Tree2StateRootTask2", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree2StateRootTask3"), Exec.Expect("Tree2StateRootTask3", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree2State1Task1"), Exec.Expect("Tree2State1Task1", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree2State1Task2"), Exec.Expect("Tree2State1Task2", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree2State1Task3"), Exec.Expect("Tree2State1Task3", TEXT("EnterState5")));
			AITEST_TRUE(TEXT("Start should enter Tree2State1Task4"), Exec.Expect("Tree2State1Task4", TEXT("EnterState5")));
			Exec.LogClear();
		}
		{
			FMetaStoryReference MetaStoryRef;
			MetaStoryRef.SetMetaStory(&MetaStory1);
			MetaStoryRef.SetPropertyOverridden(Tree1GlobalParameter_ValueID_Int, true);
			MetaStoryRef.GetMutableParameters().SetValueInt32("Tree1GlobalInt", 6);

			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			InstanceData.GetMutableStorage().SetGlobalParameters(MetaStoryRef.GetParameters());

			const EMetaStoryRunStatus Status = Exec.Tick(1.0f);
			AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);

			AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State2", "Tree1StateSub1", "Tree1State3"));
			AITEST_TRUE(TEXT("Tick should tick Tree1GlobalTask1"), Exec.Expect("Tree1GlobalTask1", TEXT("Tick6")));
			AITEST_TRUE(TEXT("Tick should tick Tree1GlobalTask2"), Exec.Expect("Tree1GlobalTask2", TEXT("Tick6")));
			AITEST_TRUE(TEXT("Tick should tick Tree1StateRootTask1"), Exec.Expect("Tree1StateRootTask1", TEXT("Tick6")));
			//@todo Binding on the state are not updated on tick. Is this a bug? See Tree1StateRootParametersInt
			//AITEST_TRUE(TEXT("Tick should tick Tree1StateRootTask2), Exec.Expect("Tree1StateRootTask2", TEXT("Tick6")));
			AITEST_TRUE(TEXT("Tick should tick Tree1StateRootTask3"), Exec.Expect("Tree1StateRootTask3", TEXT("Tick6")));
			AITEST_TRUE(TEXT("Tick should tick Tree2GlobalTask1"), Exec.Expect("Tree2GlobalTask1", TEXT("Tick6")));
			AITEST_TRUE(TEXT("Tick should tick Tree2GlobalTask2"), Exec.Expect("Tree2GlobalTask2", TEXT("Tick6")));
			AITEST_TRUE(TEXT("Tick should tick Tree2StateRootTask1"), Exec.Expect("Tree2StateRootTask1", TEXT("Tick6")));
			//@todo Binding on the state are not updated on tick. Is this a bug? See Tree2StateRootParametersInt
			//AITEST_TRUE(TEXT("Tick should tick Tree2StateRootTask2"), Exec.Expect("Tree2StateRootTask2", TEXT("Tick6")));
			AITEST_TRUE(TEXT("Tick should tick Tree2StateRootTask3"), Exec.Expect("Tree2StateRootTask3", TEXT("Tick6")));
			AITEST_TRUE(TEXT("Tick should tick Tree2State1Task1"), Exec.Expect("Tree2State1Task1", TEXT("Tick6")));
			//@todo Binding on the state are not updated on tick. Is this a bug? See Tree2StateRootParametersInt
			//AITEST_TRUE(TEXT("Tick should tick Tree2State1Task2"), Exec.Expect("Tree2State1Task2", TEXT("Tick6")));
			AITEST_TRUE(TEXT("Tick should tick Tree2State1Task3"), Exec.Expect("Tree2State1Task3", TEXT("Tick6")));
			//@todo Binding on the state are not updated on tick. Is this a bug? See Tree2State1ParametersInt
			//AITEST_TRUE(TEXT("Tick should tick Tree2State1Task4"), Exec.Expect("Tree2State1Task4", TEXT("Tick6")));

			AITEST_TRUE(TEXT("Tick should enter Tree1StateSub1Task1"), Exec.Expect("Tree1StateSub1Task1", TEXT("EnterState6")));
			AITEST_TRUE(TEXT("Tick should enter Tree1State3Task1"), Exec.Expect("Tree1State3Task1", TEXT("EnterState6")));
			AITEST_TRUE(TEXT("Tick should enter Tree1State3Task2"), Exec.Expect("Tree1State3Task2", TEXT("EnterState6")));
			AITEST_TRUE(TEXT("Tick should enter Tree1State3Task3"), Exec.Expect("Tree1State3Task3", TEXT("EnterState6")));
			AITEST_TRUE(TEXT("Tick should enter Tree1State3Task4"), Exec.Expect("Tree1State3Task4", TEXT("EnterState6")));
		}
		{
			FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
			Exec.Stop();
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Linked_GlobalParameter, "System.MetaStory.LinkedAsset.GlobalParameter");

struct FMetaStoryTest_Linked_FinishGlobalTasks : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		//Tree 1
		//  Global task and parameter
		//	Root
		//		StateLinkedTree1 (Tree2) -> Next
		//		SubTree2
		//	SubTree
		//		State3
		//Tree 2
		//  Global task and parameter
		//	Root
		//		State1


		// Tree 2
		UMetaStory& MetaStory2 = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData2 = *Cast<UMetaStoryEditorData>(MetaStory2.EditorData);

			// Global tasks
			{
				EditorData2.GlobalTasksCompletion = EMetaStoryTaskCompletionType::Any;

				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask1 = EditorData2.AddGlobalTask<FTestTask_PrintValue>("Tree2GlobalTask1");
				GlobalTask1.GetInstanceData().Value = 1;

				EditorData2.AddGlobalTask<FTestTask_Stand>("Tree2GlobalTask2").GetNode().TicksToCompletion = 99;
				EditorData2.AddGlobalTask<FTestTask_Stand>("Tree2GlobalTask3").GetNode().TicksToCompletion = 99;
				EditorData2.AddGlobalTask<FTestTask_Stand>("Tree2GlobalTask4").GetNode().TicksToCompletion = 99;
				EditorData2.AddGlobalTask<FTestTask_Stand>("Tree2GlobalTask5").GetNode().TicksToCompletion = 99;
				EditorData2.AddGlobalTask<FTestTask_Stand>("Tree2GlobalTask6").GetNode().TicksToCompletion = 99;

				TMetaStoryTypedEditorNode<FTestTask_Stand>& GlobalTask2 = EditorData2.AddGlobalTask<FTestTask_Stand>("Tree2GlobalTask7");
				GlobalTask2.GetNode().TicksToCompletion = 2;
				GlobalTask2.GetNode().TickCompletionResult = EMetaStoryRunStatus::Succeeded;
			}

			UMetaStoryState& Root = EditorData2.AddSubTree("Tree2StateRoot");
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = Root.AddTask<FTestTask_PrintValue>("Tree2StateRootTask1");
				Task1.GetInstanceData().Value = 1;
			}

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler2(Log);
			const bool bResult2 = Compiler2.Compile(MetaStory2);
			AITEST_TRUE(TEXT("MetaStory2 should get compiled"), bResult2);
		}

		// Tree 1
		UMetaStory& MetaStory1 = NewMetaStory();
		{
			UMetaStoryEditorData& EditorData1 = *Cast<UMetaStoryEditorData>(MetaStory1.EditorData);

			// Global tasks
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& GlobalTask1 = EditorData1.AddGlobalTask<FTestTask_PrintValue>("Tree1GlobalTask1");
				GlobalTask1.GetInstanceData().Value = 1;

				TMetaStoryTypedEditorNode<FTestTask_Stand>& GlobalTask2 = EditorData1.AddGlobalTask<FTestTask_Stand>("Tree1GlobalTask2");
				GlobalTask2.GetNode().TicksToCompletion = 4;
				GlobalTask2.GetNode().TickCompletionResult = EMetaStoryRunStatus::Succeeded;
			}

			UMetaStoryState& Root = EditorData1.AddSubTree("Tree1StateRoot");
			{
				TMetaStoryTypedEditorNode<FTestTask_PrintValue>& Task1 = Root.AddTask<FTestTask_PrintValue>("Tree1StateRootTask1");
				Task1.GetInstanceData().Value = 1;
			}
			{
				UMetaStoryState& State1 = Root.AddChildState("Tree1State1", EMetaStoryStateType::LinkedAsset);
				State1.SetLinkedStateAsset(&MetaStory2);
				State1.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::NextState);
			}
			{
				UMetaStoryState& State2 = Root.AddChildState("Tree1State2", EMetaStoryStateType::State);
				State2.AddTransition(EMetaStoryTransitionTrigger::OnStateSucceeded, EMetaStoryTransitionType::Succeeded);

				TMetaStoryTypedEditorNode<FTestTask_Stand>& Task1 = State2.AddTask<FTestTask_Stand>("Tree1State2Task1");
				Task1.GetNode().TicksToCompletion = 10;
				Task1.GetNode().TickCompletionResult = EMetaStoryRunStatus::Succeeded;
			}

			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler1(Log);
			const bool bResult1 = Compiler1.Compile(MetaStory1);
			AITEST_TRUE(TEXT("MetaStory1 should get compiled"), bResult1);
		}

		for (int32 Index = 0; Index < 4; ++Index)
		{
			Private::FScopedCVarBool CVarGGlobalTasksCompleteOwningFrame = TEXT("MetaStory.GlobalTasksCompleteOwningFrame");
			const bool bGlobalTasksCompleteOwningFrame = (Index % 2) == 0;
			CVarGGlobalTasksCompleteOwningFrame.Set(bGlobalTasksCompleteOwningFrame);

			Private::FScopedCVarBool CVarTickGlobalNodesWithHierarchy = TEXT("MetaStory.TickGlobalNodesFollowingTreeHierarchy");
			const bool bTickGlobalNodesWithHierarchy = Index >= 2;
			CVarTickGlobalNodesWithHierarchy.Set(bTickGlobalNodesWithHierarchy);

			FMetaStoryInstanceData InstanceData;
			{
				FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
				const bool bInitSucceeded = Exec.IsValid();
				AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);
			}
			{
				FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Start();
				AITEST_EQUAL(TEXT("Start should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State1", "Tree2StateRoot"));
				AITEST_TRUE(TEXT("Start should enter Tree1GlobalTask1"), Exec.Expect("Tree1GlobalTask1", TEXT("EnterState1")));
				AITEST_TRUE(TEXT("Start should enter Tree1GlobalTask2"), Exec.Expect("Tree1GlobalTask2", TEXT("EnterState")));
				AITEST_TRUE(TEXT("Start should enter Tree1StateRootTask1"), Exec.Expect("Tree1StateRootTask1", TEXT("EnterState1")));
				AITEST_TRUE(TEXT("Start should enter Tree2GlobalTask1"), Exec.Expect("Tree2GlobalTask1", TEXT("EnterState1")));
				AITEST_TRUE(TEXT("Start should enter Tree2GlobalTask7"), Exec.Expect("Tree2GlobalTask7", TEXT("EnterState")));
				AITEST_TRUE(TEXT("Start should enter Tree2StateRootTask1"), Exec.Expect("Tree2StateRootTask1", TEXT("EnterState1")));
				Exec.LogClear();
			}
			{
				FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(1.0f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
				AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State1", "Tree2StateRoot"));
				AITEST_TRUE(TEXT("Tick should tick Tree1GlobalTask1"), Exec.Expect("Tree1GlobalTask1", TEXT("Tick1")));
				AITEST_TRUE(TEXT("Tick should tick Tree1GlobalTask2"), Exec.Expect("Tree1GlobalTask2", TEXT("Tick")));
				AITEST_TRUE(TEXT("Tick should tick Tree1StateRootTask1"), Exec.Expect("Tree1StateRootTask1", TEXT("Tick1")));
				AITEST_TRUE(TEXT("Tick should tick Tree2GlobalTask1"), Exec.Expect("Tree2GlobalTask1", TEXT("Tick1")));
				AITEST_TRUE(TEXT("Tick should tick Tree2GlobalTask7"), Exec.Expect("Tree2GlobalTask7", TEXT("Tick")));
				AITEST_TRUE(TEXT("Tick should tick Tree2StateRootTask1"), Exec.Expect("Tree2StateRootTask1", TEXT("Tick1")));
				Exec.LogClear();
			}
			if (bGlobalTasksCompleteOwningFrame)
			{
				{
					FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
					const EMetaStoryRunStatus Status = Exec.Tick(1.0f);
					AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
					AITEST_TRUE(TEXT("In correct states"), Exec.ExpectInActiveStates("Tree1StateRoot", "Tree1State2"));
					AITEST_TRUE(TEXT("Tick should tick Tree1GlobalTask1"), Exec.Expect("Tree1GlobalTask1", TEXT("Tick1")));
					AITEST_TRUE(TEXT("Tick should tick Tree1GlobalTask2"), Exec.Expect("Tree1GlobalTask2", TEXT("Tick")));
					if (bTickGlobalNodesWithHierarchy)
					{
						AITEST_TRUE(TEXT("Tick should tick Tree1StateRootTask1"), Exec.Expect("Tree1StateRootTask1", TEXT("Tick1")));
					}
					AITEST_TRUE(TEXT("Tick should tick Tree2GlobalTask1"), Exec.Expect("Tree2GlobalTask1", TEXT("Tick1")));
					AITEST_TRUE(TEXT("Tick should tick Tree2GlobalTask7"), Exec.Expect("Tree2GlobalTask7", TEXT("Tick")));
					if (bTickGlobalNodesWithHierarchy)
					{
						AITEST_FALSE(TEXT("Tick not should tick Tree2StateRootTask1"), Exec.Expect("Tree2StateRootTask1", TEXT("Tick1")));
					}

					AITEST_TRUE(TEXT("Tick should ExitMetaStory2StateRootTask1"), Exec.Expect("Tree2StateRootTask1", TEXT("ExitState1")));
					AITEST_TRUE(TEXT("Tick should ExitMetaStory2GlobalTask7"), Exec.Expect("Tree2GlobalTask7", TEXT("ExitState")));
					AITEST_TRUE(TEXT("Tick should ExitMetaStory2GlobalTask1"), Exec.Expect("Tree2GlobalTask1", TEXT("ExitState1")));
					AITEST_TRUE(TEXT("Start should enter Tree2State2Task1"), Exec.Expect("Tree1State2Task1", TEXT("EnterState")));
					Exec.LogClear();
				}
				{
					FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
					const EMetaStoryRunStatus Status = Exec.Tick(1.0f);
					AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Running);
					Exec.LogClear();
				}
				{
					FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
					const EMetaStoryRunStatus Status = Exec.Tick(1.0f);
					AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Succeeded);
					Exec.LogClear();
				}
			}
			else
			{
				FTestMetaStoryExecutionContext Exec(MetaStory1, MetaStory1, InstanceData);
				const EMetaStoryRunStatus Status = Exec.Tick(1.0f);
				AITEST_EQUAL(TEXT("Tick should complete with Running"), Status, EMetaStoryRunStatus::Succeeded);
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
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Linked_FinishGlobalTasks, "System.MetaStory.LinkedAsset.FinishGlobalTasks");

} // namespace UE::MetaStory::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
