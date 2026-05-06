// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaStoryView.h"
#include "Debugger/MetaStoryDebuggerTypes.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "SEnumCombo.h"
#include "SPositiveActionButton.h"
#include "MetaStoryViewModel.h"
#include "MetaStoryState.h"
#include "MetaStoryEditorCommands.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryFlowTopology.h"
#include "MetaStoryEditorUserSettings.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Styling/AppStyle.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailCustomization.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"

#include "Flow/MetaStoryFlow.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

namespace MetaStoryViewFlowGraphPrivate
{
	static bool IsTransitionRuleValid(const UMetaStoryFlow* Flow, const FGuid& SourceNodeId, const FGuid& TargetNodeId)
	{
		if (!Flow || !SourceNodeId.IsValid() || !TargetNodeId.IsValid() || SourceNodeId == TargetNodeId)
		{
			return false;
		}

		const FMetaStoryFlowNode* SourceNode = Flow->Nodes.FindByPredicate([SourceNodeId](const FMetaStoryFlowNode& Node)
		{
			return Node.NodeId == SourceNodeId;
		});
		const FMetaStoryFlowNode* TargetNode = Flow->Nodes.FindByPredicate([TargetNodeId](const FMetaStoryFlowNode& Node)
		{
			return Node.NodeId == TargetNodeId;
		});
		if (!SourceNode || !TargetNode)
		{
			return false;
		}

		if (TargetNode->StageIndex <= SourceNode->StageIndex)
		{
			return false;
		}

		if (SourceNode->LayerIndex == TargetNode->LayerIndex && TargetNode->StageIndex != SourceNode->StageIndex + 1)
		{
			return false;
		}

		return true;
	}

	static bool WouldCreateCycle(const UMetaStoryFlow* Flow, const FGuid& SourceNodeId, const FGuid& TargetNodeId)
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

