// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorModeToolkit.h"

#include "ClassViewerModule.h"
#include "ContextObjectStore.h"
#include "InteractiveToolManager.h"
#include "MetaStoryEditorCommands.h"
#include "MetaStoryEditorWorkspaceTabHost.h"
#include "Blueprint/MetaStoryConditionBlueprintBase.h"
#include "Blueprint/MetaStoryConsiderationBlueprintBase.h"
#include "Blueprint/MetaStoryNodeBlueprintBase.h"
#include "Blueprint/MetaStoryTaskBlueprintBase.h"

#include "ClassViewerFilter.h"
#include "ContentBrowserModule.h"
#include "EditorModeManager.h"
#include "FindTools/SMetaStoryFind.h"
#include "IContentBrowserSingleton.h"
#include "SMetaStoryOutliner.h"
#include "MetaStoryEditingSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Debugger/SMetaStoryDebuggerView.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Toolkits/AssetEditorModeUILayer.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SPropertyBindingView.h"
#include "ToolMenus.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"

#define LOCTEXT_NAMESPACE "MetaStoryModeToolkit"

FMetaStoryEditorModeToolkit::FMetaStoryEditorModeToolkit(UMetaStoryEditorMode* InEditorMode)
	: WeakEditorMode(InEditorMode)
{
}

void FMetaStoryEditorModeToolkit::RequestModeUITabs()
{
	using namespace UE::MetaStoryEditor;
	TSharedPtr<FWorkspaceTabHost> TabHost = EditorHost->GetTabHost();
	check(TabHost);

	if (EditorHost->CanToolkitSpawnWorkspaceTab())
	{
		FModeToolkit::RequestModeUITabs();
	}

	if (ModeUILayer.IsValid())
	{
		if (EditorHost && EditorHost->CanToolkitSpawnWorkspaceTab())
		{
			TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
			TSharedPtr<FWorkspaceItem> MenuGroup = ModeUILayerPtr->GetModeMenuCategory();
			if (!MenuGroup)
			{
				return;
			}

			for (const FMinorWorkspaceTabConfig& Config : TabHost->GetTabConfigs())
			{
				FMinorTabConfig TabInfo;
				TabInfo.TabId = Config.ID;
				TabInfo.TabLabel = Config.Label;
				TabInfo.TabTooltip = Config.Tooltip;
				TabInfo.TabIcon = Config.Icon;
				TabInfo.WorkspaceGroup = MenuGroup;
				TabInfo.OnSpawnTab = TabHost->CreateSpawnDelegate(Config.ID);
				ModeUILayerPtr->SetModePanelInfo(Config.UISystemID, TabInfo);
			}
		}

		for (const FSpawnedWorkspaceTab& SpawnedTab : TabHost->GetSpawnedTabs())
		{
			HandleTabSpawned(SpawnedTab);
		}
		TabHost->OnTabSpawned.AddSP(this, &FMetaStoryEditorModeToolkit::HandleTabSpawned);
		TabHost->OnTabClosed.AddSP(this, &FMetaStoryEditorModeToolkit::HandleTabClosed);
	}
}

void FMetaStoryEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);

	if (UContextObjectStore* ContextObjectStore = InOwningMode->GetToolManager()->GetContextObjectStore())
	{
		if (UMetaStoryEditorContext* Context = ContextObjectStore->FindContext<UMetaStoryEditorContext>())
		{
			EditorHost = Context->EditorHostInterface;
		}
	}
}

void FMetaStoryEditorModeToolkit::InvokeUI()
{
	if (ModeUILayer.IsValid() && EditorHost && EditorHost->CanToolkitSpawnWorkspaceTab())
	{
		TSharedPtr<FAssetEditorModeUILayer> ModeUILayerPtr = ModeUILayer.Pin();
		ModeUILayerPtr->GetTabManager()->TryInvokeTab(UAssetEditorUISubsystem::TopLeftTabID);
		ModeUILayerPtr->GetTabManager()->TryInvokeTab(UAssetEditorUISubsystem::BottomRightTabID);
	}
}

