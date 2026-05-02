// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStorySchema.h"
#include "MetaStoryTypes.h"
#include "Blueprint/MetaStoryNodeBlueprintBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStorySchema)

namespace UE::MetaStory::Private
{
	bool bCompletedTransitionStatesCreateNewStates = true;
	FAutoConsoleVariableRef CVarCompletedTransitionStatesCreateNewStates(
		TEXT("MetaStory.SelectState.CompletedTransitionStatesCreateNewStates"),
		bCompletedTransitionStatesCreateNewStates,
		TEXT("Activate the EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates rule.")
	);

	bool bCompletedStateBeforeTransitionSourceFailsTransition = true;
	FAutoConsoleVariableRef CVarCompletedStateBeforeTransitionSourceFailsTransition(
		TEXT("MetaStory.SelectState.CompletedStateBeforeTransitionSourceFailsTransition"),
		bCompletedStateBeforeTransitionSourceFailsTransition,
		TEXT("Activate the EMetaStoryStateSelectionRules::CompletedStateBeforeTransitionSourceFailsTransition rule.")
	);
}


bool UMetaStorySchema::IsChildOfBlueprintBase(const UClass* InClass)
{
	return InClass->IsChildOf<UMetaStoryNodeBlueprintBase>();
}

EMetaStoryParameterDataType UMetaStorySchema::GetGlobalParameterDataType() const
{
	return EMetaStoryParameterDataType::GlobalParameterData;
}

EMetaStoryStateSelectionRules UMetaStorySchema::GetStateSelectionRules() const
{
	EMetaStoryStateSelectionRules Result = EMetaStoryStateSelectionRules::None;
	if (UE::MetaStory::Private::bCompletedTransitionStatesCreateNewStates)
	{
		Result |= EMetaStoryStateSelectionRules::CompletedTransitionStatesCreateNewStates;
	}

	if (UE::MetaStory::Private::bCompletedStateBeforeTransitionSourceFailsTransition)
	{
		Result |= EMetaStoryStateSelectionRules::CompletedStateBeforeTransitionSourceFailsTransition;
	}

	return Result;
}
