#include "Scenario/MetaplotScenarioAssetEditorToolkit.h"

#include "Scenario/MetaplotDetailsContext.h"
#include "Scenario/MetaplotTransitionDetailsProxy.h"
#include "Scenario/MetaplotFlowGraphWidget.h"
#include "Scenario/MetaplotFlowPlacement.h"
#include "IDetailsView.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Containers/Map.h"
#include "Misc/Guid.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Flow/MetaplotFlow.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "MetaplotScenarioAssetEditorToolkit"

namespace MetaplotScenarioEditorPrivate
{
	static bool IsTransitionRuleValid(const UMetaplotFlow* Flow, const FGuid& SourceNodeId, const FGuid& TargetNodeId)
	{
		if (!Flow || !SourceNodeId.IsValid() || !TargetNodeId.IsValid() || SourceNodeId == TargetNodeId)
		{
			return false;
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
			return false;
		}

		// 规则 3：禁止从右往左，Stage 必须严格递增。
		if (TargetNode->StageIndex <= SourceNode->StageIndex)
		{
			return false;
		}

		// 规则 2：同行仅允许连接到下一列，禁止跨列。
		if (SourceNode->LayerIndex == TargetNode->LayerIndex &&
			TargetNode->StageIndex != SourceNode->StageIndex + 1)
		{
			return false;
		}

		return true;
	}

	static bool WouldCreateCycle(const UMetaplotFlow* Flow, const FGuid& SourceNodeId, const FGuid& TargetNodeId)
	{
		if (!Flow || !SourceNodeId.IsValid() || !TargetNodeId.IsValid() || SourceNodeId == TargetNodeId)
		{
			return true;
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
}

const FName FMetaplotScenarioAssetEditorToolkit::MainTabId(TEXT("MetaplotScenarioEditor_Main"));
const FName FMetaplotScenarioAssetEditorToolkit::DetailsTabId(TEXT("MetaplotScenarioEditor_Details"));

void FMetaplotScenarioAssetEditorToolkit::InitMetaplotScenarioAssetEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UMetaplotFlow* InFlowAsset)
{
	EditingFlowAsset = InFlowAsset;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(EditingFlowAsset);
	DetailsView->OnFinishedChangingProperties().AddRaw(this, &FMetaplotScenarioAssetEditorToolkit::OnDetailsFinishedChangingProperties);
	RefreshFlowLists();

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("MetaplotScenarioEditorLayout_v3"))
		->AddArea(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.75f)
				->AddTab(MainTabId, ETabState::OpenedTab))
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.25f)
				->AddTab(DetailsTabId, ETabState::OpenedTab)));

	InitAssetEditor(Mode, InitToolkitHost, FName(TEXT("MetaplotScenarioEditorApp")), Layout, true, true, InFlowAsset);
}

FName FMetaplotScenarioAssetEditorToolkit::GetToolkitFName() const
{
	return FName(TEXT("MetaplotScenarioEditor"));
}

FText FMetaplotScenarioAssetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Metaplot Scenario Editor");
}

FString FMetaplotScenarioAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("Metaplot");
}

FLinearColor FMetaplotScenarioAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.10f, 0.18f, 0.36f, 0.5f);
}

void FMetaplotScenarioAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("MetaplotEditorWorkspace", "Metaplot Scenario Editor"));
	const TSharedRef<FWorkspaceItem> WorkspaceCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	// 维护约定：先反注册再注册，并统一 SetGroup；可避免热重载/重复初始化导致 Window 菜单出现重复项。
	InTabManager->UnregisterTabSpawner(MainTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);

	InTabManager->RegisterTabSpawner(MainTabId, FOnSpawnTab::CreateRaw(this, &FMetaplotScenarioAssetEditorToolkit::SpawnTab_Main))
		.SetDisplayName(LOCTEXT("FlowChartTabLabel", "流程图表"))
		.SetGroup(WorkspaceCategoryRef);

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateRaw(this, &FMetaplotScenarioAssetEditorToolkit::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTabLabel", "Details"))
		.SetGroup(WorkspaceCategoryRef);
}

void FMetaplotScenarioAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	// 与 RegisterTabSpawners 成对清理，避免编辑器生命周期切换后残留菜单分组/Tab 注册状态。
	InTabManager->UnregisterTabSpawner(MainTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	WorkspaceMenuCategory.Reset();
}

