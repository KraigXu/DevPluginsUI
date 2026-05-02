// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaStoryView.h"
#include "Debugger/MetaStoryDebuggerTypes.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "SEnumCombo.h"
#include "SPositiveActionButton.h"
#include "SMetaStoryViewRow.h"
#include "MetaStoryViewModel.h"
#include "MetaStoryState.h"
#include "MetaStoryEditorCommands.h"
#include "MetaStoryEditorUserSettings.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailCustomization.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

SMetaStoryView::SMetaStoryView()
	: RequestedRenameState(nullptr)
	, bItemsDirty(false)
	, bUpdatingSelection(false)
{
}

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
		}
	}
}

void SMetaStoryView::Construct(const FArguments& InArgs, TSharedRef<FMetaStoryViewModel> InStateTreeViewModel, const TSharedRef<FUICommandList>& InCommandList)
{
	MetaStoryViewModel = InStateTreeViewModel;

	MetaStoryViewModel->GetOnAssetChanged().AddSP(this, &SMetaStoryView::HandleModelAssetChanged);
	MetaStoryViewModel->GetOnStatesRemoved().AddSP(this, &SMetaStoryView::HandleModelStatesRemoved);
	MetaStoryViewModel->GetOnStatesMoved().AddSP(this, &SMetaStoryView::HandleModelStatesMoved);
	MetaStoryViewModel->GetOnStateAdded().AddSP(this, &SMetaStoryView::HandleModelStateAdded);
	MetaStoryViewModel->GetOnStatesChanged().AddSP(this, &SMetaStoryView::HandleModelStatesChanged);
	MetaStoryViewModel->GetOnSelectionChanged().AddSP(this, &SMetaStoryView::HandleModelSelectionChanged);
	MetaStoryViewModel->GetOnStateNodesChanged().AddSP(this, &SMetaStoryView::HandleModelStateNodesChanged);

	SettingsChangedHandle = GetMutableDefault<UMetaStoryEditorUserSettings>()->OnSettingsChanged.AddSP(this, &SMetaStoryView::HandleUserSettingsChanged);

	bUpdatingSelection = false;

	TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(12.0f, 12.0f));

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	MetaStoryViewModel->GetSubTrees(Subtrees);

	TreeView = SNew(STreeView<TWeakObjectPtr<UMetaStoryState>>)
		.OnGenerateRow(this, &SMetaStoryView::HandleGenerateRow)
		.OnGetChildren(this, &SMetaStoryView::HandleGetChildren)
		.TreeItemsSource(&Subtrees)
		.OnSelectionChanged(this, &SMetaStoryView::HandleTreeSelectionChanged)
		.OnExpansionChanged(this, &SMetaStoryView::HandleTreeExpansionChanged)
		.OnContextMenuOpening(this, &SMetaStoryView::HandleContextMenuOpening)
		.AllowOverscroll(EAllowOverscroll::Yes)
		.ExternalScrollbar(VerticalScrollBar);

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

				// New State
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

		+SVerticalBox::Slot()
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
				+SScrollBox::Slot()
				.FillSize(1.0f)
				[
					TreeView.ToSharedRef()
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				VerticalScrollBar
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			HorizontalScrollBar
		]
	];

	UpdateTree(true);

	CommandList = InCommandList;
	BindCommands();
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

				// Should not happen since action is not visible in this case
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
#endif // WITH_METASTORY_TRACE_DEBUGGER
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
	if(CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	else
	{
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}
}

void SMetaStoryView::SavePersistentExpandedStates()
{
	if (!MetaStoryViewModel)
	{
		return;
	}

	TSet<TWeakObjectPtr<UMetaStoryState>> ExpandedStates;
	TreeView->GetExpandedItems(ExpandedStates);
	MetaStoryViewModel->SetPersistentExpandedStates(ExpandedStates);
}

void SMetaStoryView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bItemsDirty)
	{
		UpdateTree(/*bExpandPersistent*/true);
	}

	if (RequestedRenameState && !TreeView->IsPendingRefresh())
	{
		if (TSharedPtr<SMetaStoryViewRow> Row = StaticCastSharedPtr<SMetaStoryViewRow>(TreeView->WidgetFromItem(RequestedRenameState)))
		{
			Row->RequestRename();
		}
		RequestedRenameState = nullptr;
	}
}