			for (const FMetaStoryFlowTransition& Transition : Flow->Transitions)
			{
				if (Transition.SourceNodeId == Current && Transition.TargetNodeId.IsValid())
				{
					Stack.Add(Transition.TargetNodeId);
				}
			}
		}

		return false;
	}

	static bool IsValidCellForNodeMove(const UMetaStoryFlow* Flow, const FGuid& MovingNodeId, int32 NewStage, int32 NewLayer)
	{
		if (!Flow || !MovingNodeId.IsValid())
		{
			return false;
		}

		if (NewStage < 0 || NewLayer < 0)
		{
			return false;
		}

		for (const FMetaStoryFlowNode& Node : Flow->Nodes)
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
			const FMetaStoryFlowNode* Found = Flow->Nodes.FindByPredicate([NodeId](const FMetaStoryFlowNode& N)
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
			const FMetaStoryFlowNode* Found = Flow->Nodes.FindByPredicate([NodeId](const FMetaStoryFlowNode& N)
			{
				return N.NodeId == NodeId;
			});
			return Found ? Found->LayerIndex : 0;
		};

		for (const FMetaStoryFlowTransition& Tr : Flow->Transitions)
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

SMetaStoryView::SMetaStoryView() = default;

SMetaStoryView::~SMetaStoryView()
{
	if (UObjectInitialized())
	{
		GetMutableDefault<UMetaStoryEditorUserSettings>()->OnSettingsChanged.Remove(SettingsChangedHandle);

		if (MetaStoryViewModel)
		{
			MetaStoryViewModel->GetOnAssetChanged().RemoveAll(this);
			MetaStoryViewModel->GetOnStatesRemoved().RemoveAll(this);
			MetaStoryViewModel->GetOnStatesMoved().RemoveAll(this);
			MetaStoryViewModel->GetOnStateAdded().RemoveAll(this);
			MetaStoryViewModel->GetOnStatesChanged().RemoveAll(this);
			MetaStoryViewModel->GetOnSelectionChanged().RemoveAll(this);
			MetaStoryViewModel->GetOnStateNodesChanged().RemoveAll(this);
			MetaStoryViewModel->GetOnDebuggerRuntimeOverlayChanged().RemoveAll(this);
		}
	}
}

void SMetaStoryView::Construct(const FArguments& InArgs, TSharedRef<FMetaStoryViewModel> InMetaStoryViewModel, const TSharedRef<FUICommandList>& InCommandList)
{
	MetaStoryViewModel = InMetaStoryViewModel;

	MetaStoryViewModel->GetOnAssetChanged().AddSP(this, &SMetaStoryView::HandleModelAssetChanged);
	MetaStoryViewModel->GetOnStatesRemoved().AddSP(this, &SMetaStoryView::HandleModelStatesRemoved);
	MetaStoryViewModel->GetOnStatesMoved().AddSP(this, &SMetaStoryView::HandleModelStatesMoved);
	MetaStoryViewModel->GetOnStateAdded().AddSP(this, &SMetaStoryView::HandleModelStateAdded);
	MetaStoryViewModel->GetOnStatesChanged().AddSP(this, &SMetaStoryView::HandleModelStatesChanged);
	MetaStoryViewModel->GetOnSelectionChanged().AddSP(this, &SMetaStoryView::HandleModelSelectionChanged);
	MetaStoryViewModel->GetOnStateNodesChanged().AddSP(this, &SMetaStoryView::HandleModelStateNodesChanged);
	MetaStoryViewModel->GetOnDebuggerRuntimeOverlayChanged().AddSP(this, &SMetaStoryView::HandleDebuggerRuntimeOverlayChanged);

	SettingsChangedHandle = GetMutableDefault<UMetaStoryEditorUserSettings>()->OnSettingsChanged.AddSP(this, &SMetaStoryView::HandleUserSettingsChanged);

	if (const UMetaStoryEditorData* EditorData = MetaStoryViewModel->GetMetaStoryEditorData())
	{
		EditingFlowAsset = EditorData->MetaStoryFlow;
	}

	TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(12.0f, 12.0f));

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 2.0f)
				.AutoWidth()
				[
					SNew(SPositiveActionButton)
					.ToolTipText(LOCTEXT("AddStateToolTip", "Add New State"))
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
					.Text(LOCTEXT("AddState", "Add State"))
					.OnClicked(this, &SMetaStoryView::HandleAddStateButton)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					SNew(SComboButton)
					.HasDownArrow(false)
					.ContentPadding(0.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.MenuContent()
					[
						HandleGenerateSettingsMenu()
					]
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("DetailsView.ViewOptions"))
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f)
			[
				SAssignNew(ViewBox, SScrollBox)
				.Orientation(Orient_Horizontal)
				.ExternalScrollbar(HorizontalScrollBar)
				+ SScrollBox::Slot()
				.FillSize(1.0f)
				[
					SAssignNew(FlowGraph, SMetaStoryFlowGraph)
					.FlowAsset(EditingFlowAsset)
					.OnNodeSelected(FOnMetaStoryFlowGraphNodeSelected::CreateSP(this, &SMetaStoryView::OnMainGraphNodeSelected))
					.OnCreateNodeRequested(FOnMetaStoryFlowGraphCreateNodeRequested::CreateSP(this, &SMetaStoryView::OnMainGraphCreateNodeRequested))
					.OnCreateTransition(FOnMetaStoryFlowGraphCreateTransition::CreateSP(this, &SMetaStoryView::OnMainGraphCreateTransition))
					.OnMoveNode(FOnMetaStoryFlowGraphMoveNode::CreateSP(this, &SMetaStoryView::OnMainGraphMoveNode))
					.OnDeleteNodeRequested(FOnMetaStoryFlowGraphDeleteNodeRequested::CreateSP(this, &SMetaStoryView::OnMainGraphDeleteNodeRequested))
					.OnDeleteTransitionRequested(FOnMetaStoryFlowGraphDeleteTransitionRequested::CreateSP(this, &SMetaStoryView::OnMainGraphDeleteTransitionRequested))
					.OnHorizontalPanChanged(FOnMetaStoryFlowGraphHorizontalPanChanged::CreateSP(this, &SMetaStoryView::OnMainGraphHorizontalPanChanged))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				VerticalScrollBar
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			HorizontalScrollBar
		]
	];

	SyncFlowGraphFromEditorData();

	CommandList = InCommandList;
	BindCommands();
}

void SMetaStoryView::SyncFlowGraphFromEditorData()
{
	if (!MetaStoryViewModel || !FlowGraph.IsValid())
	{
		return;
	}

	if (const UMetaStoryEditorData* EditorData = MetaStoryViewModel->GetMetaStoryEditorData())
	{
		EditingFlowAsset = EditorData->MetaStoryFlow;
		FlowGraph->SetEditorData(EditorData);
	}
	else
	{
		EditingFlowAsset = nullptr;
		FlowGraph->SetEditorData(nullptr);
	}

	if (UMetaStoryFlow* Flow = EditingFlowAsset.Get())
	{
		FlowGraph->SetFlowAsset(Flow);
	}
	FlowGraph->SetViewModel(MetaStoryViewModel);
	FlowGraph->SetSelectedNodeId(SelectedNodeId);
}

