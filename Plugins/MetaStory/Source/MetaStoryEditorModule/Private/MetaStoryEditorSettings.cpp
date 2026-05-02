// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEditorSettings)

UMetaStoryEditorSettings::UMetaStoryEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

FText UMetaStoryEditorSettings::GetSectionText() const
{
	return NSLOCTEXT("MetaStoryEditor", "MetaStoryEditorSettingsName", "MetaStory Editor");
}

FText UMetaStoryEditorSettings::GetSectionDescription() const
{
	return NSLOCTEXT("MetaStoryEditor", "MetaStoryEditorSettingsDescription", "Configure options for the MetaStory Editor.");
}

#endif // WITH_EDITOR

FName UMetaStoryEditorSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}