void SMetaStoryView::UpdateTree(bool bExpandPersistent)
{
	if (!MetaStoryViewModel)
	{
		return;
	}

	TSet<TWeakObjectPtr<UMetaStoryState>> ExpandedStates;
	if (bExpandPersistent)
	{
		// Get expanded state from the tree data.
		MetaStoryViewModel->GetPersistentExpandedStates(ExpandedStates);
	}
	else
	{
		// Restore current expanded state.
		TreeView->GetExpandedItems(ExpandedStates);
	}

	// Remember selection
	TArray<TWeakObjectPtr<UMetaStoryState>> SelectedStates;
	MetaStoryViewModel->GetSelectedStates(SelectedStates);

	// Regenerate items
	MetaStoryViewModel->GetSubTrees(Subtrees);
	TreeView->SetTreeItemsSource(&Subtrees);

	// Restore expanded state
	for (const TWeakObjectPtr<UMetaStoryState>& State : ExpandedStates)
	{
		TreeView->SetItemExpansion(State, true);
	}

	// Restore selected state
	TreeView->ClearSelection();
	TreeView->SetItemSelection(SelectedStates, true);

	TreeView->RequestTreeRefresh();

	bItemsDirty = false;
}

void SMetaStoryView::HandleUserSettingsChanged()
{
	TreeView->RebuildList();
}

void SMetaStoryView::HandleModelAssetChanged()
{
	// this only refresh the list. i.e. each row widget will not be refreshed
	bItemsDirty = true;

	// we need to rebuild the list to update each row widget
	TreeView->RebuildList();
}

void SMetaStoryView::HandleModelStatesRemoved(const TSet<UMetaStoryState*>& AffectedParents)
{
	bItemsDirty = true;
}

void SMetaStoryView::HandleModelStatesMoved(const TSet<UMetaStoryState*>& AffectedParents, const TSet<UMetaStoryState*>& MovedStates)
{
	bItemsDirty = true;
}

void SMetaStoryView::HandleModelStateAdded(UMetaStoryState* ParentState, UMetaStoryState* NewState)
{
	bItemsDirty = true;

	// Request to rename the state immediately.
	RequestedRenameState = NewState;

	if (MetaStoryViewModel.IsValid())
	{
		MetaStoryViewModel->SetSelection(NewState);
	}
}

void SMetaStoryView::HandleModelStatesChanged(const TSet<UMetaStoryState*>& AffectedStates, const FPropertyChangedEvent& PropertyChangedEvent)
{
	// When the tasks or conditions array changed(this includes both normal array operations: Add, Remove. Clear, Move,
	// and Paste or Duplicate an element in the array), The TreeView needs to be rebuilt because new elements came in or old elements have gone or both.
	// This will not rebuild the list when we change an inner property in a condition or in a task node because of InstanceStruct wrapper
	// @todo: change it to cache and re-set the content of the widget instead of rebuilding the whole list for perf
	if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaStoryState, Tasks)
		|| PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaStoryState, EnterConditions)
		|| PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaStoryState, bHasRequiredEventToEnter)
		|| PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaStoryState, RequiredEventToEnter))
	{
		TreeView->RebuildList();
	}
}

void SMetaStoryView::HandleModelStateNodesChanged(const UMetaStoryState* AffectedState)
{
	TreeView->RebuildList();
}

void SMetaStoryView::HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates)
{
	if (bUpdatingSelection)
	{
		return;
	}

	TreeView->ClearSelection();

	if (SelectedStates.Num() > 0)
	{
		TreeView->SetItemSelection(SelectedStates, /*bSelected*/true);

		if (SelectedStates.Num() == 1)
		{
			TreeView->RequestScrollIntoView(SelectedStates[0]);	
		}
	}
}


TSharedRef<ITableRow> SMetaStoryView::HandleGenerateRow(TWeakObjectPtr<UMetaStoryState> InState, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	return SNew(SMetaStoryViewRow, InOwnerTableView, InState, ViewBox, MetaStoryViewModel.ToSharedRef());
}