void SMetaStoryView::HandleDebuggerRuntimeOverlayChanged()
{
	if (FlowGraph.IsValid())
	{
		FlowGraph->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void SMetaStoryView::BindCommands()
{
	const FMetaStoryEditorCommands& Commands = FMetaStoryEditorCommands::Get();

	CommandList->MapAction(
		Commands.AddSiblingState,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandleAddSiblingState),
		FCanExecuteAction());

	CommandList->MapAction(
		Commands.AddChildState,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandleAddChildState),
		FCanExecuteAction::CreateSP(this, &SMetaStoryView::HasSelection));

	CommandList->MapAction(
		Commands.CutStates,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandleCutSelectedStates),
		FCanExecuteAction::CreateSP(this, &SMetaStoryView::HasSelection));

	CommandList->MapAction(
		Commands.CopyStates,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandleCopySelectedStates),
		FCanExecuteAction::CreateSP(this, &SMetaStoryView::HasSelection));

	CommandList->MapAction(
		Commands.DeleteStates,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandleDeleteStates),
		FCanExecuteAction::CreateSP(this, &SMetaStoryView::HasSelection));

	CommandList->MapAction(
		Commands.PasteStatesAsSiblings,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandlePasteStatesAsSiblings),
		FCanExecuteAction::CreateSP(this, &SMetaStoryView::CanPasteStates));

	CommandList->MapAction(
		Commands.PasteStatesAsChildren,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandlePasteStatesAsChildren),
		FCanExecuteAction::CreateSP(this, &SMetaStoryView::CanPasteStates));

	CommandList->MapAction(
		Commands.PasteNodesToSelectedStates,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandlePasteNodesToState),
		FCanExecuteAction::CreateSP(this, &SMetaStoryView::CanPasteNodesToSelectedStates));

	CommandList->MapAction(
		Commands.DuplicateStates,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandleDuplicateSelectedStates),
		FCanExecuteAction::CreateSP(this, &SMetaStoryView::HasSelection));

	CommandList->MapAction(
		Commands.RenameState,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandleRenameState),
		FCanExecuteAction::CreateSP(this, &SMetaStoryView::HasSelection));

	CommandList->MapAction(
		Commands.EnableStates,
		FExecuteAction::CreateSP(this, &SMetaStoryView::HandleEnableSelectedStates),
		FCanExecuteAction(),
		FGetActionCheckState::CreateSPLambda(this, [this]
			{
				const bool bCanEnable = CanEnableStates();
				const bool bCanDisable = CanDisableStates();
				if (bCanEnable && bCanDisable)
				{
					return ECheckBoxState::Undetermined;
				}

				if (bCanDisable)
				{
					return ECheckBoxState::Checked;
				}

				if (bCanEnable)
				{
					return ECheckBoxState::Unchecked;
				}

				return ECheckBoxState::Undetermined;
			}),
		FIsActionButtonVisible::CreateSPLambda(this, [this]
		{
			return CanEnableStates() || CanDisableStates();
		}));

#if WITH_METASTORY_TRACE_DEBUGGER
	CommandList->MapAction(
		Commands.EnableOnEnterStateBreakpoint,
		FExecuteAction::CreateSPLambda(this, [this]
		{
			if (MetaStoryViewModel)
			{
				MetaStoryViewModel->HandleEnableStateBreakpoint(EMetaStoryBreakpointType::OnEnter);
			}
		}),
		FCanExecuteAction::CreateSPLambda(this, [this]
		{
			return (MetaStoryViewModel)
				&& (MetaStoryViewModel->CanAddStateBreakpoint(EMetaStoryBreakpointType::OnEnter)
					|| MetaStoryViewModel->CanRemoveStateBreakpoint(EMetaStoryBreakpointType::OnEnter));
		}),
		FGetActionCheckState::CreateSPLambda(this, [this]
		{
			return MetaStoryViewModel ? MetaStoryViewModel->GetStateBreakpointCheckState(EMetaStoryBreakpointType::OnEnter) : ECheckBoxState::Unchecked;
		}),
		FIsActionButtonVisible::CreateSPLambda(this, [this]
		{
			return MetaStoryViewModel.IsValid();
		}));

	CommandList->MapAction(
		Commands.EnableOnExitStateBreakpoint,
		FExecuteAction::CreateSPLambda(this, [this]
		{
			if (MetaStoryViewModel)
			{
				MetaStoryViewModel->HandleEnableStateBreakpoint(EMetaStoryBreakpointType::OnExit);
			}
		}),
		FCanExecuteAction::CreateSPLambda(this, [this]
		{
			return (MetaStoryViewModel)
				&& (MetaStoryViewModel->CanAddStateBreakpoint(EMetaStoryBreakpointType::OnExit)
					|| MetaStoryViewModel->CanRemoveStateBreakpoint(EMetaStoryBreakpointType::OnExit));
		}),
		FGetActionCheckState::CreateSPLambda(this, [this]
		{
			return MetaStoryViewModel ? MetaStoryViewModel->GetStateBreakpointCheckState(EMetaStoryBreakpointType::OnExit) : ECheckBoxState::Unchecked;
		}),
		FIsActionButtonVisible::CreateSPLambda(this, [this]
		{
			return MetaStoryViewModel.IsValid();
		}));
#endif
}

bool SMetaStoryView::HasSelection() const
{
	return MetaStoryViewModel && MetaStoryViewModel->HasSelection();
}

bool SMetaStoryView::CanPasteStates() const
{
	return MetaStoryViewModel
			&& MetaStoryViewModel->HasSelection()
			&& MetaStoryViewModel->CanPasteStatesFromClipboard();
}

