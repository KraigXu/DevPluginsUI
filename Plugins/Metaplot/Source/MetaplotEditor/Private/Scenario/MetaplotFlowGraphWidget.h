#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Widgets/SLeafWidget.h"

class UMetaplotFlow;

DECLARE_DELEGATE_OneParam(FOnMetaplotGraphNodeSelected, FGuid);

/** 主视图：科技树风格网格画布（Stage 水平、Layer 垂直）、贝塞尔连线、平移/缩放、鹰眼。 */
class SMetaplotFlowGraphWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaplotFlowGraphWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UMetaplotFlow>, FlowAsset)
		SLATE_EVENT(FOnMetaplotGraphNodeSelected, OnNodeSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetFlowAsset(UMetaplotFlow* InFlow);
	void SetSelectedNodeId(const FGuid& InNodeId);

private:
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	static FVector2D GetNodeSize(const struct FMetaplotNode& Node);
	FVector2D GetNodeTopLeftGraph(const struct FMetaplotNode& Node) const;
	FVector2D GraphToLocal(const FVector2D& GraphPos, const FVector2D& LocalSize) const;
	FVector2D LocalToGraph(const FVector2D& LocalPos, const FVector2D& LocalSize) const;
	bool HitTestNode(const FGeometry& MyGeometry, const FVector2D& LocalPos, FGuid& OutNodeId) const;
	void GetContentBounds(float& OutMinX, float& OutMinY, float& OutMaxX, float& OutMaxY) const;
	void ClampPanToContent(const FVector2D& LocalSize);

private:
	TWeakObjectPtr<UMetaplotFlow> WeakFlow;
	FOnMetaplotGraphNodeSelected OnNodeSelected;

	FVector2D PanScreen = FVector2D(80.0f, 80.0f);
	float Zoom = 1.0f;
	bool bPanning = false;
	FVector2D PanGrabLocal;
	FVector2D PanScreenAtGrab;

	bool bDraggingMinimap = false;
	FVector2D MinimapGrabLocal;

	FGuid HoveredNodeId;
	FGuid SelectedNodeId;

	static constexpr float StageCellWidth = 220.0f;
	static constexpr float LayerCellHeight = 140.0f;
	static constexpr float NodeWidth = 160.0f;
	static constexpr float ZoomMin = 0.2f;
	static constexpr float ZoomMax = 2.0f;
};
