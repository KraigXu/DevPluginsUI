// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AsyncMetaStoryDiff.h"
#include "DiffUtils.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectKey.h"
#include "UObject/StrongObjectPtr.h"

#define UE_API METASTORYEDITORMODULE_API

class FBlueprintDifferenceTreeEntry;
class FMetaStoryViewModel;
class FUICommandList;
class SLinkableScrollBar;
class SMetaStoryView;
class SWidget;
class UMetaStory;

namespace UE::MetaStory::Diff
{
struct FSingleDiffEntry;

class FDiffWidgets
{
public:
	UE_API explicit FDiffWidgets(const UMetaStory* InStateTree);

	/** Returns actual widget that is used to display trees */
	UE_API TSharedRef<SMetaStoryView> GetStateTreeWidget() const;

private:
	TSharedPtr<SMetaStoryView> MetaStoryTreeView;
	TSharedPtr<FMetaStoryViewModel> MetaStoryViewModel;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnStateDiffEntryFocused, const FSingleDiffEntry&)

class FDiffControl : public TSharedFromThis<FDiffControl>
{
public:
	FDiffControl() = delete;
	FDiffControl(const FDiffControl& Other) = delete;
	FDiffControl(const FDiffControl&& Other) = delete;
	UE_API FDiffControl(const UMetaStory* InOldObject, const UMetaStory* InNewObject, const FOnDiffEntryFocused& InSelectionCallback);
	UE_API ~FDiffControl();

	UE_API TSharedRef<SMetaStoryView> GetDetailsWidget(const UMetaStory* Object) const;

	FOnStateDiffEntryFocused& GetOnStateDiffEntryFocused()
	{
		return OnStateDiffEntryFocused;
	};

	UE_API void GenerateTreeEntries(TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutDifferences);
	TConstArrayView<FSingleDiffEntry> GetBindingDifferences() const
	{
		return BindingDiffs;
	}

protected:
	static UE_API FText RightRevision;
	static UE_API TSharedRef<SWidget> GenerateSingleEntryWidget(FSingleDiffEntry DiffEntry, FText ObjectName);

	UE_API TSharedRef<SMetaStoryView> InsertObject(TNotNull<const UMetaStory*> MetaStory);

	UE_API void OnSelectDiffEntry(const FSingleDiffEntry StateDiff);

	FOnDiffEntryFocused OnDiffEntryFocused;
	FOnStateDiffEntryFocused OnStateDiffEntryFocused;

	TArray<TStrongObjectPtr<const UMetaStory>> DisplayedAssets;

	struct FMetaStoryTreeDiffPairs
	{
		TSharedPtr<FAsyncDiff> Left;
		TSharedPtr<FAsyncDiff> Right;
	};
	TMap<FObjectKey, FMetaStoryTreeDiffPairs> MetaStoryDifferences;
	TArray<FSingleDiffEntry> BindingDiffs;
	TMap<FObjectKey, FDiffWidgets> MetaStoryDiffWidgets;
};

} // UE::MetaStory::Diff

#undef UE_API
