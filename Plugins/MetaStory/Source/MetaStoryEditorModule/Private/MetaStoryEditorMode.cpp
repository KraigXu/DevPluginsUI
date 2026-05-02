// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorMode.h"

#include "ContextObjectStore.h"
#include "EditorModeManager.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"

#include "MetaStoryEditorModeToolkit.h"
#include "MetaStoryEditorSettings.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryDelegates.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "IMessageLogListing.h"
#include "InteractiveToolManager.h"
#include "PropertyPath.h"
#include "MetaStoryEditorCommands.h"
#include "Misc/UObjectToken.h"
#include "Toolkits/ToolkitManager.h"
#include "Modules/ModuleManager.h"

#include "IMetaStoryEditorHost.h"
#include "MessageLogModule.h"
#include "MetaStoryEditingSubsystem.h"
#include "Customizations/MetaStoryBindingExtension.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEditorMode)

#define LOCTEXT_NAMESPACE "UMetaStoryEditorMode"

class FUObjectToken;
const FEditorModeID UMetaStoryEditorMode::EM_StateTree("MetaStoryEditorMode");

UMetaStoryEditorMode::UMetaStoryEditorMode()
{
	Info = FEditorModeInfo(UMetaStoryEditorMode::EM_StateTree,
		LOCTEXT("MetaStoryEditorModeName", "MetaStoryEditorMode"),
		FSlateIcon(),
		false);
}

void UMetaStoryEditorMode::Enter()
{
	Super::Enter();
	
	DetailsViewExtensionHandler = MakeShared<FMetaStoryBindingExtension>();
	DetailsViewChildrenCustomizationHandler = MakeShared<FMetaStoryBindingsChildrenCustomization>();
	
	if (const UContextObjectStore* ContextObjectStore = GetToolManager()->GetContextObjectStore())
	{
		if (const UMetaStoryEditorContext* Context = ContextObjectStore->FindContext<UMetaStoryEditorContext>())
		{
			TSharedRef<IMetaStoryEditorHost> Host = Context->EditorHostInterface.ToSharedRef();
			Host->OnStateTreeChanged().AddUObject(this, &UMetaStoryEditorMode::OnStateTreeChanged);

			if (TSharedPtr<IMessageLogListing> MessageLogListing = GetMessageLogListing())
			{
				MessageLogListing->OnMessageTokenClicked().AddUObject(this, &UMetaStoryEditorMode::HandleMessageTokenClicked);
			}

			if (TSharedPtr<IDetailsView> DetailsView = GetDetailsView())
			{
				DetailsView->OnFinishedChangingProperties().AddUObject(this, &UMetaStoryEditorMode::OnSelectionFinishedChangingProperties);

				DetailsView->SetExtensionHandler(DetailsViewExtensionHandler);
				DetailsView->SetChildrenCustomizationHandler(DetailsViewChildrenCustomizationHandler);
			}
			
			if (TSharedPtr<IDetailsView> AssetDetailsView = GetAssetDetailsView())
			{
				AssetDetailsView->OnFinishedChangingProperties().AddUObject(this, &UMetaStoryEditorMode::OnAssetFinishedChangingProperties);
				
				AssetDetailsView->SetExtensionHandler(DetailsViewExtensionHandler);
				AssetDetailsView->SetChildrenCustomizationHandler(DetailsViewChildrenCustomizationHandler);
				bForceAssetDetailViewToRefresh = true;
			}
		}
	}

	UE::MetaStory::Delegates::OnIdentifierChanged.AddUObject(this, &UMetaStoryEditorMode::OnIdentifierChanged);
	UE::MetaStory::Delegates::OnSchemaChanged.AddUObject(this, &UMetaStoryEditorMode::OnSchemaChanged);
	UE::MetaStory::Delegates::OnParametersChanged.AddUObject(this, &UMetaStoryEditorMode::OnRefreshDetailsView);
	UE::MetaStory::Delegates::OnGlobalDataChanged.AddUObject(this, &UMetaStoryEditorMode::OnRefreshDetailsView);
	UE::MetaStory::Delegates::OnStateParametersChanged.AddUObject(this, &UMetaStoryEditorMode::OnStateParametersChanged);
	UE::MetaStory::PropertyBinding::OnMetaStoryPropertyBindingChanged.AddUObject(this, &UMetaStoryEditorMode::OnPropertyBindingChanged);
	OnStateTreeChanged();
}


