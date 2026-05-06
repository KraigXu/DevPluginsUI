// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryWorldSubsystem.h"
#include "MetaStoryExecutionContext.h"
#include "MetaStoryTypes.h"
#include "Engine/World.h"
#include "Misc/NotNull.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryWorldSubsystem)

void UMetaStoryWorldSubsystem::Deinitialize()
{
	StopMetaStory(EMetaStoryRunStatus::Stopped);
	Super::Deinitialize();
}

void UMetaStoryWorldSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !bStoryRunActive || !ActiveMetaStory)
	{
		return;
	}

	FMetaStoryExecutionContext Context(
		*this,
		*ActiveMetaStory,
		InstanceData,
		FOnCollectMetaStoryExternalData::CreateUObject(this, &UMetaStoryWorldSubsystem::HandleCollectExternalData));

	if (!NativePopulateContextData(Context) || !Context.AreContextDataViewsValid())
	{
		UE_LOG(LogMetaStory, Warning, TEXT("UMetaStoryWorldSubsystem::Tick: context data invalid for '%s'; stopping."), *ActiveMetaStory->GetName());
		bStoryRunActive = false;
		LastRunStatus = EMetaStoryRunStatus::Failed;
		return;
	}

	LastRunStatus = Context.Tick(DeltaTime);
	if (LastRunStatus != EMetaStoryRunStatus::Running)
	{
		bStoryRunActive = false;
	}
}

TStatId UMetaStoryWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMetaStoryWorldSubsystem, STATGROUP_Tickables);
}

bool UMetaStoryWorldSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->IsGameWorld() && bStoryRunActive;
}

void UMetaStoryWorldSubsystem::SetActiveMetaStory(UMetaStory* InMetaStory)
{
	if (ActiveMetaStory == InMetaStory)
	{
		return;
	}

	StopMetaStory(EMetaStoryRunStatus::Stopped);
	ActiveMetaStory = InMetaStory;
}

bool UMetaStoryWorldSubsystem::StartMetaStory()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	if (!ActiveMetaStory || !ActiveMetaStory->IsReadyToRun())
	{
		UE_LOG(LogMetaStory, Warning, TEXT("UMetaStoryWorldSubsystem::StartMetaStory: missing MetaStory or asset not ready to run."));
		return false;
	}

	StopMetaStory(EMetaStoryRunStatus::Stopped);

	InstanceData.CopyFrom(*this, ActiveMetaStory->GetDefaultInstanceData());

	FMetaStoryExecutionContext Context(
		*this,
		*ActiveMetaStory,
		InstanceData,
		FOnCollectMetaStoryExternalData::CreateUObject(this, &UMetaStoryWorldSubsystem::HandleCollectExternalData));

	if (!NativePopulateContextData(Context) || !Context.AreContextDataViewsValid())
	{
		UE_LOG(LogMetaStory, Warning, TEXT("UMetaStoryWorldSubsystem::StartMetaStory: context data invalid for '%s'."), *ActiveMetaStory->GetName());
		LastRunStatus = EMetaStoryRunStatus::Failed;
		bStoryRunActive = false;
		return false;
	}

	const EMetaStoryRunStatus StartResult = Context.Start();
	LastRunStatus = StartResult;
	bStoryRunActive = (StartResult == EMetaStoryRunStatus::Running);

	const bool bOk = (StartResult != EMetaStoryRunStatus::Failed && StartResult != EMetaStoryRunStatus::Unset);
	if (!bOk)
	{
		bStoryRunActive = false;
	}
	return bOk;
}

void UMetaStoryWorldSubsystem::StopMetaStory(const EMetaStoryRunStatus CompletionStatus)
{
	bStoryRunActive = false;

	if (!ActiveMetaStory)
	{
		return;
	}

	FMetaStoryReadOnlyExecutionContext ReadOnly(
		TNotNull<UObject*>(this),
		TNotNull<const UMetaStory*>(ActiveMetaStory.Get()),
		InstanceData);
	if (!ReadOnly.IsValid() || ReadOnly.GetMetaStoryRunStatus() != EMetaStoryRunStatus::Running)
	{
		LastRunStatus = ReadOnly.IsValid() ? ReadOnly.GetMetaStoryRunStatus() : EMetaStoryRunStatus::Unset;
		return;
	}

	FMetaStoryExecutionContext Context(
		*this,
		*ActiveMetaStory,
		InstanceData,
		FOnCollectMetaStoryExternalData::CreateUObject(this, &UMetaStoryWorldSubsystem::HandleCollectExternalData));

	(void)NativePopulateContextData(Context);
	LastRunStatus = Context.Stop(CompletionStatus);
}

bool UMetaStoryWorldSubsystem::NativePopulateContextData(FMetaStoryExecutionContext& Context)
{
	return Context.AreContextDataViewsValid();
}

bool UMetaStoryWorldSubsystem::NativeCollectExternalData(
	const FMetaStoryExecutionContext& Context,
	const UMetaStory* MetaStory,
	TArrayView<const FMetaStoryExternalDataDesc> ExternalDataDescs,
	TArrayView<FMetaStoryDataView> OutDataViews)
{
	(void)Context;
	(void)OutDataViews;

	if (ExternalDataDescs.Num() == 0)
	{
		return true;
	}

	UE_LOG(LogMetaStory, Warning,
		TEXT("UMetaStoryWorldSubsystem: MetaStory '%s' requires external data; override NativeCollectExternalData in a subsystem subclass."),
		MetaStory ? *MetaStory->GetName() : TEXT("null"));
	return false;
}

bool UMetaStoryWorldSubsystem::HandleCollectExternalData(
	const FMetaStoryExecutionContext& Context,
	const UMetaStory* MetaStory,
	TArrayView<const FMetaStoryExternalDataDesc> ExternalDataDescs,
	TArrayView<FMetaStoryDataView> OutDataViews)
{
	return NativeCollectExternalData(Context, MetaStory, ExternalDataDescs, OutDataViews);
}
