// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryEditorMode.h"
#include "IMetaStoryEditorHost.h"
#include "Toolkits/BaseToolkit.h"

#define UE_API METASTORYEDITORMODULE_API

class UMetaStoryEditorMode;
struct FPropertyBindingBindingCollection;
namespace UE::MetaStoryEditor
{
	struct FSpawnedWorkspaceTab;
}

class FMetaStoryEditorModeToolkit : public FModeToolkit
{
public:

	UE_API FMetaStoryEditorModeToolkit(UMetaStoryEditorMode* InEditorMode);
	
	/** IToolkit interface */
	UE_API virtual FName GetToolkitFName() const override;
	UE_API virtual FText GetBaseToolkitName() const override;

	UE_API virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	UE_API virtual void InvokeUI() override;
	UE_API virtual void RequestModeUITabs() override;
	
	UE_API virtual void ExtendSecondaryModeToolbar(UToolMenu* InModeToolbarMenu) override;
		
	UE_API void OnMetaStoryChanged();

protected:
	UE_API FSlateIcon GetCompileStatusImage() const;

	static UE_API FSlateIcon GetNewTaskButtonImage();
	UE_API TSharedRef<SWidget> GenerateTaskBPBaseClassesMenu() const;

	static UE_API FSlateIcon GetNewConditionButtonImage();
	UE_API TSharedRef<SWidget> GenerateConditionBPBaseClassesMenu() const;
    
	static UE_API FSlateIcon GetNewConsiderationButtonImage();
	UE_API TSharedRef<SWidget> GenerateConsiderationBPBaseClassesMenu() const;

	UE_API void OnNodeBPBaseClassPicked(UClass* NodeClass) const;
	
	UE_API FText GetStatisticsText() const;
	UE_API const FPropertyBindingBindingCollection* GetBindingCollection() const;

	UE_API void UpdateMetaStoryOutliner();

	UE_API void HandleTabSpawned(UE::MetaStoryEditor::FSpawnedWorkspaceTab SpawnedTab);
	UE_API void HandleTabClosed(UE::MetaStoryEditor::FSpawnedWorkspaceTab SpawnedTab);

protected:
	TWeakObjectPtr<UMetaStoryEditorMode> WeakEditorMode;
	TSharedPtr<IMetaStoryEditorHost> EditorHost;

	/** Tree Outliner */
	TSharedPtr<SWidget> MetaStoryOutliner = nullptr;
	TWeakPtr<SDockTab> WeakOutlinerTab = nullptr;

#if WITH_METASTORY_TRACE_DEBUGGER
	UE_API void UpdateDebuggerView();
	TWeakPtr<SDockTab> WeakDebuggerTab = nullptr;
#endif // WITH_METASTORY_TRACE_DEBUGGER

};


#undef UE_API