void FMetaStoryEditorModeToolkit::HandleTabSpawned(UE::MetaStoryEditor::FSpawnedWorkspaceTab SpawnedTab)
{
	if (SpawnedTab.TabID == UE::MetaStoryEditor::FWorkspaceTabHost::BindingTabId)
	{
		if (TSharedPtr<SDockTab> DockTab = SpawnedTab.DockTab.Pin())
		{
			const UMetaStoryEditorData* EditorData = nullptr;
			if (UMetaStoryEditorMode* EditorMode = WeakEditorMode.Get())
			{
				if (UMetaStory* MetaStory = EditorMode->GetStateTree())
				{
					if (UMetaStoryEditingSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
					{
						EditorData = Subsystem->FindOrAddViewModel(MetaStory)->GetStateTreeEditorData();
					}
				}
			}

			DockTab->SetContent(
				SNew(UE::PropertyBinding::SBindingView)
				.GetBindingCollection(this, &FMetaStoryEditorModeToolkit::GetBindingCollection)
				.CollectionOwner(TScriptInterface<const IPropertyBindingBindingCollectionOwner>(EditorData))
			);
		}
	}
	else if (SpawnedTab.TabID == UE::MetaStoryEditor::FWorkspaceTabHost::OutlinerTabId)
	{
		WeakOutlinerTab = SpawnedTab.DockTab;
		UpdateStateTreeOutliner();
	}
	else if (SpawnedTab.TabID == UE::MetaStoryEditor::FWorkspaceTabHost::SearchTabId)
	{
		if (TSharedPtr<SDockTab> DockTab = SpawnedTab.DockTab.Pin())
		{
			DockTab->SetContent(
				SNew(UE::MetaStoryEditor::SFindInAsset, EditorHost)
				.bShowSearchBar(true)
			);
		}
	}
	else if (SpawnedTab.TabID == UE::MetaStoryEditor::FWorkspaceTabHost::StatisticsTabId)
	{
		if (TSharedPtr<SDockTab> DockTab = SpawnedTab.DockTab.Pin())
		{
			DockTab->SetContent(
				SNew(SMultiLineEditableTextBox)
				.Padding(10.0f)
				.Style(FAppStyle::Get(), "Log.TextBox")
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.ForegroundColor(FLinearColor::Gray)
				.IsReadOnly(true)
				.Text(this, &FMetaStoryEditorModeToolkit::GetStatisticsText)
			);
		}
	}
	else if (SpawnedTab.TabID == UE::MetaStoryEditor::FWorkspaceTabHost::DebuggerTabId)
	{
#if WITH_METASTORY_TRACE_DEBUGGER
		WeakDebuggerTab = SpawnedTab.DockTab;
		UpdateDebuggerView();
#endif // WITH_METASTORY_TRACE_DEBUGGER
	}
}

void FMetaStoryEditorModeToolkit::HandleTabClosed(UE::MetaStoryEditor::FSpawnedWorkspaceTab SpawnedTab)
{
	if (TSharedPtr<SDockTab> DockTab = WeakDebuggerTab.Pin())
	{
		// Destroy the inner widget.
		DockTab->SetContent(SNullWidget::NullWidget);
	}
}

FName FMetaStoryEditorModeToolkit::GetToolkitFName() const
{
	return FName("MetaStoryMode");
}

FText FMetaStoryEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "MetaStory Mode");
}


FSlateIcon FMetaStoryEditorModeToolkit::GetCompileStatusImage() const
{
	static const FName CompileStatusBackground("Blueprint.CompileStatus.Background");
	static const FName CompileStatusUnknown("Blueprint.CompileStatus.Overlay.Unknown");
	static const FName CompileStatusError("Blueprint.CompileStatus.Overlay.Error");
	static const FName CompileStatusGood("Blueprint.CompileStatus.Overlay.Good");

	if (UMetaStoryEditorMode* EditorMode = WeakEditorMode.Get())
	{
		UMetaStory* MetaStory = EditorMode->GetStateTree();
		if (MetaStory)
		{
			const bool bCompiledDataResetDuringLoad = MetaStory->LastCompiledEditorDataHash == EditorMode->EditorDataHash && !MetaStory->IsReadyToRun();

			if (!EditorMode->bLastCompileSucceeded || bCompiledDataResetDuringLoad)
			{
				return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusError);
			}

			if (MetaStory->LastCompiledEditorDataHash != EditorMode->EditorDataHash)
			{
				return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusUnknown);
			}

			return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusGood);
		}
	}

	return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusUnknown);
}