TSharedRef<SDockTab> FMetaplotScenarioAssetEditorToolkit::SpawnTab_AssetList(const FSpawnTabArgs& Args)
{
	RefreshFlowLists();

	return SNew(SDockTab)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f, 2.0f, 2.0f, 4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ControlBoardTitle", "控制板"))
					.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnAddAssetClicked)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AddAssetButtonLabel", "+ 添加"))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SComboButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnGetMenuContent(this, &FMetaplotScenarioAssetEditorToolkit::BuildAssetFilterMenu)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(this, &FMetaplotScenarioAssetEditorToolkit::GetActiveFilterLabel)
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SSearchBox)
						.HintText(LOCTEXT("AssetSearchHint", "搜索资产..."))
						.OnTextChanged(this, &FMetaplotScenarioAssetEditorToolkit::OnAssetSearchTextChanged)
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f, 0.0f, 2.0f, 4.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(6.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnAddNodeClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AddNodeLabel", "+ 节点"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnDeleteNodeClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DeleteNodeLabel", "删除节点"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnAddTransitionClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AddTransitionLabel", "+ 连线"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnDeleteTransitionClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DeleteTransitionLabel", "删除连线"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnAutoLayoutClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AutoLayoutButtonLabel", "自动布局"))
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(this, &FMetaplotScenarioAssetEditorToolkit::GetStartNodeText)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 2.0f, 4.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NodeSectionTitle", "节点"))
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.56f)
		.Padding(2.0f)
		[
			SAssignNew(NodeListView, SListView<TSharedPtr<FGuid>>)
			.ListItemsSource(&NodeItems)
			.OnGenerateRow(this, &FMetaplotScenarioAssetEditorToolkit::GenerateNodeRow)
			.OnSelectionChanged(this, &FMetaplotScenarioAssetEditorToolkit::OnNodeSelectionChanged)
			.SelectionMode(ESelectionMode::Single)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 4.0f, 4.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TransitionSectionTitle", "连线"))
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.44f)
		.Padding(2.0f)
		[
			SAssignNew(TransitionListView, SListView<TSharedPtr<int32>>)
			.ListItemsSource(&TransitionItems)
			.OnGenerateRow(this, &FMetaplotScenarioAssetEditorToolkit::GenerateTransitionRow)
			.OnSelectionChanged(this, &FMetaplotScenarioAssetEditorToolkit::OnTransitionSelectionChanged)
			.SelectionMode(ESelectionMode::Single)
		]
	];
}

TSharedRef<SDockTab> FMetaplotScenarioAssetEditorToolkit::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		DetailsView.ToSharedRef()
	];
}

TSharedRef<SDockTab> FMetaplotScenarioAssetEditorToolkit::SpawnTab_Main(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> Tab = SNew(SDockTab)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(30.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("NoBorder"))
				.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.5f))
				.Padding(FMargin(8.0f, 2.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnAddNodeClicked)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ToolbarAddNodeLabel", "节点"))
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnDeleteNodeClicked)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.Delete"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ToolbarDeleteNodeLabel", "删除节点"))
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnAddTransitionClicked)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ToolbarAddTransitionLabel", "连线"))
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnDeleteTransitionClicked)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.Delete"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ToolbarDeleteTransitionLabel", "删除连线"))
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnAutoLayoutClicked)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.Layout"))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ToolbarAutoLayoutLabel", "自动布局"))
							]
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SSpacer)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &FMetaplotScenarioAssetEditorToolkit::GetStartNodeText)
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(FlowGraphWidget, SMetaplotFlowGraphWidget)
			.FlowAsset(TWeakObjectPtr<UMetaplotFlow>(EditingFlowAsset))
			.OnNodeSelected(FOnMetaplotGraphNodeSelected::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::OnMainGraphNodeSelected))
			.OnCreateNodeRequested(FOnMetaplotGraphCreateNodeRequested::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::OnMainGraphCreateNodeRequested))
			.OnCreateTransition(FOnMetaplotGraphCreateTransition::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::OnMainGraphCreateTransition))
			.OnMoveNode(FOnMetaplotGraphMoveNode::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::OnMainGraphMoveNode))
			.OnDeleteNodeRequested(FOnMetaplotGraphDeleteNodeRequested::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::OnMainGraphDeleteNodeRequested))
			.OnDeleteTransitionRequested(FOnMetaplotGraphDeleteTransitionRequested::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::OnMainGraphDeleteTransitionRequested))
			.OnHorizontalPanChanged(FOnMetaplotGraphHorizontalPanChanged::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::OnMainGraphHorizontalPanChanged))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 2.0f, 4.0f, 4.0f)
		[
			SAssignNew(MainHorizontalScrollBar, SScrollBar)
			.Orientation(Orient_Horizontal)
			.OnUserScrolled(this, &FMetaplotScenarioAssetEditorToolkit::OnMainGraphHorizontalScroll)
		]
	];
	RefreshFlowLists();
	RefreshMainHorizontalScrollBar();
	return Tab;
}

TSharedRef<SWidget> FMetaplotScenarioAssetEditorToolkit::BuildAssetFilterMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("FilterAllLabel", "全部"),
		LOCTEXT("FilterAllTooltip", "显示所有资产。"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::SetAssetFilter, EMetaplotAssetFilter::All)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("FilterUsedLabel", "已使用"),
		LOCTEXT("FilterUsedTooltip", "仅显示已被当前情景使用的资产。"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::SetAssetFilter, EMetaplotAssetFilter::Used)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("FilterUnusedLabel", "未使用"),
		LOCTEXT("FilterUnusedTooltip", "仅显示尚未被当前情景使用的资产。"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::SetAssetFilter, EMetaplotAssetFilter::Unused)));

	return MenuBuilder.MakeWidget();
}

FReply FMetaplotScenarioAssetEditorToolkit::OnAddAssetClicked()
{
	// TODO: 接入资产选择器并把选择结果写入 FlowAsset。
	return FReply::Handled();
}

