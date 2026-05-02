// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEditorUserSettings)

void UMetaStoryEditorUserSettings::SetStatesViewDisplayNodeType(EMetaStoryEditorUserSettingsNodeType Value)
{
	if (StatesViewDisplayNodeType != Value)
	{
		StatesViewDisplayNodeType = Value;
		OnSettingsChanged.Broadcast();
	}
}


void UMetaStoryEditorUserSettings::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	FName PropertyName = PropertyChangedEvent.Property->GetFName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorUserSettings, StatesViewDisplayNodeType)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorUserSettings, StatesViewStateRowHeight)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaStoryEditorUserSettings, StatesViewNodeRowHeight))
	{
		OnSettingsChanged.Broadcast();
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
