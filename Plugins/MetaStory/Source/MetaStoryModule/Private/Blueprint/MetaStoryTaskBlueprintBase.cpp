// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/MetaStoryTaskBlueprintBase.h"
#include "MetaStoryExecutionContext.h"
#include "BlueprintNodeHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryTaskBlueprintBase)

//----------------------------------------------------------------------//
//  UMetaStoryTaskBlueprintBase
//----------------------------------------------------------------------//
UMetaStoryTaskBlueprintBase::UMetaStoryTaskBlueprintBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bShouldStateChangeOnReselect(true)
	, bShouldCallTick(false) // for when a child overrides the Tick
	, bShouldCallTickOnlyOnEvents(false)
	, bShouldCopyBoundPropertiesOnTick(true)
	, bShouldCopyBoundPropertiesOnExitState(true)
#if WITH_EDITORONLY_DATA
	, bConsideredForCompletion(true)
	, bCanEditConsideredForCompletion(true)
#endif
	, bIsProcessingEnterStateOrTick(false)
{
	bHasExitState = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveExitState"), *this, *StaticClass());
	bHasStateCompleted = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveStateCompleted"), *this, *StaticClass());
	bHasLatentEnterState = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveLatentEnterState"), *this, *StaticClass());
	bHasLatentTick = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveLatentTick"), *this, *StaticClass());
PRAGMA_DISABLE_DEPRECATION_WARNINGS		
	bHasEnterState_DEPRECATED = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveEnterState"), *this, *StaticClass());
	bHasTick_DEPRECATED = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveTick"), *this, *StaticClass());
PRAGMA_ENABLE_DEPRECATION_WARNINGS	
}

EMetaStoryRunStatus UMetaStoryTaskBlueprintBase::EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition)
{
	// Task became active, cache event queue and owner.
	SetCachedInstanceDataFromContext(Context);

	FGuardValue_Bitfield(bIsProcessingEnterStateOrTick, true);

	// Reset status to running since the same task may be restarted.
	RunStatus = EMetaStoryRunStatus::Running;

	if (bHasLatentEnterState)
	{
		// Note: the name contains latent just to differentiate it from the deprecated version (the old version did not allow latent actions to be started).
		ReceiveLatentEnterState(Transition);
	}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	else if (bHasEnterState_DEPRECATED)
	{
		RunStatus = ReceiveEnterState(Transition);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	return RunStatus;
}

void UMetaStoryTaskBlueprintBase::ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition)
{
	if (bHasExitState)
	{
		ReceiveExitState(Transition);
	}

	if (UWorld* CurrentWorld = GetWorld())
	{
		CurrentWorld->GetLatentActionManager().RemoveActionsForObject(this);
		CurrentWorld->GetTimerManager().ClearAllTimersForObject(this);
	}

	// Task became inactive, clear cached event queue and owner.
	ClearCachedInstanceData();
}

void UMetaStoryTaskBlueprintBase::StateCompleted(FMetaStoryExecutionContext& Context, const EMetaStoryRunStatus CompletionStatus, const FMetaStoryActiveStates& CompletedActiveStates)
{
	if (bHasStateCompleted)
	{
		ReceiveStateCompleted(CompletionStatus, CompletedActiveStates);
	}
}

