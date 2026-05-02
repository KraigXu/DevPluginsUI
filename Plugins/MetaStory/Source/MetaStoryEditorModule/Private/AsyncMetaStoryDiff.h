// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AsyncTreeDifferences.h"

#define UE_API METASTORYEDITORMODULE_API

class SMetaStoryView;
class UMetaStoryState;

namespace UE::MetaStory::Diff
{
struct FSingleDiffEntry;

class FAsyncDiff : public TAsyncTreeDifferences<TWeakObjectPtr<UMetaStoryState>>
{
public:
	UE_API FAsyncDiff(const TSharedRef<SMetaStoryView>& LeftTree, const TSharedRef<SMetaStoryView>& RightTree);

	UE_API void GetStateTreeDifferences(TArray<FSingleDiffEntry>& OutDiffEntries) const;

private:
	UE_API void GetStatesDifferences(TArray<FSingleDiffEntry>& OutDiffEntries) const;

	static UE_API TAttribute<TArray<TWeakObjectPtr<UMetaStoryState>>> RootNodesAttribute(TWeakPtr<SMetaStoryView> MetaStoryView);

	TSharedPtr<SMetaStoryView> LeftView;
	TSharedPtr<SMetaStoryView> RightView;
};

} // UE::MetaStory::Diff

template <>
class TTreeDiffSpecification<TWeakObjectPtr<UMetaStoryState>>
{
public:
	UE_API bool AreValuesEqual(const TWeakObjectPtr<UMetaStoryState>& MetaStoryNodeA, const TWeakObjectPtr<UMetaStoryState>& MetaStoryNodeB, TArray<FPropertySoftPath>* OutDifferingProperties = nullptr) const;

	UE_API bool AreMatching(const TWeakObjectPtr<UMetaStoryState>& MetaStoryNodeA, const TWeakObjectPtr<UMetaStoryState>& MetaStoryNodeB, TArray<FPropertySoftPath>* OutDifferingProperties = nullptr) const;

	UE_API void GetChildren(const TWeakObjectPtr<UMetaStoryState>& InParent, TArray<TWeakObjectPtr<UMetaStoryState>>& OutChildren) const;

	bool ShouldMatchByValue(const TWeakObjectPtr<UMetaStoryState>&) const
	{
		return false;
	}

	bool ShouldInheritEqualFromChildren(const TWeakObjectPtr<UMetaStoryState>&, const TWeakObjectPtr<UMetaStoryState>&) const
	{
		return false;
	}
};

#undef UE_API
