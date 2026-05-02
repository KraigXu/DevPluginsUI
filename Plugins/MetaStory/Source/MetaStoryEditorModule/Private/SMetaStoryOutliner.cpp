// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaStoryOutliner.h"

#include "MetaStoryDelegates.h"
#include "MetaStoryEditorCommands.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryState.h"
#include "MetaStoryViewModel.h"
#include "Customizations/Widgets/SMetaStoryCompactTreeEditorView.h"
#include "Debugger/MetaStoryDebuggerTypes.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

SMetaStoryOutliner::SMetaStoryOutliner()
{
}

SMetaStoryOutliner::~SMetaStoryOutliner()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->GetOnAssetChanged().RemoveAll(this);
		MetaStoryViewModel->GetOnStatesRemoved().RemoveAll(this);
		MetaStoryViewModel->GetOnStatesMoved().RemoveAll(this);
		MetaStoryViewModel->GetOnStateAdded().RemoveAll(this);
		MetaStoryViewModel->GetOnStatesChanged().RemoveAll(this);
		MetaStoryViewModel->GetOnSelectionChanged().RemoveAll(this);
	}
	UE::MetaStory::Delegates::OnVisualThemeChanged.RemoveAll(this);
}

void SMetaStoryOutliner::Construct(const FArguments& InArgs, TSharedRef<FMetaStoryViewModel> InStateTreeViewModel, const TSharedRef<FUICommandList>& InCommandList)
{
	MetaStoryViewModel = InStateTreeViewModel;

	MetaStoryViewModel->GetOnAssetChanged().AddSP(this, &SMetaStoryOutliner::HandleModelAssetChanged);
	MetaStoryViewModel->GetOnStatesRemoved().AddSP(this, &SMetaStoryOutliner::HandleModelStatesRemoved);
	MetaStoryViewModel->GetOnStatesMoved().AddSP(this, &SMetaStoryOutliner::HandleModelStatesMoved);
	MetaStoryViewModel->GetOnStateAdded().AddSP(this, &SMetaStoryOutliner::HandleModelStateAdded);
	MetaStoryViewModel->GetOnStatesChanged().AddSP(this, &SMetaStoryOutliner::HandleModelStatesChanged);
	MetaStoryViewModel->GetOnSelectionChanged().AddSP(this, &SMetaStoryOutliner::HandleModelSelectionChanged);

	UE::MetaStory::Delegates::OnVisualThemeChanged.AddSP(this, &SMetaStoryOutliner::HandleVisualThemeChanged);
	
	bUpdatingSelection = false;

	ChildSlot
	[
		SAssignNew(CompactTreeView, UE::MetaStory::SCompactTreeEditorView, MetaStoryViewModel)
		.SelectionMode(ESelectionMode::Multi)
		.MetaStoryEditorData(MetaStoryViewModel->GetStateTreeEditorData())
		.OnSelectionChanged(this, &SMetaStoryOutliner::HandleTreeViewSelectionChanged)
		.OnContextMenuOpening(this, &SMetaStoryOutliner::HandleContextMenuOpening)
		.ShowLinkedStates(true)
	];
	
	CommandList = InCommandList;
	BindCommands();
}

