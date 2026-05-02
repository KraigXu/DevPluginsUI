// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Blueprint/MetaStoryTaskBlueprintBase.h"
#include "Blueprint/MetaStoryConditionBlueprintBase.h"
#include "Blueprint/MetaStoryConsiderationBlueprintBase.h"
#include "ContentBrowserModule.h"
#include "ClassViewerFilter.h"
#include "ContextObjectStore.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Customizations/MetaStoryBindingExtension.h"
#include "DetailsViewArgs.h"
#include "EditorModeManager.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "IDetailsView.h"
#include "IContentBrowserSingleton.h"
#include "ISourceCodeAccessModule.h"
#include "ISourceCodeAccessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "IMessageLogListing.h"
#include "MessageLogModule.h"
#include "Misc/UObjectToken.h"
#include "SMetaStoryView.h"
#include "MetaStory.h"
#include "MetaStoryCompiler.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryEditorCommands.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorModule.h"
#include "MetaStoryEditorSettings.h"
#include "MetaStoryEditorWorkspaceTabHost.h"
#include "MetaStoryObjectHash.h"
#include "MetaStoryTaskBase.h"
#include "MetaStoryViewModel.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenuEntry.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "FileHelpers.h"
#include "MetaStoryEditorMode.h"
#include "PropertyPath.h"
#include "SMetaStoryOutliner.h"
#include "StandaloneMetaStoryEditorHost.h"
#include "MetaStoryEditingSubsystem.h"
#include "MetaStoryEditorUILayer.h"
#include "Debugger/SMetaStoryDebuggerView.h"
#include "MetaStorySettings.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

const FName MetaStoryEditorAppName(TEXT("MetaStoryEditorApp"));

const FName FMetaStoryEditor::SelectionDetailsTabId(TEXT("MetaStoryEditor_SelectionDetails"));
const FName FMetaStoryEditor::AssetDetailsTabId(TEXT("MetaStoryEditor_AssetDetails"));
const FName FMetaStoryEditor::MetaStoryViewTabId(TEXT("MetaStoryEditor_StateTreeView"));
const FName FMetaStoryEditor::CompilerResultsTabId(TEXT("MetaStoryEditor_CompilerResults"));
const FName FMetaStoryEditor::CompilerLogListingName(TEXT("MetaStoryCompiler"));
const FName FMetaStoryEditor::LayoutLeftStackId("LeftStackId");
const FName FMetaStoryEditor::LayoutBottomMiddleStackId("BottomMiddleStackId");

namespace UE::MetaStory::Editor
{
bool GbDisplayItemIds = false;

FAutoConsoleVariableRef CVarDisplayItemIds(
	TEXT("statetree.displayitemids"),
	GbDisplayItemIds,
	TEXT("Appends Id to task and state names in the treeview and expose Ids in the details view."));
}

void FMetaStoryEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (MetaStory != nullptr)
	{
		Collector.AddReferencedObject(MetaStory);
	}
}

void FMetaStoryEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_StateTreeEditor", "MetaStory Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(SelectionDetailsTabId, FOnSpawnTab::CreateSP(this, &FMetaStoryEditor::SpawnTab_SelectionDetails) )
		.SetDisplayName( NSLOCTEXT("MetaStoryEditor", "SelectionDetailsTab", "Details" ) )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(AssetDetailsTabId, FOnSpawnTab::CreateSP(this, &FMetaStoryEditor::SpawnTab_AssetDetails))
		.SetDisplayName(NSLOCTEXT("MetaStoryEditor", "AssetDetailsTab", "Asset Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(MetaStoryViewTabId, FOnSpawnTab::CreateSP(this, &FMetaStoryEditor::SpawnTab_StateTreeView))
		.SetDisplayName(NSLOCTEXT("MetaStoryEditor", "MetaStoryViewTab", "States"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	
	InTabManager->RegisterTabSpawner(CompilerResultsTabId, FOnSpawnTab::CreateSP(this, &FMetaStoryEditor::SpawnTab_CompilerResults))
		.SetDisplayName(NSLOCTEXT("MetaStoryEditor", "CompilerResultsTab", "Compiler Results"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Log.TabIcon"));

	if (EditorHost)
	{
		using namespace UE::MetaStoryEditor;
		TSharedPtr<FWorkspaceTabHost> TabHost = EditorHost->GetTabHost();
		for (const FMinorWorkspaceTabConfig& Config : TabHost->GetTabConfigs())
		{
			FOnSpawnTab Delegate = TabHost->CreateSpawnDelegate(Config.ID);			
			InTabManager->RegisterTabSpawner(Config.ID, Delegate)
				.SetDisplayName(Config.Label)
				.SetGroup(WorkspaceMenuCategoryRef)
				.SetTooltipText(Config.Tooltip)
				.SetIcon(Config.Icon);
		}
	}
}


void FMetaStoryEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(SelectionDetailsTabId);
	InTabManager->UnregisterTabSpawner(AssetDetailsTabId);
	InTabManager->UnregisterTabSpawner(MetaStoryViewTabId);
	InTabManager->UnregisterTabSpawner(CompilerResultsTabId);

	if (EditorHost)
	{
		using namespace UE::MetaStoryEditor;
		TSharedPtr<FWorkspaceTabHost> TabHost = EditorHost->GetTabHost();
		for (const FMinorWorkspaceTabConfig& Config : TabHost->GetTabConfigs())
		{
			InTabManager->UnregisterTabSpawner(Config.ID);
		}
	}
}

void FMetaStoryEditor::InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UMetaStory* InStateTree)
{
	MetaStory = InStateTree;
	check(MetaStory != NULL);

	UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>();
	check(MetaStoryEditingSubsystem);

	EditorHost = MakeShared<FStandaloneStateTreeEditorHost>();
	EditorHost->Init(StaticCastSharedRef<FMetaStoryEditor>(AsShared()));

	MetaStoryViewModel = MetaStoryEditingSubsystem->FindOrAddViewModel(InStateTree);

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions LogOptions;
	// Show Pages so that user is never allowed to clear log messages
	LogOptions.bShowPages = false;
	LogOptions.bShowFilters = false;
	LogOptions.bAllowClear = false;
	LogOptions.MaxPageCount = 1;

	MessageLogModule.RegisterLogListing(CompilerLogListingName, FText::FromName(CompilerLogListingName), LogOptions);
	CompilerResultsListing = MessageLogModule.GetLogListing(CompilerLogListingName);
	CompilerResults = MessageLogModule.CreateLogListingWidget(CompilerResultsListing.ToSharedRef());

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_StateTree_Layout_v5")
	->AddArea
	(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.2f)
				->SetExtensionId(LayoutLeftStackId)
				->AddTab(AssetDetailsTabId, ETabState::OpenedTab)
				->AddTab(UE::MetaStoryEditor::FWorkspaceTabHost::OutlinerTabId.Resolve(), ETabState::ClosedTab)
				->AddTab(UE::MetaStoryEditor::FWorkspaceTabHost::StatisticsTabId.Resolve(), ETabState::ClosedTab)
				->SetForegroundTab(AssetDetailsTabId)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.5f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.75f)
					->AddTab(MetaStoryViewTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->SetExtensionId(LayoutBottomMiddleStackId)
					->AddTab(CompilerResultsTabId, ETabState::ClosedTab)
					->AddTab(UE::MetaStoryEditor::FWorkspaceTabHost::SearchTabId.Resolve(), ETabState::ClosedTab)
					->AddTab(UE::MetaStoryEditor::FWorkspaceTabHost::DebuggerTabId.Resolve(), ETabState::ClosedTab)
					->AddTab(UE::MetaStoryEditor::FWorkspaceTabHost::BindingTabId.Resolve(), ETabState::ClosedTab)
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->AddTab(SelectionDetailsTabId, ETabState::OpenedTab)
				->SetForegroundTab(SelectionDetailsTabId)
			)
		)
	);

	FLayoutExtender LayoutExtender;
	FMetaStoryEditorModule& MetaStoryEditorModule = FModuleManager::LoadModuleChecked<FMetaStoryEditorModule>("MetaStoryEditorModule");
	MetaStoryEditorModule.OnRegisterLayoutExtensions().Broadcast(LayoutExtender);
	StandaloneDefaultLayout->ProcessExtensions(LayoutExtender);

	CreateEditorModeManager();
	
	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, MetaStoryEditorAppName, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, MetaStory);

	RegisterMenu();
	RegisterToolbar();
	
	AddMenuExtender(MetaStoryEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	RegenerateMenusAndToolbars();
}

