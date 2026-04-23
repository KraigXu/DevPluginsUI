#include "Scenario/MetaplotFlowGraphWidget.h"

#include "Flow/MetaplotFlow.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Geometry.h"
#include "Layout/PaintGeometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

namespace MetaplotGraphWidgetPrivate
{
	static constexpr float GridHeaderTop = -36.0f;
	static constexpr float GridHeaderHeight = 16.0f;
	static constexpr float TimelineY = -18.0f;

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
}

void SMetaplotFlowGraphWidget::Construct(const FArguments& InArgs)
{
	WeakFlow = InArgs._FlowAsset;
	OnNodeSelected = InArgs._OnNodeSelected;
	SelectedNodeId = FGuid();
}

void SMetaplotFlowGraphWidget::SetFlowAsset(UMetaplotFlow* InFlow)
{
	WeakFlow = InFlow;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SMetaplotFlowGraphWidget::SetSelectedNodeId(const FGuid& InNodeId)
{
	SelectedNodeId = InNodeId;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SMetaplotFlowGraphWidget::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(320.0f, 240.0f);
}

FVector2D SMetaplotFlowGraphWidget::GetNodeSize(const FMetaplotNode& Node)
{
	const FString Desc = Node.Description.ToString();
	const int32 DescLen = Desc.Len();
	const int32 Lines = DescLen > 0 ? FMath::Clamp((DescLen + 39) / 40, 1, 3) : 0;
	const float DescHeight = Lines > 0 ? Lines * 14.0f + 6.0f : 0.0f;
	const float Height = FMath::Clamp(52.0f + DescHeight, 72.0f, 200.0f);
	return FVector2D(NodeWidth, Height);
}

FVector2D SMetaplotFlowGraphWidget::GetNodeTopLeftGraph(const FMetaplotNode& Node) const
{
	const FVector2D Size = GetNodeSize(Node);
	const float X = Node.StageIndex * StageCellWidth + (StageCellWidth - NodeWidth) * 0.5f;
	const float Y = Node.LayerIndex * LayerCellHeight + (LayerCellHeight - Size.Y) * 0.5f;
	return FVector2D(X, Y);
}

FVector2D SMetaplotFlowGraphWidget::GraphToLocal(const FVector2D& GraphPos, const FVector2D& LocalSize) const
{
	(void)LocalSize;
	return GraphPos * Zoom + PanScreen;
}

FVector2D SMetaplotFlowGraphWidget::LocalToGraph(const FVector2D& LocalPos, const FVector2D& LocalSize) const
{
	(void)LocalSize;
	return (LocalPos - PanScreen) / Zoom;
}

void SMetaplotFlowGraphWidget::ClampPanToContent(const FVector2D& LocalSize)
{
	float BoundMinX, BoundMinY, BoundMaxX, BoundMaxY;
	GetContentBounds(BoundMinX, BoundMinY, BoundMaxX, BoundMaxY);

	const float ClampPadX = 0.0f;
	const float ClampPadY = 0.0f;

	const float MinPanX = LocalSize.X - (BoundMaxX + ClampPadX) * Zoom;
	const float MaxPanX = -(BoundMinX - ClampPadX) * Zoom;
	if (MinPanX > MaxPanX)
	{
		PanScreen.X = (MinPanX + MaxPanX) * 0.5f;
	}
	else
	{
		PanScreen.X = FMath::Clamp(PanScreen.X, MinPanX, MaxPanX);
	}

	const float MinPanY = LocalSize.Y - (BoundMaxY + ClampPadY) * Zoom;
	const float MaxPanY = -(BoundMinY - ClampPadY) * Zoom;
	if (MinPanY > MaxPanY)
	{
		PanScreen.Y = (MinPanY + MaxPanY) * 0.5f;
	}
	else
	{
		PanScreen.Y = FMath::Clamp(PanScreen.Y, MinPanY, MaxPanY);
	}
}

void SMetaplotFlowGraphWidget::GetContentBounds(float& OutMinX, float& OutMinY, float& OutMaxX, float& OutMaxY) const
{
	UMetaplotFlow* Flow = WeakFlow.Get();
	const MetaplotGraphWidgetPrivate::FGridRange GridRange = MetaplotGraphWidgetPrivate::BuildGridRange(Flow);
	OutMinX = GridRange.MinStage * StageCellWidth;
	OutMaxX = (GridRange.MaxStage + 1) * StageCellWidth;
	OutMinY = MetaplotGraphWidgetPrivate::GridHeaderTop;
	OutMaxY = (GridRange.MaxLayer + 1) * LayerCellHeight;
}

bool SMetaplotFlowGraphWidget::HitTestNode(const FGeometry& MyGeometry, const FVector2D& LocalPos, FGuid& OutNodeId) const
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

FReply SMetaplotFlowGraphWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D LocalSize = MyGeometry.GetLocalSize();

	const float Pad = 12.0f;
	const float MW = 200.0f;
	const float MH = 120.0f;
	const FVector2D MiniTL(LocalSize.X - MW - Pad, LocalSize.Y - MH - Pad);
	const FVector2D MiniBR(LocalSize.X - Pad, LocalSize.Y - Pad);
	const bool bInMinimap = LocalPos.X >= MiniTL.X && LocalPos.X <= MiniBR.X && LocalPos.Y >= MiniTL.Y && LocalPos.Y <= MiniBR.Y;

	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		bPanning = true;
		PanGrabLocal = LocalPos;
		PanScreenAtGrab = PanScreen;
		return FReply::Handled().CaptureMouse(AsShared());
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bInMinimap)
	{
		bDraggingMinimap = true;
		MinimapGrabLocal = LocalPos;
		return FReply::Handled().CaptureMouse(AsShared());
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FGuid HitId;
		if (HitTestNode(MyGeometry, LocalPos, HitId))
		{
			SelectedNodeId = HitId;
			if (OnNodeSelected.IsBound())
			{
				OnNodeSelected.Execute(HitId);
			}
			Invalidate(EInvalidateWidgetReason::Paint);
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

	return FReply::Unhandled();
}

FReply SMetaplotFlowGraphWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && bPanning)
	{
		bPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDraggingMinimap)
	{
		bDraggingMinimap = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply SMetaplotFlowGraphWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D LocalSize = MyGeometry.GetLocalSize();

	if (bPanning && MouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton))
	{
		const FVector2D Delta = LocalPos - PanGrabLocal;
		PanScreen = PanScreenAtGrab + Delta;
		ClampPanToContent(LocalSize);
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}

	if (bDraggingMinimap)
	{
		float MinX, MinY, MaxX, MaxY;
		GetContentBounds(MinX, MinY, MaxX, MaxY);
		const float ContentW = FMath::Max(1.0f, MaxX - MinX);
		const float ContentH = FMath::Max(1.0f, MaxY - MinY);

		const float Pad = 12.0f;
		const float MW = 200.0f;
		const float MH = 120.0f;
		const FVector2D MiniTL(LocalSize.X - MW - Pad, LocalSize.Y - MH - Pad);

		const float U = (LocalPos.X - MiniTL.X) / MW;
		const float V = (LocalPos.Y - MiniTL.Y) / MH;
		const float TargetCenterGX = MinX + U * ContentW;
		const float TargetCenterGY = MinY + V * ContentH;

		const FVector2D ViewHalf = LocalSize * 0.5f;
		PanScreen = ViewHalf - FVector2D(TargetCenterGX, TargetCenterGY) * Zoom;
		ClampPanToContent(LocalSize);
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

FReply SMetaplotFlowGraphWidget::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const float Delta = MouseEvent.GetWheelDelta();
	if (FMath::IsNearlyZero(Delta))
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const FVector2D GraphUnderCursor = LocalToGraph(LocalPos, LocalSize);

	const float OldZoom = Zoom;
	const float Factor = Delta > 0.0f ? 1.1f : 0.9f;
	const float NewZoom = FMath::Clamp(OldZoom * Factor, ZoomMin, ZoomMax);
	if (FMath::IsNearlyEqual(NewZoom, OldZoom))
	{
		return FReply::Handled();
	}

	Zoom = NewZoom;
	PanScreen = LocalPos - GraphUnderCursor * Zoom;
	ClampPanToContent(LocalSize);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

FCursorReply SMetaplotFlowGraphWidget::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (bPanning)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}
	return FCursorReply::Unhandled();
}