void UMetaStoryEditorMode::OnIdentifierChanged(const UMetaStory& InStateTree)
{
	if (GetStateTree() == &InStateTree)
	{
		UpdateAsset();
	}
}

void UMetaStoryEditorMode::OnSchemaChanged(const UMetaStory& InStateTree)
{
	if (GetStateTree() == &InStateTree)
	{
		UpdateAsset();

		if(UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
		{
			TSharedRef<FMetaStoryViewModel> ViewModel = MetaStoryEditingSubsystem->FindOrAddViewModel(GetStateTree());
			ViewModel->NotifyAssetChangedExternally();
		}
		
		ForceRefreshDetailsView();
	}
}

void UMetaStoryEditorMode::ForceRefreshDetailsView() const
{
	if (TSharedPtr<IDetailsView> DetailsView = GetDetailsView())
	{
		if (!GEditor->GetTimerManager()->IsTimerActive(SetObjectTimerHandle))
		{
			DetailsView->ForceRefresh();
		}
	}
}

void UMetaStoryEditorMode::OnRefreshDetailsView(const UMetaStory& InStateTree) const
{
	if (GetStateTree() == &InStateTree)
	{
		// Accessible structs might be different after modifying parameters so forcing refresh
		// so the FMetaStoryBindingExtension can rebuild the list of bindable structs
		ForceRefreshDetailsView();
	}
}

void UMetaStoryEditorMode::OnStateParametersChanged(const UMetaStory& InStateTree, const FGuid ChangedStateID) const
{
	UMetaStory* MetaStory = GetStateTree(); 
	if (MetaStory == &InStateTree)
	{
		if (const UMetaStoryEditorData* TreeData = Cast<UMetaStoryEditorData>(MetaStory->EditorData))
		{
			TreeData->VisitHierarchy([&ChangedStateID](UMetaStoryState& State, UMetaStoryState* /*ParentState*/)
			{
				if (State.Type == EMetaStoryStateType::Linked && State.LinkedSubtree.ID == ChangedStateID)
				{
					State.UpdateParametersFromLinkedSubtree();
				}
				return EMetaStoryVisitor::Continue;
			});
		}

		// Accessible structs might be different after modifying parameters so forcing refresh
		// so the FMetaStoryBindingExtension can rebuild the list of bindable structs
		ForceRefreshDetailsView();
	}
}

void UMetaStoryEditorMode::HandleMessageTokenClicked(const TSharedRef<IMessageToken>& InMessageToken) const
{
	if (InMessageToken->GetType() == EMessageToken::Object)
	{
		const TSharedRef<FUObjectToken> ObjectToken = StaticCastSharedRef<FUObjectToken>(InMessageToken);

		if (UMetaStoryState* State = Cast<UMetaStoryState>(ObjectToken->GetObject().Get()))
		{
			if(UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
			{
				MetaStoryEditingSubsystem->FindOrAddViewModel(GetStateTree())->SetSelection(State);
			}
			
		}
	}
}

void UMetaStoryEditorMode::Exit()
{
	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}
	
	if (UContextObjectStore* ContextObjectStore = GetToolManager()->GetContextObjectStore())
	{
		if (const UMetaStoryEditorContext* Context = ContextObjectStore->FindContext<UMetaStoryEditorContext>())
		{
			Context->EditorHostInterface->OnStateTreeChanged().RemoveAll(this);
			
			if (TSharedPtr<IMessageLogListing> MessageLogListing = GetMessageLogListing())
			{
				MessageLogListing->OnMessageTokenClicked().RemoveAll(this);
			}
			
			if (TSharedPtr<IDetailsView> DetailsView = GetDetailsView())
			{
				DetailsView->OnFinishedChangingProperties().RemoveAll(this);
				DetailsView->SetExtensionHandler(nullptr);
				DetailsView->SetChildrenCustomizationHandler(nullptr);
			}
			
			if (TSharedPtr<IDetailsView> AssetDetailsView = GetAssetDetailsView())
			{
				AssetDetailsView->OnFinishedChangingProperties().RemoveAll(this);
				AssetDetailsView->SetExtensionHandler(nullptr);
				AssetDetailsView->SetChildrenCustomizationHandler(nullptr);
				bForceAssetDetailViewToRefresh = true;
			}
		}
	}

	if (CachedStateTree.IsValid())
	{
		if (UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
		{		
			TSharedRef<FMetaStoryViewModel> ViewModel = MetaStoryEditingSubsystem->FindOrAddViewModel(CachedStateTree.Get());
			{
				ViewModel->GetOnAssetChanged().RemoveAll(this);
				ViewModel->GetOnStateAdded().RemoveAll(this);
				ViewModel->GetOnStatesRemoved().RemoveAll(this);
				ViewModel->GetOnStatesMoved().RemoveAll(this);
				ViewModel->GetOnStateNodesChanged().RemoveAll(this);
				ViewModel->GetOnSelectionChanged().RemoveAll(this);
				ViewModel->GetOnBringNodeToFocus().RemoveAll(this);
			}
		}
	}

	UE::MetaStory::Delegates::OnIdentifierChanged.RemoveAll(this);
	UE::MetaStory::Delegates::OnSchemaChanged.RemoveAll(this);
	UE::MetaStory::Delegates::OnParametersChanged.RemoveAll(this);
	UE::MetaStory::Delegates::OnGlobalDataChanged.RemoveAll(this);
	UE::MetaStory::Delegates::OnStateParametersChanged.RemoveAll(this);
	UE::MetaStory::PropertyBinding::OnMetaStoryPropertyBindingChanged.RemoveAll(this);
	Super::Exit();
}

void UMetaStoryEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FMetaStoryEditorModeToolkit(this));
}

