// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/MetaStoryConditionBlueprintBase.h"
#include "Blueprint/MetaStoryNodeBlueprintBase.h"
#include "MetaStoryExecutionContext.h"
#include "BlueprintNodeHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryConditionBlueprintBase)

//----------------------------------------------------------------------//
//  UMetaStoryConditionBlueprintBase
//----------------------------------------------------------------------//

UMetaStoryConditionBlueprintBase::UMetaStoryConditionBlueprintBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasTestCondition = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveTestCondition"), *this, *StaticClass());
}

bool UMetaStoryConditionBlueprintBase::TestCondition(FMetaStoryExecutionContext& Context) const
{
	if (bHasTestCondition)
	{
		// Cache the owner and event queue for the duration the condition is evaluated.
		SetCachedInstanceDataFromContext(Context);

		const bool bResult = ReceiveTestCondition();

		ClearCachedInstanceData();

		return bResult;
	}
	return false;
}

//----------------------------------------------------------------------//
//  FMetaStoryBlueprintConditionWrapper
//----------------------------------------------------------------------//

bool FMetaStoryBlueprintConditionWrapper::TestCondition(FMetaStoryExecutionContext& Context) const
{
	UMetaStoryConditionBlueprintBase* Condition = Context.GetInstanceDataPtr<UMetaStoryConditionBlueprintBase>(*this);
	check(Condition);
	return Condition->TestCondition(Context);
}

#if WITH_EDITOR
FText FMetaStoryBlueprintConditionWrapper::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const
{
	FText Description;
	if (const UMetaStoryConditionBlueprintBase* Instance = InstanceDataView.GetPtr<UMetaStoryConditionBlueprintBase>())
	{
		Description = Instance->GetDescription(ID, InstanceDataView, BindingLookup, Formatting);
	}
	if (Description.IsEmpty() && ConditionClass)
	{
		Description = ConditionClass->GetDisplayNameText();
	}
	return Description;
}

FName FMetaStoryBlueprintConditionWrapper::GetIconName() const
{
	if (ConditionClass)
	{
		if (const UMetaStoryNodeBlueprintBase* NodeCDO = GetDefault<const UMetaStoryNodeBlueprintBase>(ConditionClass))
		{
			return NodeCDO->GetIconName();
		}
	}

	return FMetaStoryConditionBase::GetIconName();
}

FColor FMetaStoryBlueprintConditionWrapper::GetIconColor() const
{
	if (ConditionClass)
	{
		if (const UMetaStoryNodeBlueprintBase* NodeCDO = GetDefault<const UMetaStoryNodeBlueprintBase>(ConditionClass))
		{
			return NodeCDO->GetIconColor();
		}
	}

	return FMetaStoryConditionBase::GetIconColor();
}
#endif