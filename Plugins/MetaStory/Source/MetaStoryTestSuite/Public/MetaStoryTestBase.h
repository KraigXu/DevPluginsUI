// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "AITestsCommon.h"

#include "MetaStory.h"
#include "MetaStoryPropertyBindings.h"

class UMetaStoryEditorData;
namespace UE::MetaStory::Tests
{
	
/**
 * Base class for MetaStory test
 */
struct FMetaStoryTestBase : public FAITestBase
{
protected:
	UMetaStory& NewMetaStory() const;
	static FMetaStoryPropertyPathBinding MakeBinding(const FGuid& SourceID, const FStringView Source, const FGuid& TargetID, const FStringView Target, const bool bInIsOutputBinding = false);
	static FGameplayTag GetTestTag1();
	static FGameplayTag GetTestTag2();
	static FGameplayTag GetTestTag3();
	FInstancedPropertyBag& GetRootPropertyBag(UMetaStoryEditorData& EditorData) const;
};

} // namespace UE::MetaStory::Tests