void UMetaStoryEditorMode::OnStateTreeChanged()
{
	UContextObjectStore* ContextStore = GetInteractiveToolsContext()->ToolManager->GetContextObjectStore();
	if (const UMetaStoryEditorContext* Context = ContextStore->FindContext<UMetaStoryEditorContext>())
	{
		if (UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
		{
			if (CachedStateTree.IsValid())
			{
				TSharedRef<FMetaStoryViewModel> OldViewModel = MetaStoryEditingSubsystem->FindOrAddViewModel(CachedStateTree.Get());
				{
					OldViewModel->GetOnAssetChanged().RemoveAll(this);
					OldViewModel->GetOnStateAdded().RemoveAll(this);
					OldViewModel->GetOnStatesRemoved().RemoveAll(this);
					OldViewModel->GetOnStatesMoved().RemoveAll(this);
					OldViewModel->GetOnStateNodesChanged().RemoveAll(this);
					OldViewModel->GetOnSelectionChanged().RemoveAll(this);
					OldViewModel->GetOnBringNodeToFocus().RemoveAll(this);
				}
			}
		}

		UMetaStory* MetaStory = Context->EditorHostInterface->GetStateTree();
		CachedStateTree = MetaStory;
		UpdateAsset();

		if (TSharedPtr<IDetailsView> AssetDetailsView = GetAssetDetailsView())
		{
			AssetDetailsView->SetObject(MetaStory ? MetaStory->EditorData : nullptr, bForceAssetDetailViewToRefresh);
			bForceAssetDetailViewToRefresh = false;
		}

		if (MetaStory)
		{
			if (UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
			{				
				TSharedRef<FMetaStoryViewModel> NewViewModel = MetaStoryEditingSubsystem->FindOrAddViewModel(MetaStory);
				{
					NewViewModel->GetOnAssetChanged().AddUObject(this, &UMetaStoryEditorMode::HandleModelAssetChanged);
					NewViewModel->GetOnStateAdded().AddUObject(this, &UMetaStoryEditorMode::HandleStateAdded);
					NewViewModel->GetOnStatesRemoved().AddUObject(this, &UMetaStoryEditorMode::HandleStatesRemoved);
					NewViewModel->GetOnStatesMoved().AddUObject(this, &UMetaStoryEditorMode::HandleOnStatesMoved);
					NewViewModel->GetOnStateNodesChanged().AddUObject(this, &UMetaStoryEditorMode::HandleOnStateNodesChanged);
					NewViewModel->GetOnSelectionChanged().AddUObject(this, &UMetaStoryEditorMode::HandleModelSelectionChanged);
					NewViewModel->GetOnBringNodeToFocus().AddUObject(this, &UMetaStoryEditorMode::HandleModelBringNodeToFocus);
				}
			}
		}
	}

	if (Toolkit)
	{
		StaticCastSharedPtr<FMetaStoryEditorModeToolkit>(Toolkit)->OnStateTreeChanged();
	}
}


namespace UE::MetaStory::Editor::Internal
{
static void SetSaveOnCompileSetting(const EMetaStorySaveOnCompile NewSetting)
{
	UMetaStoryEditorSettings* Settings = GetMutableDefault<UMetaStoryEditorSettings>();
	Settings->SaveOnCompile = NewSetting;
	Settings->SaveConfig();
}

static bool IsSaveOnCompileOptionSet(const EMetaStorySaveOnCompile Option)
{
	const UMetaStoryEditorSettings* Settings = GetDefault<UMetaStoryEditorSettings>();
	return (Settings->SaveOnCompile == Option);
}

static IConsoleVariable* GetLogCompilationResultCVar()
{
	static IConsoleVariable* FoundVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("MetaStory.Compiler.LogResultOnCompilationCompleted"));
	return FoundVariable;
}

static void ToggleLogCompilationResult()
{
	IConsoleVariable* LogResultCVar = GetLogCompilationResultCVar();
	if (ensure(LogResultCVar))
	{
		LogResultCVar->Set(!LogResultCVar->GetBool(), ECVF_SetByConsole);
	}
}

static bool IsLogCompilationResult()
{
	IConsoleVariable* LogResultCVar = GetLogCompilationResultCVar();
	return LogResultCVar ? LogResultCVar->GetBool() : false;
}

static IConsoleVariable* GetLogDependenciesCVar()
{
	static IConsoleVariable* FoundVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("MetaStory.Compiler.LogDependenciesOnCompilation"));
	return FoundVariable;
}

static void ToggleLogDependencies()
{
	IConsoleVariable* LogResultCVar = GetLogDependenciesCVar();
	if (ensure(LogResultCVar))
	{
		LogResultCVar->Set(!LogResultCVar->GetBool(), ECVF_SetByConsole);
	}
}

static bool IsLogDependencies()
{
	IConsoleVariable* LogResultCVar = GetLogDependenciesCVar();
	return LogResultCVar ? LogResultCVar->GetBool() : false;
}
} // namespace UE::MetaStory::Editor::Internal