void FMetaStoryEditor::PostInitAssetEditor()
{
	IMetaStoryEditor::PostInitAssetEditor();
	
	ModeUILayer = MakeShared<FMetaStoryEditorModeUILayer>(ToolkitHost.Pin().Get());
	ModeUILayer->SetModeMenuCategory(WorkspaceMenuCategory);
	ModeUILayer->SetSecondaryModeToolbarName(GetToolMenuToolbarName());
	ToolkitCommands->Append(ModeUILayer->GetModeCommands());

	if (UContextObjectStore* ContextStore = EditorModeManager->GetInteractiveToolsContext()->ContextObjectStore)
	{
		UMetaStoryEditorContext* MetaStoryEditorContext = ContextStore->FindContext<UMetaStoryEditorContext>();
		if (!MetaStoryEditorContext)
		{
			MetaStoryEditorContext = NewObject<UMetaStoryEditorContext>();
			MetaStoryEditorContext->EditorHostInterface = EditorHost;
			ContextStore->AddContextObject(MetaStoryEditorContext);
		}
	}

	EditorModeManager->SetDefaultMode(UMetaStoryEditorMode::EM_StateTree);
	EditorModeManager->ActivateDefaultMode();
}

void FMetaStoryEditor::OnToolkitHostingStarted(const TSharedRef<IToolkit>& Toolkit)
{
	ModeUILayer->OnToolkitHostingStarted(Toolkit);
	HostedToolkit = Toolkit;
}

void FMetaStoryEditor::OnToolkitHostingFinished(const TSharedRef<IToolkit>& Toolkit)
{
	ModeUILayer->OnToolkitHostingFinished(Toolkit);
	HostedToolkit = nullptr;
}

FName FMetaStoryEditor::GetToolkitFName() const
{
	return FName("MetaStoryEditor");
}

FText FMetaStoryEditor::GetBaseToolkitName() const
{
	return NSLOCTEXT("MetaStoryEditor", "AppLabel", "MetaStory");
}

FString FMetaStoryEditor::GetWorldCentricTabPrefix() const
{
	return NSLOCTEXT("MetaStoryEditor", "WorldCentricTabPrefix", "MetaStory").ToString();
}

FLinearColor FMetaStoryEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

void FMetaStoryEditor::OnClose()
{
	if (HostedToolkit.IsValid())
	{
		UToolMenus::UnregisterOwner(&(*HostedToolkit));	
		HostedToolkit = nullptr;
	}
}

TSharedRef<SDockTab> FMetaStoryEditor::SpawnTab_StateTreeView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == MetaStoryViewTabId);

	return SNew(SDockTab)
		.Label(NSLOCTEXT("MetaStoryEditor", "MetaStoryViewTab", "States"))
		.TabColorScale(GetTabColorScale())
		[
			SAssignNew(MetaStoryView, SMetaStoryView, MetaStoryViewModel.ToSharedRef(), TreeViewCommandList)
		];
}