FReply FMetaplotScenarioAssetEditorToolkit::OnAddNodeClicked()
{
	if (!EditingFlowAsset)
	{
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("AddNodeTransaction", "Add Metaplot Node"));
	EditingFlowAsset->Modify();

	FMetaplotNode NewNode;
	NewNode.NodeId = FGuid::NewGuid();
	NewNode.NodeName = FText::Format(LOCTEXT("DefaultNodeNameFormat", "Node {0}"), FText::AsNumber(EditingFlowAsset->Nodes.Num() + 1));
	NewNode.Description = LOCTEXT("DefaultNodeDescription", "New node");
	NewNode.NodeType = EditingFlowAsset->Nodes.IsEmpty() ? EMetaplotNodeType::Start : EMetaplotNodeType::Normal;
	NewNode.StageIndex = EditingFlowAsset->Nodes.Num();
	NewNode.LayerIndex = 0;

	EditingFlowAsset->Nodes.Add(NewNode);
	EditingFlowAsset->SyncNodeStatesWithNodes();
	if (!EditingFlowAsset->StartNodeId.IsValid())
	{
		EditingFlowAsset->StartNodeId = NewNode.NodeId;
	}

	EditingFlowAsset->MarkPackageDirty();
	RefreshFlowLists();
	UpdateDetailsSelectionContext();

	return FReply::Handled();
}

