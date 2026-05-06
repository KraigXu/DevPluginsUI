

#include "SMetaStoryFlowGraph.h"

#include "Flow/MetaplotFlow.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layout/Geometry.h"
#include "Layout/PaintGeometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

namespace MetaStoryFlowGraphPlacement
{
	/** 与 MetaplotEditor/MetaplotFlowPlacement 一致；MetaStory 模块不依赖 MetaplotEditor Private 头。 */
	bool IsValidCellForNodeMove(const UMetaplotFlow* Flow, const FGuid& MovingNodeId, int32 NewStage, int32 NewLayer)
	{
		if (!Flow || !MovingNodeId.IsValid())
		{
			return false;
		}

		if (NewStage < 0 || NewLayer < 0)
		{
			return false;
		}

		for (const FMetaplotNode& Node : Flow->Nodes)
		{
			if (Node.NodeId == MovingNodeId)
			{
				continue;
			}
			if (Node.StageIndex == NewStage && Node.LayerIndex == NewLayer)
			{
				return false;
			}
		}

		auto ResolveStage = [&](const FGuid& NodeId) -> int32
		{
			if (NodeId == MovingNodeId)
			{
				return NewStage;
			}
			const FMetaplotNode* Found = Flow->Nodes.FindByPredicate([NodeId](const FMetaplotNode& N)
			{
				return N.NodeId == NodeId;
			});
			return Found ? Found->StageIndex : 0;
		};

		auto ResolveLayer = [&](const FGuid& NodeId) -> int32
		{
			if (NodeId == MovingNodeId)
			{
				return NewLayer;
			}
			const FMetaplotNode* Found = Flow->Nodes.FindByPredicate([NodeId](const FMetaplotNode& N)
			{
				return N.NodeId == NodeId;
			});
			return Found ? Found->LayerIndex : 0;
		};

		for (const FMetaplotTransition& Tr : Flow->Transitions)
		{
			if (!Tr.SourceNodeId.IsValid() || !Tr.TargetNodeId.IsValid() || Tr.SourceNodeId == Tr.TargetNodeId)
			{
				continue;
			}

			const int32 SrcStage = ResolveStage(Tr.SourceNodeId);
			const int32 DstStage = ResolveStage(Tr.TargetNodeId);
			const int32 SrcLayer = ResolveLayer(Tr.SourceNodeId);
			const int32 DstLayer = ResolveLayer(Tr.TargetNodeId);

			if (DstStage <= SrcStage)
			{
				return false;
			}

			if (SrcLayer == DstLayer && DstStage != SrcStage + 1)
			{
				return false;
			}
		}

		return true;
	}
}

namespace MetaplotGraphWidgetPrivate
{
	struct FNodeSearchItem
	{
		EMetaplotNodeType Type = EMetaplotNodeType::Normal;
		FText Label;
		FText Keywords;
	};

	class SNodeSearchMenuWidget : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SNodeSearchMenuWidget) {}
			SLATE_EVENT(FSimpleDelegate, OnCloseMenu)
			SLATE_EVENT(FOnMetaplotGraphCreateNodeRequested, OnCreateNodeRequested)
			SLATE_ARGUMENT(int32, StageIndex)
			SLATE_ARGUMENT(int32, LayerIndex)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			OnCloseMenu = InArgs._OnCloseMenu;
			OnCreateNodeRequested = InArgs._OnCreateNodeRequested;
			StageIndex = InArgs._StageIndex;
			LayerIndex = InArgs._LayerIndex;

			AllItems = {
				MakeItem(EMetaplotNodeType::Start, TEXT("Start"), TEXT("开始 起始 起点")),
				MakeItem(EMetaplotNodeType::Normal, TEXT("Normal"), TEXT("普通 常规")),
				MakeItem(EMetaplotNodeType::Conditional, TEXT("Conditional"), TEXT("条件 分支 判断")),
				MakeItem(EMetaplotNodeType::Parallel, TEXT("Parallel"), TEXT("并行")),
				MakeItem(EMetaplotNodeType::Terminal, TEXT("Terminal"), TEXT("结束 终止"))
			};
			FilteredItems = AllItems;

			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Background"))
				.Padding(8.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(SearchBox, SSearchBox)
						.HintText(FText::FromString(TEXT("搜索节点...")))
						.OnTextChanged(this, &SNodeSearchMenuWidget::OnSearchTextChanged)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 6.0f)
					[
						SNew(STextBlock)
						.Text(FText::Format(FText::FromString(TEXT("创建到 Stage {0} / Layer {1}")), FText::AsNumber(StageIndex), FText::AsNumber(LayerIndex)))
						.ColorAndOpacity(FLinearColor(0.78f, 0.80f, 0.84f, 0.95f))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(ListView, SListView<TSharedPtr<FNodeSearchItem>>)
						.ListItemsSource(&FilteredItems)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SNodeSearchMenuWidget::GenerateRow)
						.OnSelectionChanged(this, &SNodeSearchMenuWidget::OnItemSelected)
						.OnMouseButtonDoubleClick(this, &SNodeSearchMenuWidget::OnItemDoubleClicked)
					]
				]
			];

			if (SearchBox.IsValid())
			{
				FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly);
			}
		}

	private:
		static TSharedPtr<FNodeSearchItem> MakeItem(EMetaplotNodeType Type, const TCHAR* LabelText, const TCHAR* KeywordsText)
		{
			TSharedPtr<FNodeSearchItem> Item = MakeShared<FNodeSearchItem>();
			Item->Type = Type;
			Item->Label = FText::FromString(LabelText);
			Item->Keywords = FText::FromString(KeywordsText);
			return Item;
		}

		void OnSearchTextChanged(const FText& InText)
		{
			const FString Query = InText.ToString().TrimStartAndEnd();
			FilteredItems.Reset();

			for (const TSharedPtr<FNodeSearchItem>& Item : AllItems)
			{
				if (!Item.IsValid())
				{
					continue;
				}

				if (Query.IsEmpty() ||
					Item->Label.ToString().Contains(Query, ESearchCase::IgnoreCase) ||
					Item->Keywords.ToString().Contains(Query, ESearchCase::IgnoreCase))
				{
					FilteredItems.Add(Item);
				}
			}

			if (ListView.IsValid())
			{
				ListView->RequestListRefresh();
			}
		}

		TSharedRef<ITableRow> GenerateRow(TSharedPtr<FNodeSearchItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const
		{
			const FText DisplayText = Item.IsValid() ? Item->Label : FText::FromString(TEXT("Unknown"));
			return SNew(STableRow<TSharedPtr<FNodeSearchItem>>, OwnerTable)
			[
				SNew(STextBlock).Text(DisplayText)
			];
		}

		void OnItemDoubleClicked(TSharedPtr<FNodeSearchItem> Item)
		{
			CommitCreate(Item);
		}

		void OnItemSelected(TSharedPtr<FNodeSearchItem> Item, ESelectInfo::Type SelectInfo)
		{
			if (SelectInfo == ESelectInfo::OnMouseClick || SelectInfo == ESelectInfo::OnKeyPress)
			{
				CommitCreate(Item);
			}
		}

		void CommitCreate(const TSharedPtr<FNodeSearchItem>& Item)
		{
			if (!Item.IsValid())
			{
				return;
			}
			if (OnCreateNodeRequested.IsBound())
			{
				OnCreateNodeRequested.Execute(Item->Type, StageIndex, LayerIndex);
			}
			if (OnCloseMenu.IsBound())
			{
				OnCloseMenu.Execute();
			}
		}

	private:
		FSimpleDelegate OnCloseMenu;
		FOnMetaplotGraphCreateNodeRequested OnCreateNodeRequested;
		int32 StageIndex = 0;
		int32 LayerIndex = 0;
		TSharedPtr<SSearchBox> SearchBox;
		TSharedPtr<SListView<TSharedPtr<FNodeSearchItem>>> ListView;
		TArray<TSharedPtr<FNodeSearchItem>> AllItems;
		TArray<TSharedPtr<FNodeSearchItem>> FilteredItems;
	};

	static constexpr float GridHeaderTop = -36.0f;
	static constexpr float GridHeaderHeight = 16.0f;
	static constexpr float TimelineY = -18.0f;
	static constexpr float StageGridStep = 220.0f;
	static constexpr float LayerGridStep = 140.0f;
	static constexpr float FitPaddingX = 28.0f;
	static constexpr float FitPaddingY = 20.0f;
	static constexpr float ViewInsetLeft = 20.0f;
	static constexpr float ViewInsetTop = 28.0f;
	static constexpr float ViewInsetRight = 12.0f;
	static constexpr float ViewInsetBottom = 12.0f;

	struct FGridRange
	{
		int32 MinStage = 0;
		int32 MaxStage = 3;
		int32 MinLayer = 0;
		int32 MaxLayer = 3;
	};

	static FGridRange BuildGridRange(const UMetaplotFlow* Flow)
	{
		FGridRange Range;
		if (!Flow || Flow->Nodes.IsEmpty())
		{
			return Range;
		}

		Range.MinStage = Flow->Nodes[0].StageIndex;
		Range.MaxStage = Flow->Nodes[0].StageIndex;
		Range.MinLayer = Flow->Nodes[0].LayerIndex;
		Range.MaxLayer = Flow->Nodes[0].LayerIndex;

		for (const FMetaplotNode& Node : Flow->Nodes)
		{
			Range.MinStage = FMath::Min(Range.MinStage, Node.StageIndex);
			Range.MaxStage = FMath::Max(Range.MaxStage, Node.StageIndex);
			Range.MinLayer = FMath::Min(Range.MinLayer, Node.LayerIndex);
			Range.MaxLayer = FMath::Max(Range.MaxLayer, Node.LayerIndex);
		}

		Range.MinStage = FMath::Min(Range.MinStage, 0);
		Range.MinLayer = FMath::Min(Range.MinLayer, 0);
		Range.MaxStage = FMath::Max(Range.MaxStage, Range.MinStage + 2);
		Range.MaxLayer = FMath::Max(Range.MaxLayer, Range.MinLayer + 1);
		return Range;
	}

	static FLinearColor GetTypeAccent(const EMetaplotNodeType Type)
	{
		switch (Type)
		{
		case EMetaplotNodeType::Start:
			return FLinearColor(0.25f, 0.65f, 0.35f, 1.0f);
		case EMetaplotNodeType::Normal:
			return FLinearColor(0.30f, 0.55f, 0.95f, 1.0f);
		case EMetaplotNodeType::Conditional:
			return FLinearColor(0.95f, 0.75f, 0.25f, 1.0f);
		case EMetaplotNodeType::Parallel:
			return FLinearColor(0.75f, 0.45f, 0.95f, 1.0f);
		case EMetaplotNodeType::Terminal:
			return FLinearColor(0.95f, 0.40f, 0.35f, 1.0f);
		default:
			return FLinearColor(0.55f, 0.55f, 0.58f, 1.0f);
		}
	}

	static TCHAR GetTypeGlyph(const EMetaplotNodeType Type)
	{
		switch (Type)
		{
		case EMetaplotNodeType::Start: return TEXT('S');
		case EMetaplotNodeType::Normal: return TEXT('N');
		case EMetaplotNodeType::Conditional: return TEXT('C');
		case EMetaplotNodeType::Parallel: return TEXT('P');
		case EMetaplotNodeType::Terminal: return TEXT('T');
		default: return TEXT('?');
		}
	}

	static FString GetTypeLabel(const EMetaplotNodeType Type)
	{
		switch (Type)
		{
		case EMetaplotNodeType::Start: return TEXT("Start");
		case EMetaplotNodeType::Normal: return TEXT("Normal");
		case EMetaplotNodeType::Conditional: return TEXT("Conditional");
		case EMetaplotNodeType::Parallel: return TEXT("Parallel");
		case EMetaplotNodeType::Terminal: return TEXT("Terminal");
		default: return TEXT("Node");
		}
	}

	static int32 GetTaskCount(const UMetaplotFlow* Flow, const FGuid& NodeId)
	{
		if (!Flow || !NodeId.IsValid())
		{
			return 0;
		}

		const FMetaplotNodeState* NodeState = Flow->NodeStates.FindByPredicate([NodeId](const FMetaplotNodeState& Entry)
		{
			return Entry.ID == NodeId;
		});
		return NodeState ? NodeState->Tasks.Num() : 0;
	}

	static void AppendUniquePoint(TArray<FVector2D>& Points, const FVector2D& Point)
	{
		if (Points.IsEmpty() || !Points.Last().Equals(Point, 0.1f))
		{
			Points.Add(Point);
		}
	}

	static float SnapToGridStep(const float Value, const float Step, const float GridOrigin)
	{
		if (Step <= KINDA_SMALL_NUMBER)
		{
			return Value;
		}
		return FMath::RoundToFloat((Value - GridOrigin) / Step) * Step + GridOrigin;
	}

	static void BuildRoundedPolyline(const TArray<FVector2D>& RawPoints, TArray<FVector2D>& OutPoints)
	{
		OutPoints.Reset();
		if (RawPoints.Num() < 2)
		{
			return;
		}

	// Keep pure orthogonal polyline corners (no arc smoothing),
	// so the transition is rendered as right-angle segments.
	for (const FVector2D& Point : RawPoints)
	{
		AppendUniquePoint(OutPoints, Point);
	}
	}

	static void BuildRoundedOrthogonalPathViaX(
		const FVector2D& Start,
		const FVector2D& End,
		const float MidX,
		const FVector2D& GridOrigin,
		TArray<FVector2D>& OutPoints)
	{
		const float GridMidX = SnapToGridStep(MidX, StageGridStep, GridOrigin.X);
		const TArray<FVector2D> RawPoints =
		{
			Start,
			FVector2D(GridMidX, Start.Y),
			FVector2D(GridMidX, End.Y),
			End
		};
		BuildRoundedPolyline(RawPoints, OutPoints);
	}

	static void BuildBundledOrthogonalPath(
		const FVector2D& Start,
		const FVector2D& End,
		const float EntryX,
		const float ExitX,
		const float BridgeY,
		const FVector2D& GridOrigin,
		TArray<FVector2D>& OutPoints)
	{
		const float GridEntryX = SnapToGridStep(EntryX, StageGridStep, GridOrigin.X);
		const float GridExitX = SnapToGridStep(ExitX, StageGridStep, GridOrigin.X);
		const float GridBridgeY = SnapToGridStep(BridgeY, LayerGridStep, GridOrigin.Y);
		const TArray<FVector2D> RawPoints =
		{
			Start,
			FVector2D(GridEntryX, Start.Y),
			FVector2D(GridEntryX, GridBridgeY),
			FVector2D(GridExitX, GridBridgeY),
			FVector2D(GridExitX, End.Y),
			End
		};
		BuildRoundedPolyline(RawPoints, OutPoints);
	}

	static void BuildRoundedOrthogonalPath(
		const FVector2D& Start,
		const FVector2D& End,
		const FVector2D& GridOrigin,
		TArray<FVector2D>& OutPoints)
	{
		BuildRoundedOrthogonalPathViaX(Start, End, (Start.X + End.X) * 0.5f, GridOrigin, OutPoints);
	}

	static float DistanceSquaredToSegment(const FVector2D& Point, const FVector2D& SegmentStart, const FVector2D& SegmentEnd)
	{
		const FVector2D Segment = SegmentEnd - SegmentStart;
		const float SegmentLenSq = Segment.SizeSquared();
		if (SegmentLenSq <= KINDA_SMALL_NUMBER)
		{
			return (Point - SegmentStart).SizeSquared();
		}

		const float T = FMath::Clamp(FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentLenSq, 0.0f, 1.0f);
		const FVector2D Closest = SegmentStart + Segment * T;
		return (Point - Closest).SizeSquared();
	}
}

