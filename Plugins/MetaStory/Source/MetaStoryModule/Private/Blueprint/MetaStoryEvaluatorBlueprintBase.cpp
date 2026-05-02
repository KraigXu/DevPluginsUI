// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/MetaStoryEvaluatorBlueprintBase.h"
#include "Blueprint/MetaStoryNodeBlueprintBase.h"
#include "MetaStoryExecutionContext.h"
#include "BlueprintNodeHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEvaluatorBlueprintBase)

//----------------------------------------------------------------------//
//  UMetaStoryEvaluatorBlueprintBase
//----------------------------------------------------------------------//

UMetaStoryEvaluatorBlueprintBase::UMetaStoryEvaluatorBlueprintBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasTreeStart = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveTreeStart"), *this, *StaticClass());
	bHasTreeStop = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveTreeStop"), *this, *StaticClass());
	bHasTick = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveTick"), *this, *StaticClass());
}

void UMetaStoryEvaluatorBlueprintBase::TreeStart(FMetaStoryExecutionContext& Context)
{
	// Evaluator became active, cache event queue and owner.
	SetCachedInstanceDataFromContext(Context);

	if (bHasTreeStart)
	{
		ReceiveTreeStart();
	}
}

void UMetaStoryEvaluatorBlueprintBase::TreeStop(FMetaStoryExecutionContext& Context)
{
	if (bHasTreeStop)
	{
		ReceiveTreeStop();
	}

	// Evaluator became inactive, clear cached event queue and owner.
	ClearCachedInstanceData();
}

void UMetaStoryEvaluatorBlueprintBase::Tick(FMetaStoryExecutionContext& Context, const float DeltaTime)
{
	if (bHasTick)
	{
		ReceiveTick(DeltaTime);
	}
}

//----------------------------------------------------------------------//
//  FMetaStoryBlueprintEvaluatorWrapper
//----------------------------------------------------------------------//

void FMetaStoryBlueprintEvaluatorWrapper::TreeStart(FMetaStoryExecutionContext& Context) const
{
	UMetaStoryEvaluatorBlueprintBase* Instance = Context.GetInstanceDataPtr<UMetaStoryEvaluatorBlueprintBase>(*this);
	check(Instance);
	Instance->TreeStart(Context);
}

void FMetaStoryBlueprintEvaluatorWrapper::TreeStop(FMetaStoryExecutionContext& Context) const
{
	UMetaStoryEvaluatorBlueprintBase* Instance = Context.GetInstanceDataPtr<UMetaStoryEvaluatorBlueprintBase>(*this);
	check(Instance);
	Instance->TreeStop(Context);
}

void FMetaStoryBlueprintEvaluatorWrapper::Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const
{
	UMetaStoryEvaluatorBlueprintBase* Instance = Context.GetInstanceDataPtr<UMetaStoryEvaluatorBlueprintBase>(*this);
	check(Instance);
	Instance->Tick(Context, DeltaTime);
}

#if WITH_EDITOR
FText FMetaStoryBlueprintEvaluatorWrapper::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	FText Description;
	if (const UMetaStoryEvaluatorBlueprintBase* Instance = InstanceDataView.GetPtr<UMetaStoryEvaluatorBlueprintBase>())
	{
		Description = Instance->GetDescription(ID, InstanceDataView, BindingLookup, Formatting);
	}
	if (Description.IsEmpty() && EvaluatorClass)
	{
		Description = EvaluatorClass->GetDisplayNameText();
	}
	return Description;
}

FName FMetaStoryBlueprintEvaluatorWrapper::GetIconName() const
{
	if (EvaluatorClass)
	{
		if (const UMetaStoryNodeBlueprintBase* NodeCDO = GetDefault<const UMetaStoryNodeBlueprintBase>(EvaluatorClass))
		{
			return NodeCDO->GetIconName();
		}
	}
	return FMetaStoryEvaluatorBase::GetIconName();
}

FColor FMetaStoryBlueprintEvaluatorWrapper::GetIconColor() const
{
	if (EvaluatorClass)
	{
		if (const UMetaStoryNodeBlueprintBase* NodeCDO = GetDefault<const UMetaStoryNodeBlueprintBase>(EvaluatorClass))
		{
			return NodeCDO->GetIconColor();
		}
	}
	return FMetaStoryEvaluatorBase::GetIconColor();
}
#endif