FReply FMetaplotScenarioAssetEditorToolkit::OnDeleteNodeClicked()
{
	if (!EditingFlowAsset || !SelectedNodeId.IsValid())
	{
		return FReply::Handled();
	}

	const int32 NodeIndex = EditingFlowAsset->Nodes.IndexOfByPredicate([this](const FMetaplotNode& Node)
	{
		return Node.NodeId == SelectedNodeId;
	});
	if (NodeIndex == INDEX_NONE)
	{
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteNodeTransaction", "Delete Metaplot Node"));
	EditingFlowAsset->Modify();

	const FGuid NodeIdToDelete = SelectedNodeId;
	EditingFlowAsset->Nodes.RemoveAt(NodeIndex);
	EditingFlowAsset->NodeStates.RemoveAll([NodeIdToDelete](const FMetaplotNodeState& State)
	{
		return State.ID == NodeIdToDelete;
	});
	EditingFlowAsset->Transitions.RemoveAll([NodeIdToDelete](const FMetaplotTransition& Transition)
	{
		return Transition.SourceNodeId == NodeIdToDelete || Transition.TargetNodeId == NodeIdToDelete;
	});

	if (EditingFlowAsset->StartNodeId == NodeIdToDelete)
	{
		EditingFlowAsset->StartNodeId = EditingFlowAsset->Nodes.IsEmpty() ? FGuid() : EditingFlowAsset->Nodes[0].NodeId;
	}

	SelectedNodeId.Invalidate();
	EditingFlowAsset->MarkPackageDirty();
	RefreshFlowLists();
	UpdateDetailsSelectionContext();

	return FReply::Handled();
}

FReply FMetaplotScenarioAssetEditorToolkit::OnAddTransitionClicked()
{
	if (!EditingFlowAsset || EditingFlowAsset->Nodes.Num() < 2)
	{
		return FReply::Handled();
	}

	const FGuid SourceNodeId = SelectedNodeId.IsValid() ? SelectedNodeId : EditingFlowAsset->Nodes[0].NodeId;
	FGuid TargetNodeId;
	for (const FMetaplotNode& Node : EditingFlowAsset->Nodes)
	{
		if (Node.NodeId != SourceNodeId)
		{
			TargetNodeId = Node.NodeId;
			break;
		}
	}
	if (!TargetNodeId.IsValid())
	{
		return FReply::Handled();
	}

	const bool bAlreadyExists = EditingFlowAsset->Transitions.ContainsByPredicate([SourceNodeId, TargetNodeId](const FMetaplotTransition& Transition)
	{
		return Transition.SourceNodeId == SourceNodeId && Transition.TargetNodeId == TargetNodeId;
	});
	if (bAlreadyExists || !MetaplotScenarioEditorPrivate::IsTransitionRuleValid(EditingFlowAsset, SourceNodeId, TargetNodeId))
	{
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("AddTransitionTransaction", "Add Metaplot Transition"));
	EditingFlowAsset->Modify();

	FMetaplotTransition NewTransition;
	NewTransition.SourceNodeId = SourceNodeId;
	NewTransition.TargetNodeId = TargetNodeId;
	EditingFlowAsset->Transitions.Add(NewTransition);

	EditingFlowAsset->MarkPackageDirty();
	RefreshFlowLists();
	UpdateDetailsSelectionContext();

	return FReply::Handled();
}

FReply FMetaplotScenarioAssetEditorToolkit::OnDeleteTransitionClicked()
{
	if (!EditingFlowAsset || SelectedTransitionIndex == INDEX_NONE || !EditingFlowAsset->Transitions.IsValidIndex(SelectedTransitionIndex))
	{
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteTransitionTransaction", "Delete Metaplot Transition"));
	EditingFlowAsset->Modify();
	EditingFlowAsset->Transitions.RemoveAt(SelectedTransitionIndex);

	SelectedTransitionIndex = INDEX_NONE;
	EditingFlowAsset->MarkPackageDirty();
	RefreshFlowLists();
	UpdateDetailsSelectionContext();

	return FReply::Handled();
}

FReply FMetaplotScenarioAssetEditorToolkit::OnAutoLayoutClicked()
{
	if (!EditingFlowAsset || EditingFlowAsset->Nodes.IsEmpty())
	{
		return FReply::Handled();
	}

	const FScopedTransaction Transaction(LOCTEXT("AutoLayoutTransaction", "Auto Layout Metaplot Nodes"));
	EditingFlowAsset->Modify();
	AutoLayoutNodesByTimeline();
	EditingFlowAsset->MarkPackageDirty();

	RefreshFlowLists();
	UpdateDetailsSelectionContext();
	return FReply::Handled();
}

void FMetaplotScenarioAssetEditorToolkit::SetAssetFilter(const EMetaplotAssetFilter InFilter)
{
	ActiveAssetFilter = InFilter;
}

void FMetaplotScenarioAssetEditorToolkit::OnAssetSearchTextChanged(const FText& InSearchText)
{
	AssetSearchText = InSearchText;
}

FText FMetaplotScenarioAssetEditorToolkit::GetActiveFilterLabel() const
{
	switch (ActiveAssetFilter)
	{
	case EMetaplotAssetFilter::Used:
		return LOCTEXT("ActiveFilterUsed", "过滤: 已使用");
	case EMetaplotAssetFilter::Unused:
		return LOCTEXT("ActiveFilterUnused", "过滤: 未使用");
	case EMetaplotAssetFilter::All:
	default:
		return LOCTEXT("ActiveFilterAll", "过滤: 全部");
	}
}

FText FMetaplotScenarioAssetEditorToolkit::GetStartNodeText() const
{
	if (!EditingFlowAsset || !EditingFlowAsset->StartNodeId.IsValid())
	{
		return LOCTEXT("StartNodeUnset", "Start: 未设置");
	}

	const FMetaplotNode* StartNode = EditingFlowAsset->Nodes.FindByPredicate([this](const FMetaplotNode& Node)
	{
		return Node.NodeId == EditingFlowAsset->StartNodeId;
	});
	if (!StartNode)
	{
		return LOCTEXT("StartNodeInvalid", "Start: 无效节点");
	}

	const FText NameText = StartNode->NodeName.IsEmpty() ? LOCTEXT("UnnamedNode", "Unnamed") : StartNode->NodeName;
	return FText::Format(LOCTEXT("StartNodeFormat", "Start: {0}"), NameText);
}

void FMetaplotScenarioAssetEditorToolkit::AutoLayoutNodesByTimeline()
{
	if (!EditingFlowAsset)
	{
		return;
	}

	const int32 NodeCount = EditingFlowAsset->Nodes.Num();
	if (NodeCount <= 0)
	{
		return;
	}

	TMap<FGuid, int32> NodeIndexById;
	NodeIndexById.Reserve(NodeCount);
	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		NodeIndexById.Add(EditingFlowAsset->Nodes[Index].NodeId, Index);
	}

	TArray<int32> InDegree;
	TArray<int32> ComputedStage;
	TArray<TArray<int32>> OutEdges;
	InDegree.Init(0, NodeCount);
	ComputedStage.Init(0, NodeCount);
	OutEdges.SetNum(NodeCount);

	for (const FMetaplotTransition& Transition : EditingFlowAsset->Transitions)
	{
		const int32* SourceIndex = NodeIndexById.Find(Transition.SourceNodeId);
		const int32* TargetIndex = NodeIndexById.Find(Transition.TargetNodeId);
		if (!SourceIndex || !TargetIndex || *SourceIndex == *TargetIndex)
		{
			continue;
		}

		OutEdges[*SourceIndex].AddUnique(*TargetIndex);
		++InDegree[*TargetIndex];
	}

	TArray<int32> Ready;
	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		if (InDegree[Index] == 0)
		{
			Ready.Add(Index);
		}
	}

	Ready.Sort([this](int32 A, int32 B)
	{
		const FMetaplotNode& NodeA = EditingFlowAsset->Nodes[A];
		const FMetaplotNode& NodeB = EditingFlowAsset->Nodes[B];
		if (NodeA.StageIndex != NodeB.StageIndex)
		{
			return NodeA.StageIndex < NodeB.StageIndex;
		}
		if (NodeA.LayerIndex != NodeB.LayerIndex)
		{
			return NodeA.LayerIndex < NodeB.LayerIndex;
		}
		return NodeA.NodeName.ToString() < NodeB.NodeName.ToString();
	});

	TArray<int32> TopologyOrder;
	TopologyOrder.Reserve(NodeCount);
	while (!Ready.IsEmpty())
	{
		const int32 Current = Ready[0];
		Ready.RemoveAt(0, 1, EAllowShrinking::No);
		TopologyOrder.Add(Current);

		for (const int32 Next : OutEdges[Current])
		{
			ComputedStage[Next] = FMath::Max(ComputedStage[Next], ComputedStage[Current] + 1);
			--InDegree[Next];
			if (InDegree[Next] == 0)
			{
				Ready.Add(Next);
			}
		}

		Ready.Sort([this](int32 A, int32 B)
		{
			const FMetaplotNode& NodeA = EditingFlowAsset->Nodes[A];
			const FMetaplotNode& NodeB = EditingFlowAsset->Nodes[B];
			if (NodeA.StageIndex != NodeB.StageIndex)
			{
				return NodeA.StageIndex < NodeB.StageIndex;
			}
			if (NodeA.LayerIndex != NodeB.LayerIndex)
			{
				return NodeA.LayerIndex < NodeB.LayerIndex;
			}
			return NodeA.NodeName.ToString() < NodeB.NodeName.ToString();
		});
	}

	int32 MaxResolvedStage = 0;
	for (const int32 SortedIndex : TopologyOrder)
	{
		MaxResolvedStage = FMath::Max(MaxResolvedStage, ComputedStage[SortedIndex]);
	}

	if (TopologyOrder.Num() != NodeCount)
	{
		// 环/异常图在正式连线校验前先做兜底，避免布局中断。
		for (int32 Index = 0; Index < NodeCount; ++Index)
		{
			if (!TopologyOrder.Contains(Index))
			{
				ComputedStage[Index] = ++MaxResolvedStage;
				TopologyOrder.Add(Index);
			}
		}
	}

	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		EditingFlowAsset->Nodes[Index].StageIndex = ComputedStage[Index];
	}

	TMap<int32, TArray<int32>> StageBuckets;
	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		StageBuckets.FindOrAdd(ComputedStage[Index]).Add(Index);
	}

	for (TPair<int32, TArray<int32>>& Pair : StageBuckets)
	{
		TArray<int32>& Bucket = Pair.Value;
		Bucket.Sort([this](int32 A, int32 B)
		{
			const FMetaplotNode& NodeA = EditingFlowAsset->Nodes[A];
			const FMetaplotNode& NodeB = EditingFlowAsset->Nodes[B];
			if (NodeA.LayerIndex != NodeB.LayerIndex)
			{
				return NodeA.LayerIndex < NodeB.LayerIndex;
			}
			return NodeA.NodeName.ToString() < NodeB.NodeName.ToString();
		});

		for (int32 Layer = 0; Layer < Bucket.Num(); ++Layer)
		{
			EditingFlowAsset->Nodes[Bucket[Layer]].LayerIndex = Layer;
		}
	}
}