void UMetaStoryEditorMode::BindToolkitCommands(const TSharedRef<FUICommandList>& ToolkitCommands)
{
	FMetaStoryEditorCommands::Register();
	const FMetaStoryEditorCommands& Commands = FMetaStoryEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.Compile,
		FExecuteAction::CreateUObject(this, &UMetaStoryEditorMode::Compile),
		FCanExecuteAction::CreateUObject(this, &UMetaStoryEditorMode::CanCompile),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateUObject(this, &UMetaStoryEditorMode::IsCompileVisible));

	ToolkitCommands->MapAction(
		FMetaStoryEditorCommands::Get().SaveOnCompile_Never,
		FExecuteAction::CreateStatic(&UE::MetaStory::Editor::Internal::SetSaveOnCompileSetting, EMetaStorySaveOnCompile::Never),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&UE::MetaStory::Editor::Internal::IsSaveOnCompileOptionSet, EMetaStorySaveOnCompile::Never),
		FIsActionButtonVisible::CreateUObject(this, &UMetaStoryEditorMode::HasValidStateTree)
	);
	ToolkitCommands->MapAction(
		FMetaStoryEditorCommands::Get().SaveOnCompile_SuccessOnly,
		FExecuteAction::CreateStatic(&UE::MetaStory::Editor::Internal::SetSaveOnCompileSetting, EMetaStorySaveOnCompile::SuccessOnly),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&UE::MetaStory::Editor::Internal::IsSaveOnCompileOptionSet,  EMetaStorySaveOnCompile::SuccessOnly),
		FIsActionButtonVisible::CreateUObject(this, &UMetaStoryEditorMode::HasValidStateTree)
	);
	ToolkitCommands->MapAction(
		FMetaStoryEditorCommands::Get().SaveOnCompile_Always,
		FExecuteAction::CreateStatic(&UE::MetaStory::Editor::Internal::SetSaveOnCompileSetting, EMetaStorySaveOnCompile::Always),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&UE::MetaStory::Editor::Internal::IsSaveOnCompileOptionSet,  EMetaStorySaveOnCompile::Always),
		FIsActionButtonVisible::CreateUObject(this, &UMetaStoryEditorMode::HasValidStateTree)
	);
	ToolkitCommands->MapAction(
		FMetaStoryEditorCommands::Get().LogCompilationResult,
		FExecuteAction::CreateStatic(&UE::MetaStory::Editor::Internal::ToggleLogCompilationResult),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&UE::MetaStory::Editor::Internal::IsLogCompilationResult)
	);
	ToolkitCommands->MapAction(
		FMetaStoryEditorCommands::Get().LogDependencies,
		FExecuteAction::CreateStatic(&UE::MetaStory::Editor::Internal::ToggleLogDependencies),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&UE::MetaStory::Editor::Internal::IsLogDependencies)
	);
}

