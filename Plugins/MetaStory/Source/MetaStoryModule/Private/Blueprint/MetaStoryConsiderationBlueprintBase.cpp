// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/MetaStoryConsiderationBlueprintBase.h"
#include "BlueprintNodeHelpers.h"
#include "MetaStoryExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryConsiderationBlueprintBase)

//----------------------------------------------------------------------//
//  UMetaStoryConsiderationBlueprintBase
//----------------------------------------------------------------------//

UMetaStoryConsiderationBlueprintBase::UMetaStoryConsiderationBlueprintBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasGetScore = BlueprintNodeHelpers::HasBlueprintFunction(GET_FUNCTION_NAME_CHECKED(UMetaStoryConsiderationBlueprintBase, ReceiveGetScore), *this, *StaticClass());
}

float UMetaStoryConsiderationBlueprintBase::GetScore(FMetaStoryExecutionContext& Context) const
{
	if (bHasGetScore)
	{
		// Cache the owner and event queue for the duration the consideration is evaluated.
		SetCachedInstanceDataFromContext(Context);

		const float Score = ReceiveGetScore();

		ClearCachedInstanceData();

		return Score;
	}

	return .0f;
}

//----------------------------------------------------------------------//
//  FMetaStoryBlueprintConsiderationWrapper
//----------------------------------------------------------------------//

#if WITH_EDITOR
FText FMetaStoryBlueprintConsiderationWrapper::GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting /*= EMetaStoryNodeFormatting::Text*/) const
{
	FText Description;
	if (const UMetaStoryConsiderationBlueprintBase* Instance = InstanceDataView.GetPtr<UMetaStoryConsiderationBlueprintBase>())
	{
		Description = Instance->GetDescription(ID, InstanceDataView, BindingLookup, Formatting);
	}
	if (Description.IsEmpty() && ConsiderationClass)
	{
		Description = ConsiderationClass->GetDisplayNameText();
	}
	return Description;
}

FName FMetaStoryBlueprintConsiderationWrapper::GetIconName() const
{
	if (ConsiderationClass)
	{
		if (const UMetaStoryNodeBlueprintBase* NodeCDO = GetDefault<const UMetaStoryNodeBlueprintBase>(ConsiderationClass))
		{
			return NodeCDO->GetIconName();
		}
	}

	return FMetaStoryConsiderationBase::GetIconName();
}

FColor FMetaStoryBlueprintConsiderationWrapper::GetIconColor() const
{
	if (ConsiderationClass)
	{
		if (const UMetaStoryNodeBlueprintBase* NodeCDO = GetDefault<const UMetaStoryNodeBlueprintBase>(ConsiderationClass))
		{
			return NodeCDO->GetIconColor();
		}
	}

	return FMetaStoryConsiderationBase::GetIconColor();
}
#endif //WITH_EDITOR

float FMetaStoryBlueprintConsiderationWrapper::GetScore(FMetaStoryExecutionContext& Context) const
{
	UMetaStoryConsiderationBlueprintBase* Consideration = Context.GetInstanceDataPtr<UMetaStoryConsiderationBlueprintBase>(*this);
	check(Consideration);
	return Consideration->GetScore(Context);
}