void FMetaplotScenarioAssetEditorToolkit::RefreshFlowLists()
{
	NodeItems.Reset();
	TransitionItems.Reset();

	if (EditingFlowAsset)
	{
		EditingFlowAsset->SyncNodeStatesWithNodes();
		for (const FMetaplotNode& Node : EditingFlowAsset->Nodes)
		{
			NodeItems.Add(MakeShared<FGuid>(Node.NodeId));
		}

		for (int32 TransitionIndex = 0; TransitionIndex < EditingFlowAsset->Transitions.Num(); ++TransitionIndex)
		{
			TransitionItems.Add(MakeShared<int32>(TransitionIndex));
		}
	}

	if (NodeListView.IsValid())
	{
		NodeListView->RequestListRefresh();
	}
	if (TransitionListView.IsValid())
	{
		TransitionListView->RequestListRefresh();
	}

	if (FlowGraphWidget.IsValid())
	{
		FlowGraphWidget->SetFlowAsset(EditingFlowAsset);
		FlowGraphWidget->SetSelectedNodeId(SelectedNodeId);
	}

	RefreshMainHorizontalScrollBar();
	UpdateDetailsSelectionContext();
}

TSharedRef<ITableRow> FMetaplotScenarioAssetEditorToolkit::GenerateNodeRow(TSharedPtr<FGuid> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FText Label = LOCTEXT("InvalidNodeRow", "Invalid Node");
	if (EditingFlowAsset && Item.IsValid())
	{
		const FMetaplotNode* Node = EditingFlowAsset->Nodes.FindByPredicate([Item](const FMetaplotNode& Candidate)
		{
			return Candidate.NodeId == *Item;
		});

		if (Node)
		{
			const FText NodeName = Node->NodeName.IsEmpty() ? LOCTEXT("UnnamedNodeLabel", "Unnamed") : Node->NodeName;
			Label = FText::Format(
				LOCTEXT("NodeRowFormat", "{0}  [Stage {1}, Layer {2}]"),
				NodeName,
				FText::AsNumber(Node->StageIndex),
				FText::AsNumber(Node->LayerIndex));
		}
	}

	return SNew(STableRow<TSharedPtr<FGuid>>, OwnerTable)
	[
		SNew(STextBlock).Text(Label)
	];
}

