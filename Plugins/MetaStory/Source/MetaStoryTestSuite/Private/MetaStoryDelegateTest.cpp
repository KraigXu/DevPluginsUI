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
struct FMetaStoryTest_Delegate_ConcurrentListeners : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		UMetaStoryState& Root = EditorData.AddSubTree(FName("Root"));

		TMetaStoryTypedEditorNode<FTestTask_BroadcastDelegate>& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName("DispatcherTask"));
		TMetaStoryTypedEditorNode<FTestTask_ListenDelegate>& ListenerTaskA = Root.AddTask<FTestTask_ListenDelegate>(FName("ListenerTaskA"));
		TMetaStoryTypedEditorNode<FTestTask_ListenDelegate>& ListenerTaskB = Root.AddTask<FTestTask_ListenDelegate>(FName("ListenerTaskB"));

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTaskB, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));
		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTaskA, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE("MetaStory should get compiled", bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("MetaStory should init", bInitSucceeded);

		Exec.Start();
		AITEST_FALSE(TEXT("MetaStory ListenerTaskA should not trigger."), Exec.Expect(ListenerTaskA.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_FALSE(TEXT("MetaStory ListenerTaskB should not trigger."), Exec.Expect(ListenerTaskB.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory ListenerTaskA should be triggered once."), Exec.Expect(ListenerTaskA.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("MetaStory ListenerTaskB should be triggered once."), Exec.Expect(ListenerTaskB.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));

		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory ListenerTaskA should be triggered twice."), Exec.Expect(ListenerTaskA.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 2)));
		AITEST_TRUE(TEXT("MetaStory ListenerTaskB should be triggered twice."), Exec.Expect(ListenerTaskB.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 2)));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Delegate_ConcurrentListeners, "System.MetaStory.Delegate.ConcurrentListeners");

struct FMetaStoryTest_Delegate_MutuallyExclusiveListeners : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		UMetaStoryState& StateA = Root.AddChildState("A");
		UMetaStoryState& StateB = Root.AddChildState("B");

		StateA.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &StateB);
		StateB.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &StateA);

		TMetaStoryTypedEditorNode<FTestTask_BroadcastDelegate>& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		TMetaStoryTypedEditorNode<FTestTask_ListenDelegate>& ListenerTaskA0 = StateA.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTaskA0")));
		TMetaStoryTypedEditorNode<FTestTask_ListenDelegate>& ListenerTaskA1 = StateA.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTaskA1")));
		TMetaStoryTypedEditorNode<FTestTask_ListenDelegate>& ListenerTaskB = StateB.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTaskB")));

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTaskA0, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));
		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTaskA1, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));
		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTaskB, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE("MetaStory should get compiled", bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("MetaStory should init", bInitSucceeded);

		Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/A"), Exec.ExpectInActiveStates(Root.Name, StateA.Name));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory ListenerTaskA0 should be triggered once"), Exec.Expect(ListenerTaskA0.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("MetaStory ListenerTaskA1 should be triggered once"), Exec.Expect(ListenerTaskA1.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("MetaStory ListenerTaskB shouldn't be triggered."), !Exec.Expect(ListenerTaskB.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE("MetaStory Active States should be in Root/B", Exec.ExpectInActiveStates(Root.Name, StateB.Name));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory ListenerTaskB should be triggered once"), Exec.Expect(ListenerTaskB.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("MetaStory ListenerTaskA0 shouldn't be triggered."), !Exec.Expect(ListenerTaskA0.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("MetaStory ListenerTaskA1 shouldn't be triggered."), !Exec.Expect(ListenerTaskA1.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/A"), Exec.ExpectInActiveStates(Root.Name, StateA.Name));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Delegate_MutuallyExclusiveListeners, "System.MetaStory.Delegate.MutuallyExclusiveListeners");

