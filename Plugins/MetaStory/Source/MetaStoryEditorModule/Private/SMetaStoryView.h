// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SMetaStoryFlowGraph.h"
#include "Widgets/SCompoundWidget.h"

class FMetaStoryViewModel;
struct FPropertyChangedEvent;
class UMetaStoryState;
class UMetaStoryFlow;
class SScrollBox;
class FUICommandList;

/** MetaStory 主视图：内嵌 Flow 流程图编辑；状态树 UI 已移除，状态操作仍通过 ViewModel（与 Outliner 等一致）。 */
class SMetaStoryView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaStoryView) {}
	SLATE_END_ARGS()

	SMetaStoryView();
	virtual ~SMetaStoryView() override;

	void Construct(const FArguments& InArgs, TSharedRef<FMetaStoryViewModel> MetaStoryViewModel, const TSharedRef<FUICommandList>& InCommandList);

	/** 状态树展开状态已无 UI 可保存；保留空实现以兼容 MetaStoryEditor 调用。 */
	void SavePersistentExpandedStates();

	TSharedPtr<FMetaStoryViewModel> GetViewModel() const;

	void SetSelection(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates) const;

private:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void HandleUserSettingsChanged();

	void HandleModelAssetChanged();
	void HandleModelStatesRemoved(const TSet<UMetaStoryState*>& AffectedParents);
	void HandleModelStatesMoved(const TSet<UMetaStoryState*>& AffectedParents, const TSet<UMetaStoryState*>& MovedStates);
	void HandleModelStateAdded(UMetaStoryState* ParentState, UMetaStoryState* NewState);
	void HandleModelStatesChanged(const TSet<UMetaStoryState*>& AffectedStates, const FPropertyChangedEvent& PropertyChangedEvent);
	void HandleModelStateNodesChanged(const UMetaStoryState* AffectedState);
	void HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates);

	TSharedPtr<SWidget> HandleContextMenuOpening();
	TSharedRef<SWidget> HandleGenerateSettingsMenu();

	UMetaStoryState* GetFirstSelectedState() const;
	FReply HandleAddStateButton();
	void HandleAddSiblingState();
	void HandleAddChildState();
	void HandleCutSelectedStates();
	void HandleCopySelectedStates();
	void HandlePasteStatesAsSiblings();
	void HandlePasteStatesAsChildren();
	void HandleDuplicateSelectedStates();
	void HandlePasteNodesToState();
	void HandleRenameState();
	void HandleDeleteStates();
	void HandleEnableSelectedStates();
	void HandleDisableSelectedStates();

	void OnMainGraphNodeSelected(FGuid NodeId);
	void OnMainGraphCreateNodeRequested(int32 StageIndex, int32 LayerIndex);
	void OnMainGraphCreateTransition(FGuid SourceNodeId, FGuid TargetNodeId);
	void OnMainGraphMoveNode(FGuid NodeId, int32 NewStage, int32 NewLayer);
	void OnMainGraphDeleteNodeRequested(FGuid NodeId);
	void OnMainGraphDeleteTransitionRequested(FGuid SourceNodeId, FGuid TargetNodeId);
	void OnMainGraphHorizontalPanChanged(float InPanScreenX);

	bool HasSelection() const;
	bool CanPasteStates() const;
	bool CanEnableStates() const;
	bool CanDisableStates() const;
	bool CanPasteNodesToSelectedStates() const;

	void BindCommands();

	/** 从 ViewModel 的 EditorData 刷新 MetaStoryFlow 指针并推送到 FlowGraph。 */
	void SyncFlowGraphFromEditorData();
	void HandleDebuggerRuntimeOverlayChanged();
	void DeleteFlowNode(FGuid NodeId);
	void DeleteFlowTransitionByPair(FGuid SourceNodeId, FGuid TargetNodeId);

	enum class EMetaStoryFlowToolbarAddOp : uint8
	{
		Main,
		Sibling,
		Child,
	};
	bool IsMetaStoryFlowTopologyActive() const;
	/** 在 Flow 拓扑模式下向 Flow 添加节点并 Rebuild 影子树；成功则不再走 SubTrees 的 AddState。 */
	bool TryFlowToolbarAddState(EMetaStoryFlowToolbarAddOp Op);

	TSharedPtr<FMetaStoryViewModel> MetaStoryViewModel;

	TSharedPtr<SMetaStoryFlowGraph> FlowGraph;
	TSharedPtr<SScrollBox> ViewBox;

	TWeakObjectPtr<UMetaStoryFlow> EditingFlowAsset;
	FGuid SelectedNodeId;

	TSharedPtr<FUICommandList> CommandList;

	FDelegateHandle SettingsChangedHandle;
};