TSharedRef<ITableRow> FMetaplotScenarioAssetEditorToolkit::GenerateTransitionRow(TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FText Label = LOCTEXT("InvalidTransitionRow", "Invalid Transition");
	if (EditingFlowAsset && Item.IsValid() && EditingFlowAsset->Transitions.IsValidIndex(*Item))
	{
		const FMetaplotTransition& Transition = EditingFlowAsset->Transitions[*Item];

		auto ResolveNodeName = [this](const FGuid& NodeId) -> FText
		{
			if (!EditingFlowAsset)
			{
				return LOCTEXT("UnknownNodeName", "Unknown");
			}

			const FMetaplotNode* Node = EditingFlowAsset->Nodes.FindByPredicate([NodeId](const FMetaplotNode& Candidate)
			{
				return Candidate.NodeId == NodeId;
			});
			if (!Node)
			{
				return LOCTEXT("MissingNodeName", "Missing");
			}
			return Node->NodeName.IsEmpty() ? LOCTEXT("UnnamedNodeTransition", "Unnamed") : Node->NodeName;
		};

		Label = FText::Format(
			LOCTEXT("TransitionRowFormat", "{0} -> {1}"),
			ResolveNodeName(Transition.SourceNodeId),
			ResolveNodeName(Transition.TargetNodeId));
	}

	return SNew(STableRow<TSharedPtr<int32>>, OwnerTable)
	[
		SNew(STextBlock).Text(Label)
	];
}

void FMetaplotScenarioAssetEditorToolkit::OnNodeSelectionChanged(TSharedPtr<FGuid> Item, ESelectInfo::Type /*SelectInfo*/)
{
	SelectedNodeId = (Item.IsValid() ? *Item : FGuid());
	SelectedTransitionIndex = INDEX_NONE;
	if (FlowGraphWidget.IsValid())
	{
		FlowGraphWidget->SetSelectedNodeId(SelectedNodeId);
	}
	UpdateDetailsSelectionContext();
}

void FMetaplotScenarioAssetEditorToolkit::OnMainGraphNodeSelected(FGuid NodeId)
{
	SelectedNodeId = NodeId;
	SelectedTransitionIndex = INDEX_NONE;
	if (NodeListView.IsValid())
	{
		if (!NodeId.IsValid())
		{
			NodeListView->ClearSelection();
		}
		else
		{
			for (const TSharedPtr<FGuid>& Ptr : NodeItems)
			{
				if (Ptr.IsValid() && *Ptr == NodeId)
				{
					NodeListView->SetSelection(Ptr, ESelectInfo::OnNavigation);
					break;
				}
			}
		}
	}
	UpdateDetailsSelectionContext();
}

void FMetaplotScenarioAssetEditorToolkit::OnMainGraphMoveNode(FGuid NodeId, int32 NewStage, int32 NewLayer)
{
	if (!EditingFlowAsset || !NodeId.IsValid())
	{
		return;
	}

	NewStage = FMath::Max(0, NewStage);
	NewLayer = FMath::Max(0, NewLayer);

	if (!MetaplotFlowPlacement::IsValidCellForNodeMove(EditingFlowAsset, NodeId, NewStage, NewLayer))
	{
		return;
	}

	const int32 NodeIndex = EditingFlowAsset->Nodes.IndexOfByPredicate([NodeId](const FMetaplotNode& N)
	{
		return N.NodeId == NodeId;
	});
	if (!EditingFlowAsset->Nodes.IsValidIndex(NodeIndex))
	{
		return;
	}

	FMetaplotNode& Node = EditingFlowAsset->Nodes[NodeIndex];
	if (Node.StageIndex == NewStage && Node.LayerIndex == NewLayer)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("MoveMetaplotNodeTransaction", "Move Metaplot Node"));
	EditingFlowAsset->Modify();

	Node.StageIndex = NewStage;
	Node.LayerIndex = NewLayer;

	EditingFlowAsset->MarkPackageDirty();
	RefreshFlowLists();
	if (FlowGraphWidget.IsValid() && EditingFlowAsset)
	{
		FlowGraphWidget->SetFlowAsset(EditingFlowAsset);
	}
	if (DetailsView.IsValid())
	{
		UpdateDetailsSelectionContext();
	}
}

void FMetaplotScenarioAssetEditorToolkit::OnMainGraphCreateNodeRequested(EMetaplotNodeType NodeType, int32 StageIndex, int32 LayerIndex)
{
	if (!EditingFlowAsset)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateNodeByContextMenuTransaction", "Create Metaplot Node From Graph Search"));
	EditingFlowAsset->Modify();

	FMetaplotNode NewNode;
	NewNode.NodeId = FGuid::NewGuid();
	NewNode.NodeType = NodeType;
	const UEnum* NodeTypeEnum = StaticEnum<EMetaplotNodeType>();
	const FText NodeTypeText = NodeTypeEnum
		? NodeTypeEnum->GetDisplayNameTextByValue(static_cast<int64>(NodeType))
		: LOCTEXT("FallbackNodeTypeText", "Normal");
	NewNode.NodeName = FText::Format(LOCTEXT("ContextCreateNodeNameFormat", "{0} Node"), NodeTypeText);
	NewNode.Description = LOCTEXT("ContextCreateNodeDescription", "Created from graph search");
	NewNode.StageIndex = FMath::Max(0, StageIndex);
	NewNode.LayerIndex = FMath::Max(0, LayerIndex);

	// 避免同格冲突，向下寻找第一个空 Layer。
	while (EditingFlowAsset->Nodes.ContainsByPredicate([&NewNode](const FMetaplotNode& Node)
	{
		return Node.StageIndex == NewNode.StageIndex && Node.LayerIndex == NewNode.LayerIndex;
	}))
	{
		++NewNode.LayerIndex;
	}

	EditingFlowAsset->Nodes.Add(NewNode);
	EditingFlowAsset->SyncNodeStatesWithNodes();
	if (NodeType == EMetaplotNodeType::Start || !EditingFlowAsset->StartNodeId.IsValid())
	{
		EditingFlowAsset->StartNodeId = NewNode.NodeId;
	}

	SelectedNodeId = NewNode.NodeId;
	EditingFlowAsset->MarkPackageDirty();
	RefreshFlowLists();
	if (DetailsView.IsValid())
	{
		UpdateDetailsSelectionContext();
	}
}