void SMetaStoryFlowGraph::Construct(const FArguments& InArgs)
{
	WeakFlow = InArgs._FlowAsset;
	OnNodeSelected = InArgs._OnNodeSelected;
	OnHorizontalPanChanged = InArgs._OnHorizontalPanChanged;
	OnCreateTransition = InArgs._OnCreateTransition;
	OnMoveNode = InArgs._OnMoveNode;
	OnCreateNodeRequested = InArgs._OnCreateNodeRequested;
	OnDeleteNodeRequested = InArgs._OnDeleteNodeRequested;
	OnDeleteTransitionRequested = InArgs._OnDeleteTransitionRequested;
	SelectedNodeId = FGuid();
}

void SMetaStoryFlowGraph::SetFlowAsset(UMetaplotFlow* InFlow)
{
	WeakFlow = InFlow;
	PanScreen = FVector2D::ZeroVector;
	ClampPanToContent(CachedLocalSize);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SMetaStoryFlowGraph::SetSelectedNodeId(const FGuid& InNodeId)
{
	SelectedNodeId = InNodeId;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SMetaStoryFlowGraph::SetHorizontalPanScreen(float InPanScreenX)
{
	PanScreen.X = InPanScreenX;
	ClampPanToContent(CachedLocalSize);
	BroadcastHorizontalPanChanged();
	Invalidate(EInvalidateWidgetReason::Paint);
}

bool SMetaStoryFlowGraph::GetHorizontalScrollbarState(float& OutOffsetFraction, float& OutThumbSizeFraction) const
{
	float MinX, MinY, MaxX, MaxY;
	GetContentBounds(MinX, MinY, MaxX, MaxY);
	const float ContentWidth = FMath::Max(1.0f, MaxX - MinX);
	const float ViewWidth = FMath::Max(1.0f, CachedLocalSize.X);

	OutThumbSizeFraction = FMath::Clamp(ViewWidth / ContentWidth, 0.0f, 1.0f);
	if (OutThumbSizeFraction >= 1.0f)
	{
		OutOffsetFraction = 0.0f;
		return false;
	}

	const float MaxOffset = ContentWidth - ViewWidth;
	const float CurrentOffset = FMath::Clamp(-PanScreen.X - MinX, 0.0f, MaxOffset);
	OutOffsetFraction = MaxOffset > KINDA_SMALL_NUMBER ? (CurrentOffset / MaxOffset) : 0.0f;
	return true;
}

FVector2D SMetaStoryFlowGraph::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(320.0f, 240.0f);
}

FVector2D SMetaStoryFlowGraph::GetNodeSize(const FMetaplotNode& Node)
{
	const FString Desc = Node.Description.ToString();
	const int32 DescLen = Desc.Len();
	const int32 Lines = DescLen > 0 ? FMath::Clamp((DescLen + 39) / 40, 1, 3) : 0;
	const float DescHeight = Lines > 0 ? Lines * 14.0f + 6.0f : 0.0f;
	const float Height = FMath::Clamp(52.0f + DescHeight, 72.0f, 200.0f);
	return FVector2D(NodeWidth, Height);
}

FVector2D SMetaStoryFlowGraph::GetNodeTopLeftGraph(const FMetaplotNode& Node) const
{
	const FVector2D Size = GetNodeSize(Node);
	const float X = Node.StageIndex * StageCellWidth + (StageCellWidth - NodeWidth) * 0.5f;
	const float Y = Node.LayerIndex * LayerCellHeight + (LayerCellHeight - Size.Y) * 0.5f;
	return FVector2D(X, Y);
}

FVector2D SMetaStoryFlowGraph::GetPinGraphPosition(const FMetaplotNode& Node, const EPinSide Side) const
{
	const FVector2D TL = GetNodeTopLeftGraph(Node);
	const FVector2D Sz = GetNodeSize(Node);
	if (Side == EPinSide::Left)
	{
		return TL + FVector2D(0.0f, Sz.Y * 0.5f);
	}
	if (Side == EPinSide::Right)
	{
		return TL + FVector2D(Sz.X, Sz.Y * 0.5f);
	}
	return TL + Sz * 0.5f;
}

FVector2D SMetaStoryFlowGraph::GetPinStubGraphPosition(const FMetaplotNode& Node, const EPinSide Side) const
{
	const FVector2D Pin = GetPinGraphPosition(Node, Side);
	if (Side == EPinSide::Left)
	{
		return Pin + FVector2D(-PinStubLength, 0.0f);
	}
	if (Side == EPinSide::Right)
	{
		return Pin + FVector2D(PinStubLength, 0.0f);
	}
	return Pin;
}

FVector2D SMetaStoryFlowGraph::GraphToLocal(const FVector2D& GraphPos, const FVector2D& LocalSize) const
{
	(void)LocalSize;
	return GraphPos + PanScreen;
}

FVector2D SMetaStoryFlowGraph::LocalToGraph(const FVector2D& LocalPos, const FVector2D& LocalSize) const
{
	(void)LocalSize;
	return LocalPos - PanScreen;
}

void SMetaStoryFlowGraph::ClampPanToContent(const FVector2D& LocalSize)
{
	const float PrevPanX = PanScreen.X;

	float BoundMinX, BoundMinY, BoundMaxX, BoundMaxY;
	GetContentBounds(BoundMinX, BoundMinY, BoundMaxX, BoundMaxY);

	const float MinPanX = (LocalSize.X - MetaplotGraphWidgetPrivate::ViewInsetRight) - BoundMaxX;
	const float MaxPanX = MetaplotGraphWidgetPrivate::ViewInsetLeft - BoundMinX;
	if (MinPanX > MaxPanX)
	{
		PanScreen.X = MaxPanX;
	}
	else
	{
		PanScreen.X = FMath::Clamp(PanScreen.X, MinPanX, MaxPanX);
	}

	const float MinPanY = (LocalSize.Y - MetaplotGraphWidgetPrivate::ViewInsetBottom) - BoundMaxY;
	const float MaxPanY = MetaplotGraphWidgetPrivate::ViewInsetTop - BoundMinY;
	if (MinPanY > MaxPanY)
	{
		PanScreen.Y = MaxPanY;
	}
	else
	{
		PanScreen.Y = FMath::Clamp(PanScreen.Y, MinPanY, MaxPanY);
	}

	if (!FMath::IsNearlyEqual(PrevPanX, PanScreen.X))
	{
		BroadcastHorizontalPanChanged();
	}
}

void SMetaStoryFlowGraph::GetContentBounds(float& OutMinX, float& OutMinY, float& OutMaxX, float& OutMaxY) const
{
	UMetaplotFlow* Flow = WeakFlow.Get();
	if (!Flow || Flow->Nodes.IsEmpty())
	{
		const MetaplotGraphWidgetPrivate::FGridRange GridRange = MetaplotGraphWidgetPrivate::BuildGridRange(Flow);
		OutMinX = GridRange.MinStage * StageCellWidth;
		OutMaxX = (GridRange.MaxStage + 1) * StageCellWidth;
		OutMinY = MetaplotGraphWidgetPrivate::GridHeaderTop;
		OutMaxY = (GridRange.MaxLayer + 1) * LayerCellHeight;
		return;
	}

	float MinX = TNumericLimits<float>::Max();
	float MinY = TNumericLimits<float>::Max();
	float MaxX = TNumericLimits<float>::Lowest();
	float MaxY = TNumericLimits<float>::Lowest();
	int32 MaxLayer = 0;

	for (const FMetaplotNode& Node : Flow->Nodes)
	{
		const FVector2D TopLeft = GetNodeTopLeftGraph(Node);
		const FVector2D Size = GetNodeSize(Node);
		MinX = FMath::Min(MinX, TopLeft.X - PinStubLength);
		MinY = FMath::Min(MinY, TopLeft.Y);
		MaxX = FMath::Max(MaxX, TopLeft.X + Size.X + PinStubLength);
		MaxY = FMath::Max(MaxY, TopLeft.Y + Size.Y);
		MaxLayer = FMath::Max(MaxLayer, Node.LayerIndex);
	}

	OutMinX = MinX - MetaplotGraphWidgetPrivate::FitPaddingX;
	OutMaxX = MaxX + MetaplotGraphWidgetPrivate::FitPaddingX;
	OutMinY = FMath::Min(MinY - MetaplotGraphWidgetPrivate::FitPaddingY, MetaplotGraphWidgetPrivate::GridHeaderTop);
	const float GridBottomY = (MaxLayer + 1) * LayerCellHeight;
	OutMaxY = FMath::Max(MaxY + MetaplotGraphWidgetPrivate::FitPaddingY, GridBottomY);
}

bool SMetaStoryFlowGraph::HitTestNode(const FGeometry& MyGeometry, const FVector2D& LocalPos, FGuid& OutNodeId) const
{
	OutNodeId = FGuid();
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const FVector2D G = LocalToGraph(LocalPos, LocalSize);

	UMetaplotFlow* Flow = WeakFlow.Get();
	if (!Flow)
	{
		return false;
	}

	for (int32 Index = Flow->Nodes.Num() - 1; Index >= 0; --Index)
	{
		const FMetaplotNode& Node = Flow->Nodes[Index];
		const FVector2D TL = GetNodeTopLeftGraph(Node);
		const FVector2D Sz = GetNodeSize(Node);
		if (G.X >= TL.X && G.X <= TL.X + Sz.X && G.Y >= TL.Y && G.Y <= TL.Y + Sz.Y)
		{
			OutNodeId = Node.NodeId;
			return true;
		}
	}

	return false;
}

bool SMetaStoryFlowGraph::HitTestPin(const FGeometry& MyGeometry, const FVector2D& LocalPos, FGuid& OutNodeId, EPinSide& OutSide) const
{
	OutNodeId = FGuid();
	OutSide = EPinSide::None;

	UMetaplotFlow* Flow = WeakFlow.Get();
	if (!Flow)
	{
		return false;
	}

	const float HitRadius = PinRadius + 4.0f;
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	for (int32 Index = Flow->Nodes.Num() - 1; Index >= 0; --Index)
	{
		const FMetaplotNode& Node = Flow->Nodes[Index];
		const FVector2D LeftLocal = GraphToLocal(GetPinGraphPosition(Node, EPinSide::Left), LocalSize);
		if ((LocalPos - LeftLocal).SizeSquared() <= FMath::Square(HitRadius))
		{
			OutNodeId = Node.NodeId;
			OutSide = EPinSide::Left;
			return true;
		}

		const FVector2D RightLocal = GraphToLocal(GetPinGraphPosition(Node, EPinSide::Right), LocalSize);
		if ((LocalPos - RightLocal).SizeSquared() <= FMath::Square(HitRadius))
		{
			OutNodeId = Node.NodeId;
			OutSide = EPinSide::Right;
			return true;
		}
	}

	return false;
}

bool SMetaStoryFlowGraph::HitTestTransition(
	const FGeometry& MyGeometry,
	const FVector2D& LocalPos,
	FGuid& OutSourceNodeId,
	FGuid& OutTargetNodeId) const
{
	OutSourceNodeId.Invalidate();
	OutTargetNodeId.Invalidate();

	UMetaplotFlow* Flow = WeakFlow.Get();
	if (!Flow || Flow->Transitions.IsEmpty())
	{
		return false;
	}

	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const float HitThresholdSq = FMath::Square(7.0f);
	float BestDistSq = TNumericLimits<float>::Max();

	for (const FMetaplotTransition& Transition : Flow->Transitions)
	{
		const FMetaplotNode* SourceNode = Flow->Nodes.FindByPredicate([&Transition](const FMetaplotNode& Node)
		{
			return Node.NodeId == Transition.SourceNodeId;
		});
		const FMetaplotNode* TargetNode = Flow->Nodes.FindByPredicate([&Transition](const FMetaplotNode& Node)
		{
			return Node.NodeId == Transition.TargetNodeId;
		});
		if (!SourceNode || !TargetNode)
		{
			continue;
		}

		const FVector2D StartLocal = GraphToLocal(GetPinStubGraphPosition(*SourceNode, EPinSide::Right), LocalSize);
		const FVector2D EndLocal = GraphToLocal(GetPinStubGraphPosition(*TargetNode, EPinSide::Left), LocalSize);
		TArray<FVector2D> PathPoints;
		MetaplotGraphWidgetPrivate::BuildRoundedOrthogonalPath(StartLocal, EndLocal, PanScreen, PathPoints);
		if (PathPoints.Num() < 2)
		{
			continue;
		}

		for (int32 Index = 1; Index < PathPoints.Num(); ++Index)
		{
			const float DistSq = MetaplotGraphWidgetPrivate::DistanceSquaredToSegment(LocalPos, PathPoints[Index - 1], PathPoints[Index]);
			if (DistSq <= HitThresholdSq && DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutSourceNodeId = Transition.SourceNodeId;
				OutTargetNodeId = Transition.TargetNodeId;
			}
		}
	}

	return OutSourceNodeId.IsValid() && OutTargetNodeId.IsValid();
}

void SMetaStoryFlowGraph::OpenContextMenuAtScreen(
	const FPointerEvent& MouseEvent,
	const FGuid& NodeId,
	const FGuid& SourceNodeId,
	const FGuid& TargetNodeId)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	if (NodeId.IsValid())
	{
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("删除节点")),
			FText::FromString(TEXT("删除当前节点及其相关连线")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, NodeId]()
			{
				if (OnDeleteNodeRequested.IsBound())
				{
					OnDeleteNodeRequested.Execute(NodeId);
				}
			})));
	}
	else if (SourceNodeId.IsValid() && TargetNodeId.IsValid())
	{
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("删除连线")),
			FText::FromString(TEXT("删除当前选中的连线")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, SourceNodeId, TargetNodeId]()
			{
				if (OnDeleteTransitionRequested.IsBound())
				{
					OnDeleteTransitionRequested.Execute(SourceNodeId, TargetNodeId);
				}
			})));
	}

	TSharedRef<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		MenuWidget,
		MouseEvent.GetScreenSpacePosition(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

void SMetaStoryFlowGraph::OpenCreateNodeSearchMenuAtScreen(const FPointerEvent& MouseEvent, int32 StageIndex, int32 LayerIndex)
{
	TSharedRef<SWidget> MenuWidget =
		SNew(MetaplotGraphWidgetPrivate::SNodeSearchMenuWidget)
		.OnCloseMenu(FSimpleDelegate::CreateLambda([]()
		{
			FSlateApplication::Get().DismissAllMenus();
		}))
		.OnCreateNodeRequested(OnCreateNodeRequested)
		.StageIndex(StageIndex)
		.LayerIndex(LayerIndex);

	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		MenuWidget,
		MouseEvent.GetScreenSpacePosition(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

bool SMetaStoryFlowGraph::IsTransitionRuleValid(const FGuid& SourceNodeId, const FGuid& TargetNodeId) const
{
	return GetTransitionInvalidReason(SourceNodeId, TargetNodeId) == EConnectionInvalidReason::None;
}

SMetaStoryFlowGraph::EConnectionInvalidReason SMetaStoryFlowGraph::GetTransitionInvalidReason(const FGuid& SourceNodeId, const FGuid& TargetNodeId) const
{
	if (!SourceNodeId.IsValid() || !TargetNodeId.IsValid())
	{
		return EConnectionInvalidReason::InvalidPinPair;
	}
	if (SourceNodeId == TargetNodeId)
	{
		return EConnectionInvalidReason::SameNode;
	}

	UMetaplotFlow* Flow = WeakFlow.Get();
	if (!Flow)
	{
		return EConnectionInvalidReason::InvalidPinPair;
	}

	const FMetaplotNode* SourceNode = Flow->Nodes.FindByPredicate([SourceNodeId](const FMetaplotNode& Node)
	{
		return Node.NodeId == SourceNodeId;
	});
	const FMetaplotNode* TargetNode = Flow->Nodes.FindByPredicate([TargetNodeId](const FMetaplotNode& Node)
	{
		return Node.NodeId == TargetNodeId;
	});
	if (!SourceNode || !TargetNode)
	{
		return EConnectionInvalidReason::InvalidPinPair;
	}

	if (TargetNode->StageIndex <= SourceNode->StageIndex)
	{
		return EConnectionInvalidReason::BackwardStage;
	}

	if (SourceNode->LayerIndex == TargetNode->LayerIndex &&
		TargetNode->StageIndex != SourceNode->StageIndex + 1)
	{
		return EConnectionInvalidReason::SameRowSkipStage;
	}

	return EConnectionInvalidReason::None;
}

SMetaStoryFlowGraph::EConnectionInvalidReason SMetaStoryFlowGraph::GetConnectionCandidateInvalidReason(const FGuid& PinDragNodeId, EPinSide DragSide, const FGuid& HoverNodeId, EPinSide HoverSide) const
{
	if (!PinDragNodeId.IsValid() || !HoverNodeId.IsValid() || DragSide == EPinSide::None || HoverSide == EPinSide::None)
	{
		return EConnectionInvalidReason::InvalidPinPair;
	}
	if (PinDragNodeId == HoverNodeId)
	{
		return EConnectionInvalidReason::SameNode;
	}
	if (HoverSide == DragSide)
	{
		return EConnectionInvalidReason::InvalidPinPair;
	}

	FGuid SourceNodeId;
	FGuid TargetNodeId;
	if (DragSide == EPinSide::Right && HoverSide == EPinSide::Left)
	{
		SourceNodeId = PinDragNodeId;
		TargetNodeId = HoverNodeId;
	}
	else if (DragSide == EPinSide::Left && HoverSide == EPinSide::Right)
	{
		SourceNodeId = HoverNodeId;
		TargetNodeId = PinDragNodeId;
	}
	else
	{
		return EConnectionInvalidReason::InvalidPinPair;
	}

	UMetaplotFlow* Flow = WeakFlow.Get();
	if (Flow && Flow->Transitions.ContainsByPredicate([SourceNodeId, TargetNodeId](const FMetaplotTransition& Transition)
	{
		return Transition.SourceNodeId == SourceNodeId && Transition.TargetNodeId == TargetNodeId;
	}))
	{
		return EConnectionInvalidReason::Duplicate;
	}

	const EConnectionInvalidReason RuleReason = GetTransitionInvalidReason(SourceNodeId, TargetNodeId);
	if (RuleReason != EConnectionInvalidReason::None)
	{
		return RuleReason;
	}

	if (WouldCreateCycle(SourceNodeId, TargetNodeId))
	{
		return EConnectionInvalidReason::Cycle;
	}

	return EConnectionInvalidReason::None;
}

bool SMetaStoryFlowGraph::WouldCreateCycle(const FGuid& SourceNodeId, const FGuid& TargetNodeId) const
{
	if (!SourceNodeId.IsValid() || !TargetNodeId.IsValid() || SourceNodeId == TargetNodeId)
	{
		return true;
	}

	UMetaplotFlow* Flow = WeakFlow.Get();
	if (!Flow)
	{
		return false;
	}

	TSet<FGuid> Visited;
	TArray<FGuid> Stack;
	Stack.Add(TargetNodeId);

	while (!Stack.IsEmpty())
	{
		const FGuid Current = Stack.Pop(EAllowShrinking::No);
		if (!Current.IsValid() || Visited.Contains(Current))
		{
			continue;
		}

		Visited.Add(Current);
		if (Current == SourceNodeId)
		{
			return true;
		}

		for (const FMetaplotTransition& Transition : Flow->Transitions)
		{
			if (Transition.SourceNodeId == Current && Transition.TargetNodeId.IsValid())
			{
				Stack.Add(Transition.TargetNodeId);
			}
		}
	}

	return false;
}

bool SMetaStoryFlowGraph::IsConnectionCandidateValid(const FGuid& PinDragNodeId, EPinSide DragSide, const FGuid& HoverNodeId, EPinSide HoverSide) const
{
	return GetConnectionCandidateInvalidReason(PinDragNodeId, DragSide, HoverNodeId, HoverSide) == EConnectionInvalidReason::None;
}

void SMetaStoryFlowGraph::UpdateDragNodePlacementPreview(const FVector2D& LocalPos, const FVector2D& LocalSize)
{
	UMetaplotFlow* Flow = WeakFlow.Get();
	if (!Flow || !DragNodeId.IsValid())
	{
		return;
	}

	const FMetaplotNode* Node = Flow->Nodes.FindByPredicate([this](const FMetaplotNode& N)
	{
		return N.NodeId == DragNodeId;
	});
	if (!Node)
	{
		return;
	}

	const FVector2D TLGraph = LocalToGraph(LocalPos, LocalSize) - DragGrabOffsetGraph;
	const FVector2D Sz = GetNodeSize(*Node);
	const FVector2D Center = TLGraph + Sz * 0.5f;
	DragPreviewStage = FMath::Max(0, FMath::FloorToInt(Center.X / StageCellWidth));
	DragPreviewLayer = FMath::Max(0, FMath::FloorToInt(Center.Y / LayerCellHeight));
	bDragNodePlacementValid = MetaStoryFlowGraphPlacement::IsValidCellForNodeMove(Flow, DragNodeId, DragPreviewStage, DragPreviewLayer);
}

FReply SMetaStoryFlowGraph::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	CachedLocalSize = MyGeometry.GetLocalSize();

	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bPanning = true;
		PanGrabLocal = LocalPos;
		PanScreenAtGrab = PanScreen;
		return FReply::Handled().CaptureMouse(AsShared());
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FGuid PinNodeId;
		EPinSide PinSide = EPinSide::None;
		if (HitTestPin(MyGeometry, LocalPos, PinNodeId, PinSide))
		{
			bDraggingConnection = true;
			DragPinNodeId = PinNodeId;
			DragPinSide = PinSide;
			DragCurrentLocal = LocalPos;
			HoveredPinInvalidReason = EConnectionInvalidReason::None;

			SelectedNodeId = PinNodeId;
			if (OnNodeSelected.IsBound())
			{
				OnNodeSelected.Execute(PinNodeId);
			}

			Invalidate(EInvalidateWidgetReason::Paint);
			return FReply::Handled().CaptureMouse(AsShared());
		}

		FGuid HitId;
		if (HitTestNode(MyGeometry, LocalPos, HitId))
		{
			UMetaplotFlow* Flow = WeakFlow.Get();
			const FMetaplotNode* HitNode = Flow ? Flow->Nodes.FindByPredicate([HitId](const FMetaplotNode& N)
			{
				return N.NodeId == HitId;
			}) : nullptr;

			if (HitNode)
			{
				bDraggingNode = true;
				DragNodeId = HitId;
				DragGrabOffsetGraph = LocalToGraph(LocalPos, CachedLocalSize) - GetNodeTopLeftGraph(*HitNode);
				DragOrigStage = HitNode->StageIndex;
				DragOrigLayer = HitNode->LayerIndex;
				UpdateDragNodePlacementPreview(LocalPos, CachedLocalSize);
				DragCurrentLocal = LocalPos;
			}

			SelectedNodeId = HitId;
			if (OnNodeSelected.IsBound())
			{
				OnNodeSelected.Execute(HitId);
			}
			Invalidate(EInvalidateWidgetReason::Paint);
			if (HitNode)
			{
				return FReply::Handled().CaptureMouse(AsShared());
			}
			return FReply::Handled();
		}

		SelectedNodeId = FGuid();
		if (OnNodeSelected.IsBound())
		{
			OnNodeSelected.Execute(FGuid());
		}
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FGuid HitNodeId;
		if (HitTestNode(MyGeometry, LocalPos, HitNodeId))
		{
			SelectedNodeId = HitNodeId;
			if (OnNodeSelected.IsBound())
			{
				OnNodeSelected.Execute(HitNodeId);
			}
			OpenContextMenuAtScreen(MouseEvent, HitNodeId, FGuid(), FGuid());
			Invalidate(EInvalidateWidgetReason::Paint);
			return FReply::Handled();
		}

		FGuid HitSourceNodeId;
		FGuid HitTargetNodeId;
		if (HitTestTransition(MyGeometry, LocalPos, HitSourceNodeId, HitTargetNodeId))
		{
			OpenContextMenuAtScreen(MouseEvent, FGuid(), HitSourceNodeId, HitTargetNodeId);
			return FReply::Handled();
		}

		const FVector2D GraphPos = LocalToGraph(LocalPos, CachedLocalSize);
		const int32 TargetStage = FMath::Max(0, FMath::FloorToInt(GraphPos.X / StageCellWidth));
		const int32 TargetLayer = FMath::Max(0, FMath::FloorToInt(GraphPos.Y / LayerCellHeight));
		OpenCreateNodeSearchMenuAtScreen(MouseEvent, TargetStage, TargetLayer);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SMetaStoryFlowGraph::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	CachedLocalSize = MyGeometry.GetLocalSize();

	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && bPanning)
	{
		bPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDraggingNode)
	{
		UMetaplotFlow* Flow = WeakFlow.Get();
		const bool bMoved = (DragPreviewStage != DragOrigStage || DragPreviewLayer != DragOrigLayer);
		if (Flow && DragNodeId.IsValid() && bDragNodePlacementValid && bMoved && OnMoveNode.IsBound())
		{
			OnMoveNode.Execute(DragNodeId, DragPreviewStage, DragPreviewLayer);
		}

		bDraggingNode = false;
		DragNodeId.Invalidate();
		DragGrabOffsetGraph = FVector2D::ZeroVector;
		bDragNodePlacementValid = false;
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled().ReleaseMouseCapture();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDraggingConnection)
	{
		FGuid HitPinNodeId;
		EPinSide HitPinSide = EPinSide::None;
		if (HitTestPin(MyGeometry, LocalPos, HitPinNodeId, HitPinSide) &&
			IsConnectionCandidateValid(DragPinNodeId, DragPinSide, HitPinNodeId, HitPinSide))
		{
			FGuid SourceNodeId;
			FGuid TargetNodeId;
			if (DragPinSide == EPinSide::Right && HitPinSide == EPinSide::Left)
			{
				SourceNodeId = DragPinNodeId;
				TargetNodeId = HitPinNodeId;
			}
			else if (DragPinSide == EPinSide::Left && HitPinSide == EPinSide::Right)
			{
				SourceNodeId = HitPinNodeId;
				TargetNodeId = DragPinNodeId;
			}

			if (SourceNodeId.IsValid() && TargetNodeId.IsValid() && OnCreateTransition.IsBound())
			{
				OnCreateTransition.Execute(SourceNodeId, TargetNodeId);
			}
		}

		bDraggingConnection = false;
		DragPinNodeId.Invalidate();
		DragPinSide = EPinSide::None;
		HoveredPinNodeId.Invalidate();
		HoveredPinSide = EPinSide::None;
		bHoveredPinAcceptsConnection = false;
		HoveredPinInvalidReason = EConnectionInvalidReason::None;
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply SMetaStoryFlowGraph::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	CachedLocalSize = LocalSize;

	if (bPanning && MouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton))
	{
		const FVector2D Delta = LocalPos - PanGrabLocal;
		PanScreen = PanScreenAtGrab + Delta;
		ClampPanToContent(LocalSize);
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}

	if (bDraggingNode)
	{
		DragCurrentLocal = LocalPos;
		UpdateDragNodePlacementPreview(LocalPos, LocalSize);
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}

	if (bDraggingConnection)
	{
		DragCurrentLocal = LocalPos;
		FGuid HitPinNodeId;
		EPinSide HitPinSide = EPinSide::None;
		if (HitTestPin(MyGeometry, LocalPos, HitPinNodeId, HitPinSide) &&
			HitPinNodeId.IsValid())
		{
			HoveredPinNodeId = HitPinNodeId;
			HoveredPinSide = HitPinSide;
			HoveredPinInvalidReason = GetConnectionCandidateInvalidReason(DragPinNodeId, DragPinSide, HitPinNodeId, HitPinSide);
			bHoveredPinAcceptsConnection = HoveredPinInvalidReason == EConnectionInvalidReason::None;
		}
		else
		{
			HoveredPinNodeId.Invalidate();
			HoveredPinSide = EPinSide::None;
			bHoveredPinAcceptsConnection = false;
			HoveredPinInvalidReason = EConnectionInvalidReason::None;
		}

		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}

	FGuid NewHover;
	if (HitTestNode(MyGeometry, LocalPos, NewHover))
	{
		if (NewHover != HoveredNodeId)
		{
			HoveredNodeId = NewHover;
			Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
	else if (HoveredNodeId.IsValid())
	{
		HoveredNodeId = FGuid();
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	return FReply::Unhandled();
}

FCursorReply SMetaStoryFlowGraph::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	(void)MyGeometry;
	(void)CursorEvent;

	if (bPanning)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}

	if (bDraggingConnection)
	{
		return FCursorReply::Cursor(EMouseCursor::Crosshairs);
	}

	if (bDraggingNode)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}
	return FCursorReply::Unhandled();
}

void SMetaStoryFlowGraph::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	(void)InCurrentTime;
	(void)InDeltaTime;

	const FVector2D NewLocalSize = AllottedGeometry.GetLocalSize();
	if (!CachedLocalSize.Equals(NewLocalSize, 0.1f))
	{
		CachedLocalSize = NewLocalSize;
		ClampPanToContent(CachedLocalSize);
	}
}