struct FMetaStoryTest_Delegate_Transitions : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		UMetaStoryState& StateA = Root.AddChildState("A");
		UMetaStoryState& StateB = Root.AddChildState("B");

		FMetaStoryTransition& TransitionAToB = StateA.AddTransition(EMetaStoryTransitionTrigger::OnDelegate, EMetaStoryTransitionType::GotoState, &StateB);
		FMetaStoryTransition& TransitionBToA = StateB.AddTransition(EMetaStoryTransitionTrigger::OnDelegate, EMetaStoryTransitionType::GotoState, &StateA);

		TMetaStoryTypedEditorNode<FTestTask_BroadcastDelegate>& DispatcherTask0 = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask0")));
		TMetaStoryTypedEditorNode<FTestTask_BroadcastDelegate>& DispatcherTask1 = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask1")));
		
		EditorData.AddPropertyBinding(FPropertyBindingPath(DispatcherTask0.ID, GET_MEMBER_NAME_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate)),  FPropertyBindingPath(TransitionAToB.ID, GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, DelegateListener)));
		EditorData.AddPropertyBinding(FPropertyBindingPath(DispatcherTask1.ID, GET_MEMBER_NAME_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate)), FPropertyBindingPath(TransitionBToA.ID, GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, DelegateListener)));

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE("MetaStory should get compiled", bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("MetaStory should init", bInitSucceeded);

		Exec.Start();
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/A"), Exec.ExpectInActiveStates(Root.Name, StateA.Name));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/B"), Exec.ExpectInActiveStates(Root.Name, StateB.Name));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/A"), Exec.ExpectInActiveStates(Root.Name, StateA.Name));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Delegate_Transitions, "System.MetaStory.Delegate.Transitions");

struct FMetaStoryTest_Delegate_Rebroadcasting : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		TMetaStoryTypedEditorNode<FTestTask_BroadcastDelegate>& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		TMetaStoryTypedEditorNode<FTestTask_RebroadcastDelegate>& RedispatcherTask = Root.AddTask<FTestTask_RebroadcastDelegate>(FName(TEXT("RedispatcherTask")));
		TMetaStoryTypedEditorNode<FTestTask_ListenDelegate>& ListenerTask = Root.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTask")));

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), RedispatcherTask, TEXT("Listener"));
		EditorData.AddPropertyBinding(RedispatcherTask, TEXT("Dispatcher"), ListenerTask, TEXT("Listener"));

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		Exec.Start();
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory ListenerTask should be triggered once."), Exec.Expect(ListenerTask.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory ListenerTask should be triggered twice."), Exec.Expect(ListenerTask.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 2)));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Delegate_Rebroadcasting, "System.MetaStory.Delegate.Rebroadcasting");

struct FMetaStoryTest_Delegate_SelfRemoval : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		TMetaStoryTypedEditorNode<FTestTask_BroadcastDelegate>& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		TMetaStoryTypedEditorNode<FTestTask_CustomFuncOnDelegate>& CustomFuncTask = Root.AddTask<FTestTask_CustomFuncOnDelegate>(FName(TEXT("CustomFuncTask")));

		uint32 TriggersCounter = 0;

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), CustomFuncTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_CustomFuncOnDelegate_InstanceData, Listener));
		CustomFuncTask.GetNode().CustomFunc = [&TriggersCounter](const FMetaStoryWeakExecutionContext& WeakContext, FMetaStoryDelegateListener Listener)
			{
				++TriggersCounter;
				WeakContext.UnbindDelegate(Listener);
			};

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		Exec.Start();
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("MetaStory Delegate should be triggered once"), TriggersCounter, 1);
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_EQUAL(TEXT("MetaStory Delegate should be triggered once"), TriggersCounter, 1);
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Delegate_SelfRemoval, "System.MetaStory.Delegate.SelfRemoval");