bool SMetaStoryView::CanEnableStates() const
{
	return MetaStoryViewModel
			&& MetaStoryViewModel->HasSelection()
			&& MetaStoryViewModel->CanEnableStates();
}

bool SMetaStoryView::CanDisableStates() const
{
	return MetaStoryViewModel
			&& MetaStoryViewModel->HasSelection()
			&& MetaStoryViewModel->CanDisableStates();
}

bool SMetaStoryView::CanPasteNodesToSelectedStates() const
{
	return MetaStoryViewModel && MetaStoryViewModel->HasSelection() && MetaStoryViewModel->CanPasteNodesToSelectedStates();
}

FReply SMetaStoryView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SMetaStoryView::SavePersistentExpandedStates()
{
}

void SMetaStoryView::HandleUserSettingsChanged()
{
}

void SMetaStoryView::HandleModelAssetChanged()
{
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::HandleModelStatesRemoved(const TSet<UMetaStoryState*>& AffectedParents)
{
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::HandleModelStatesMoved(const TSet<UMetaStoryState*>& AffectedParents, const TSet<UMetaStoryState*>& MovedStates)
{
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::HandleModelStateAdded(UMetaStoryState* ParentState, UMetaStoryState* NewState)
{
	if (MetaStoryViewModel.IsValid())
	{
		MetaStoryViewModel->SetSelection(NewState);
	}
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::HandleModelStatesChanged(const TSet<UMetaStoryState*>& AffectedStates, const FPropertyChangedEvent& PropertyChangedEvent)
{
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::HandleModelStateNodesChanged(const UMetaStoryState* AffectedState)
{
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates)
{
	(void)SelectedStates;
}

TSharedRef<SWidget> SMetaStoryView::HandleGenerateSettingsMenu()
{
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.ViewIdentifier = NAME_None;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsView->RegisterInstancedCustomPropertyLayout(UMetaStoryEditorUserSettings::StaticClass(), FOnGetDetailCustomizationInstance::CreateLambda([]()
		{
			class FMetaStoryEditorUserSettingsDetailsCustomication : public IDetailCustomization
			{
			public:
				virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override
				{
					DetailLayout.HideCategory("OtherStuff");
					{
						IDetailCategoryBuilder& CategoryBuilder = DetailLayout.EditCategory("State View");
						TArray<TSharedRef<IPropertyHandle>> AllProperties;
						CategoryBuilder.GetDefaultProperties(AllProperties);

						const FName PropertyToFind = "StatesViewDisplayNodeType";
						TSharedRef<IPropertyHandle>* FoundProperty = AllProperties.FindByPredicate([PropertyToFind](TSharedRef<IPropertyHandle>& Other)
							{
								return Other->GetProperty()->GetFName() == PropertyToFind;
							});
						if (ensure(FoundProperty))
						{
							CategoryBuilder.AddProperty(*FoundProperty).CustomWidget()
							.NameContent()
							[
								(*FoundProperty)->CreatePropertyNameWidget()
							]
							.ValueContent()
							[
								SNew(SEnumComboBox, StaticEnum<EMetaStoryEditorUserSettingsNodeType>())
									.OnEnumSelectionChanged(SEnumComboBox::FOnEnumSelectionChanged::CreateLambda([](int32 NewValue, ESelectInfo::Type)
										{
											GetMutableDefault<UMetaStoryEditorUserSettings>()->SetStatesViewDisplayNodeType((EMetaStoryEditorUserSettingsNodeType)NewValue);
										}))
									.CurrentValue(MakeAttributeLambda([]() { return (int32)GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewDisplayNodeType(); }))
											.Font(IDetailLayoutBuilder::GetDetailFont())
							];
						}
					}
				}
			};
			return MakeShared<FMetaStoryEditorUserSettingsDetailsCustomication>();
		}));

	DetailsView->SetObject(GetMutableDefault<UMetaStoryEditorUserSettings>());
	return DetailsView;
}

TSharedPtr<SWidget> SMetaStoryView::HandleContextMenuOpening()
{
	if (!MetaStoryViewModel)
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.AddSubMenu(
		LOCTEXT("AddState", "Add State"),
		FText(),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InMenuBuilder)
		{
			InMenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().AddSiblingState);
			InMenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().AddChildState);
		}),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus")
	);

	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().CutStates);
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().CopyStates);

	MenuBuilder.AddSubMenu(
		LOCTEXT("Paste", "Paste"),
		FText(),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InMenuBuilder)
		{
			InMenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().PasteStatesAsSiblings);
			InMenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().PasteStatesAsChildren);
			InMenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().PasteNodesToSelectedStates);
		}),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste")
	);

	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().DuplicateStates);
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().DeleteStates);
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().RenameState);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().EnableStates);

#if WITH_METASTORY_TRACE_DEBUGGER
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().EnableOnEnterStateBreakpoint);
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().EnableOnExitStateBreakpoint);
#endif

	return MenuBuilder.MakeWidget();
}