TSharedRef<SDockTab> FMetaStoryEditor::SpawnTab_SelectionDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == SelectionDetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	SelectionDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	SelectionDetailsView->SetObject(nullptr);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(NSLOCTEXT("MetaStoryEditor", "SelectionDetailsTab", "Details"))
		[
			SelectionDetailsView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FMetaStoryEditor::SpawnTab_AssetDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == AssetDetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	AssetDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	AssetDetailsView->SetObject(MetaStory ? MetaStory->EditorData : nullptr);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(NSLOCTEXT("MetaStoryEditor", "AssetDetailsTabLabel", "Asset Details"))
		[
			AssetDetailsView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FMetaStoryEditor::SpawnTab_CompilerResults(const FSpawnTabArgs& Args) const
{
	check(Args.GetTabId() == CompilerResultsTabId);
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("CompilerResultsTitle", "Compiler Results"))
		[
			SNew(SBox)
			[
				CompilerResults.ToSharedRef()
			]
		];
	return SpawnedTab;
}

void FMetaStoryEditor::SaveAsset_Execute()
{
	// Remember the treeview expansion state
	if (MetaStoryView)
	{
		MetaStoryView->SavePersistentExpandedStates();
	}

	// save it
	FAssetEditorToolkit::SaveAsset_Execute();
}

namespace UE::MetaStory::Editor::Private
{
void FillDeveloperMenu(UToolMenu* InMenu)
{
	const FMetaStoryEditorCommands& Commands = FMetaStoryEditorCommands::Get();
	{
		FToolMenuSection& Section = InMenu->AddSection("FileDeveloperCompilerSettings", LOCTEXT("CompileOptionsHeading", "Compiler Settings"));
		Section.AddMenuEntry(Commands.LogCompilationResult);
		Section.AddMenuEntry(Commands.LogDependencies);
	}
	{
		FToolMenuSection& Section = InMenu->AddSection("FileDeveloperSettings", LOCTEXT("DeveloperOptionsHeading", "Settings"));
		Section.AddEntry(FToolMenuEntry::InitMenuEntry(
			"DisplayItemIds",
			LOCTEXT("DisplayItemIds", "Display Nodes IDs"),
			UE::MetaStory::Editor::CVarDisplayItemIds->GetDetailedHelp(),
			TAttribute<FSlateIcon>(),
			FUIAction(
				FExecuteAction::CreateLambda([]()
					{
						UE::MetaStory::Editor::CVarDisplayItemIds->Set(!UE::MetaStory::Editor::CVarDisplayItemIds->GetBool(), ECVF_SetByConsole);
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([]()
					{
						return UE::MetaStory::Editor::CVarDisplayItemIds->GetBool();
					})
				),
			EUserInterfaceActionType::ToggleButton
			));
	}
}
void FillDynamicDeveloperMenu(FToolMenuSection& Section)
{
	// Only show the developer menu on machines with the solution (assuming they can build it)
	ISourceCodeAccessModule* SourceCodeAccessModule = FModuleManager::GetModulePtr<ISourceCodeAccessModule>("SourceCodeAccess");
	if (SourceCodeAccessModule != nullptr && SourceCodeAccessModule->GetAccessor().CanAccessSourceCode())
	{
		Section.AddSubMenu(
			"DeveloperMenu",
			LOCTEXT("DeveloperMenu", "Developer"),
			LOCTEXT("DeveloperMenu_ToolTip", "Open the developer menu"),
			FNewToolMenuDelegate::CreateStatic(FillDeveloperMenu),
			false);
	}
}
} // UE::MetaStory::Editor::Private

void FMetaStoryEditor::RegisterMenu()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	const FName FileMenuName = *(GetToolMenuName().ToString() + TEXT(".File"));
	if (!ToolMenus->IsMenuRegistered(FileMenuName))
	{
		const FName ParentFileMenuName = TEXT("MainFrame.MainMenu.File");
		UToolMenus::Get()->RegisterMenu(FileMenuName, ParentFileMenuName, EMultiBoxType::ToolBar);
		{
			UToolMenu* FileMenu = UToolMenus::Get()->RegisterMenu(FileMenuName, ParentFileMenuName);
			const FName FileStateTreeSection = "FileStateTree";
			FToolMenuSection& Section = FileMenu->AddSection("MetaStory", LOCTEXT("MetaStoryHeading", "MetaStory"));
			FToolMenuInsert InsertPosition("FileLoadAndSave", EToolMenuInsertType::After);
			Section.InsertPosition = InsertPosition;

			Section.AddDynamicEntry("FileDeveloper", FNewToolMenuSectionDelegate::CreateStatic(UE::MetaStory::Editor::Private::FillDynamicDeveloperMenu));
		}
	}

	const FName EditMenuName = *(GetToolMenuName().ToString() + TEXT(".Edit"));
	if (!UToolMenus::Get()->IsMenuRegistered(EditMenuName))
	{
		const FName ParentEditMenuName = TEXT("MainFrame.MainMenu.Edit");
		UToolMenu* EditMenu = UToolMenus::Get()->RegisterMenu(EditMenuName, ParentEditMenuName);
		FToolMenuSection& Section = EditMenu->AddSection("MetaStory", LOCTEXT("MetaStoryHeading", "MetaStory"));
		FToolMenuInsert InsertPosition("Configuration", EToolMenuInsertType::After);
		Section.InsertPosition = InsertPosition;
	}
}

void FMetaStoryEditor::RegisterToolbar()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	FName ParentName;
	const FName MenuName = GetToolMenuToolbarName(ParentName);
	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenus::Get()->RegisterMenu(MenuName, ParentName, EMultiBoxType::ToolBar);
	}
}

#undef LOCTEXT_NAMESPACE