namespace UE::MetaStory::Editor::Internal
{
static void MakeSaveOnCompileSubMenu(UToolMenu* InMenu)
{
	FToolMenuSection& Section = InMenu->AddSection("Section");
	const FMetaStoryEditorCommands& Commands = FMetaStoryEditorCommands::Get();
	Section.AddMenuEntry(Commands.SaveOnCompile_Never);
	Section.AddMenuEntry(Commands.SaveOnCompile_SuccessOnly);
	Section.AddMenuEntry(Commands.SaveOnCompile_Always);
}

static void GenerateCompileOptionsMenu(UToolMenu* InMenu)
{
	FToolMenuSection& Section = InMenu->AddSection("Section");

	// @TODO: disable the menu and change up the tooltip when all sub items are disabled
	Section.AddSubMenu(
		"SaveOnCompile",
		LOCTEXT("SaveOnCompileSubMenu", "Save on Compile"),
		LOCTEXT("SaveOnCompileSubMenu_ToolTip", "Determines how the MetaStory is saved whenever you compile it."),
		FNewToolMenuDelegate::CreateStatic(&MakeSaveOnCompileSubMenu));
}
}

void FMetaStoryEditorModeToolkit::ExtendSecondaryModeToolbar(UToolMenu* ToolBar)
{
	ToolBar->Context.AppendCommandList(ToolkitCommands);

	const FMetaStoryEditorCommands& Commands = FMetaStoryEditorCommands::Get();
	ensure(ToolBar->Context.GetActionForCommand(Commands.Compile));

	static const FToolMenuInsert InsertLast(NAME_None, EToolMenuInsertType::Last);

	FToolMenuSection& CompileSection = ToolBar->AddSection("Compile", FText(), InsertLast);

	auto GetToolkitFromAssetEditorContext = [](const UAssetEditorToolkitMenuContext* InContext) -> TSharedPtr<FMetaStoryEditorModeToolkit>
	{
		if (InContext)
		{
			if (TSharedPtr<FAssetEditorToolkit> SharedToolkit = InContext->Toolkit.Pin())
			{
				if(UMetaStoryEditorMode* Mode = Cast<UMetaStoryEditorMode>(SharedToolkit->GetEditorModeManager().GetActiveScriptableMode(UMetaStoryEditorMode::EM_StateTree)))
				{
					if (TSharedPtr<FModeToolkit> Toolkit = Mode->GetToolkit().Pin())
					{
						return StaticCastSharedPtr<FMetaStoryEditorModeToolkit>(Toolkit);
					}
				}
			}
		}

		return nullptr;
	};

	CompileSection.AddDynamicEntry("CompileCommands", FNewToolMenuSectionDelegate::CreateLambda([GetToolkitFromAssetEditorContext](FToolMenuSection& InSection)
	{
		if (UAssetEditorToolkitMenuContext* ToolkitContextObject = InSection.FindContext<UAssetEditorToolkitMenuContext>())
		{
			const FMetaStoryEditorCommands& Commands = FMetaStoryEditorCommands::Get();
			FToolMenuEntry& CompileButton = InSection.AddEntry(FToolMenuEntry::InitToolBarButton(
				Commands.Compile,
				TAttribute<FText>(),
				TAttribute<FText>(),
				TAttribute<FSlateIcon>::CreateLambda([GetToolkitFromAssetEditorContext, ToolkitContextObject]() -> FSlateIcon
				{
					if(TSharedPtr<FMetaStoryEditorModeToolkit> SharedToolkit = GetToolkitFromAssetEditorContext(ToolkitContextObject))
					{
						return SharedToolkit->GetCompileStatusImage();
					}

					return FSlateIcon();
				}))
			);
			CompileButton.Name = "MetaStoryCompile";
			CompileButton.StyleNameOverride = "CalloutToolbar";

			FToolMenuEntry& CompileOptions = InSection.AddEntry(FToolMenuEntry::InitComboButton(
				"CompileComboButton",
				FUIAction(),
				FNewToolMenuDelegate::CreateStatic(&UE::MetaStory::Editor::Internal::GenerateCompileOptionsMenu),
				LOCTEXT("CompileOptions_ToolbarTooltip", "Options to customize how MetaStory assets compile")
			));
			CompileOptions.StyleNameOverride = "CalloutToolbar";
			CompileOptions.ToolBarData.bSimpleComboBox = true;
		}
	}));

	static const FToolMenuInsert InsertAfterCompileSection("Compile", EToolMenuInsertType::After);

	FToolMenuSection& CreateNewNodeSection = ToolBar->AddSection("CreateNewNodes", TAttribute<FText>(), InsertAfterCompileSection);
	CreateNewNodeSection.AddDynamicEntry("CreateNewNodes", FNewToolMenuSectionDelegate::CreateLambda([GetToolkitFromAssetEditorContext](FToolMenuSection& InSection)
	{
		if (UAssetEditorToolkitMenuContext* ToolkitContextObject = InSection.FindContext<UAssetEditorToolkitMenuContext>())
		{
			InSection.AddEntry(FToolMenuEntry::InitComboButton(
				 "CreateNewTaskComboButton",
				 FUIAction(FExecuteAction(), FCanExecuteAction(), FIsActionChecked(),
				 	FIsActionButtonVisible::CreateLambda([ToolkitContextObject, GetToolkitFromAssetEditorContext]() -> bool
					{
				 		if(TSharedPtr<FMetaStoryEditorModeToolkit> SharedToolkit = GetToolkitFromAssetEditorContext(ToolkitContextObject))
					 	{
							 if(UMetaStory* MetaStory = SharedToolkit->EditorHost->GetStateTree())
							 {
								 if (const UMetaStoryEditorData* EditorData = Cast<UMetaStoryEditorData>(MetaStory->EditorData))
								 {
									 if (const UMetaStorySchema* Schema = EditorData->Schema.Get())
									 {
										 return Schema->IsClassAllowed(UMetaStoryTaskBlueprintBase::StaticClass());
									 }
								 }
							 }
						}

						return false;
					})),
				 FOnGetContent::CreateLambda([ToolkitContextObject, GetToolkitFromAssetEditorContext]() -> TSharedRef<SWidget>
				 {
				 	if(TSharedPtr<FMetaStoryEditorModeToolkit> SharedToolkit = GetToolkitFromAssetEditorContext(ToolkitContextObject))
					{
						return SharedToolkit->GenerateTaskBPBaseClassesMenu();
					}

					return SNullWidget::NullWidget;
				 }),
				 LOCTEXT("CreateNewTask_Title", "New Task"),
				 LOCTEXT("CreateNewTask_ToolbarTooltip", "Create a new Blueprint MetaStory task"),
				 GetNewTaskButtonImage()
			 ));

			InSection.AddEntry(FToolMenuEntry::InitComboButton(
				 "CreateNewConditionComboButton",
				 FUIAction(FExecuteAction(), FCanExecuteAction(), FIsActionChecked(),
					 FIsActionButtonVisible::CreateLambda([ToolkitContextObject, GetToolkitFromAssetEditorContext]() -> bool
					{
						if(TSharedPtr<FMetaStoryEditorModeToolkit> SharedToolkit = GetToolkitFromAssetEditorContext(ToolkitContextObject))
						{
							if(UMetaStory* MetaStory = SharedToolkit->EditorHost->GetStateTree())
							{
								if (const UMetaStoryEditorData* EditorData = Cast<UMetaStoryEditorData>(MetaStory->EditorData))
								{
									if (const UMetaStorySchema* Schema = EditorData->Schema.Get())
									{
										return Schema->IsClassAllowed(UMetaStoryConditionBlueprintBase::StaticClass());
									}
								}
							}
						}

						return false;
					})),
				FOnGetContent::CreateLambda([ToolkitContextObject, GetToolkitFromAssetEditorContext]() -> TSharedRef<SWidget>
				{
					if(TSharedPtr<FMetaStoryEditorModeToolkit> SharedToolkit = GetToolkitFromAssetEditorContext(ToolkitContextObject))
					{
					   return SharedToolkit->GenerateConditionBPBaseClassesMenu();
					}

					return SNullWidget::NullWidget;
				}),
				 LOCTEXT("CreateNewCondition_Title", "New Condition"),
				 LOCTEXT("CreateNewCondition_ToolbarTooltip", "Create a new Blueprint MetaStory condition"),
				 GetNewConditionButtonImage()
			 ));

			 InSection.AddEntry(FToolMenuEntry::InitComboButton(
				 "CreateNewConsiderationComboButton",
				 FUIAction(FExecuteAction(), FCanExecuteAction(), FIsActionChecked(),
					 FIsActionButtonVisible::CreateLambda([ToolkitContextObject, GetToolkitFromAssetEditorContext]() -> bool
					{
					 	if(TSharedPtr<FMetaStoryEditorModeToolkit> SharedToolkit = GetToolkitFromAssetEditorContext(ToolkitContextObject))
					 	{
							 if(UMetaStory* MetaStory = SharedToolkit->EditorHost->GetStateTree())
							 {
								 if (const UMetaStoryEditorData* EditorData = Cast<UMetaStoryEditorData>(MetaStory->EditorData))
								 {
									 if (const UMetaStorySchema* Schema = EditorData->Schema.Get())
									 {
										 return Schema->IsClassAllowed(UMetaStoryConsiderationBlueprintBase::StaticClass());
									 }
								 }
							 }
						}

						return false;
					})),
				 FOnGetContent::CreateLambda([ToolkitContextObject, GetToolkitFromAssetEditorContext]() -> TSharedRef<SWidget>
				{
				 	if(TSharedPtr<FMetaStoryEditorModeToolkit> SharedToolkit = GetToolkitFromAssetEditorContext(ToolkitContextObject))
				 	{
						return SharedToolkit->GenerateConsiderationBPBaseClassesMenu();
					}

					return SNullWidget::NullWidget;
				}),
				 LOCTEXT("CreateNewConsideration_Title", "New Consideration"),
				 LOCTEXT("CreateNewConsideration_ToolbarTooltip", "Create a new Blueprint MetaStory utility consideration"),
				 GetNewConsiderationButtonImage()
			 ));
		}
	}));

	const FName MetaStoryEditModeProfile = TEXT("MetaStoryEditModeDisabledProfile");
	FToolMenuProfile* ToolbarProfile = UToolMenus::Get()->AddRuntimeMenuProfile(ToolBar->GetMenuName(), MetaStoryEditModeProfile);
	{
		ToolbarProfile->MenuPermissions.AddDenyListItem("CompileCommands", "MetaStoryCompile");
		ToolbarProfile->MenuPermissions.AddDenyListItem("CreateNewNodes", "CreateNewTaskComboButton");
		ToolbarProfile->MenuPermissions.AddDenyListItem("CreateNewNodes", "CreateNewConditionComboButton");
		ToolbarProfile->MenuPermissions.AddDenyListItem("CreateNewNodes", "CreateNewConsiderationComboButton");
	}
}