struct FMetaStoryTest_Delegate_WithoutRemoval : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		UMetaStoryState& StateA = Root.AddChildState("A");
		UMetaStoryState& StateB = Root.AddChildState("B");

		FMetaStoryTransition& TransitionAToB = StateA.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &StateB);

		TMetaStoryTypedEditorNode<FTestTask_BroadcastDelegate>& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		TMetaStoryTypedEditorNode<FTestTask_ListenDelegate>& ListenerTask = StateA.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTask")));
		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_ListenDelegate_InstanceData, Listener));
		ListenerTask.GetNode().bRemoveOnExit = false;

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		Exec.Start();
		Exec.LogClear();
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/A"), Exec.ExpectInActiveStates(Root.Name, StateA.Name));

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Delegate should be triggered once."), Exec.Expect(ListenerTask.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/B"), Exec.ExpectInActiveStates(Root.Name, StateB.Name));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_FALSE(TEXT("MetaStory Delegate shouldn't be triggered again."), Exec.Expect(ListenerTask.GetName()));
		AITEST_TRUE(TEXT("MetaStory Active States should be in Root/B"), Exec.ExpectInActiveStates(Root.Name, StateB.Name));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Delegate_WithoutRemoval, "System.MetaStory.Delegate.WithoutRemoval");

struct FMetaStoryTest_Delegate_GlobalDispatcherAndListener : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		TMetaStoryTypedEditorNode<FTestTask_Stand>& RootTask = Root.AddTask<FTestTask_Stand>();
		RootTask.GetNode().TicksToCompletion = 100;

		TMetaStoryTypedEditorNode<FTestTask_BroadcastDelegate>& DispatcherTask = EditorData.AddGlobalTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		TMetaStoryTypedEditorNode<FTestTask_ListenDelegate>& ListenerTask = EditorData.AddGlobalTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTask")));
		ListenerTask.GetNode().bRemoveOnExit = false;

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnTickDelegate), ListenerTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_CustomFuncOnDelegate_InstanceData, Listener));

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		Exec.Start();
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Delegate should be triggered once."), Exec.Expect(ListenerTask.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 1)));
		Exec.LogClear();

		Exec.Tick(0.1f);
		AITEST_TRUE(TEXT("MetaStory Delegate should be triggered twice."), Exec.Expect(ListenerTask.GetName(), *FString::Printf(TEXT("OnDelegate%d"), 2)));
		Exec.LogClear();

		Exec.Stop();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Delegate_GlobalDispatcherAndListener, "System.MetaStory.Delegate.GlobalDispatcherAndListener");

struct FMetaStoryTest_Delegate_ListeningToDelegateOnExit : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		TMetaStoryTypedEditorNode<FTestTask_BroadcastDelegate>& DispatcherTask = Root.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
		TMetaStoryTypedEditorNode<FTestTask_ListenDelegate>& ListenerTask = Root.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTask")));
		ListenerTask.GetNode().bRemoveOnExit = false;

		EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnExitDelegate), ListenerTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_CustomFuncOnDelegate_InstanceData, Listener));

		FMetaStoryCompilerLog Log;
		FMetaStoryCompiler Compiler(Log);
		const bool bResult = Compiler.Compile(MetaStory);
		AITEST_TRUE(TEXT("MetaStory should get compiled"), bResult);

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE(TEXT("MetaStory should init"), bInitSucceeded);

		Exec.Start();
		AITEST_TRUE("Expected Root to be active.", Exec.ExpectInActiveStates(Root.Name));
		Exec.LogClear();

		Exec.Stop();
		AITEST_FALSE(TEXT("MetaStory Delegate shouldn't be triggered"), Exec.Expect(ListenerTask.GetName()));
		Exec.LogClear();

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_Delegate_ListeningToDelegateOnExit, "System.MetaStory.Delegate.ListeningToDelegateOnExit");

