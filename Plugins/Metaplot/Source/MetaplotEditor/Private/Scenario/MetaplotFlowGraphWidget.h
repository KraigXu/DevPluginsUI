#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Widgets/SLeafWidget.h"

class UMetaplotFlow;
enum class EMetaplotNodeType : uint8;

DECLARE_DELEGATE_OneParam(FOnMetaplotGraphNodeSelected, FGuid);
DECLARE_DELEGATE_OneParam(FOnMetaplotGraphHorizontalPanChanged, float);
DECLARE_DELEGATE_TwoParams(FOnMetaplotGraphCreateTransition, FGuid, FGuid);
DECLARE_DELEGATE_ThreeParams(FOnMetaplotGraphMoveNode, FGuid, int32, int32);
DECLARE_DELEGATE_ThreeParams(FOnMetaplotGraphCreateNodeRequested, EMetaplotNodeType, int32, int32);
DECLARE_DELEGATE_OneParam(FOnMetaplotGraphDeleteNodeRequested, FGuid);
DECLARE_DELEGATE_TwoParams(FOnMetaplotGraphDeleteTransitionRequested, FGuid, FGuid);

/** 主视图：科技树风格网格画布（Stage 水平、Layer 垂直）、正交连线与中键平移。 */
class SMetaplotFlowGraphWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaplotFlowGraphWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UMetaplotFlow>, FlowAsset)
		SLATE_EVENT(FOnMetaplotGraphNodeSelected, OnNodeSelected)
		SLATE_EVENT(FOnMetaplotGraphHorizontalPanChanged, OnHorizontalPanChanged)
		SLATE_EVENT(FOnMetaplotGraphCreateTransition, OnCreateTransition)
		SLATE_EVENT(FOnMetaplotGraphMoveNode, OnMoveNode)
		SLATE_EVENT(FOnMetaplotGraphCreateNodeRequested, OnCreateNodeRequested)
		SLATE_EVENT(FOnMetaplotGraphDeleteNodeRequested, OnDeleteNodeRequested)
		SLATE_EVENT(FOnMetaplotGraphDeleteTransitionRequested, OnDeleteTransitionRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetFlowAsset(UMetaplotFlow* InFlow);
	void SetSelectedNodeId(const FGuid& InNodeId);
	void SetHorizontalPanScreen(float InPanScreenX);
	bool GetHorizontalScrollbarState(float& OutOffsetFraction, float& OutThumbSizeFraction) const;

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
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	enum class EPinSide : uint8
	{
		None,
		Left,
		Right
	};

	enum class EConnectionInvalidReason : uint8
	{
		None,
		InvalidPinPair,
		SameNode,
		BackwardStage,
		SameRowSkipStage,
		Duplicate,
		Cycle
	};

	static FVector2D GetNodeSize(const struct FMetaplotNode& Node);
	FVector2D GetNodeTopLeftGraph(const struct FMetaplotNode& Node) const;
	FVector2D GetPinGraphPosition(const struct FMetaplotNode& Node, EPinSide Side) const;
	FVector2D GetPinStubGraphPosition(const struct FMetaplotNode& Node, EPinSide Side) const;
	FVector2D GraphToLocal(const FVector2D& GraphPos, const FVector2D& LocalSize) const;
	FVector2D LocalToGraph(const FVector2D& LocalPos, const FVector2D& LocalSize) const;
	bool HitTestNode(const FGeometry& MyGeometry, const FVector2D& LocalPos, FGuid& OutNodeId) const;
	bool HitTestPin(const FGeometry& MyGeometry, const FVector2D& LocalPos, FGuid& OutNodeId, EPinSide& OutSide) const;
	bool HitTestTransition(const FGeometry& MyGeometry, const FVector2D& LocalPos, FGuid& OutSourceNodeId, FGuid& OutTargetNodeId) const;
	bool IsTransitionRuleValid(const FGuid& SourceNodeId, const FGuid& TargetNodeId) const;
	EConnectionInvalidReason GetTransitionInvalidReason(const FGuid& SourceNodeId, const FGuid& TargetNodeId) const;
	EConnectionInvalidReason GetConnectionCandidateInvalidReason(const FGuid& PinDragNodeId, EPinSide DragSide, const FGuid& HoverNodeId, EPinSide HoverSide) const;
	bool WouldCreateCycle(const FGuid& SourceNodeId, const FGuid& TargetNodeId) const;
	bool IsConnectionCandidateValid(const FGuid& PinDragNodeId, EPinSide DragSide, const FGuid& HoverNodeId, EPinSide HoverSide) const;
	void UpdateDragNodePlacementPreview(const FVector2D& LocalPos, const FVector2D& LocalSize);
	void GetContentBounds(float& OutMinX, float& OutMinY, float& OutMaxX, float& OutMaxY) const;
	void ClampPanToContent(const FVector2D& LocalSize);
	void BroadcastHorizontalPanChanged() const;
	void OpenContextMenuAtScreen(const FPointerEvent& MouseEvent, const FGuid& NodeId, const FGuid& SourceNodeId, const FGuid& TargetNodeId);
	void OpenCreateNodeSearchMenuAtScreen(const FPointerEvent& MouseEvent, int32 StageIndex, int32 LayerIndex);

private:
	TWeakObjectPtr<UMetaplotFlow> WeakFlow;
	FOnMetaplotGraphNodeSelected OnNodeSelected;
	FOnMetaplotGraphHorizontalPanChanged OnHorizontalPanChanged;
	FOnMetaplotGraphCreateTransition OnCreateTransition;
	FOnMetaplotGraphMoveNode OnMoveNode;
	FOnMetaplotGraphCreateNodeRequested OnCreateNodeRequested;
	FOnMetaplotGraphDeleteNodeRequested OnDeleteNodeRequested;
	FOnMetaplotGraphDeleteTransitionRequested OnDeleteTransitionRequested;

	FVector2D PanScreen = FVector2D(80.0f, 80.0f);
	bool bPanning = false;
	FVector2D PanGrabLocal;
	FVector2D PanScreenAtGrab;
	mutable FVector2D CachedLocalSize = FVector2D(1.0f, 1.0f);

	FGuid HoveredNodeId;
	FGuid SelectedNodeId;
	FGuid HoveredPinNodeId;
	EPinSide HoveredPinSide = EPinSide::None;
	bool bHoveredPinAcceptsConnection = false;
	EConnectionInvalidReason HoveredPinInvalidReason = EConnectionInvalidReason::None;
	bool bDraggingConnection = false;
	FGuid DragPinNodeId;
	EPinSide DragPinSide = EPinSide::None;
	FVector2D DragCurrentLocal = FVector2D::ZeroVector;

	bool bDraggingNode = false;
	FGuid DragNodeId;
	FVector2D DragGrabOffsetGraph = FVector2D::ZeroVector;
	int32 DragPreviewStage = 0;
	int32 DragPreviewLayer = 0;
	bool bDragNodePlacementValid = false;
	int32 DragOrigStage = 0;
	int32 DragOrigLayer = 0;

	static constexpr float StageCellWidth = 220.0f;
	static constexpr float LayerCellHeight = 140.0f;
	static constexpr float NodeWidth = 160.0f;
	static constexpr float PinRadius = 4.0f;
	static constexpr float PinStubLength = 10.0f;
};