void FMetaStoryEditorModeToolkit::OnStateTreeChanged()
{
	// Update underlying state tree state
	UpdateStateTreeOutliner();

#if WITH_METASTORY_TRACE_DEBUGGER
	UpdateDebuggerView();
#endif // WITH_METASTORY_TRACE_DEBUGGER
}


namespace UE::MetaStory::Editor
{
template <typename ClassType, typename = typename TEnableIf<TIsDerivedFrom<ClassType, UMetaStoryNodeBlueprintBase>::Value>::Type>
class FEditorNodeClassFilter : public IClassViewerFilter
{
public:
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		check(InClass);
		return InClass->IsChildOf(ClassType::StaticClass());
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return InUnloadedClassData->IsChildOf(ClassType::StaticClass());
	}
};

using FMetaStoryTaskBPClassFilter = FEditorNodeClassFilter<UMetaStoryTaskBlueprintBase>;
using FMetaStoryConditionBPClassFilter = FEditorNodeClassFilter<UMetaStoryConditionBlueprintBase>;
using FMetaStoryConsiderationBPClassFilter = FEditorNodeClassFilter<UMetaStoryConsiderationBlueprintBase>;
}; // UE::MetaStory::Editor

FSlateIcon FMetaStoryEditorModeToolkit::GetNewTaskButtonImage()
{
	return FSlateIcon("MetaStoryEditorStyle", "MetaStoryEditor.Tasks.Large");
}

