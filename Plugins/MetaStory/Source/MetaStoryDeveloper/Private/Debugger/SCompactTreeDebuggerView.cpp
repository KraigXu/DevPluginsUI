// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_METASTORY_TRACE_DEBUGGER

#include "Debugger/SMetaStoryCompactTreeDebuggerView.h"
#include "MetaStory.h"
#include "MetaStoryStyle.h"
#include "Widgets/Views/SListView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SMetaStoryCompactTreeDebuggerView)

namespace UE::MetaStory
{

void SCompactTreeDebuggerView::Construct(const FArguments& InArgs, const TNotNull<const UMetaStory*> MetaStory)
{
	AllActiveStates = InArgs._ActiveStates;

	SCompactTreeView::Construct(
		SCompactTreeView::FArguments()
		.SelectionMode(ESelectionMode::Single)
		.TextStyle(&FMetaStoryStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.State.Title"))
		, MetaStory);
}

void SCompactTreeDebuggerView::CacheStatesInternal()
{
	if (const UMetaStory* MetaStory = WeakMetaStory.Get())
	{
		const FMetaStoryTraceActiveStates::FAssetActiveStates* PerAssetActiveStates = AllActiveStates.Get().PerAssetStates.FindByPredicate(
			[MetaStory](const FMetaStoryTraceActiveStates::FAssetActiveStates& AssetActiveStates)
			{
				return AssetActiveStates.WeakMetaStory == MetaStory;
			});

		if (PerAssetActiveStates && PerAssetActiveStates->ActiveStates.Num() > 0)
		{
			FMetaStoryTraceActiveStates::FAssetActiveStates AssetActiveStates = *PerAssetActiveStates;
			TArray<FProcessedState> ProcessedStates;
			CacheStateRecursive(&AssetActiveStates, RootItem, AssetActiveStates.ActiveStates[0].Index, ProcessedStates);
		}
	}
}

void SCompactTreeDebuggerView::CacheStateRecursive(
	TNotNull<const FMetaStoryTraceActiveStates::FAssetActiveStates*> InAssetActiveStates
	, TSharedPtr<FStateItem> InParentItem
	, const uint16 InStateIdx
	, TArray<FProcessedState>& OutProcessedStates)
{
	TNotNull<const UMetaStory*> MetaStory = InAssetActiveStates->WeakMetaStory.Get();
	const FProcessedState ProcessedState(MetaStory, InStateIdx);
	if (OutProcessedStates.Contains(ProcessedState))
	{
		return;
	}

	OutProcessedStates.Add(ProcessedState);

	const FMetaStoryCompactState& State = MetaStory->GetStates()[InStateIdx];
	FString StringForDebug = State.Name.ToString();

	const bool bIsActiveState = InAssetActiveStates->ActiveStates.Contains(FMetaStoryStateHandle(InStateIdx));
	auto AddStateItem = [&InParentItem, State, MetaStory, InStateIdx](const TSharedRef<FStateItem>& StateItem, const bool bIsActiveState)
		{
			StateItem->Desc = FText::FromName(State.Name);
			StateItem->StateID = MetaStory->GetStateIdFromHandle(FMetaStoryStateHandle(InStateIdx));
			StateItem->TooltipText = FText::FromName(State.Name);
			StateItem->bIsEnabled = State.bEnabled;
			StateItem->CustomData.GetMutable<UE::MetaStory::CompactTreeView::FMetaStoryStateItemDebuggerData>().bIsActive = bIsActiveState;
			StateItem->Color = FMetaStoryStyle::Get().GetSlateColor("MetaStory.CompactView.State");
			StateItem->Icon = FMetaStoryStyle::GetBrushForSelectionBehaviorType(State.SelectionBehavior, State.HasChildren(), State.Type);

			InParentItem->Children.Add(StateItem);
			InParentItem = StateItem;
		};

	// Add subtree
	if (State.LinkedState.IsValid())
	{
		AddStateItem(CreateStateItemInternal(), bIsActiveState);

		// Recurse to linked tree only for active states
		if (bIsActiveState)
		{
			CacheStateRecursive(InAssetActiveStates, InParentItem, State.LinkedState.Index, OutProcessedStates);
		}
	}
	// Add external subtree
	else if (State.LinkedAsset)
	{
		AddStateItem(CreateStateItemInternal(), bIsActiveState);

		// Recurse to linked tree only for active states
		if (bIsActiveState)
		{
			const TConstArrayView<FMetaStoryCompactState> States = State.LinkedAsset->GetStates();
			if (States.Num())
			{
				const FMetaStoryTraceActiveStates::FAssetActiveStates* LinkedAssetActiveStates = AllActiveStates.Get().PerAssetStates.FindByPredicate(
					[MetaStory = State.LinkedAsset](const FMetaStoryTraceActiveStates::FAssetActiveStates& AssetActiveStates)
					{
						return AssetActiveStates.WeakMetaStory == MetaStory;
					});
				if (LinkedAssetActiveStates && LinkedAssetActiveStates->ActiveStates.Num() > 0)
				{
					FMetaStoryTraceActiveStates::FAssetActiveStates AssetActiveStates = *LinkedAssetActiveStates;
					TArray<FProcessedState> ProcessedStates;
					CacheStateRecursive(&AssetActiveStates, InParentItem, AssetActiveStates.ActiveStates[0].Index, OutProcessedStates);
				}
			}
		}
	}
	// Skip empty groups
	else if (!(State.Type == EMetaStoryStateType::Group && !State.HasChildren()))
	{
		AddStateItem(CreateStateItemInternal(), bIsActiveState);

		if (State.HasChildren())
		{
			for (uint16 ChildIdx = State.ChildrenBegin; ChildIdx < State.ChildrenEnd; ChildIdx++)
			{
				CacheStateRecursive(InAssetActiveStates, InParentItem, ChildIdx, OutProcessedStates);
			}
		}
	}
}

TSharedRef<SCompactTreeView::FStateItem> SCompactTreeDebuggerView::CreateStateItemInternal() const
{
	const TSharedRef<FStateItem> StateItem = MakeShared<FStateItem>();
	StateItem->CustomData = TInstancedStruct<UE::MetaStory::CompactTreeView::FMetaStoryStateItemDebuggerData>::Make({});
	return StateItem;
}

TSharedRef<SWidget> SCompactTreeDebuggerView::CreateNameWidgetInternal(TSharedPtr<FStateItem> Item) const
{
	return SNew(SBox)
		.VAlign(VAlign_Fill)
		.Padding(0.f, 2.f)
		[
			SNew(SBorder)
			.BorderImage(FMetaStoryStyle::Get().GetBrush("MetaStory.State.Border"))
			.BorderBackgroundColor_Lambda(
				[Item]
				{
					if (const FStateItem* StateItem = Item.Get())
					{
						if (StateItem->CustomData.Get<UE::MetaStory::CompactTreeView::FMetaStoryStateItemDebuggerData>().bIsActive)
						{
							return FMetaStoryStyle::Get().GetSlateColor("MetaStory.Debugger.State.Active");
						}
					}
					return FSlateColor(FLinearColor::Transparent);
				})
			[
				SNew(SBorder)
				.BorderImage(FMetaStoryStyle::Get().GetBrush("MetaStory.State"))
				.BorderBackgroundColor(FMetaStoryStyle::Get().GetSlateColor("MetaStory.CompactView.State"))
				.Padding(FMargin(0.f, 0.f, 12.f, 0.f))
				.ToolTipText(Item->TooltipText)
				.IsEnabled_Lambda([Item]
					{
						return Item && Item->bIsEnabled;
					})
				[
					SCompactTreeView::CreateNameWidgetInternal(Item)
				]
			]
		];
}

} // UE::MetaStory
#endif // WITH_METASTORY_TRACE_DEBUGGER