void FMetaplotScenarioAssetEditorToolkit::OnMainGraphDeleteNodeRequested(FGuid NodeId)
{
	if (!NodeId.IsValid())
	{
		return;
	}

	SelectedNodeId = NodeId;
	OnDeleteNodeClicked();
}

void FMetaplotScenarioAssetEditorToolkit::OnMainGraphDeleteTransitionRequested(FGuid SourceNodeId, FGuid TargetNodeId)
{
	if (!EditingFlowAsset || !SourceNodeId.IsValid() || !TargetNodeId.IsValid())
	{
		return;
	}

	const int32 TransitionIndex = EditingFlowAsset->Transitions.IndexOfByPredicate([SourceNodeId, TargetNodeId](const FMetaplotTransition& Transition)
	{
		return Transition.SourceNodeId == SourceNodeId && Transition.TargetNodeId == TargetNodeId;
	});
	if (TransitionIndex == INDEX_NONE)
	{
		return;
	}

	SelectedTransitionIndex = TransitionIndex;
	OnDeleteTransitionClicked();
}

void FMetaplotScenarioAssetEditorToolkit::OnMainGraphCreateTransition(FGuid SourceNodeId, FGuid TargetNodeId)
{
	if (!EditingFlowAsset || !SourceNodeId.IsValid() || !TargetNodeId.IsValid() || SourceNodeId == TargetNodeId)
	{
		return;
	}

	const bool bSourceExists = EditingFlowAsset->Nodes.ContainsByPredicate([SourceNodeId](const FMetaplotNode& Node)
	{
		return Node.NodeId == SourceNodeId;
	});
	const bool bTargetExists = EditingFlowAsset->Nodes.ContainsByPredicate([TargetNodeId](const FMetaplotNode& Node)
	{
		return Node.NodeId == TargetNodeId;
	});
	if (!bSourceExists || !bTargetExists)
	{
		return;
	}

	const bool bAlreadyExists = EditingFlowAsset->Transitions.ContainsByPredicate([SourceNodeId, TargetNodeId](const FMetaplotTransition& Transition)
	{
		return Transition.SourceNodeId == SourceNodeId && Transition.TargetNodeId == TargetNodeId;
	});
	if (bAlreadyExists ||
		!MetaplotScenarioEditorPrivate::IsTransitionRuleValid(EditingFlowAsset, SourceNodeId, TargetNodeId) ||
		MetaplotScenarioEditorPrivate::WouldCreateCycle(EditingFlowAsset, SourceNodeId, TargetNodeId))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateTransitionByPinTransaction", "Create Metaplot Transition By Pin"));
	EditingFlowAsset->Modify();

	FMetaplotTransition NewTransition;
	NewTransition.SourceNodeId = SourceNodeId;
	NewTransition.TargetNodeId = TargetNodeId;
	EditingFlowAsset->Transitions.Add(NewTransition);

	EditingFlowAsset->MarkPackageDirty();
	RefreshFlowLists();
	UpdateDetailsSelectionContext();
}

void FMetaplotScenarioAssetEditorToolkit::OnMainGraphHorizontalPanChanged(float InPanScreenX)
{
	(void)InPanScreenX;
	RefreshMainHorizontalScrollBar();
}

void FMetaplotScenarioAssetEditorToolkit::OnTransitionSelectionChanged(TSharedPtr<int32> Item, ESelectInfo::Type /*SelectInfo*/)
{
	SelectedTransitionIndex = (Item.IsValid() ? *Item : INDEX_NONE);
	if (SelectedTransitionIndex != INDEX_NONE)
	{
		SelectedNodeId.Invalidate();
		if (FlowGraphWidget.IsValid())
		{
			FlowGraphWidget->SetSelectedNodeId(FGuid());
		}
		if (NodeListView.IsValid())
		{
			NodeListView->ClearSelection();
		}
	}
	UpdateDetailsSelectionContext();
}