int32 SMetaStoryFlowGraph::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const FSlateBrush* WhiteBox = FAppStyle::GetBrush("WhiteBrush");
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	CachedLocalSize = LocalSize;

	auto MakeGeo = [&AllottedGeometry](const FVector2D& Position, const FVector2D& Size) -> FPaintGeometry
	{
		return AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position));
	};

	const FPaintGeometry RootGeo = AllottedGeometry.ToPaintGeometry(LocalSize, FSlateLayoutTransform());

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		RootGeo,
		WhiteBox,
		ESlateDrawEffect::None,
		FLinearColor(0.07f, 0.07f, 0.075f, 1.0f));

	++LayerId;

	UMetaplotFlow* Flow = WeakFlow.Get();

	float BoundMinX, BoundMinY, BoundMaxX, BoundMaxY;
	GetContentBounds(BoundMinX, BoundMinY, BoundMaxX, BoundMaxY);

	// Grid (major lines at stage/layer cells)
	{
		const MetaplotGraphWidgetPrivate::FGridRange GridRange = MetaplotGraphWidgetPrivate::BuildGridRange(Flow);
		const int32 MinStage = GridRange.MinStage;
		const int32 MaxStage = GridRange.MaxStage;
		const int32 MinLayer = GridRange.MinLayer;
		const int32 MaxLayer = GridRange.MaxLayer;
		const FLinearColor GridColor(1.0f, 1.0f, 1.0f, 0.06f);
		const FLinearColor StageBandA(0.17f, 0.23f, 0.34f, 0.05f);
		const FLinearColor StageBandB(0.11f, 0.15f, 0.22f, 0.035f);
		const float HeaderTop = MetaplotGraphWidgetPrivate::GridHeaderTop;
		const float HeaderHeight = MetaplotGraphWidgetPrivate::GridHeaderHeight;

		for (int32 S = MinStage; S <= MaxStage; ++S)
		{
			const float Left = S * StageCellWidth;
			const FVector2D LocalTL = GraphToLocal(FVector2D(Left, HeaderTop), LocalSize);
			const FVector2D LocalSizeCell(StageCellWidth, (BoundMaxY - HeaderTop));
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				MakeGeo(LocalTL, LocalSizeCell),
				WhiteBox,
				ESlateDrawEffect::None,
				(S % 2 == 0) ? StageBandA : StageBandB);

			const FVector2D LocalHeaderTL = GraphToLocal(FVector2D(Left, HeaderTop), LocalSize);
			const FVector2D LocalHeaderSize(StageCellWidth, HeaderHeight);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				MakeGeo(LocalHeaderTL, LocalHeaderSize),
				WhiteBox,
				ESlateDrawEffect::None,
				FLinearColor(0.08f, 0.11f, 0.17f, 0.78f));

			const FString StageLabel = FString::Printf(TEXT("S%d"), S);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId + 2,
				MakeGeo(LocalHeaderTL + FVector2D(6.0f, 2.0f), FVector2D(LocalHeaderSize.X - 12.0f, LocalHeaderSize.Y - 4.0f)),
				StageLabel,
				SmallFont,
				ESlateDrawEffect::None,
				FLinearColor(0.82f, 0.90f, 1.0f, 0.95f));
		}

		for (int32 S = MinStage; S <= MaxStage + 1; ++S)
		{
			const float GX = S * StageCellWidth;
			const FVector2D A = GraphToLocal(FVector2D(GX, HeaderTop), LocalSize);
			const FVector2D B = GraphToLocal(FVector2D(GX, BoundMaxY), LocalSize);
			TArray<FVector2D> Line;
			Line.Add(A);
			Line.Add(B);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				RootGeo,
				Line,
				ESlateDrawEffect::None,
				GridColor,
				true,
				1.0f);
		}

		for (int32 L = MinLayer; L <= MaxLayer + 1; ++L)
		{
			const float GY = L * LayerCellHeight;
			const FVector2D A = GraphToLocal(FVector2D(BoundMinX, GY), LocalSize);
			const FVector2D B = GraphToLocal(FVector2D(BoundMaxX, GY), LocalSize);
			TArray<FVector2D> Line;
			Line.Add(A);
			Line.Add(B);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				RootGeo,
				Line,
				ESlateDrawEffect::None,
				GridColor,
				true,
				1.0f);

			if (L <= MaxLayer)
			{
				const FVector2D LabelPos = GraphToLocal(FVector2D(BoundMinX + 6.0f, GY + 6.0f), LocalSize);
				const FString LayerLabel = FString::Printf(TEXT("L%d"), L);
				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId + 1,
					MakeGeo(LabelPos, FVector2D(28.0f, 16.0f)),
					LayerLabel,
					SmallFont,
					ESlateDrawEffect::None,
					FLinearColor(0.70f, 0.76f, 0.84f, 0.9f));
			}
		}

		const FVector2D TimelineStart = GraphToLocal(FVector2D(MinStage * StageCellWidth, MetaplotGraphWidgetPrivate::TimelineY), LocalSize);
		const FVector2D TimelineEnd = GraphToLocal(FVector2D((MaxStage + 1) * StageCellWidth, MetaplotGraphWidgetPrivate::TimelineY), LocalSize);
		TArray<FVector2D> TimelineLine;
		TimelineLine.Add(TimelineStart);
		TimelineLine.Add(TimelineEnd);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 2,
			RootGeo,
			TimelineLine,
			ESlateDrawEffect::None,
			FLinearColor(0.40f, 0.72f, 0.98f, 0.95f),
			true,
			2.0f);

		const FVector2D Dir = (TimelineEnd - TimelineStart).GetSafeNormal();
		const FVector2D Ortho(-Dir.Y, Dir.X);
		TArray<FVector2D> Arrow;
		Arrow.Add(TimelineEnd);
		Arrow.Add(TimelineEnd - Dir * 12.0f + Ortho * 6.0f);
		Arrow.Add(TimelineEnd - Dir * 12.0f - Ortho * 6.0f);
		Arrow.Add(TimelineEnd);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 2,
			RootGeo,
			Arrow,
			ESlateDrawEffect::None,
			FLinearColor(0.40f, 0.72f, 0.98f, 0.95f),
			true,
			2.0f);
	}

	++LayerId;

	auto ResolveNode = [Flow](const FGuid& Id) -> const FMetaplotNode*
	{
		if (!Flow)
		{
			return nullptr;
		}
		return Flow->Nodes.FindByPredicate([Id](const FMetaplotNode& N)
		{
			return N.NodeId == Id;
		});
	};

	const auto LayoutForDraw = [this](const FMetaplotNode& N) -> FMetaplotNode
	{
		if (bDraggingNode && N.NodeId == DragNodeId)
		{
			FMetaplotNode M = N;
			M.StageIndex = DragPreviewStage;
			M.LayerIndex = DragPreviewLayer;
			return M;
		}
		return N;
	};

	if (bDraggingNode && Flow && DragNodeId.IsValid())
	{
		const MetaplotGraphWidgetPrivate::FGridRange GR0 = MetaplotGraphWidgetPrivate::BuildGridRange(Flow);
		const int32 HS0 = FMath::Min(GR0.MinStage, DragPreviewStage - 2);
		const int32 HS1 = FMath::Max(GR0.MaxStage, DragPreviewStage + 2);
		const int32 HL0 = FMath::Min(GR0.MinLayer, DragPreviewLayer - 2);
		const int32 HL1 = FMath::Max(GR0.MaxLayer, DragPreviewLayer + 2);

		for (int32 S = HS0; S <= HS1; ++S)
		{
			for (int32 L = HL0; L <= HL1; ++L)
			{
				const bool bOk = MetaStoryFlowGraphPlacement::IsValidCellForNodeMove(Flow, DragNodeId, S, L);
				const FVector2D CellTL = GraphToLocal(FVector2D(S * StageCellWidth, L * LayerCellHeight), LocalSize);
				const FVector2D CellSz(StageCellWidth, LayerCellHeight);
				const FLinearColor CellColor = bOk
					? FLinearColor(0.15f, 0.85f, 0.35f, 0.14f)
					: FLinearColor(0.95f, 0.28f, 0.22f, 0.11f);
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					MakeGeo(CellTL, CellSz),
					WhiteBox,
					ESlateDrawEffect::None,
					CellColor);
			}
		}
	}

	// Transitions (under nodes)
	if (Flow)
	{
		struct FTransitionDrawLaneData
		{
			const FMetaplotTransition* Transition = nullptr;
			FMetaplotNode Src;
			FMetaplotNode Dst;
			int32 SrcStage = 0;
			int32 DstStage = 0;
			int32 SrcLayer = 0;
			int32 DstLayer = 0;
		};

		TArray<FTransitionDrawLaneData> TransitionLanes;
		TransitionLanes.Reserve(Flow->Transitions.Num());

		for (const FMetaplotTransition& Tr : Flow->Transitions)
		{
			const FMetaplotNode* Src = ResolveNode(Tr.SourceNodeId);
			const FMetaplotNode* Dst = ResolveNode(Tr.TargetNodeId);
			if (!Src || !Dst)
			{
				continue;
			}

			FTransitionDrawLaneData LaneData;
			LaneData.Transition = &Tr;
			LaneData.Src = LayoutForDraw(*Src);
			LaneData.Dst = LayoutForDraw(*Dst);
			LaneData.SrcStage = LaneData.Src.StageIndex;
			LaneData.DstStage = LaneData.Dst.StageIndex;
			LaneData.SrcLayer = LaneData.Src.LayerIndex;
			LaneData.DstLayer = LaneData.Dst.LayerIndex;
			TransitionLanes.Add(LaneData);
		}

		TransitionLanes.Sort([](const FTransitionDrawLaneData& A, const FTransitionDrawLaneData& B)
		{
			if (A.SrcStage != B.SrcStage)
			{
				return A.SrcStage < B.SrcStage;
			}
			if (A.DstStage != B.DstStage)
			{
				return A.DstStage < B.DstStage;
			}
			if (A.SrcLayer != B.SrcLayer)
			{
				return A.SrcLayer < B.SrcLayer;
			}
			if (A.DstLayer != B.DstLayer)
			{
				return A.DstLayer < B.DstLayer;
			}
			if (A.Transition->SourceNodeId.A != B.Transition->SourceNodeId.A)
			{
				return A.Transition->SourceNodeId.A < B.Transition->SourceNodeId.A;
			}
			if (A.Transition->SourceNodeId.B != B.Transition->SourceNodeId.B)
			{
				return A.Transition->SourceNodeId.B < B.Transition->SourceNodeId.B;
			}
			if (A.Transition->SourceNodeId.C != B.Transition->SourceNodeId.C)
			{
				return A.Transition->SourceNodeId.C < B.Transition->SourceNodeId.C;
			}
			return A.Transition->SourceNodeId.D < B.Transition->SourceNodeId.D;
		});

		TMap<uint64, int32> GroupCount;
		for (const FTransitionDrawLaneData& Lane : TransitionLanes)
		{
			const uint64 Key = (static_cast<uint64>(static_cast<uint32>(Lane.SrcStage)) << 32)
				| static_cast<uint32>(Lane.DstStage);
			GroupCount.FindOrAdd(Key) += 1;
		}

		TMap<uint64, int32> GroupNextIndex;
		for (const FTransitionDrawLaneData& Lane : TransitionLanes)
		{
			const uint64 Key = (static_cast<uint64>(static_cast<uint32>(Lane.SrcStage)) << 32)
				| static_cast<uint32>(Lane.DstStage);
			const int32 LaneCount = GroupCount.FindRef(Key);
			const int32 LaneIndex = GroupNextIndex.FindOrAdd(Key);
			GroupNextIndex[Key] = LaneIndex + 1;

			const FVector2D P0 = GraphToLocal(GetPinStubGraphPosition(Lane.Src, EPinSide::Right), LocalSize);
			const FVector2D P3 = GraphToLocal(GetPinStubGraphPosition(Lane.Dst, EPinSide::Left), LocalSize);
			const float LaneOffset = (static_cast<float>(LaneIndex) - (static_cast<float>(LaneCount - 1) * 0.5f));
			const float MidXOffset = LaneOffset * 14.0f;
			const int32 StageSpan = FMath::Max(0, Lane.DstStage - Lane.SrcStage);
			TArray<FVector2D> PathPoints;
			if (StageSpan >= 2)
			{
				const float EntryGraphX = (Lane.SrcStage + 1) * StageCellWidth - 28.0f;
				const float ExitGraphX = Lane.DstStage * StageCellWidth + 28.0f;
				const float EntryX = GraphToLocal(FVector2D(EntryGraphX, 0.0f), LocalSize).X;
				const float ExitX = GraphToLocal(FVector2D(ExitGraphX, 0.0f), LocalSize).X;
				const float BridgeY = ((P0.Y + P3.Y) * 0.5f) + LaneOffset * 10.0f;
				MetaplotGraphWidgetPrivate::BuildBundledOrthogonalPath(
					P0,
					P3,
					FMath::Clamp(EntryX, P0.X + 22.0f, P3.X - 40.0f),
					FMath::Clamp(ExitX, P0.X + 40.0f, P3.X - 22.0f),
					BridgeY,
					PanScreen,
					PathPoints);
			}
			else
			{
				const float MidBase = (P0.X + P3.X) * 0.5f;
				const float MidX = FMath::Clamp(MidBase + MidXOffset, P0.X + 24.0f, P3.X - 24.0f);
				MetaplotGraphWidgetPrivate::BuildRoundedOrthogonalPathViaX(P0, P3, MidX, PanScreen, PathPoints);
			}
			if (PathPoints.Num() >= 2)
			{
				const bool bEmphasized = (Lane.Src.NodeId == SelectedNodeId) ||
					(Lane.Dst.NodeId == SelectedNodeId) ||
					(Lane.Src.NodeId == HoveredNodeId) ||
					(Lane.Dst.NodeId == HoveredNodeId);
				const FLinearColor LineColor = bEmphasized
					? FLinearColor(0.38f, 0.78f, 1.0f, 0.96f)
					: FLinearColor(0.68f, 0.70f, 0.74f, 0.52f);
				const float LineThickness = bEmphasized ? 3.4f : 2.5f;

				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId,
					RootGeo,
					PathPoints,
					ESlateDrawEffect::None,
					LineColor,
					true,
					LineThickness);
			}
		}
	}

	if (bDraggingConnection && DragPinNodeId.IsValid())
	{
		const FMetaplotNode* DragNode = ResolveNode(DragPinNodeId);
		if (DragNode)
		{
			const FMetaplotNode DragLayout = LayoutForDraw(*DragNode);
			const FVector2D P0 = GraphToLocal(GetPinStubGraphPosition(DragLayout, DragPinSide), LocalSize);
			const FVector2D P3 = DragCurrentLocal;
			TArray<FVector2D> PreviewPath;
			MetaplotGraphWidgetPrivate::BuildRoundedOrthogonalPath(P0, P3, PanScreen, PreviewPath);
			FLinearColor InvalidPreviewColor(1.0f, 0.45f, 0.35f, 0.92f);
			switch (HoveredPinInvalidReason)
			{
			case EConnectionInvalidReason::BackwardStage:
				InvalidPreviewColor = FLinearColor(0.65f, 0.65f, 0.68f, 0.95f);
				break;
			case EConnectionInvalidReason::SameRowSkipStage:
				InvalidPreviewColor = FLinearColor(1.0f, 0.62f, 0.30f, 0.95f);
				break;
			case EConnectionInvalidReason::Duplicate:
				InvalidPreviewColor = FLinearColor(0.95f, 0.80f, 0.30f, 0.95f);
				break;
			default:
				break;
			}
			const FLinearColor PreviewColor = bHoveredPinAcceptsConnection
				? FLinearColor(0.35f, 0.75f, 1.0f, 0.9f)
				: (HoveredPinInvalidReason == EConnectionInvalidReason::None
					? FLinearColor(0.35f, 0.75f, 1.0f, 0.9f)
					: InvalidPreviewColor);
			if (PreviewPath.Num() >= 2)
			{
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId + 1,
					RootGeo,
					PreviewPath,
					ESlateDrawEffect::None,
					PreviewColor,
					true,
					2.8f);
			}
		}
	}

	++LayerId;

	// Nodes
	if (Flow)
	{
		for (const FMetaplotNode& Node : Flow->Nodes)
		{
			const FMetaplotNode DrawLayout = LayoutForDraw(Node);
			const FVector2D TL = GetNodeTopLeftGraph(DrawLayout);
			const FVector2D Sz = GetNodeSize(Node);
			const FVector2D LocalTL = GraphToLocal(TL, LocalSize);

			const FLinearColor Accent = MetaplotGraphWidgetPrivate::GetTypeAccent(Node.NodeType);
			const bool bIsStart = Flow->StartNodeId == Node.NodeId;
			const bool bSelected = Node.NodeId == SelectedNodeId;
			const bool bHover = Node.NodeId == HoveredNodeId;

			const FLinearColor Fill = bHover ? Accent * 1.15f : Accent * 0.85f;
			const FLinearColor Border = bSelected ? FLinearColor(0.35f, 0.75f, 1.0f, 1.0f) : FLinearColor(0.12f, 0.12f, 0.14f, 1.0f);
			const float BorderThick = bSelected ? 2.5f : 1.5f;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				MakeGeo(LocalTL, Sz),
				WhiteBox,
				ESlateDrawEffect::None,
				Fill * FLinearColor(1.0f, 1.0f, 1.0f, 0.22f));

			const FLinearColor EdgeCol = Border;
			const float T = BorderThick;
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, MakeGeo(LocalTL, FVector2D(Sz.X, T)), WhiteBox, ESlateDrawEffect::None, EdgeCol);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, MakeGeo(LocalTL + FVector2D(0.0f, Sz.Y - T), FVector2D(Sz.X, T)), WhiteBox, ESlateDrawEffect::None, EdgeCol);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, MakeGeo(LocalTL, FVector2D(T, Sz.Y)), WhiteBox, ESlateDrawEffect::None, EdgeCol);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, MakeGeo(LocalTL + FVector2D(Sz.X - T, 0.0f), FVector2D(T, Sz.Y)), WhiteBox, ESlateDrawEffect::None, EdgeCol);

			const FString Title = Node.NodeName.IsEmpty() ? FString(TEXT("Unnamed")) : Node.NodeName.ToString();
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId,
				MakeGeo(LocalTL + FVector2D(10.0f, 8.0f), FVector2D(Sz.X - 20.0f, 20.0f)),
				Title,
				TitleFont,
				ESlateDrawEffect::None,
				FLinearColor::White);

			const FString TypeLine = MetaplotGraphWidgetPrivate::GetTypeLabel(Node.NodeType) + (bIsStart ? TEXT("  · Start") : TEXT(""));
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId,
				MakeGeo(LocalTL + FVector2D(10.0f, 26.0f), FVector2D(Sz.X - 20.0f, 16.0f)),
				TypeLine,
				SmallFont,
				ESlateDrawEffect::None,
				FLinearColor(0.85f, 0.88f, 0.92f, 1.0f));

			const FString Meta = FString::Printf(TEXT("Stage %d  ·  Layer %d"), Node.StageIndex, Node.LayerIndex);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId,
				MakeGeo(LocalTL + FVector2D(10.0f, 40.0f), FVector2D(Sz.X - 20.0f, 20.0f)),
				Meta,
				SmallFont,
				ESlateDrawEffect::None,
				FLinearColor(0.65f, 0.68f, 0.72f, 1.0f));

			const int32 TaskCount = MetaplotGraphWidgetPrivate::GetTaskCount(Flow, Node.NodeId);
			const FString TaskMeta = FString::Printf(TEXT("Tasks %d"), TaskCount);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId,
				MakeGeo(LocalTL + FVector2D(10.0f, 52.0f), FVector2D(Sz.X - 20.0f, 16.0f)),
				TaskMeta,
				SmallFont,
				ESlateDrawEffect::None,
				FLinearColor(0.60f, 0.76f, 0.95f, 1.0f));

			const FString Desc = Node.Description.ToString();
			if (Desc.Len() > 0)
			{
				const int32 MaxChars = 120;
				FString Short = Desc.Left(MaxChars);
				if (Desc.Len() > MaxChars)
				{
					Short.Append(TEXT("…"));
				}
				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId,
					MakeGeo(LocalTL + FVector2D(10.0f, 68.0f), FVector2D(Sz.X - 20.0f, 68.0f)),
					Short,
					SmallFont,
					ESlateDrawEffect::None,
					FLinearColor(0.75f, 0.78f, 0.82f, 1.0f));
			}

			FString Badge;
			Badge.AppendChar(MetaplotGraphWidgetPrivate::GetTypeGlyph(Node.NodeType));
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId,
				MakeGeo(LocalTL + FVector2D(Sz.X - 26.0f, 8.0f), FVector2D(24.0f, 24.0f)),
				Badge,
				FCoreStyle::GetDefaultFontStyle("Black", 14),
				ESlateDrawEffect::None,
				Accent);

			const FVector2D LeftPinLocal = GraphToLocal(GetPinGraphPosition(DrawLayout, EPinSide::Left), LocalSize);
			const FVector2D RightPinLocal = GraphToLocal(GetPinGraphPosition(DrawLayout, EPinSide::Right), LocalSize);

			const bool bLeftHover = (HoveredPinNodeId == Node.NodeId && HoveredPinSide == EPinSide::Left);
			const bool bRightHover = (HoveredPinNodeId == Node.NodeId && HoveredPinSide == EPinSide::Right);
			const bool bLeftDrag = (bDraggingConnection && DragPinNodeId == Node.NodeId && DragPinSide == EPinSide::Left);
			const bool bRightDrag = (bDraggingConnection && DragPinNodeId == Node.NodeId && DragPinSide == EPinSide::Right);

			const float LeftRadius = PinRadius + (bLeftHover || bLeftDrag ? 1.5f : 0.0f);
			const float RightRadius = PinRadius + (bRightHover || bRightDrag ? 1.5f : 0.0f);
			const FLinearColor PinFill(0.11f, 0.13f, 0.16f, 1.0f);
			const FLinearColor ActiveValidColor(0.35f, 0.75f, 1.0f, 1.0f);
			FLinearColor ActiveInvalidColor(1.0f, 0.35f, 0.35f, 1.0f);
			switch (HoveredPinInvalidReason)
			{
			case EConnectionInvalidReason::BackwardStage:
				ActiveInvalidColor = FLinearColor(0.65f, 0.65f, 0.68f, 1.0f);
				break;
			case EConnectionInvalidReason::SameRowSkipStage:
				ActiveInvalidColor = FLinearColor(1.0f, 0.62f, 0.30f, 1.0f);
				break;
			case EConnectionInvalidReason::Cycle:
				ActiveInvalidColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);
				break;
			case EConnectionInvalidReason::Duplicate:
				ActiveInvalidColor = FLinearColor(0.95f, 0.80f, 0.30f, 1.0f);
				break;
			case EConnectionInvalidReason::SameNode:
				ActiveInvalidColor = FLinearColor(1.0f, 0.45f, 0.45f, 1.0f);
				break;
			default:
				ActiveInvalidColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);
				break;
			}
			const bool bLeftInvalid = bLeftHover && bDraggingConnection && !bHoveredPinAcceptsConnection;
			const bool bRightInvalid = bRightHover && bDraggingConnection && !bHoveredPinAcceptsConnection;
			const FLinearColor LeftOutline = bLeftDrag ? ActiveValidColor : (bLeftInvalid ? ActiveInvalidColor : ((bLeftHover || bLeftDrag) ? ActiveValidColor : FLinearColor(0.70f, 0.74f, 0.80f, 0.95f)));
			const FLinearColor RightOutline = bRightDrag ? ActiveValidColor : (bRightInvalid ? ActiveInvalidColor : ((bRightHover || bRightDrag) ? ActiveValidColor : FLinearColor(0.70f, 0.74f, 0.80f, 0.95f)));
			const FVector2D LeftStubLocal = GraphToLocal(GetPinStubGraphPosition(DrawLayout, EPinSide::Left), LocalSize);
			const FVector2D RightStubLocal = GraphToLocal(GetPinStubGraphPosition(DrawLayout, EPinSide::Right), LocalSize);

			TArray<FVector2D> LeftStubLine;
			LeftStubLine.Add(LeftPinLocal);
			LeftStubLine.Add(LeftStubLocal);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 1,
				RootGeo,
				LeftStubLine,
				ESlateDrawEffect::None,
				LeftOutline.CopyWithNewOpacity(0.9f),
				true,
				2.2f);

			TArray<FVector2D> RightStubLine;
			RightStubLine.Add(RightPinLocal);
			RightStubLine.Add(RightStubLocal);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 1,
				RootGeo,
				RightStubLine,
				ESlateDrawEffect::None,
				RightOutline.CopyWithNewOpacity(0.9f),
				true,
				2.2f);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				MakeGeo(LeftPinLocal - FVector2D(LeftRadius, LeftRadius), FVector2D(LeftRadius * 2.0f, LeftRadius * 2.0f)),
				WhiteBox,
				ESlateDrawEffect::None,
				PinFill);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				MakeGeo(LeftPinLocal - FVector2D(LeftRadius + 1.0f, LeftRadius + 1.0f), FVector2D((LeftRadius + 1.0f) * 2.0f, 2.0f)),
				WhiteBox,
				ESlateDrawEffect::None,
				LeftOutline);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				MakeGeo(LeftPinLocal - FVector2D(LeftRadius + 1.0f, -LeftRadius - 1.0f), FVector2D((LeftRadius + 1.0f) * 2.0f, 2.0f)),
				WhiteBox,
				ESlateDrawEffect::None,
				LeftOutline);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				MakeGeo(LeftPinLocal - FVector2D(LeftRadius + 1.0f, LeftRadius + 1.0f), FVector2D(2.0f, (LeftRadius + 1.0f) * 2.0f)),
				WhiteBox,
				ESlateDrawEffect::None,
				LeftOutline);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				MakeGeo(LeftPinLocal + FVector2D(LeftRadius - 1.0f, -LeftRadius - 1.0f), FVector2D(2.0f, (LeftRadius + 1.0f) * 2.0f)),
				WhiteBox,
				ESlateDrawEffect::None,
				LeftOutline);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				MakeGeo(RightPinLocal - FVector2D(RightRadius, RightRadius), FVector2D(RightRadius * 2.0f, RightRadius * 2.0f)),
				WhiteBox,
				ESlateDrawEffect::None,
				PinFill);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				MakeGeo(RightPinLocal - FVector2D(RightRadius + 1.0f, RightRadius + 1.0f), FVector2D((RightRadius + 1.0f) * 2.0f, 2.0f)),
				WhiteBox,
				ESlateDrawEffect::None,
				RightOutline);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				MakeGeo(RightPinLocal - FVector2D(RightRadius + 1.0f, -RightRadius - 1.0f), FVector2D((RightRadius + 1.0f) * 2.0f, 2.0f)),
				WhiteBox,
				ESlateDrawEffect::None,
				RightOutline);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				MakeGeo(RightPinLocal - FVector2D(RightRadius + 1.0f, RightRadius + 1.0f), FVector2D(2.0f, (RightRadius + 1.0f) * 2.0f)),
				WhiteBox,
				ESlateDrawEffect::None,
				RightOutline);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				MakeGeo(RightPinLocal + FVector2D(RightRadius - 1.0f, -RightRadius - 1.0f), FVector2D(2.0f, (RightRadius + 1.0f) * 2.0f)),
				WhiteBox,
				ESlateDrawEffect::None,
				RightOutline);
		}
	}

	if (bDraggingConnection && HoveredPinInvalidReason != EConnectionInvalidReason::None && !bHoveredPinAcceptsConnection)
	{
		FString ReasonText = TEXT("连接无效");
		switch (HoveredPinInvalidReason)
		{
		case EConnectionInvalidReason::SameNode:
			ReasonText = TEXT("无效：不能连接到自身");
			break;
		case EConnectionInvalidReason::BackwardStage:
			ReasonText = TEXT("无效：禁止从右往左连接");
			break;
		case EConnectionInvalidReason::SameRowSkipStage:
			ReasonText = TEXT("无效：同行只能连接下一列");
			break;
		case EConnectionInvalidReason::Cycle:
			ReasonText = TEXT("无效：该连接会形成环");
			break;
		case EConnectionInvalidReason::Duplicate:
			ReasonText = TEXT("无效：连线已存在");
			break;
		case EConnectionInvalidReason::InvalidPinPair:
			ReasonText = TEXT("无效：请从右引脚连接到左引脚");
			break;
		default:
			break;
		}

		const FVector2D HintPos = DragCurrentLocal + FVector2D(12.0f, 10.0f);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 3,
			MakeGeo(HintPos, FVector2D(220.0f, 20.0f)),
			ReasonText,
			SmallFont,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 0.70f, 0.65f, 0.98f));
	}

	if (bDraggingNode && DragNodeId.IsValid() && !bDragNodePlacementValid)
	{
		const FVector2D HintPos = DragCurrentLocal + FVector2D(12.0f, 10.0f);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 3,
			MakeGeo(HintPos, FVector2D(300.0f, 22.0f)),
			FString(TEXT("不可放置：与连线 Stage/Layer 规则冲突或格子已被占用")),
			SmallFont,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 0.70f, 0.65f, 0.98f));
	}

	++LayerId;

	return LayerId;
}

void SMetaStoryFlowGraph::BroadcastHorizontalPanChanged() const
{
	if (OnHorizontalPanChanged.IsBound())
	{
		OnHorizontalPanChanged.Execute(PanScreen.X);
	}
}