EMetaStoryRunStatus UMetaStoryTaskBlueprintBase::Tick(FMetaStoryExecutionContext& Context, const float DeltaTime)
{
	FGuardValue_Bitfield(bIsProcessingEnterStateOrTick, true);

	if (bHasLatentTick)
	{
		// Note: the name contains latent just to differentiate it from the deprecated version (the old version did not allow latent actions to be started).
		ReceiveLatentTick(DeltaTime);
	}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	else if (bHasTick_DEPRECATED)
	{
		RunStatus = ReceiveTick(DeltaTime);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	return RunStatus;
}

void UMetaStoryTaskBlueprintBase::FinishTask(const bool bSucceeded)
{
	RunStatus = bSucceeded ? EMetaStoryRunStatus::Succeeded : EMetaStoryRunStatus::Failed;
	if (!bIsProcessingEnterStateOrTick)
	{
		EMetaStoryFinishTaskType CompletionType = bSucceeded ? EMetaStoryFinishTaskType::Succeeded : EMetaStoryFinishTaskType::Failed;
		GetWeakExecutionContext().FinishTask(CompletionType);
	}
}

void UMetaStoryTaskBlueprintBase::BroadcastDelegate(FMetaStoryDelegateDispatcher Dispatcher)
{
	if (!GetWeakExecutionContext().BroadcastDelegate(Dispatcher)) 
	{
		UE_VLOG_UELOG(this, LogMetaStory, Error, TEXT("Failed to broadcast the delegate. The instance probably stopped."));
	}
}

void UMetaStoryTaskBlueprintBase::BindDelegate(const FMetaStoryDelegateListener& Listener, const FMetaStoryDynamicDelegate& Delegate)
{
	FSimpleDelegate SimpleDelegate = FSimpleDelegate::CreateUFunction(const_cast<UObject*>(Delegate.GetUObject()), Delegate.GetFunctionName());
	if (!GetWeakExecutionContext().BindDelegate(Listener, MoveTemp(SimpleDelegate)))
	{
		UE_VLOG_UELOG(this, LogMetaStory, Error, TEXT("Failed to bind the delegate. The instance probably stopped."));
	}
}

void UMetaStoryTaskBlueprintBase::UnbindDelegate(const FMetaStoryDelegateListener& Listener)
{
	if (!GetWeakExecutionContext().UnbindDelegate(Listener))
	{
		UE_VLOG_UELOG(this, LogMetaStory, Error, TEXT("Failed to unbind the delegate. The instance probably stopped."));
	}
}

//----------------------------------------------------------------------//
//  FMetaStoryBlueprintTaskWrapper
//----------------------------------------------------------------------//

bool FMetaStoryBlueprintTaskWrapper::Link(FMetaStoryLinker& Linker)
{
	bShouldStateChangeOnReselect = (TaskFlags & 0x01) != 0;
	bShouldCallTick = (TaskFlags & 0x02) != 0;
	bShouldCallTickOnlyOnEvents = (TaskFlags & 0x04) != 0;
	bShouldCopyBoundPropertiesOnTick = (TaskFlags & 0x08) != 0;
	bShouldCopyBoundPropertiesOnExitState = (TaskFlags & 0x10) != 0;

	return Super::Link(Linker);
}

EMetaStoryRunStatus FMetaStoryBlueprintTaskWrapper::EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const
{
	UMetaStoryTaskBlueprintBase* Instance = Context.GetInstanceDataPtr<UMetaStoryTaskBlueprintBase>(*this);
	check(Instance);
	return Instance->EnterState(Context, Transition);
}

void FMetaStoryBlueprintTaskWrapper::ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const
{
	UMetaStoryTaskBlueprintBase* Instance = Context.GetInstanceDataPtr<UMetaStoryTaskBlueprintBase>(*this);
	check(Instance);
	Instance->ExitState(Context, Transition);
}

void FMetaStoryBlueprintTaskWrapper::StateCompleted(FMetaStoryExecutionContext& Context, const EMetaStoryRunStatus CompletionStatus, const FMetaStoryActiveStates& CompletedActiveStates) const
{
	UMetaStoryTaskBlueprintBase* Instance = Context.GetInstanceDataPtr<UMetaStoryTaskBlueprintBase>(*this);
	check(Instance);
	Instance->StateCompleted(Context, CompletionStatus, CompletedActiveStates);
}

EMetaStoryRunStatus FMetaStoryBlueprintTaskWrapper::Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const
{
	UMetaStoryTaskBlueprintBase* Instance = Context.GetInstanceDataPtr<UMetaStoryTaskBlueprintBase>(*this);
	check(Instance);
	return Instance->Tick(Context, DeltaTime);
}

#if WITH_EDITOR
EDataValidationResult FMetaStoryBlueprintTaskWrapper::Compile(UE::MetaStory::ICompileNodeContext& Context)
{
	const UMetaStoryTaskBlueprintBase& InstanceData = Context.GetInstanceDataView().Get<UMetaStoryTaskBlueprintBase>();

	// Copy over ticking related options.
	bShouldStateChangeOnReselect = InstanceData.bShouldStateChangeOnReselect;

	bShouldCallTick = InstanceData.bShouldCallTick || InstanceData.bHasLatentTick;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		bShouldCallTick |= InstanceData.bHasTick_DEPRECATED;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
		bShouldCallTickOnlyOnEvents = InstanceData.bShouldCallTickOnlyOnEvents;
	bShouldCopyBoundPropertiesOnTick = InstanceData.bShouldCopyBoundPropertiesOnTick;
	bShouldCopyBoundPropertiesOnExitState = InstanceData.bShouldCopyBoundPropertiesOnExitState;

	// The flags on the FMetaStoryTaskBase are not saved.
	TaskFlags = (bShouldStateChangeOnReselect ? 0x01 : 0)
		| (bShouldCallTick ? 0x02 : 0)
		| (bShouldCallTickOnlyOnEvents ? 0x04 : 0)
		| (bShouldCopyBoundPropertiesOnTick ? 0x08 : 0)
		| (bShouldCopyBoundPropertiesOnExitState ? 0x10 : 0);

	return EDataValidationResult::Valid;
}

FText FMetaStoryBlueprintTaskWrapper::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	FText Description;
	if (const UMetaStoryTaskBlueprintBase* Instance = InstanceDataView.GetPtr<UMetaStoryTaskBlueprintBase>())
	{
		Description = Instance->GetDescription(ID, InstanceDataView, BindingLookup, Formatting);
	}
	if (Description.IsEmpty() && TaskClass)
	{
		Description = TaskClass->GetDisplayNameText();
	}
	return Description;
}

FName FMetaStoryBlueprintTaskWrapper::GetIconName() const
{
	if (TaskClass)
	{
		if (const UMetaStoryNodeBlueprintBase* NodeCDO = GetDefault<const UMetaStoryNodeBlueprintBase>(TaskClass))
		{
			return NodeCDO->GetIconName();
		}
	}

	return FMetaStoryTaskBase::GetIconName();
}

FColor FMetaStoryBlueprintTaskWrapper::GetIconColor() const
{
	if (TaskClass)
	{
		if (const UMetaStoryNodeBlueprintBase* NodeCDO = GetDefault<const UMetaStoryNodeBlueprintBase>(TaskClass))
		{
			return NodeCDO->GetIconColor();
		}
	}

	return FMetaStoryTaskBase::GetIconColor();
}
#endif