void UMetaStoryEditorMode::OnPropertyBindingChanged(const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath)
{
	UpdateAsset();
}

void UMetaStoryEditorMode::BindCommands()
{
	UEdMode::BindCommands();
	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();
	BindToolkitCommands(CommandList);
}

void UMetaStoryEditorMode::Compile()
{
	UMetaStory* MetaStory = GetStateTree();

	if (!MetaStory)
	{
		return;
	}

	UpdateAsset();

	if (TSharedPtr<IMessageLogListing> Listing = GetMessageLogListing())
	{
		Listing->ClearMessages();
	}
	
	FMetaStoryCompilerLog Log;
	bLastCompileSucceeded = UMetaStoryEditingSubsystem::CompileStateTree(MetaStory, Log);

	if (TSharedPtr<IMessageLogListing> Listing = GetMessageLogListing())
	{					
		Log.AppendToLog(Listing.Get());

		if (!bLastCompileSucceeded)
		{
			// Show log
			ShowCompilerTab();
		}
	}
	

	const UMetaStoryEditorSettings* Settings = GetMutableDefault<UMetaStoryEditorSettings>();
	const bool bShouldSaveOnCompile = ((Settings->SaveOnCompile == EMetaStorySaveOnCompile::Always)
									|| ((Settings->SaveOnCompile == EMetaStorySaveOnCompile::SuccessOnly) && bLastCompileSucceeded));

	if (bShouldSaveOnCompile)
	{
		const TArray<UPackage*> PackagesToSave { MetaStory->GetOutermost() };
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty =*/true, /*bPromptToSave =*/false);
	}
}