void SMetaStoryView::HandleGetChildren(TWeakObjectPtr<UMetaStoryState> InParent, TArray<TWeakObjectPtr<UMetaStoryState>>& OutChildren)
{
	if (const UMetaStoryState* Parent = InParent.Get())
	{
		OutChildren.Append(Parent->Children);
	}
}

void SMetaStoryView::HandleTreeSelectionChanged(TWeakObjectPtr<UMetaStoryState> InSelectedItem, ESelectInfo::Type SelectionType)
{
	if (!MetaStoryViewModel)
	{
		return;
	}

	// Do not report code based selection changes.
	if (SelectionType == ESelectInfo::Direct)
	{
		return;
	}

	TArray<TWeakObjectPtr<UMetaStoryState>> SelectedItems = TreeView->GetSelectedItems();

	bUpdatingSelection = true;
	MetaStoryViewModel->SetSelection(SelectedItems);
	bUpdatingSelection = false;
}

void SMetaStoryView::HandleTreeExpansionChanged(TWeakObjectPtr<UMetaStoryState> InSelectedItem, bool bExpanded)
{
	// Not calling Modify() on the state as we don't want the expansion to dirty the asset.
	// @todo: this is temporary fix for a bug where adding a state will reset the expansion state. 
	if (UMetaStoryState* State = InSelectedItem.Get())
	{
		State->bExpanded = bExpanded;
	}
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
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().AddSiblingState);
			MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().AddChildState);
		}),
		/*bInOpenSubMenuOnClick =*/false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus")
	);

	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().CutStates);
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().CopyStates);

	MenuBuilder.AddSubMenu(
		LOCTEXT("Paste", "Paste"),
		FText(),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().PasteStatesAsSiblings);
			MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().PasteStatesAsChildren);
			MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().PasteNodesToSelectedStates);
		}),
		/*bInOpenSubMenuOnClick =*/false,
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
#endif // WITH_METASTORY_TRACE_DEBUGGER
	
	return MenuBuilder.MakeWidget();
}


FReply SMetaStoryView::HandleAddStateButton()
{
	if (MetaStoryViewModel == nullptr)
	{
		return FReply::Handled();
	}
	
	TArray<UMetaStoryState*> SelectedStates;
	MetaStoryViewModel->GetSelectedStates(SelectedStates);
	UMetaStoryState* FirstSelectedState = SelectedStates.Num() > 0 ? SelectedStates[0] : nullptr;

	if (FirstSelectedState != nullptr)
	{
		// If the state is root, add child state, else sibling.
		if (FirstSelectedState->Parent == nullptr)
		{
			MetaStoryViewModel->AddChildState(FirstSelectedState);
			TreeView->SetItemExpansion(FirstSelectedState, true);
		}
		else
		{
			MetaStoryViewModel->AddState(FirstSelectedState);
		}
	}
	else
	{
		// Add root state at the lowest level.
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
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->AddState(GetFirstSelectedState());
	}
}

void SMetaStoryView::HandleAddChildState()
{
	if (MetaStoryViewModel)
	{
		UMetaStoryState* ParentState = GetFirstSelectedState();
		if (ParentState)
		{
			MetaStoryViewModel->AddChildState(ParentState);
			TreeView->SetItemExpansion(ParentState, true);
		}
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
	RequestedRenameState = GetFirstSelectedState();
}

void SMetaStoryView::HandleEnableSelectedStates()
{
	if (MetaStoryViewModel)
	{
		// Process CanEnable first so in case of undetermined state (mixed selection) we Enable by default. 
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

TSharedPtr<FMetaStoryViewModel> SMetaStoryView::GetViewModel() const
{
	return MetaStoryViewModel;
}

void SMetaStoryView::SetSelection(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates) const
{
	for (const TWeakObjectPtr<UMetaStoryState>& WeakState : SelectedStates)
	{
		if (const UMetaStoryState* SelectedState = WeakState.Get())
		{
			UMetaStoryState* ParentState = SelectedState->Parent;
			while (ParentState)
			{
				constexpr bool bShouldExpandItem(true);
				TreeView->SetItemExpansion(ParentState, bShouldExpandItem);
				ParentState = ParentState->Parent;
			}
		}
	}
	MetaStoryViewModel->SetSelection(SelectedStates);
}

#undef LOCTEXT_NAMESPACE