/** Test same state and the state index < number of instance data. */
struct FMetaStoryTest_Delegate_ListeningToDelegateOnExit2 : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		UMetaStoryState& StateB = Root.AddChildState("StateA")
			.AddChildState("StateB");
		UMetaStoryState& StateC= Root.AddChildState("StateC");
		{
			EditorData.AddGlobalTask<FTestTask_Stand>().GetNode().TicksToCompletion = 100;
			EditorData.AddGlobalTask<FTestTask_Stand>().GetNode().TicksToCompletion = 100;
			EditorData.AddGlobalTask<FTestTask_Stand>().GetNode().TicksToCompletion = 100;
			EditorData.AddGlobalTask<FTestTask_Stand>().GetNode().TicksToCompletion = 100;
			EditorData.AddGlobalTask<FTestTask_Stand>().GetNode().TicksToCompletion = 100;
			EditorData.AddGlobalTask<FTestTask_Stand>().GetNode().TicksToCompletion = 100;
			EditorData.AddGlobalTask<FTestTask_Stand>().GetNode().TicksToCompletion = 100;
			EditorData.AddGlobalTask<FTestTask_Stand>().GetNode().TicksToCompletion = 100;
			EditorData.AddGlobalTask<FTestTask_Stand>().GetNode().TicksToCompletion = 100;
			EditorData.AddGlobalTask<FTestTask_Stand>().GetNode().TicksToCompletion = 100;
		}
		{
			TMetaStoryTypedEditorNode<FTestTask_BroadcastDelegate>& DispatcherTask = StateB.AddTask<FTestTask_BroadcastDelegate>(FName(TEXT("DispatcherTask")));
			TMetaStoryTypedEditorNode<FTestTask_ListenDelegate>& ListenerTask = StateB.AddTask<FTestTask_ListenDelegate>(FName(TEXT("ListenerTask")));
			ListenerTask.GetNode().bRemoveOnExit = false;

			EditorData.AddPropertyBinding(DispatcherTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_BroadcastDelegate_InstanceData, OnExitDelegate), ListenerTask, GET_MEMBER_NAME_STRING_CHECKED(FTestTask_CustomFuncOnDelegate_InstanceData, Listener));

			FMetaStoryTransition& Transition = StateB.AddTransition(EMetaStoryTransitionTrigger::OnTick, EMetaStoryTransitionType::GotoState, &StateC);
			Transition.bDelayTransition = true;
			Transition.DelayDuration = 0.1f;
		}
		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory);
			AITEST_TRUE("MetaStory should get compiled", bResult);
		}

		FMetaStoryInstanceData InstanceData;
		FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
		const bool bInitSucceeded = Exec.IsValid();
		AITEST_TRUE("MetaStory should init", bInitSucceeded);

		Exec.Start();
		AITEST_TRUE("Expected Root to be active.", Exec.ExpectInActiveStates("Root", "StateA", "StateB"));
		Exec.LogClear();

		Exec.Tick(1.0f);
		AITEST_TRUE("Expected Root to be active.", Exec.ExpectInActiveStates("Root", "StateA", "StateB"));
		AITEST_FALSE(TEXT("MetaStory Delegate shouldn't be triggered"), Exec.Expect("ListenerTask"));
		Exec.LogClear();

		Exec.Tick(1.0f);
		AITEST_TRUE("Expected Root to be active.", Exec.ExpectInActiveStates("Root", "StateC"));
		AITEST_FALSE(TEXT("MetaStory Delegate shouldn't be triggered"), Exec.Expect("ListenerTask"));
		Exec.LogClear();

		Exec.Stop();
		AITEST_FALSE(TEXT("MetaStory Delegate shouldn't be triggered"), Exec.Expect("ListenerTask"));
		Exec.LogClear();

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMetaStoryTest_Delegate_ListeningToDelegateOnExit2, "System.MetaStory.Delegate.ListeningToDelegateOnExit2");