bool SMetaStoryView::IsMetaStoryFlowTopologyActive() const
{
	if (!MetaStoryViewModel)
	{
		return false;
	}
	const UMetaStoryEditorData* EditorData = MetaStoryViewModel->GetMetaStoryEditorData();
	return EditorData && EditorData->MetaStoryFlow != nullptr;
}

bool SMetaStoryView::TryFlowToolbarAddState(EMetaStoryFlowToolbarAddOp Op)
{
	if (!IsMetaStoryFlowTopologyActive() || !MetaStoryViewModel)
	{
		return false;
	}

	UMetaStoryEditorData* EditorData = const_cast<UMetaStoryEditorData*>(MetaStoryViewModel->GetMetaStoryEditorData());
	UMetaStoryFlow* Flow = EditorData->MetaStoryFlow;
	if (!Flow)
	{
		return false;
	}

	UMetaStoryState* Sel = GetFirstSelectedState();
	const auto FindPlotNodePtr = [Flow](const FGuid& Id) -> const FMetaStoryFlowNode*
	{
		if (!Id.IsValid())
		{
			return nullptr;
		}
		for (const FMetaStoryFlowNode& N : Flow->Nodes)
		{
			if (N.NodeId == Id)
			{
				return &N;
			}
		}
		return nullptr;
	};
	const FMetaStoryFlowNode* SelPlotNode = Sel ? FindPlotNodePtr(Sel->ID) : nullptr;

	const auto IsContainerRoot = [&](const UMetaStoryState* S) -> bool
	{
		return S && EditorData->SubTrees.Num() > 0 && EditorData->SubTrees[0] == S;
	};

	bool bHaveSource = false;
	FGuid SourceNodeId;
	int32 NewStage = 0;
	int32 NewLayer = 0;

	const auto PlanAppendAfterMaxStage = [&]()
	{
		int32 MaxStage = 0;
		for (const FMetaStoryFlowNode& N : Flow->Nodes)
		{
			MaxStage = FMath::Max(MaxStage, N.StageIndex);
		}
		NewStage = MaxStage + 1;
		NewLayer = 0;
		const FMetaStoryFlowNode* SrcPick = nullptr;
		for (const FMetaStoryFlowNode& N : Flow->Nodes)
		{
			if (N.StageIndex == MaxStage && (!SrcPick || N.LayerIndex > SrcPick->LayerIndex))
			{
				SrcPick = &N;
			}
		}
		if (SrcPick)
		{
			bHaveSource = true;
			SourceNodeId = SrcPick->NodeId;
		}
		else if (Flow->StartNodeId.IsValid())
		{
			bHaveSource = true;
			SourceNodeId = Flow->StartNodeId;
		}
	};

	switch (Op)
	{
	case EMetaStoryFlowToolbarAddOp::Main:
		if (!Sel)
		{
			PlanAppendAfterMaxStage();
		}
		else if (IsContainerRoot(Sel))
		{
			const FMetaStoryFlowNode* StartN = FindPlotNodePtr(Flow->StartNodeId);
			if (StartN)
			{
				NewStage = StartN->StageIndex + 1;
				NewLayer = StartN->LayerIndex;
			}
			else
			{
				NewStage = 1;
				NewLayer = 0;
			}
			if (Flow->StartNodeId.IsValid())
			{
				bHaveSource = true;
				SourceNodeId = Flow->StartNodeId;
			}
		}
		else if (SelPlotNode)
		{
			NewStage = SelPlotNode->StageIndex + 1;
			NewLayer = SelPlotNode->LayerIndex;
			bHaveSource = true;
			SourceNodeId = SelPlotNode->NodeId;
		}
		else
		{
			return false;
		}
		break;

	case EMetaStoryFlowToolbarAddOp::Sibling:
		if (!Sel)
		{
			PlanAppendAfterMaxStage();
		}
		else if (!SelPlotNode)
		{
			return false;
		}
		else
		{
			NewStage = SelPlotNode->StageIndex;
			NewLayer = SelPlotNode->LayerIndex + 1;
			for (const FMetaStoryFlowTransition& T : Flow->Transitions)
			{
				if (T.TargetNodeId == SelPlotNode->NodeId && T.SourceNodeId.IsValid())
				{
					bHaveSource = true;
					SourceNodeId = T.SourceNodeId;
					break;
				}
			}
		}
		break;

	case EMetaStoryFlowToolbarAddOp::Child:
		if (!Sel)
		{
			return false;
		}
		if (IsContainerRoot(Sel))
		{
			const FMetaStoryFlowNode* StartN = FindPlotNodePtr(Flow->StartNodeId);
			if (StartN)
			{
				NewStage = StartN->StageIndex + 1;
				NewLayer = StartN->LayerIndex;
			}
			else
			{
				NewStage = 1;
				NewLayer = 0;
			}
			if (Flow->StartNodeId.IsValid())
			{
				bHaveSource = true;
				SourceNodeId = Flow->StartNodeId;
			}
		}
		else if (SelPlotNode)
		{
			NewStage = SelPlotNode->StageIndex + 1;
			NewLayer = SelPlotNode->LayerIndex;
			bHaveSource = true;
			SourceNodeId = SelPlotNode->NodeId;
		}
		else
		{
			return false;
		}
		break;

	default:
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("MetaStoryToolbarAddFlowState", "Add Flow State"));
	EditorData->Modify();
	Flow->Modify();

	FMetaStoryFlowNode NewNode;
	NewNode.NodeId = FGuid::NewGuid();
	NewNode.NodeName = LOCTEXT("ToolbarNewFlowNodeName", "节点");
	NewNode.Description = FText::GetEmpty();
	NewStage = FMath::Max(0, NewStage);
	NewLayer = FMath::Max(0, NewLayer);
	NewNode.StageIndex = NewStage;
	NewNode.LayerIndex = NewLayer;

	while (Flow->Nodes.ContainsByPredicate([&](const FMetaStoryFlowNode& Node)
	{
		return Node.NodeId != NewNode.NodeId && Node.StageIndex == NewNode.StageIndex && Node.LayerIndex == NewNode.LayerIndex;
	}))
	{
		++NewNode.LayerIndex;
	}

	Flow->Nodes.Add(NewNode);
	Flow->SyncNodeStatesWithNodes();

	if (bHaveSource && SourceNodeId.IsValid() && SourceNodeId != NewNode.NodeId)
	{
		const bool bDup = Flow->Transitions.ContainsByPredicate([&](const FMetaStoryFlowTransition& T)
		{
			return T.SourceNodeId == SourceNodeId && T.TargetNodeId == NewNode.NodeId;
		});
		if (!bDup
			&& MetaStoryViewFlowGraphPrivate::IsTransitionRuleValid(Flow, SourceNodeId, NewNode.NodeId)
			&& !MetaStoryViewFlowGraphPrivate::WouldCreateCycle(Flow, SourceNodeId, NewNode.NodeId))
		{
			FMetaStoryFlowTransition Tr;
			Tr.SourceNodeId = SourceNodeId;
			Tr.TargetNodeId = NewNode.NodeId;
			Flow->Transitions.Add(Tr);
		}
	}

	Flow->MarkPackageDirty();

	(void)UE::MetaStory::FlowTopology::RebuildShadowStates(*EditorData, nullptr);

	if (UMetaStoryState* NewState = MetaStoryViewModel->GetMutableStateByID(NewNode.NodeId))
	{
		MetaStoryViewModel->SetSelection(NewState);
	}

	MetaStoryViewModel->NotifyAssetChangedExternally();
	SyncFlowGraphFromEditorData();

	return true;
}