bool UMetaStoryEditorMode::CanCompile() const
{
	if (GetStateTree() == nullptr)
	{
		return false;
	}

	// We can't recompile while in PIE
	if (GEditor->IsPlaySessionInProgress())
	{
		return false;
	}

	return true;
}

bool UMetaStoryEditorMode::IsCompileVisible() const
{
	if(!HasValidStateTree())
	{
		return false;
	}

	if (const UContextObjectStore* ContextObjectStore = GetToolManager()->GetContextObjectStore())
	{
		if (const UMetaStoryEditorContext* Context = ContextObjectStore->FindContext<UMetaStoryEditorContext>())
		{
			return Context->EditorHostInterface->ShouldShowCompileButton();
		}
	}
	return true;
}

bool UMetaStoryEditorMode::HasValidStateTree() const
{
	return GetStateTree() != nullptr;
}

void UMetaStoryEditorMode::HandleModelAssetChanged()
{
	UpdateAsset();
}

void UMetaStoryEditorMode::HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UMetaStoryState>>& InSelectedStates) const
{
	if (TSharedPtr<IDetailsView> DetailsView = GetDetailsView())
	{
		TArray<UObject*> Selected;
		Selected.Reserve(InSelectedStates.Num());
		for (const TWeakObjectPtr<UMetaStoryState>& WeakState : InSelectedStates)
		{
			if (UMetaStoryState* State = WeakState.Get())
			{
				Selected.Add(State);
			}
		}
		DetailsView->SetObjects(Selected);
	}
}