struct FMetaStoryTest_Delegate_ListenerDispatcherOnNode : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		// Root(Dispatcher, ListenerOnNode1 -> DispatcherOnNode, ListenerOnNode2 -> DispatcherOnInstance, ListenerOnInstance -> DispatcherOnNode)

		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);
		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));

		bool bListenerOnNode1Broadcasted = false;
		bool bListenerOnNode2Broadcasted = false;
		bool bListenerOnInstanceBroadcasted = false;

		{
			TMetaStoryTypedEditorNode<FTestTask_DispatcherOnNodeAndInstance>& DispatcherTaskNode = Root.AddTask<FTestTask_DispatcherOnNodeAndInstance>(FName(TEXT("Dispatcher")));

			TMetaStoryTypedEditorNode<FTestTask_ListenerOnNode>& ListenerOnNode1TaskNode = Root.AddTask<FTestTask_ListenerOnNode>(FName(TEXT("ListenerOnNode1")));
			ListenerOnNode1TaskNode.GetNode().CustomFunc = [&bListenerOnNode1Broadcasted](const FMetaStoryWeakExecutionContext& WeakExecContext)
			{
				bListenerOnNode1Broadcasted = true;
			};

			TMetaStoryTypedEditorNode<FTestTask_ListenerOnNode>& ListenerOnNode2TaskNode = Root.AddTask<FTestTask_ListenerOnNode>(FName(TEXT("ListenerOnNode2")));
			ListenerOnNode2TaskNode.GetNode().CustomFunc = [&bListenerOnNode2Broadcasted](const FMetaStoryWeakExecutionContext& WeakExecContext)
			{
				bListenerOnNode2Broadcasted = true;
			};

			TMetaStoryTypedEditorNode<FTestTask_ListenerOnInstance>& ListenerOnInstanceTaskNode = Root.AddTask<FTestTask_ListenerOnInstance>(FName(TEXT("ListenerOnInstance")));
			ListenerOnInstanceTaskNode.GetNode().CustomFunc = [&bListenerOnInstanceBroadcasted](const FMetaStoryWeakExecutionContext& WeakExecContext)
			{
				bListenerOnInstanceBroadcasted = true;
			};

			EditorData.AddPropertyBinding(
				FPropertyBindingPath(DispatcherTaskNode.GetNodeID(), GET_MEMBER_NAME_CHECKED(FTestTask_DispatcherOnNodeAndInstance, DispatcherOnNode)),
				FPropertyBindingPath(ListenerOnNode1TaskNode.GetNodeID(), GET_MEMBER_NAME_CHECKED(FTestTask_ListenerOnNode, ListenerOnNode)));

			EditorData.AddPropertyBinding(
				FPropertyBindingPath(DispatcherTaskNode.ID, GET_MEMBER_NAME_CHECKED(FTestTask_DispatcherOnNodeAndInstance_InstanceData, DispatcherOnInstance)),
				FPropertyBindingPath(ListenerOnNode2TaskNode.GetNodeID(), GET_MEMBER_NAME_CHECKED(FTestTask_ListenerOnNode, ListenerOnNode)));

			EditorData.AddPropertyBinding(
				FPropertyBindingPath(DispatcherTaskNode.GetNodeID(), GET_MEMBER_NAME_CHECKED(FTestTask_DispatcherOnNodeAndInstance, DispatcherOnNode)),
				FPropertyBindingPath(ListenerOnInstanceTaskNode.ID, GET_MEMBER_NAME_CHECKED(FTestTask_ListenerOnInstance_InstanceData, ListenerOnInstance)));
		}

		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory);
			AITEST_TRUE("MetaStory should get compiled", bResult);

			const FMetaStoryCompactState& RootState = MetaStory.GetStates()[0];
			int32 TaskNodeIndex = RootState.TasksBegin;

			FConstStructView DispatcherView = MetaStory.GetNode(TaskNodeIndex++);
			FConstStructView DispatcherInstanceDataView = MetaStory.GetDefaultInstanceData().GetStruct(DispatcherView.Get<const FMetaStoryTaskBase>().InstanceTemplateIndex.Get());

			FConstStructView ListenerOnNode1View = MetaStory.GetNode(TaskNodeIndex++);

			FConstStructView ListenerOnNode2View = MetaStory.GetNode(TaskNodeIndex++);

			FConstStructView ListenerOnInstanceView = MetaStory.GetNode(TaskNodeIndex++);
			FConstStructView ListenerOnInstanceInstanceDataView = MetaStory.GetDefaultInstanceData().GetStruct(ListenerOnInstanceView.Get<const FMetaStoryTaskBase>().InstanceTemplateIndex.Get());

			const FMetaStoryDelegateDispatcher DispatcherOnNode = DispatcherView.Get<const FTestTask_DispatcherOnNodeAndInstance>().DispatcherOnNode;
			const FMetaStoryDelegateDispatcher DispatcherOnInstance = DispatcherInstanceDataView.Get<const FTestTask_DispatcherOnNodeAndInstance_InstanceData>().DispatcherOnInstance;
			const FMetaStoryDelegateListener ListenerOnNode1 = ListenerOnNode1View.Get<const FTestTask_ListenerOnNode>().ListenerOnNode;
			const FMetaStoryDelegateListener ListenerOnNode2 = ListenerOnNode2View.Get<const FTestTask_ListenerOnNode>().ListenerOnNode;
			const FMetaStoryDelegateListener ListenerOnInstance = ListenerOnInstanceInstanceDataView.Get<const FTestTask_ListenerOnInstance_InstanceData>().ListenerOnInstance;

			AITEST_TRUE("Dispatcher on Node should init", DispatcherOnNode.IsValid());
			AITEST_TRUE("Dispatcher on Instance should init", DispatcherOnInstance.IsValid());
			AITEST_TRUE("Listener on Node should init", ListenerOnNode1.IsValid() && ListenerOnNode2.IsValid());
			AITEST_TRUE("Listener on Instance should init", ListenerOnInstance.IsValid());

			AITEST_EQUAL("ListenerOnNode1 should be bound to Dispatcher on Node", DispatcherOnNode, ListenerOnNode1.GetDispatcher());
			AITEST_EQUAL("ListenerOnNode2 should be bound to Dispatcher on Instance", DispatcherOnInstance, ListenerOnNode2.GetDispatcher());
			AITEST_EQUAL("ListenerOnInstance should be bound to Dispatcher on Node", DispatcherOnNode, ListenerOnInstance.GetDispatcher());
		}

		{
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("MetaStory should init", bInitSucceeded);

			Exec.Start();
			AITEST_TRUE("Expected Root to be active.", Exec.ExpectInActiveStates("Root"));
			Exec.LogClear();

			Exec.Tick(0.1f);
			AITEST_TRUE("Expected Root to be active.", Exec.ExpectInActiveStates("Root"));
			AITEST_TRUE("ListenerOnNode1 should be broadcasted", bListenerOnNode1Broadcasted);
			AITEST_TRUE("ListenerOnNode2 should be broadcasted", bListenerOnNode2Broadcasted);
			AITEST_TRUE("ListenerOnInstance should be broadcasted", bListenerOnInstanceBroadcasted);

			Exec.Stop();
		}


		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMetaStoryTest_Delegate_ListenerDispatcherOnNode, "System.MetaStory.Delegate.ListenerDispatcherOnNode");