void SMetaStoryOutliner::BindCommands()
{
	const FMetaStoryEditorCommands& Commands = FMetaStoryEditorCommands::Get();

	CommandList->MapAction(
		Commands.AddSiblingState,
		FExecuteAction::CreateSP(this, &SMetaStoryOutliner::HandleAddSiblingState),
		FCanExecuteAction());

	CommandList->MapAction(
		Commands.AddChildState,
		FExecuteAction::CreateSP(this, &SMetaStoryOutliner::HandleAddChildState),
		FCanExecuteAction::CreateSP(this, &SMetaStoryOutliner::HasSelection));

	CommandList->MapAction(
		Commands.CutStates,
		FExecuteAction::CreateSP(this, &SMetaStoryOutliner::HandleCutSelectedStates),
		FCanExecuteAction::CreateSP(this, &SMetaStoryOutliner::HasSelection));

	CommandList->MapAction(
		Commands.CopyStates,
		FExecuteAction::CreateSP(this, &SMetaStoryOutliner::HandleCopySelectedStates),
		FCanExecuteAction::CreateSP(this, &SMetaStoryOutliner::HasSelection));

	CommandList->MapAction(
		Commands.DeleteStates,
		FExecuteAction::CreateSP(this, &SMetaStoryOutliner::HandleDeleteStates),
		FCanExecuteAction::CreateSP(this, &SMetaStoryOutliner::HasSelection));

	CommandList->MapAction(
		Commands.PasteStatesAsSiblings,
		FExecuteAction::CreateSP(this, &SMetaStoryOutliner::HandlePasteStatesAsSiblings),
		FCanExecuteAction::CreateSP(this, &SMetaStoryOutliner::CanPaste));

	CommandList->MapAction(
		Commands.PasteStatesAsChildren,
		FExecuteAction::CreateSP(this, &SMetaStoryOutliner::HandlePasteStatesAsChildren),
		FCanExecuteAction::CreateSP(this, &SMetaStoryOutliner::CanPaste));

	CommandList->MapAction(
		Commands.DuplicateStates,
		FExecuteAction::CreateSP(this, &SMetaStoryOutliner::HandleDuplicateSelectedStates),
		FCanExecuteAction::CreateSP(this, &SMetaStoryOutliner::HasSelection));

	CommandList->MapAction(
		Commands.EnableStates,
		FExecuteAction::CreateSP(this, &SMetaStoryOutliner::HandleEnableSelectedStates),
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
		FCanExecuteAction(),
		FGetActionCheckState::CreateSPLambda(this, [this]
		{
			return MetaStoryViewModel ? MetaStoryViewModel->GetStateBreakpointCheckState(EMetaStoryBreakpointType::OnEnter) : ECheckBoxState::Unchecked;
		}),
		FIsActionButtonVisible::CreateSPLambda(this, [this]
		{
			return (MetaStoryViewModel)
				&& (MetaStoryViewModel->CanAddStateBreakpoint(EMetaStoryBreakpointType::OnEnter)
					|| MetaStoryViewModel->CanRemoveStateBreakpoint(EMetaStoryBreakpointType::OnEnter));
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
		FCanExecuteAction(),
		FGetActionCheckState::CreateSPLambda(this, [this]
		{
			return MetaStoryViewModel ? MetaStoryViewModel->GetStateBreakpointCheckState(EMetaStoryBreakpointType::OnExit) : ECheckBoxState::Unchecked;
		}),
		FIsActionButtonVisible::CreateSPLambda(this, [this]
		{
			return (MetaStoryViewModel)
				&& (MetaStoryViewModel->CanAddStateBreakpoint(EMetaStoryBreakpointType::OnExit)
					|| MetaStoryViewModel->CanRemoveStateBreakpoint(EMetaStoryBreakpointType::OnExit));
		}));
#endif // WITH_METASTORY_TRACE_DEBUGGER
}


void SMetaStoryOutliner::HandleModelAssetChanged()
{
	bItemsDirty = true;

	if (CompactTreeView && MetaStoryViewModel)
	{
		CompactTreeView->Refresh(MetaStoryViewModel->GetStateTreeEditorData());
	}
}

void SMetaStoryOutliner::HandleModelStatesRemoved(const TSet<UMetaStoryState*>& AffectedParents)
{
	bItemsDirty = true;
	
	if (CompactTreeView && MetaStoryViewModel)
	{
		CompactTreeView->Refresh(MetaStoryViewModel->GetStateTreeEditorData());
	}
}

void SMetaStoryOutliner::HandleModelStatesMoved(const TSet<UMetaStoryState*>& AffectedParents, const TSet<UMetaStoryState*>& MovedStates)
{
	bItemsDirty = true;
	
	if (CompactTreeView && MetaStoryViewModel)
	{
		CompactTreeView->Refresh(MetaStoryViewModel->GetStateTreeEditorData());
	}
}

void SMetaStoryOutliner::HandleModelStateAdded(UMetaStoryState* ParentState, UMetaStoryState* NewState)
{
	bItemsDirty = true;
	
	if (CompactTreeView && MetaStoryViewModel)
	{
		CompactTreeView->Refresh(MetaStoryViewModel->GetStateTreeEditorData());
	}
}

void SMetaStoryOutliner::HandleModelStatesChanged(const TSet<UMetaStoryState*>& AffectedStates, const FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bArraysChanged = false;

	// The purpose of the rebuild below is to update the task visualization (number of widgets change).
	// This method is called when anything in a state changes, make sure to only rebuild when needed.
	if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaStoryState, Tasks))
	{
		bArraysChanged = true;
	}
		
	if (bArraysChanged)
	{
	}

	if (CompactTreeView && MetaStoryViewModel)
	{
		CompactTreeView->Refresh(MetaStoryViewModel->GetStateTreeEditorData());
	}
}

