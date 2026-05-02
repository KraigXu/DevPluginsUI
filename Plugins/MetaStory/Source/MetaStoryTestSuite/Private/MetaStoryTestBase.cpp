// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryTestBase.h"
#include "MetaStoryTest.h"
#include "MetaStoryEditorData.h"
#include "Engine/World.h"
#include "GameplayTagsManager.h"

#define LOCTEXT_NAMESPACE "AITestSuite_MetaStoryTest"

namespace UE::MetaStory::Tests
{

UMetaStory& FMetaStoryTestBase::NewMetaStory() const
{
	UMetaStory* MetaStory = NewObject<UMetaStory>(&GetWorld());
	check(MetaStory);
	UMetaStoryEditorData* EditorData = NewObject<UMetaStoryEditorData>(MetaStory);
	check(EditorData);
	MetaStory->EditorData = EditorData;
	EditorData->Schema = NewObject<UMetaStoryTestSchema>();
	return *MetaStory;
}

FMetaStoryPropertyPathBinding FMetaStoryTestBase::MakeBinding(const FGuid& SourceID, const FStringView Source, const FGuid& TargetID, const FStringView Target, const bool bInIsOutputBinding /*false*/)
{
	FPropertyBindingPath SourcePath;
	SourcePath.FromString(Source);
	SourcePath.SetStructID(SourceID);

	FPropertyBindingPath TargetPath;
	TargetPath.FromString(Target);
	TargetPath.SetStructID(TargetID);

	return FMetaStoryPropertyPathBinding(SourcePath, TargetPath, bInIsOutputBinding);
}

// Helper struct to define some test tags
struct FNativeGameplayTags : public FGameplayTagNativeAdder
{
	virtual ~FNativeGameplayTags() {}

	FGameplayTag TestTag;
	FGameplayTag TestTag2;
	FGameplayTag TestTag3;

	virtual void AddTags() override
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		TestTag = Manager.AddNativeGameplayTag(TEXT("Test.MetaStory.Tag"));
		TestTag2 = Manager.AddNativeGameplayTag(TEXT("Test.MetaStory.Tag2"));
		TestTag2 = Manager.AddNativeGameplayTag(TEXT("Test.MetaStory.Tag3"));
	}
};
static FNativeGameplayTags GameplayTagStaticInstance;

FGameplayTag FMetaStoryTestBase::GetTestTag1()
{
	return GameplayTagStaticInstance.TestTag;
}

FGameplayTag FMetaStoryTestBase::GetTestTag2()
{
	return GameplayTagStaticInstance.TestTag2;
}

FGameplayTag FMetaStoryTestBase::GetTestTag3()
{
	return GameplayTagStaticInstance.TestTag3;
}

FInstancedPropertyBag& FMetaStoryTestBase::GetRootPropertyBag(UMetaStoryEditorData& EditorData) const
{
	return const_cast<FInstancedPropertyBag&>(EditorData.GetRootParametersPropertyBag());
}
}//namespace UE::MetaStory::Tests

#undef LOCTEXT_NAMESPACE

