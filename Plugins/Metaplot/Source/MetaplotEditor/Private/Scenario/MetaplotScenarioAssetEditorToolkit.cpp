#include "Scenario/MetaplotScenarioAssetEditorToolkit.h"

#include "Scenario/MetaplotFlowGraphWidget.h"
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
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "MetaplotScenarioAssetEditorToolkit"

const FName FMetaplotScenarioAssetEditorToolkit::NodeListTabId(TEXT("MetaplotScenarioEditor_NodeList"));
const FName FMetaplotScenarioAssetEditorToolkit::AssetListTabId(TEXT("MetaplotScenarioEditor_AssetList"));
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
	RefreshFlowLists();

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("MetaplotScenarioEditorLayout_v2"))
		->AddArea(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.22f)
				->Split(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->AddTab(NodeListTabId, ETabState::OpenedTab))
				->Split(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->AddTab(AssetListTabId, ETabState::OpenedTab)))
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.53f)
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

	InTabManager->RegisterTabSpawner(NodeListTabId, FOnSpawnTab::CreateRaw(this, &FMetaplotScenarioAssetEditorToolkit::SpawnTab_NodeList))
		.SetDisplayName(LOCTEXT("NodeListTabLabel", "Node List"));

	InTabManager->RegisterTabSpawner(AssetListTabId, FOnSpawnTab::CreateRaw(this, &FMetaplotScenarioAssetEditorToolkit::SpawnTab_AssetList))
		.SetDisplayName(LOCTEXT("AssetListTabLabel", "Asset List"));

	InTabManager->RegisterTabSpawner(MainTabId, FOnSpawnTab::CreateRaw(this, &FMetaplotScenarioAssetEditorToolkit::SpawnTab_Main))
		.SetDisplayName(LOCTEXT("MainTabLabel", "Main"));

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateRaw(this, &FMetaplotScenarioAssetEditorToolkit::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTabLabel", "Details"));
}

void FMetaplotScenarioAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(NodeListTabId);
	InTabManager->UnregisterTabSpawner(AssetListTabId);
	InTabManager->UnregisterTabSpawner(MainTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FMetaplotScenarioAssetEditorToolkit::SpawnTab_NodeList(const FSpawnTabArgs& Args)
{
	RefreshFlowLists();

	return SNew(SDockTab)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(4.0f))
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
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &FMetaplotScenarioAssetEditorToolkit::OnAutoLayoutClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AutoLayoutButtonLabel", "自动布局"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FMetaplotScenarioAssetEditorToolkit::GetStartNodeText)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f, 6.0f, 2.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NodeSectionTitle", "节点"))
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.55f)
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
		.Padding(2.0f, 8.0f, 2.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TransitionSectionTitle", "连线"))
		]
		+ SVerticalBox::Slot()
		.FillHeight(0.45f)
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

TSharedRef<SDockTab> FMetaplotScenarioAssetEditorToolkit::SpawnTab_AssetList(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(4.0f))
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
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(2.0f, 4.0f, 2.0f, 2.0f)
		[
			SNew(SBorder)
			.Padding(8.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AssetListBodyPlaceholder", "资产列表内容区域（后续接入 SListView）"))
			]
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
		SAssignNew(FlowGraphWidget, SMetaplotFlowGraphWidget)
		.FlowAsset(TWeakObjectPtr<UMetaplotFlow>(EditingFlowAsset))
		.OnNodeSelected(FOnMetaplotGraphNodeSelected::CreateSP(this, &FMetaplotScenarioAssetEditorToolkit::OnMainGraphNodeSelected))
	];
	RefreshFlowLists();
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
	if (!EditingFlowAsset->StartNodeId.IsValid())
	{
		EditingFlowAsset->StartNodeId = NewNode.NodeId;
	}

	EditingFlowAsset->MarkPackageDirty();
	RefreshFlowLists();
	DetailsView->ForceRefresh();

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
	DetailsView->ForceRefresh();

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
	if (bAlreadyExists)
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
	DetailsView->ForceRefresh();

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
	DetailsView->ForceRefresh();

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
	DetailsView->ForceRefresh();
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
	if (FlowGraphWidget.IsValid())
	{
		FlowGraphWidget->SetSelectedNodeId(SelectedNodeId);
	}
}

void FMetaplotScenarioAssetEditorToolkit::OnMainGraphNodeSelected(FGuid NodeId)
{
	SelectedNodeId = NodeId;
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
}

void FMetaplotScenarioAssetEditorToolkit::OnTransitionSelectionChanged(TSharedPtr<int32> Item, ESelectInfo::Type /*SelectInfo*/)
{
	SelectedTransitionIndex = (Item.IsValid() ? *Item : INDEX_NONE);
}

#undef LOCTEXT_NAMESPACE