void SMetaStoryOutliner::HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates)
{
	if (bUpdatingSelection)
	{
		return;
	}

	if (CompactTreeView && MetaStoryViewModel)
	{
		TArray<FGuid> StateIDs;
		for (const TWeakObjectPtr<UMetaStoryState>& WeakState : SelectedStates)
		{
			if (const UMetaStoryState* State = WeakState.Get())
			{
				StateIDs.Add(State->ID);
			}
		}
	
		CompactTreeView->SetSelection(StateIDs);
	}
}

void SMetaStoryOutliner::HandleTreeViewSelectionChanged(TConstArrayView<FGuid> SelectedStateIDs)
{
	if (MetaStoryViewModel)
	{
		TArray<TWeakObjectPtr<UMetaStoryState>> Selection;

		if (const UMetaStoryEditorData* MetaStoryEditorData = MetaStoryViewModel->GetStateTreeEditorData())
		{
			for (const FGuid& StateID : SelectedStateIDs)
			{
				if (const UMetaStoryState* State = MetaStoryEditorData->GetStateByID(StateID))
				{
					Selection.Add(const_cast<UMetaStoryState*>(State));
				}
			}
		}
		
		MetaStoryViewModel->SetSelection(Selection);
	}
}

void SMetaStoryOutliner::HandleVisualThemeChanged(const UMetaStory& MetaStory)
{
	if (MetaStoryViewModel
		&& MetaStoryViewModel->GetStateTree() == &MetaStory)
	{
		CompactTreeView->Refresh(MetaStoryViewModel->GetStateTreeEditorData());
	}
}

FReply SMetaStoryOutliner::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
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

TSharedPtr<SWidget> SMetaStoryOutliner::HandleContextMenuOpening()
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
		}),
		/*bInOpenSubMenuOnClick =*/false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste")
	);
	
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().DuplicateStates);
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().DeleteStates);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().EnableStates);

#if WITH_METASTORY_TRACE_DEBUGGER
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().EnableOnEnterStateBreakpoint);
	MenuBuilder.AddMenuEntry(FMetaStoryEditorCommands::Get().EnableOnExitStateBreakpoint);
#endif // WITH_METASTORY_TRACE_DEBUGGER
	
	return MenuBuilder.MakeWidget();
}

UMetaStoryState* SMetaStoryOutliner::GetFirstSelectedState() const
{
	TArray<UMetaStoryState*> SelectedStates;
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->GetSelectedStates(SelectedStates);
	}
	return SelectedStates.IsEmpty() ? nullptr : SelectedStates[0];
}

void SMetaStoryOutliner::HandleAddSiblingState()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->AddState(GetFirstSelectedState());
	}
}

void SMetaStoryOutliner::HandleAddChildState()
{
	if (MetaStoryViewModel)
	{
		UMetaStoryState* ParentState = GetFirstSelectedState();
		if (ParentState)
		{
			MetaStoryViewModel->AddChildState(ParentState);
		}
	}
}

void SMetaStoryOutliner::HandleCutSelectedStates()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->CopySelectedStates();
		MetaStoryViewModel->RemoveSelectedStates();
	}
}

void SMetaStoryOutliner::HandleCopySelectedStates()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->CopySelectedStates();
	}
}

void SMetaStoryOutliner::HandlePasteStatesAsSiblings()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->PasteStatesFromClipboard(GetFirstSelectedState());
	}
}

void SMetaStoryOutliner::HandlePasteStatesAsChildren()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->PasteStatesAsChildrenFromClipboard(GetFirstSelectedState());
	}
}

void SMetaStoryOutliner::HandleDuplicateSelectedStates()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->DuplicateSelectedStates();
	}
}

void SMetaStoryOutliner::HandleDeleteStates()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->RemoveSelectedStates();
	}
}

void SMetaStoryOutliner::HandleEnableSelectedStates()
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

void SMetaStoryOutliner::HandleDisableSelectedStates()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->SetSelectedStatesEnabled(false);
	}
}

bool SMetaStoryOutliner::HasSelection() const
{
	return MetaStoryViewModel && MetaStoryViewModel->HasSelection();
}

bool SMetaStoryOutliner::CanPaste() const
{
	return MetaStoryViewModel
			&& MetaStoryViewModel->HasSelection()
			&& MetaStoryViewModel->CanPasteStatesFromClipboard();
}

bool SMetaStoryOutliner::CanEnableStates() const
{
	return MetaStoryViewModel
			&& MetaStoryViewModel->HasSelection()
			&& MetaStoryViewModel->CanEnableStates();
}

bool SMetaStoryOutliner::CanDisableStates() const
{
	return MetaStoryViewModel
			&& MetaStoryViewModel->HasSelection()
			&& MetaStoryViewModel->CanDisableStates();
}


#undef LOCTEXT_NAMESPACE