struct FMetaStoryTest_Delegate_DispatcherOnNodeListenerOnTransition : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		// Root(Dispatcher)
		//     State1(Transition on Delegate -> DispatcherOnNode) -> State2
		//	   State2(Transition on Delegate -> DispatcherOnInstance) -> Tree Succeeded

		UMetaStory& MetaStory = NewMetaStory();
		UMetaStoryEditorData& EditorData = *Cast<UMetaStoryEditorData>(MetaStory.EditorData);

		UMetaStoryState& Root = EditorData.AddSubTree(FName(TEXT("Root")));
		TMetaStoryTypedEditorNode<FTestTask_DispatcherOnNodeAndInstance> DispatcherTaskNode = Root.AddTask<FTestTask_DispatcherOnNodeAndInstance>(TEXT("Dispatcher"));
		UMetaStoryState& State1 = Root.AddChildState(FName(TEXT("State1")));
		FMetaStoryTransition& DelegateOnNodeTransition = State1.AddTransition(EMetaStoryTransitionTrigger::OnDelegate, EMetaStoryTransitionType::NextState);

		UMetaStoryState& State2 = Root.AddChildState(FName(TEXT("State2")));
		FMetaStoryTransition& DelegateOnInstanceTransition = State2.AddTransition(EMetaStoryTransitionTrigger::OnDelegate, EMetaStoryTransitionType::Succeeded);

		EditorData.AddPropertyBinding(
			FPropertyBindingPath(DispatcherTaskNode.GetNodeID(), GET_MEMBER_NAME_CHECKED(FTestTask_DispatcherOnNodeAndInstance, DispatcherOnNode)),
			FPropertyBindingPath(DelegateOnNodeTransition.ID, GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, DelegateListener)));

		EditorData.AddPropertyBinding(
			FPropertyBindingPath(DispatcherTaskNode.ID, GET_MEMBER_NAME_CHECKED(FTestTask_DispatcherOnNodeAndInstance_InstanceData, DispatcherOnInstance)),
			FPropertyBindingPath(DelegateOnInstanceTransition.ID, GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, DelegateListener)));

		{
			FMetaStoryCompilerLog Log;
			FMetaStoryCompiler Compiler(Log);
			const bool bResult = Compiler.Compile(MetaStory);
			AITEST_TRUE("MetaStory should get compiled", bResult);

			const FMetaStoryCompactState& RootState = MetaStory.GetStates()[0];
			const FMetaStoryCompactState& State1State = MetaStory.GetStates()[1];
			const FMetaStoryCompactState& State2State = MetaStory.GetStates()[2];

			const FTestTask_DispatcherOnNodeAndInstance& DispatcherTask = MetaStory.GetNode(RootState.TasksBegin).Get<const FTestTask_DispatcherOnNodeAndInstance>();
			const FMetaStoryDelegateDispatcher DispatcherOnNode = DispatcherTask.DispatcherOnNode;
			const FMetaStoryDelegateDispatcher DispatcherOnInstance = MetaStory.GetDefaultInstanceData().GetStruct(DispatcherTask.InstanceTemplateIndex.Get()).Get<const FTestTask_DispatcherOnNodeAndInstance_InstanceData>().DispatcherOnInstance;

			AITEST_TRUE("Dispatcher on node should be valid", DispatcherOnNode.IsValid());
			AITEST_TRUE("Dispatcher on instance should be valid", DispatcherOnInstance.IsValid());

			const FMetaStoryCompactStateTransition* CompactDelegateOnNodeTransition = MetaStory.GetTransitionFromIndex(FMetaStoryIndex16(State1State.TransitionsBegin));
			const FMetaStoryCompactStateTransition* CompactDelegateOnInstanceTransition = MetaStory.GetTransitionFromIndex(FMetaStoryIndex16(State2State.TransitionsBegin));

			AITEST_EQUAL("State1 Transition Delegate Listener should be bound to Dispatcher on Node", CompactDelegateOnNodeTransition->RequiredDelegateDispatcher, DispatcherOnNode);
			AITEST_EQUAL("State2 Transition Delegate Listener should be bound to Dispatcher on Instance", CompactDelegateOnInstanceTransition->RequiredDelegateDispatcher, DispatcherOnInstance);
		}

		{
			FMetaStoryInstanceData InstanceData;
			FTestMetaStoryExecutionContext Exec(MetaStory, MetaStory, InstanceData);
			const bool bInitSucceeded = Exec.IsValid();
			AITEST_TRUE("MetaStory should init", bInitSucceeded);

			Exec.Start();
			AITEST_TRUE("Expected [Root, State1] to be active.", Exec.ExpectInActiveStates("Root", "State1"));
			Exec.LogClear();

			Exec.Tick(0.1f);
			AITEST_TRUE("Expected [Root, State2] to be active.", Exec.ExpectInActiveStates("Root", "State2"));
			Exec.LogClear();

			Exec.Tick(0.1f);
			AITEST_TRUE("Expected Tree to be succeeded.", Exec.GetMetaStoryRunStatus() == EMetaStoryRunStatus::Succeeded);
			Exec.LogClear();
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMetaStoryTest_Delegate_DispatcherOnNodeListenerOnTransition, "System.MetaStory.Delegate.DispatcherOnNodeListenerOnTransition");
} // namespace UE::MetaStory::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