int32 SMetaplotFlowGraphWidget::OnPaint(
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

	auto EvalBezier = [](const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, float T) -> FVector2D
	{
		const float U = 1.0f - T;
		return U * U * U * P0 + 3.0f * U * U * T * P1 + 3.0f * U * T * T * P2 + T * T * T * P3;
	};

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
			const FVector2D LocalSizeCell(StageCellWidth * Zoom, (BoundMaxY - HeaderTop) * Zoom);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				MakeGeo(LocalTL, LocalSizeCell),
				WhiteBox,
				ESlateDrawEffect::None,
				(S % 2 == 0) ? StageBandA : StageBandB);

			const FVector2D LocalHeaderTL = GraphToLocal(FVector2D(Left, HeaderTop), LocalSize);
			const FVector2D LocalHeaderSize(StageCellWidth * Zoom, HeaderHeight * Zoom);
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

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 2,
			MakeGeo(TimelineStart + FVector2D(4.0f, -14.0f), FVector2D(120.0f, 14.0f)),
			FString(TEXT("时间轴")),
			SmallFont,
			ESlateDrawEffect::None,
			FLinearColor(0.80f, 0.90f, 1.0f, 0.95f));
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

	// Transitions (under nodes)
	if (Flow)
	{
		for (const FMetaplotTransition& Tr : Flow->Transitions)
		{
			const FMetaplotNode* Src = ResolveNode(Tr.SourceNodeId);
			const FMetaplotNode* Dst = ResolveNode(Tr.TargetNodeId);
			if (!Src || !Dst)
			{
				continue;
			}

			const FVector2D SrcTL = GetNodeTopLeftGraph(*Src);
			const FVector2D SrcSz = GetNodeSize(*Src);
			const FVector2D DstTL = GetNodeTopLeftGraph(*Dst);
			const FVector2D DstSz = GetNodeSize(*Dst);

			const FVector2D P0 = GraphToLocal(SrcTL + FVector2D(SrcSz.X, SrcSz.Y * 0.5f), LocalSize);
			const FVector2D P3 = GraphToLocal(DstTL + FVector2D(0.0f, DstSz.Y * 0.5f), LocalSize);

			const float Horizontal = FMath::Max(48.0f, FMath::Abs(P3.X - P0.X) * 0.45f);
			const FVector2D P1 = P0 + FVector2D(Horizontal, 0.0f);
			const FVector2D P2 = P3 - FVector2D(Horizontal, 0.0f);

			{
				const int32 Segments = 24;
				for (int32 Seg = 0; Seg < Segments; ++Seg)
				{
					const float T0 = static_cast<float>(Seg) / static_cast<float>(Segments);
					const float T1 = static_cast<float>(Seg + 1) / static_cast<float>(Segments);
					const FVector2D A = EvalBezier(P0, P1, P2, P3, T0);
					const FVector2D B = EvalBezier(P0, P1, P2, P3, T1);
					TArray<FVector2D> Line;
					Line.Add(A);
					Line.Add(B);
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						LayerId,
						RootGeo,
						Line,
						ESlateDrawEffect::None,
						FLinearColor(0.72f, 0.74f, 0.78f, 0.85f),
						true,
						2.0f);
				}
			}

			// Arrow head at P3
			{
				const FVector2D Tangent = (P3 - EvalBezier(P0, P1, P2, P3, 0.92f)).GetSafeNormal();
				const FVector2D Ortho(-Tangent.Y, Tangent.X);
				const FVector2D A = P3;
				const FVector2D B = P3 - Tangent * 10.0f + Ortho * 5.0f;
				const FVector2D C = P3 - Tangent * 10.0f - Ortho * 5.0f;
				TArray<FVector2D> Tri;
				Tri.Add(A);
				Tri.Add(B);
				Tri.Add(C);
				Tri.Add(A);
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId,
					RootGeo,
					Tri,
					ESlateDrawEffect::None,
					FLinearColor(0.72f, 0.74f, 0.78f, 0.9f),
					true,
					1.5f);
			}
		}
	}

	++LayerId;

	// Nodes
	if (Flow)
	{
		for (const FMetaplotNode& Node : Flow->Nodes)
		{
			const FVector2D TL = GetNodeTopLeftGraph(Node);
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
					MakeGeo(LocalTL + FVector2D(10.0f, 56.0f), FVector2D(Sz.X - 20.0f, 80.0f)),
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
		}
	}

	++LayerId;

	// Minimap (eagle-eye)
	{
		const float Pad = 12.0f;
		const float MW = 200.0f;
		const float MH = 120.0f;
		const FVector2D MiniTL(LocalSize.X - MW - Pad, LocalSize.Y - MH - Pad);
		const FPaintGeometry MiniGeo = MakeGeo(MiniTL, FVector2D(MW, MH));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			MiniGeo,
			WhiteBox,
			ESlateDrawEffect::None,
			FLinearColor(0.04f, 0.04f, 0.045f, 0.92f));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			MakeGeo(MiniTL, FVector2D(MW, 1.0f)),
			WhiteBox,
			ESlateDrawEffect::None,
			FLinearColor(0.25f, 0.45f, 0.85f, 0.9f));

		const float ContentW = FMath::Max(1.0f, BoundMaxX - BoundMinX);
		const float ContentH = FMath::Max(1.0f, BoundMaxY - BoundMinY);

		auto GraphToMini = [&](const FVector2D& G) -> FVector2D
		{
			const float U = (G.X - BoundMinX) / ContentW;
			const float V = (G.Y - BoundMinY) / ContentH;
			return MiniTL + FVector2D(U * MW, V * MH);
		};

		if (Flow)
		{
			for (const FMetaplotNode& Node : Flow->Nodes)
			{
				const FVector2D TL = GetNodeTopLeftGraph(Node);
				const FVector2D Sz = GetNodeSize(Node);
				const FVector2D C = TL + Sz * 0.5f;
				const FVector2D P = GraphToMini(C);
				const FPaintGeometry DotGeo = MakeGeo(P - FVector2D(1.5f, 1.5f), FVector2D(3.0f, 3.0f));
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					DotGeo,
					WhiteBox,
					ESlateDrawEffect::None,
					MetaplotGraphWidgetPrivate::GetTypeAccent(Node.NodeType));
			}
		}

		// Viewport rect on minimap
		{
			const FVector2D V0 = LocalToGraph(FVector2D::ZeroVector, LocalSize);
			const FVector2D V1 = LocalToGraph(LocalSize, LocalSize);
			const FVector2D Mini0 = GraphToMini(V0);
			const FVector2D Mini1 = GraphToMini(V1);
			const FVector2D R0(FMath::Min(Mini0.X, Mini1.X), FMath::Min(Mini0.Y, Mini1.Y));
			const FVector2D R1(FMath::Max(Mini0.X, Mini1.X), FMath::Max(Mini0.Y, Mini1.Y));

			TArray<FVector2D> Frame;
			Frame.Add(R0);
			Frame.Add(FVector2D(R1.X, R0.Y));
			Frame.Add(R1);
			Frame.Add(FVector2D(R0.X, R1.Y));
			Frame.Add(R0);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				RootGeo,
				Frame,
				ESlateDrawEffect::None,
				FLinearColor(0.35f, 0.75f, 1.0f, 0.95f),
				true,
				1.5f);
		}

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			MakeGeo(MiniTL + FVector2D(6.0f, 4.0f), FVector2D(MW - 12.0f, 20.0f)),
			FString(TEXT("鹰眼导航")),
			SmallFont,
			ESlateDrawEffect::None,
			FLinearColor(0.85f, 0.88f, 0.92f, 1.0f));
	}

	return LayerId;
}