void FMetaplotScenarioAssetEditorToolkit::UpdateDetailsSelectionContext()
{
	if (!DetailsView.IsValid())
	{
		return;
	}

	if (!DetailsContext.IsValid())
	{
		DetailsContext.Reset(NewObject<UMetaplotDetailsContext>(GetTransientPackage()));
	}
	DetailsContext->Initialize(EditingFlowAsset, SelectedNodeId);

	if (EditingFlowAsset && SelectedNodeId.IsValid())
	{
		DetailsView->SetObject(EditingFlowAsset);
		// 选中节点时主对象仍是同一 Flow，SetObject 可能不会触发重建；必须强制刷新才能让
		// MetaplotDetailsContext 的 SelectedNodeId 参与 CustomizeDetails 重新计算。
		DetailsView->ForceRefresh();
		return;
	}

	if (EditingFlowAsset && SelectedTransitionIndex != INDEX_NONE && EditingFlowAsset->Transitions.IsValidIndex(SelectedTransitionIndex))
	{
		const FMetaplotTransition& Transition = EditingFlowAsset->Transitions[SelectedTransitionIndex];
		if (!TransitionDetailsProxy.IsValid())
		{
			TransitionDetailsProxy.Reset(NewObject<UMetaplotTransitionDetailsProxy>(GetTransientPackage()));
		}
		TransitionDetailsProxy->Initialize(EditingFlowAsset, Transition.SourceNodeId, Transition.TargetNodeId);
		TransitionDetailsProxy->SetDetailsContext(DetailsContext.Get());
		DetailsView->SetObject(TransitionDetailsProxy.Get());
		return;
	}

	DetailsView->SetObject(EditingFlowAsset);
	DetailsView->ForceRefresh();
}

void FMetaplotScenarioAssetEditorToolkit::OnDetailsFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	(void)PropertyChangedEvent;
	RefreshFlowLists();
	UpdateDetailsSelectionContext();
}

void FMetaplotScenarioAssetEditorToolkit::OnMainGraphHorizontalScroll(float ScrollOffsetFraction)
{
	if (bSyncingHorizontalScrollBar || !FlowGraphWidget.IsValid())
	{
		return;
	}

	float BoundMinX = 0.0f;
	float BoundMaxX = 0.0f;
	float OffsetFraction = 0.0f;
	float ThumbFraction = 1.0f;

	const bool bCanScroll = FlowGraphWidget->GetHorizontalScrollbarState(OffsetFraction, ThumbFraction);
	if (!bCanScroll)
	{
		return;
	}

	// 将滚动条归一化位置映射回画布 PanScreen.X。
	UMetaplotFlow* Flow = EditingFlowAsset.Get();
	if (!Flow)
	{
		return;
	}

	// 与 SMetaplotFlowGraphWidget::GetContentBounds 的默认范围保持一致。
	BoundMinX = 0.0f;
	BoundMaxX = 3.0f * 220.0f;
	if (!Flow->Nodes.IsEmpty())
	{
		int32 MinStage = Flow->Nodes[0].StageIndex;
		int32 MaxStage = Flow->Nodes[0].StageIndex;
		for (const FMetaplotNode& Node : Flow->Nodes)
		{
			MinStage = FMath::Min(MinStage, Node.StageIndex);
			MaxStage = FMath::Max(MaxStage, Node.StageIndex);
		}
		MinStage = FMath::Min(MinStage, 0);
		MaxStage = FMath::Max(MaxStage, MinStage + 2);
		BoundMinX = MinStage * 220.0f;
		BoundMaxX = (MaxStage + 1) * 220.0f;
	}

	const float ContentWidth = FMath::Max(1.0f, BoundMaxX - BoundMinX);
	const float ViewWidth = ContentWidth * FMath::Clamp(ThumbFraction, 0.0f, 1.0f);
	const float MaxOffset = FMath::Max(0.0f, ContentWidth - ViewWidth);
	const float TargetOffset = FMath::Clamp(ScrollOffsetFraction, 0.0f, 1.0f) * MaxOffset;
	const float TargetPanX = -BoundMinX - TargetOffset;
	FlowGraphWidget->SetHorizontalPanScreen(TargetPanX);
}

void FMetaplotScenarioAssetEditorToolkit::RefreshMainHorizontalScrollBar()
{
	if (!MainHorizontalScrollBar.IsValid() || !FlowGraphWidget.IsValid())
	{
		return;
	}

	float OffsetFraction = 0.0f;
	float ThumbFraction = 1.0f;
	FlowGraphWidget->GetHorizontalScrollbarState(OffsetFraction, ThumbFraction);

	TGuardValue<bool> SyncGuard(bSyncingHorizontalScrollBar, true);
	MainHorizontalScrollBar->SetState(OffsetFraction, ThumbFraction);
}

int32 FMetaplotScenarioAssetEditorToolkit::GetTaskCountForNode(const FGuid& NodeId) const
{
	if (!EditingFlowAsset || !NodeId.IsValid())
	{
		return 0;
	}

	const FMetaplotNodeState* TaskSet = EditingFlowAsset->NodeStates.FindByPredicate([NodeId](const FMetaplotNodeState& Entry)
	{
		return Entry.ID == NodeId;
	});
	return TaskSet ? TaskSet->Tasks.Num() : 0;
}

#undef LOCTEXT_NAMESPACE
