// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SMetaStoryCompactTreeView.h"

#if WITH_METASTORY_TRACE_DEBUGGER
#include "Debugger/MetaStoryTraceTypes.h"
#include "Misc/Attribute.h"
#endif

#include "SMetaStoryCompactTreeDebuggerView.generated.h"

namespace UE::MetaStory
{

namespace CompactTreeView
{
	USTRUCT()
	struct FMetaStoryStateItemDebuggerData : public FMetaStoryStateItemCustomData
	{
		GENERATED_BODY()

		FMetaStoryStateItemDebuggerData() = default;
		explicit FMetaStoryStateItemDebuggerData(const bool bIsActive)
			: bIsActive(bIsActive)
		{
		}

		bool bIsActive = false;
	};
} // CompactTreeView
} // UE::MetaStory

#if WITH_METASTORY_TRACE_DEBUGGER

#define UE_API METASTORYDEVELOPER_API

namespace UE::MetaStory
{

/**
 * Widget that displays a list of MetaStory nodes which match base types and specified schema.
 * Can be used e.g. in popup menus to select node types.
 */
class SCompactTreeDebuggerView final : public SCompactTreeView
{
public:

	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, TConstArrayView<FGuid> /*SelectedStateIDs*/);

	SLATE_BEGIN_ARGS(SCompactTreeDebuggerView)
	{}
		SLATE_ATTRIBUTE(FMetaStoryTraceActiveStates, ActiveStates)
	SLATE_END_ARGS()

	UE_API void Construct(const FArguments& InArgs, TNotNull<const UMetaStory*> MetaStory);

private:
	UE_API virtual TSharedRef<FStateItem> CreateStateItemInternal() const override;
	UE_API virtual void CacheStatesInternal() override;
	UE_API virtual TSharedRef<SWidget> CreateNameWidgetInternal(TSharedPtr<FStateItem> Item) const override;

	struct FProcessedState
	{
		bool operator==(const FProcessedState& Other) const = default;

		FObjectKey MetaStory;
		uint16 StateIdx;
	};

	void CacheStateRecursive(TNotNull<const FMetaStoryTraceActiveStates::FAssetActiveStates*> InAssetActiveStates
		, TSharedPtr<FStateItem> InParentItem
		, const uint16 InStateIdx
		, TArray<FProcessedState>& OutProcessedStates);

	TAttribute<FMetaStoryTraceActiveStates> AllActiveStates;
};

} // UE::MetaStory

#undef UE_API

#endif // WITH_METASTORY_TRACE_DEBUGGER
