#include "Runtime/MetaplotSubsystem.h"

#include "Runtime/MetaplotInstance.h"
#include "Engine/World.h"

void UMetaplotSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	TickAll(DeltaTime);
}

TStatId UMetaplotSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMetaplotSubsystem, STATGROUP_Tickables);
}

bool UMetaplotSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->IsGameWorld();
}

UMetaplotInstance* UMetaplotSubsystem::StartMetaplotInstance(UMetaplotFlow* FlowAsset)
{
	if (!FlowAsset)
	{
		return nullptr;
	}

	UMetaplotInstance* NewInstance = NewObject<UMetaplotInstance>(this);
	if (!NewInstance)
	{
		return nullptr;
	}

	if (!NewInstance->Initialize(FlowAsset) || !NewInstance->Start())
	{
		return nullptr;
	}

	Instances.Add(NewInstance);
	return NewInstance;
}

void UMetaplotSubsystem::TickAll(float DeltaTime)
{
	for (int32 i = Instances.Num() - 1; i >= 0; --i)
	{
		UMetaplotInstance* Instance = Instances[i];
		if (!Instance)
		{
			Instances.RemoveAtSwap(i);
			continue;
		}

		Instance->TickInstance(DeltaTime);
		if (!Instance->IsRunning())
		{
			Instances.RemoveAtSwap(i);
		}
	}
}