FReply SMetaStoryView::HandleAddStateButton()
{
	if (MetaStoryViewModel == nullptr)
	{
		return FReply::Handled();
	}

	if (TryFlowToolbarAddState(EMetaStoryFlowToolbarAddOp::Main))
	{
		return FReply::Handled();
	}

	if (IsMetaStoryFlowTopologyActive())
	{
		return FReply::Handled();
	}

	TArray<UMetaStoryState*> SelectedStates;
	MetaStoryViewModel->GetSelectedStates(SelectedStates);
	UMetaStoryState* FirstSelectedState = SelectedStates.Num() > 0 ? SelectedStates[0] : nullptr;

	if (FirstSelectedState != nullptr)
	{
		if (FirstSelectedState->Parent == nullptr)
		{
			MetaStoryViewModel->AddChildState(FirstSelectedState);
		}
		else
		{
			MetaStoryViewModel->AddState(FirstSelectedState);
		}
	}
	else
	{
		MetaStoryViewModel->AddState(nullptr);
	}

	return FReply::Handled();
}

UMetaStoryState* SMetaStoryView::GetFirstSelectedState() const
{
	TArray<UMetaStoryState*> SelectedStates;
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->GetSelectedStates(SelectedStates);
	}
	return SelectedStates.IsEmpty() ? nullptr : SelectedStates[0];
}

void SMetaStoryView::HandleAddSiblingState()
{
	if (!MetaStoryViewModel)
	{
		return;
	}

	if (TryFlowToolbarAddState(EMetaStoryFlowToolbarAddOp::Sibling))
	{
		return;
	}

	if (IsMetaStoryFlowTopologyActive())
	{
		return;
	}

	MetaStoryViewModel->AddState(GetFirstSelectedState());
}

void SMetaStoryView::HandleAddChildState()
{
	if (!MetaStoryViewModel)
	{
		return;
	}

	if (TryFlowToolbarAddState(EMetaStoryFlowToolbarAddOp::Child))
	{
		return;
	}

	if (IsMetaStoryFlowTopologyActive())
	{
		return;
	}

	if (UMetaStoryState* ParentState = GetFirstSelectedState())
	{
		MetaStoryViewModel->AddChildState(ParentState);
	}
}

void SMetaStoryView::HandleCutSelectedStates()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->CopySelectedStates();
		MetaStoryViewModel->RemoveSelectedStates();
	}
}