TSharedRef<SWidget> FMetaStoryEditorModeToolkit::GenerateTaskBPBaseClassesMenu() const
{
	FClassViewerInitializationOptions Options;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.ClassFilters.Add(MakeShareable(new UE::MetaStory::Editor::FMetaStoryTaskBPClassFilter));

	FOnClassPicked OnPicked(FOnClassPicked::CreateSP(this, &FMetaStoryEditorModeToolkit::OnNodeBPBaseClassPicked));

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

FSlateIcon FMetaStoryEditorModeToolkit::GetNewConditionButtonImage()
{
	return FSlateIcon("MetaStoryEditorStyle", "MetaStoryEditor.Conditions.Large");
}

TSharedRef<SWidget> FMetaStoryEditorModeToolkit::GenerateConditionBPBaseClassesMenu() const
{
	FClassViewerInitializationOptions Options;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.ClassFilters.Add(MakeShareable(new UE::MetaStory::Editor::FMetaStoryConditionBPClassFilter));

	FOnClassPicked OnPicked(FOnClassPicked::CreateSP(this, &FMetaStoryEditorModeToolkit::OnNodeBPBaseClassPicked));

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

FSlateIcon FMetaStoryEditorModeToolkit::GetNewConsiderationButtonImage()
{
    return FSlateIcon("MetaStoryEditorStyle", "MetaStoryEditor.Utility.Large");
}

TSharedRef<SWidget> FMetaStoryEditorModeToolkit::GenerateConsiderationBPBaseClassesMenu() const
{
	FClassViewerInitializationOptions Options;
    Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
    Options.ClassFilters.Add(MakeShareable(new UE::MetaStory::Editor::FMetaStoryConsiderationBPClassFilter));

    FOnClassPicked OnPicked(FOnClassPicked::CreateSP(this, &FMetaStoryEditorModeToolkit::OnNodeBPBaseClassPicked));

    return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

void FMetaStoryEditorModeToolkit::OnNodeBPBaseClassPicked(UClass* NodeClass) const
{
	check(NodeClass);

	UMetaStoryEditorMode* EditorMode = WeakEditorMode.Get();
	if (EditorMode == nullptr)
	{
		return;
	}
	UMetaStory* MetaStory = EditorMode->GetStateTree();
	if (MetaStory == nullptr)
	{
		return;
	}

	const FString ClassName = FBlueprintEditorUtils::GetClassNameWithoutSuffix(NodeClass);
	const FString PathName = FPaths::GetPath(MetaStory->GetOutermost()->GetPathName());

	// Now that we've generated some reasonable default locations/names for the package, allow the user to have the final say
	// before we create the package and initialize the blueprint inside of it.
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = PathName;
	SaveAssetDialogConfig.DefaultAssetName = ClassName + TEXT("_New");
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;

	const FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
	if (!SaveObjectPath.IsEmpty())
	{
		const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
		const FString SavePackagePath = FPaths::GetPath(SavePackageName);
		const FString SaveAssetName = FPaths::GetBaseFilename(SavePackageName);

		if (UPackage* Package = CreatePackage(*SavePackageName))
		{
			// Create and init a new Blueprint
			if (UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(NodeClass, Package, FName(*SaveAssetName), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()))
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewBP);

				// Notify the asset registry
				FAssetRegistryModule::AssetCreated(NewBP);

				Package->MarkPackageDirty();
			}
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

FText FMetaStoryEditorModeToolkit::GetStatisticsText() const
{
	UMetaStoryEditorMode* EditorMode = WeakEditorMode.Get();
	if (EditorMode == nullptr)
	{
		return FText::GetEmpty();
	}

	UMetaStory* MetaStory = EditorMode->GetStateTree();
	if (MetaStory == nullptr)
	{
		return FText::GetEmpty();
	}


   TArray<FMetaStoryMemoryUsage> MemoryUsages = MetaStory->CalculateEstimatedMemoryUsage();
   if (MemoryUsages.IsEmpty())
   {
	return FText::GetEmpty();
   }

	TArray<FText> Rows;
	for (const FMetaStoryMemoryUsage& Usage : MemoryUsages)
	{
		const FText SizeText = FText::AsMemory(Usage.EstimatedMemoryUsage);
		const FText NumNodesText = FText::AsNumber(Usage.NodeCount);
		Rows.Add(FText::Format(LOCTEXT("UsageRow", "{0}: {1}, {2} nodes"), FText::FromString(Usage.Name), SizeText, NumNodesText));
	}

	return FText::Join(FText::FromString(TEXT("\n")), Rows);
}

const FPropertyBindingBindingCollection* FMetaStoryEditorModeToolkit::GetBindingCollection() const
{
	if (TStrongObjectPtr<UMetaStoryEditorMode> EditorMode = WeakEditorMode.Pin())
	{
		if (UMetaStory* MetaStory = EditorMode->GetStateTree())
		{
			if (UMetaStoryEditingSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
			{
				if (const UMetaStoryEditorData* EditorData = Subsystem->FindOrAddViewModel(MetaStory)->GetStateTreeEditorData())
				{
					return EditorData->GetPropertyEditorBindings();
				}
			}
		}
	}
	return nullptr;
}

void FMetaStoryEditorModeToolkit::UpdateStateTreeOutliner()
{
	MetaStoryOutliner = SNullWidget::NullWidget;
	if (UMetaStoryEditorMode* EditorMode = WeakEditorMode.Get())
	{
		if (EditorMode && EditorMode->GetStateTree())
		{
			if (UMetaStoryEditingSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
			{
				MetaStoryOutliner = SNew(SMetaStoryOutliner, Subsystem->FindOrAddViewModel(EditorMode->GetStateTree()), GetToolkitCommands());
			}
		}
	}

	if(TSharedPtr<SDockTab> SharedOutlinerTab = WeakOutlinerTab.Pin())
	{
		SharedOutlinerTab->SetContent(MetaStoryOutliner.ToSharedRef());
	}
}

#if WITH_METASTORY_TRACE_DEBUGGER
void FMetaStoryEditorModeToolkit::UpdateDebuggerView()
{
	TSharedRef<SWidget> DebuggerView = SNullWidget::NullWidget;
	TSharedPtr<SDockTab> SharedDebuggerTab = WeakDebuggerTab.Pin();

	if (SharedDebuggerTab)
	{
		// Clear any references the previous tab might have to a previous debugger view.
		// The view will clear the shared debugger's bindings on dtor.
		// We don't want it to clear newly created bindings from our new view.
		SharedDebuggerTab->SetContent(DebuggerView);
	}

	if (UMetaStoryEditorMode* EditorMode = WeakEditorMode.Get())
	{
		if (UMetaStory* MetaStory = EditorMode->GetStateTree())
		{
			if (UMetaStoryEditingSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>())
			{
				DebuggerView = SNew(SMetaStoryDebuggerView, MetaStory, Subsystem->FindOrAddViewModel(MetaStory), GetToolkitCommands());
			}
		}
	}

	if (SharedDebuggerTab)
	{
		SharedDebuggerTab->SetContent(DebuggerView);
	}
}
#endif // WITH_METASTORY_TRACE_DEBUGGER

#undef LOCTEXT_NAMESPACE // "MetaStoryModeToolkit"
