#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Views/STableViewBase.h"

class IDetailsView;
class SDockTab;
template <typename ItemType> class SListView;
class ITableRow;
class UMetaplotFlow;
class UMetaplotNodeDetailsProxy;
class UMetaplotTransitionDetailsProxy;
class SMetaplotFlowGraphWidget;
class SScrollBar;
class FWorkspaceItem;
struct FPropertyChangedEvent;
enum class EMetaplotNodeType : uint8;

enum class EMetaplotAssetFilter : uint8
{
	All,
	Used,
	Unused
};

class FMetaplotScenarioAssetEditorToolkit : public FAssetEditorToolkit
{
public:
	void InitMetaplotScenarioAssetEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UMetaplotFlow* InFlowAsset);

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;

private:
	TSharedRef<SDockTab> SpawnTab_AssetList(const class FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const class FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_NodeDetails(const class FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Main(const class FSpawnTabArgs& Args);
	TSharedRef<class SWidget> BuildAssetFilterMenu();
	FReply OnAddAssetClicked();
	FReply OnAddNodeClicked();
	FReply OnDeleteNodeClicked();
	FReply OnAddTransitionClicked();
	FReply OnDeleteTransitionClicked();
	FReply OnAutoLayoutClicked();
	void SetAssetFilter(EMetaplotAssetFilter InFilter);
	void OnAssetSearchTextChanged(const FText& InSearchText);
	FText GetActiveFilterLabel() const;
	FText GetStartNodeText() const;
	void RefreshFlowLists();
	void EnsureTaskSetForNode(const FGuid& NodeId);
	void RemoveTaskSetForNode(const FGuid& NodeId);
	int32 GetTaskCountForNode(const FGuid& NodeId) const;
	void AutoLayoutNodesByTimeline();
	TSharedRef<ITableRow> GenerateNodeRow(TSharedPtr<FGuid> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> GenerateTransitionRow(TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnNodeSelectionChanged(TSharedPtr<FGuid> Item, ESelectInfo::Type SelectInfo);
	void OnTransitionSelectionChanged(TSharedPtr<int32> Item, ESelectInfo::Type SelectInfo);
	void OnMainGraphNodeSelected(FGuid NodeId);
	void OnMainGraphCreateNodeRequested(EMetaplotNodeType NodeType, int32 StageIndex, int32 LayerIndex);
	void OnMainGraphCreateTransition(FGuid SourceNodeId, FGuid TargetNodeId);
	void OnMainGraphMoveNode(FGuid NodeId, int32 NewStage, int32 NewLayer);
	void OnMainGraphDeleteNodeRequested(FGuid NodeId);
	void OnMainGraphDeleteTransitionRequested(FGuid SourceNodeId, FGuid TargetNodeId);
	void OnMainGraphHorizontalPanChanged(float InPanScreenX);
	void OnMainGraphHorizontalScroll(float ScrollOffsetFraction);
	void RefreshMainHorizontalScrollBar();
	void UpdateDetailsSelectionContext();
	void OnDetailsFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);
	FText GetNodeDetailsTaskSetHintText() const;
	FReply OnFocusSelectedNodeTaskSetClicked();

private:
	TObjectPtr<UMetaplotFlow> EditingFlowAsset = nullptr;
	TSharedPtr<IDetailsView> DetailsView;
	TArray<TSharedPtr<FGuid>> NodeItems;
	TArray<TSharedPtr<int32>> TransitionItems;
	TSharedPtr<SListView<TSharedPtr<FGuid>>> NodeListView;
	TSharedPtr<SListView<TSharedPtr<int32>>> TransitionListView;
	TSharedPtr<SMetaplotFlowGraphWidget> FlowGraphWidget;
	TSharedPtr<SScrollBar> MainHorizontalScrollBar;
	FGuid SelectedNodeId;
	int32 SelectedTransitionIndex = INDEX_NONE;
	TStrongObjectPtr<UMetaplotNodeDetailsProxy> NodeDetailsProxy;
	TStrongObjectPtr<UMetaplotTransitionDetailsProxy> TransitionDetailsProxy;
	EMetaplotAssetFilter ActiveAssetFilter = EMetaplotAssetFilter::All;
	FText AssetSearchText;
	bool bSyncingHorizontalScrollBar = false;
	TSharedPtr<FWorkspaceItem> WorkspaceMenuCategory;

	static const FName MainTabId;
	static const FName DetailsTabId;
	static const FName NodeDetailsTabId;
};