void UMetaStoryEditorMode::HandleModelBringNodeToFocus(const UMetaStoryState* State, const FGuid NodeID) const
{
	auto BringToFocus = [Self = this](const FPropertyPath& HighlightPath, TSharedPtr<IDetailsView>& DetailsView)
		{
			if (HighlightPath.IsValid())
			{
				constexpr bool bExpandProperty = true;
				DetailsView->ScrollPropertyIntoView(HighlightPath, bExpandProperty);
				DetailsView->HighlightProperty(HighlightPath);

				constexpr bool bLoop = false;
				GEditor->GetTimerManager()->SetTimer(
					Self->HighlightTimerHandle,
					FTimerDelegate::CreateLambda([WeakSelectionDetailsView = DetailsView.ToWeakPtr()]()
						{
							if (TSharedPtr<IDetailsView> SelectionDetailsView = WeakSelectionDetailsView.Pin())
							{
								SelectionDetailsView->HighlightProperty({});
							}
						}),
					1.0f,
					bLoop);
			}
			else if (Self->HighlightTimerHandle.IsValid())
			{
				// NB. SetTimer also clear the timer.
				GEditor->GetTimerManager()->ClearTimer(Self->HighlightTimerHandle);
			}
		};

	if (State)
	{
		TSharedPtr<IDetailsView> DetailsView = GetDetailsView();
		if (DetailsView == nullptr)
		{
			return;
		}

		FPropertyPath HighlightPath;

		if (!HighlightPath.IsValid())
		{
			FArrayProperty* TasksProperty = CastFieldChecked<FArrayProperty>(UMetaStoryState::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaStoryState, Tasks)));
			const int32 TaskIndex = State->Tasks.IndexOfByPredicate([&NodeID](const FMetaStoryEditorNode& Node)
			{
				return Node.ID == NodeID;
			});
			if (TaskIndex != INDEX_NONE)
			{
				HighlightPath.AddProperty(FPropertyInfo(TasksProperty));
				HighlightPath.AddProperty(FPropertyInfo(TasksProperty->Inner, TaskIndex));
			}
		}

		if (!HighlightPath.IsValid())
		{
			FProperty* SingleTaskProperty = CastFieldChecked<FProperty>(UMetaStoryState::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaStoryState, SingleTask)));
			if (State->SingleTask.ID == NodeID)
			{
				HighlightPath.AddProperty(FPropertyInfo(SingleTaskProperty));
			}
		}

		if (!HighlightPath.IsValid())
		{
			FArrayProperty* TransitionsProperty = CastFieldChecked<FArrayProperty>(UMetaStoryState::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaStoryState, Transitions)));
			const int32 TransitionIndex = State->Transitions.IndexOfByPredicate([&NodeID](const FMetaStoryTransition& Transition)
			{
				return Transition.ID == NodeID;
			});
			if (TransitionIndex != INDEX_NONE)
			{
				HighlightPath.AddProperty(FPropertyInfo(TransitionsProperty));
				HighlightPath.AddProperty(FPropertyInfo(TransitionsProperty->Inner, TransitionIndex));
			}
		}

		if (!HighlightPath.IsValid())
		{
			FArrayProperty* EnterConditionsProperty = CastFieldChecked<FArrayProperty>(UMetaStoryState::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaStoryState, EnterConditions)));
			const int32 EnterConditionIndex = State->EnterConditions.IndexOfByPredicate([&NodeID](const FMetaStoryEditorNode& Node)
			{
				return Node.ID == NodeID;
			});
			if (EnterConditionIndex != INDEX_NONE)
			{
				HighlightPath.AddProperty(FPropertyInfo(EnterConditionsProperty));
				HighlightPath.AddProperty(FPropertyInfo(EnterConditionsProperty->Inner, EnterConditionIndex));
			}
		}

		BringToFocus(HighlightPath, DetailsView);
	}
	else
	{
		UMetaStory* MetaStory = GetStateTree();
		if (MetaStory == nullptr)
		{
			return;
		}
		const UMetaStoryEditorData* TreeData = Cast<UMetaStoryEditorData>(MetaStory->EditorData);
		if (TreeData == nullptr)
		{
			return;
		}
		TSharedPtr<IDetailsView> DetailsView = GetAssetDetailsView();
		if (DetailsView == nullptr)
		{
			return;
		}

		FPropertyPath HighlightPath;
		if (!HighlightPath.IsValid())
		{
			FArrayProperty* TasksProperty = CastFieldChecked<FArrayProperty>(UMetaStoryEditorData::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, GlobalTasks)));
			const int32 TaskIndex = TreeData->GlobalTasks.IndexOfByPredicate([&NodeID](const FMetaStoryEditorNode& Node)
			{
				return Node.ID == NodeID;
			});
			if (TaskIndex != INDEX_NONE)
			{
				HighlightPath.AddProperty(FPropertyInfo(TasksProperty));
				HighlightPath.AddProperty(FPropertyInfo(TasksProperty->Inner, TaskIndex));
			}
		}

		if (!HighlightPath.IsValid())
		{
			FArrayProperty* EvaluatorsProperty = CastFieldChecked<FArrayProperty>(UMetaStoryEditorData::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, Evaluators)));
			const int32 TaskIndex = TreeData->Evaluators.IndexOfByPredicate([&NodeID](const FMetaStoryEditorNode& Node)
			{
				return Node.ID == NodeID;
			});
			if (TaskIndex != INDEX_NONE)
			{
				HighlightPath.AddProperty(FPropertyInfo(EvaluatorsProperty));
				HighlightPath.AddProperty(FPropertyInfo(EvaluatorsProperty->Inner, TaskIndex));
			}
		}

		BringToFocus(HighlightPath, DetailsView);
	}
}

void UMetaStoryEditorMode::UpdateAsset()
{
	UMetaStory* MetaStory = GetStateTree();
	if (!MetaStory)
	{
		return;
	}

	UMetaStoryEditingSubsystem::ValidateStateTree(MetaStory);
	EditorDataHash = UMetaStoryEditingSubsystem::CalculateStateTreeHash(MetaStory);
}

