#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Widgets/SLeafWidget.h"

class UMetaStoryFlow;
class UMetaStoryEditorData;
class FMetaStoryViewModel;

DECLARE_DELEGATE_OneParam(FOnMetaStoryFlowGraphNodeSelected, FGuid);
DECLARE_DELEGATE_OneParam(FOnMetaStoryFlowGraphHorizontalPanChanged, float);
DECLARE_DELEGATE_TwoParams(FOnMetaStoryFlowGraphCreateTransition, FGuid, FGuid);
DECLARE_DELEGATE_ThreeParams(FOnMetaStoryFlowGraphMoveNode, FGuid, int32, int32);
DECLARE_DELEGATE_TwoParams(FOnMetaStoryFlowGraphCreateNodeRequested, int32, int32);
DECLARE_DELEGATE_OneParam(FOnMetaStoryFlowGraphDeleteNodeRequested, FGuid);
DECLARE_DELEGATE_TwoParams(FOnMetaStoryFlowGraphDeleteTransitionRequested, FGuid, FGuid);

/** 主视图：科技树风格网格画布（Stage 水平、Layer 垂直）、正交连线与中键平移。 */
class SMetaStoryFlowGraph : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaStoryFlowGraph) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UMetaStoryFlow>, FlowAsset)
		SLATE_EVENT(FOnMetaStoryFlowGraphNodeSelected, OnNodeSelected)
		SLATE_EVENT(FOnMetaStoryFlowGraphHorizontalPanChanged, OnHorizontalPanChanged)
		SLATE_EVENT(FOnMetaStoryFlowGraphCreateTransition, OnCreateTransition)
		SLATE_EVENT(FOnMetaStoryFlowGraphMoveNode, OnMoveNode)
		SLATE_EVENT(FOnMetaStoryFlowGraphCreateNodeRequested, OnCreateNodeRequested)
		SLATE_EVENT(FOnMetaStoryFlowGraphDeleteNodeRequested, OnDeleteNodeRequested)
		SLATE_EVENT(FOnMetaStoryFlowGraphDeleteTransitionRequested, OnDeleteTransitionRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetFlowAsset(UMetaStoryFlow* InFlow);
	/** 可选；用于与影子 UMetaStoryState::Tasks 对齐任务数量（与 Flow NodeStates 取 max）。 */
	void SetEditorData(const UMetaStoryEditorData* InEditorData);
	void SetSelectedNodeId(const FGuid& InNodeId);
	/** 可选；用于 PIE 下调试活动状态与 Flow 节点（NodeId）对齐高亮。 */
	void SetViewModel(TSharedPtr<FMetaStoryViewModel> InViewModel);
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

	static FVector2D GetNodeSize(const struct FMetaStoryFlowNode& Node);
	FVector2D GetNodeTopLeftGraph(const struct FMetaStoryFlowNode& Node) const;
	FVector2D GetPinGraphPosition(const struct FMetaStoryFlowNode& Node, EPinSide Side) const;
	FVector2D GetPinStubGraphPosition(const struct FMetaStoryFlowNode& Node, EPinSide Side) const;
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
	TWeakObjectPtr<UMetaStoryFlow> WeakFlow;
	TWeakObjectPtr<const UMetaStoryEditorData> WeakEditorData;
	TWeakPtr<FMetaStoryViewModel> WeakViewModel;
	FOnMetaStoryFlowGraphNodeSelected OnNodeSelected;
	FOnMetaStoryFlowGraphHorizontalPanChanged OnHorizontalPanChanged;
	FOnMetaStoryFlowGraphCreateTransition OnCreateTransition;
	FOnMetaStoryFlowGraphMoveNode OnMoveNode;
	FOnMetaStoryFlowGraphCreateNodeRequested OnCreateNodeRequested;
	FOnMetaStoryFlowGraphDeleteNodeRequested OnDeleteNodeRequested;
	FOnMetaStoryFlowGraphDeleteTransitionRequested OnDeleteTransitionRequested;

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