void SMetaStoryView::HandleCopySelectedStates()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->CopySelectedStates();
	}
}

void SMetaStoryView::HandlePasteStatesAsSiblings()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->PasteStatesFromClipboard(GetFirstSelectedState());
	}
}

void SMetaStoryView::HandlePasteStatesAsChildren()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->PasteStatesAsChildrenFromClipboard(GetFirstSelectedState());
	}
}

void SMetaStoryView::HandleDuplicateSelectedStates()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->DuplicateSelectedStates();
	}
}

void SMetaStoryView::HandlePasteNodesToState()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->PasteNodesToSelectedStates();
	}
}

void SMetaStoryView::HandleDeleteStates()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->RemoveSelectedStates();
	}
}

void SMetaStoryView::HandleRenameState()
{
}

void SMetaStoryView::HandleEnableSelectedStates()
{
	if (MetaStoryViewModel)
	{
		if (CanEnableStates())
		{
			MetaStoryViewModel->SetSelectedStatesEnabled(true);
		}
		else if (CanDisableStates())
		{
			MetaStoryViewModel->SetSelectedStatesEnabled(false);
		}
	}
}

void SMetaStoryView::HandleDisableSelectedStates()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->SetSelectedStatesEnabled(false);
	}
}

void SMetaStoryView::OnMainGraphNodeSelected(FGuid NodeId)
{
	SelectedNodeId = NodeId;
	if (FlowGraph.IsValid())
	{
		FlowGraph->SetSelectedNodeId(NodeId);
	}

	// 与 Outliner / UMetaStoryEditorMode 一致：通过 ViewModel 选中状态，Details 才会 SetObjects。
	if (!MetaStoryViewModel)
	{
		return;
	}

	if (!NodeId.IsValid())
	{
		MetaStoryViewModel->ClearSelection();
		return;
	}

	if (UMetaStoryState* State = MetaStoryViewModel->GetMutableStateByID(NodeId))
	{
		MetaStoryViewModel->SetSelection(State);
	}
}