TSharedPtr<IDetailsView> UMetaStoryEditorMode::GetDetailsView() const
{
	if (const UContextObjectStore* ContextObjectStore = GetToolManager()->GetContextObjectStore())
	{
		if (const UMetaStoryEditorContext* Context = ContextObjectStore->FindContext<UMetaStoryEditorContext>())
		{
			return Context->EditorHostInterface->GetDetailsView();
		}
	}

	return nullptr;
}

TSharedPtr<IDetailsView> UMetaStoryEditorMode::GetAssetDetailsView() const
{
	if (UContextObjectStore* ContextObjectStore = GetToolManager()->GetContextObjectStore())
	{
		if (const UMetaStoryEditorContext* Context = ContextObjectStore->FindContext<UMetaStoryEditorContext>())
		{
			return Context->EditorHostInterface->GetAssetDetailsView();
		}
	}

	return nullptr;
}

TSharedPtr<IMessageLogListing> UMetaStoryEditorMode::GetMessageLogListing() const
{
	if (UContextObjectStore* ContextObjectStore = GetToolManager()->GetContextObjectStore())
	{
		if (const UMetaStoryEditorContext* Context = ContextObjectStore->FindContext<UMetaStoryEditorContext>())
		{
			FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
			return MessageLogModule.GetLogListing(Context->EditorHostInterface->GetCompilerLogName());
		}
	}

	return nullptr;
}

void UMetaStoryEditorMode::ShowCompilerTab() const
{
	if (UContextObjectStore* ContextObjectStore = GetToolManager()->GetContextObjectStore())
	{
		if (const UMetaStoryEditorContext* Context = ContextObjectStore->FindContext<UMetaStoryEditorContext>())
		{
			if(TSharedPtr<FTabManager> TabManager = GetModeManager()->GetToolkitHost()->GetTabManager())
			{
				TabManager->TryInvokeTab(Context->EditorHostInterface->GetCompilerTabName());
			}
		}
	}
}

UMetaStory* UMetaStoryEditorMode::GetStateTree() const
{
	return CachedStateTree.Get();
}

void UMetaStoryEditorMode::OnAssetFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) const
{
	// Make sure nodes get updates when properties are changed.
	if(UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
	{
		if (TSharedPtr<IDetailsView> AssetDetailsView = GetAssetDetailsView())
		{
			// From the path FPropertyHandleBase::NotifyFinishedChangingProperties(), UObject info is not included in PropertyChangedEvent
			// So we fetch it from DetailsView
			const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = AssetDetailsView->GetSelectedObjects();
			for (const TWeakObjectPtr<UObject>& WeakObject : SelectedObjects)
			{
				if (const UMetaStoryEditorData* EditorData = Cast<UMetaStoryEditorData>(WeakObject.Get()))
				{
					if (const UMetaStory* MetaStory = Cast<UMetaStory>(EditorData->GetOuter()))
					{
						if (GetStateTree() == MetaStory)
						{
							MetaStoryEditingSubsystem->FindOrAddViewModel(GetStateTree())->NotifyAssetChangedExternally();
							break;
						}
					}
				}
			}
		}
	}
}

void UMetaStoryEditorMode::OnSelectionFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	// Make sure nodes get updates when properties are changed.
	if(UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
	{
		if (TSharedPtr<IDetailsView> DetailsView = GetDetailsView())
		{
			const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailsView->GetSelectedObjects();
			TSet<UMetaStoryState*> ChangedStates;
			for (const TWeakObjectPtr<UObject>& WeakObject : SelectedObjects)
			{
				if (UObject* Object = WeakObject.Get())
				{
					if (UMetaStoryState* State = Cast<UMetaStoryState>(Object))
					{
						ChangedStates.Add(State);
					}
				}
			}
			if (ChangedStates.Num() > 0)
			{
				MetaStoryEditingSubsystem->FindOrAddViewModel(GetStateTree())->NotifyStatesChangedExternally(ChangedStates, PropertyChangedEvent);
				UpdateAsset();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE // "UMetaStoryEditorMode"