void SMetaStoryView::OnMainGraphMoveNode(FGuid NodeId, int32 NewStage, int32 NewLayer)
{
	if (!EditingFlowAsset.IsValid() || !NodeId.IsValid())
	{
		return;
	}

	NewStage = FMath::Max(0, NewStage);
	NewLayer = FMath::Max(0, NewLayer);

	UMetaStoryFlow* Flow = EditingFlowAsset.Get();
	if (!MetaStoryViewFlowGraphPrivate::IsValidCellForNodeMove(Flow, NodeId, NewStage, NewLayer))
	{
		return;
	}

	const int32 NodeIndex = Flow->Nodes.IndexOfByPredicate([NodeId](const FMetaStoryFlowNode& N)
	{
		return N.NodeId == NodeId;
	});
	if (!Flow->Nodes.IsValidIndex(NodeIndex))
	{
		return;
	}

	FMetaStoryFlowNode& Node = Flow->Nodes[NodeIndex];
	if (Node.StageIndex == NewStage && Node.LayerIndex == NewLayer)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("MoveFlowNodeTransaction", "Move Flow Node"));
	Flow->Modify();

	Node.StageIndex = NewStage;
	Node.LayerIndex = NewLayer;

	Flow->MarkPackageDirty();
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::OnMainGraphCreateNodeRequested(int32 StageIndex, int32 LayerIndex)
{
	if (!EditingFlowAsset.IsValid())
	{
		return;
	}

	UMetaStoryFlow* Flow = EditingFlowAsset.Get();

	const FScopedTransaction Transaction(LOCTEXT("CreateNodeByContextMenuTransaction", "Create Flow Node From Graph Search"));
	Flow->Modify();

	FMetaStoryFlowNode NewNode;
	NewNode.NodeId = FGuid::NewGuid();
	NewNode.NodeName = LOCTEXT("ContextCreateNodeName", "节点");
	NewNode.Description = LOCTEXT("ContextCreateNodeDescription", "");
	NewNode.StageIndex = FMath::Max(0, StageIndex);
	NewNode.LayerIndex = FMath::Max(0, LayerIndex);

	while (Flow->Nodes.ContainsByPredicate([&NewNode](const FMetaStoryFlowNode& Node)
	{
		return Node.StageIndex == NewNode.StageIndex && Node.LayerIndex == NewNode.LayerIndex;
	}))
	{
		++NewNode.LayerIndex;
	}

	Flow->Nodes.Add(NewNode);
	Flow->SyncNodeStatesWithNodes();
	if (!Flow->StartNodeId.IsValid())
	{
		Flow->StartNodeId = NewNode.NodeId;
	}

	SelectedNodeId = NewNode.NodeId;
	Flow->MarkPackageDirty();
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::OnMainGraphDeleteNodeRequested(FGuid NodeId)
{
	if (!NodeId.IsValid())
	{
		return;
	}

	SelectedNodeId = NodeId;
	DeleteFlowNode(NodeId);
}

void SMetaStoryView::DeleteFlowNode(FGuid NodeId)
{
	if (!EditingFlowAsset.IsValid() || !NodeId.IsValid())
	{
		return;
	}

	UMetaStoryFlow* Flow = EditingFlowAsset.Get();
	const int32 NodeIndex = Flow->Nodes.IndexOfByPredicate([NodeId](const FMetaStoryFlowNode& Node)
	{
		return Node.NodeId == NodeId;
	});
	if (NodeIndex == INDEX_NONE)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteNodeTransaction", "Delete Flow Node"));
	Flow->Modify();

	const FGuid NodeIdToDelete = NodeId;
	Flow->Nodes.RemoveAt(NodeIndex);
	Flow->NodeStates.RemoveAll([NodeIdToDelete](const FMetaStoryFlowNodeState& State)
	{
		return State.ID == NodeIdToDelete;
	});
	Flow->Transitions.RemoveAll([NodeIdToDelete](const FMetaStoryFlowTransition& Transition)
	{
		return Transition.SourceNodeId == NodeIdToDelete || Transition.TargetNodeId == NodeIdToDelete;
	});

	if (Flow->StartNodeId == NodeIdToDelete)
	{
		Flow->StartNodeId = Flow->Nodes.IsEmpty() ? FGuid() : Flow->Nodes[0].NodeId;
	}

	if (SelectedNodeId == NodeIdToDelete)
	{
		SelectedNodeId.Invalidate();
	}

	Flow->MarkPackageDirty();
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::OnMainGraphDeleteTransitionRequested(FGuid SourceNodeId, FGuid TargetNodeId)
{
	DeleteFlowTransitionByPair(SourceNodeId, TargetNodeId);
}

void SMetaStoryView::DeleteFlowTransitionByPair(FGuid SourceNodeId, FGuid TargetNodeId)
{
	if (!EditingFlowAsset.IsValid() || !SourceNodeId.IsValid() || !TargetNodeId.IsValid())
	{
		return;
	}

	UMetaStoryFlow* Flow = EditingFlowAsset.Get();
	const int32 TransitionIndex = Flow->Transitions.IndexOfByPredicate([SourceNodeId, TargetNodeId](const FMetaStoryFlowTransition& Transition)
	{
		return Transition.SourceNodeId == SourceNodeId && Transition.TargetNodeId == TargetNodeId;
	});
	if (TransitionIndex == INDEX_NONE)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteTransitionTransaction", "Delete Flow Transition"));
	Flow->Modify();
	Flow->Transitions.RemoveAt(TransitionIndex);

	Flow->MarkPackageDirty();
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::OnMainGraphCreateTransition(FGuid SourceNodeId, FGuid TargetNodeId)
{
	if (!EditingFlowAsset.IsValid() || !SourceNodeId.IsValid() || !TargetNodeId.IsValid() || SourceNodeId == TargetNodeId)
	{
		return;
	}

	UMetaStoryFlow* Flow = EditingFlowAsset.Get();

	const bool bSourceExists = Flow->Nodes.ContainsByPredicate([SourceNodeId](const FMetaStoryFlowNode& Node)
	{
		return Node.NodeId == SourceNodeId;
	});
	const bool bTargetExists = Flow->Nodes.ContainsByPredicate([TargetNodeId](const FMetaStoryFlowNode& Node)
	{
		return Node.NodeId == TargetNodeId;
	});
	if (!bSourceExists || !bTargetExists)
	{
		return;
	}

	const bool bAlreadyExists = Flow->Transitions.ContainsByPredicate([SourceNodeId, TargetNodeId](const FMetaStoryFlowTransition& Transition)
	{
		return Transition.SourceNodeId == SourceNodeId && Transition.TargetNodeId == TargetNodeId;
	});
	if (bAlreadyExists ||
		!MetaStoryViewFlowGraphPrivate::IsTransitionRuleValid(Flow, SourceNodeId, TargetNodeId) ||
		MetaStoryViewFlowGraphPrivate::WouldCreateCycle(Flow, SourceNodeId, TargetNodeId))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateTransitionByPinTransaction", "Create Flow Transition By Pin"));
	Flow->Modify();

	FMetaStoryFlowTransition NewTransition;
	NewTransition.SourceNodeId = SourceNodeId;
	NewTransition.TargetNodeId = TargetNodeId;
	Flow->Transitions.Add(NewTransition);

	Flow->MarkPackageDirty();
	SyncFlowGraphFromEditorData();
}

void SMetaStoryView::OnMainGraphHorizontalPanChanged(float InPanScreenX)
{
	(void)InPanScreenX;
}

TSharedPtr<FMetaStoryViewModel> SMetaStoryView::GetViewModel() const
{
	return MetaStoryViewModel;
}

void SMetaStoryView::SetSelection(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates) const
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->SetSelection(SelectedStates);
	}
}

#undef LOCTEXT_NAMESPACE
