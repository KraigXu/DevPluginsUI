// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdMode.h"
#include "MetaStory.h"
#include "MetaStoryEditorData.h"

#include "MetaStoryEditorMode.generated.h"

#define UE_API METASTORYEDITORMODULE_API

class FMetaStoryBindingExtension;
class FMetaStoryBindingsChildrenCustomization;
class IDetailsView;
class IMessageLogListing;

UCLASS(MinimalAPI, Transient)
class UMetaStoryEditorMode : public UEdMode
{
	GENERATED_BODY()
public:
	UE_API const static FEditorModeID EM_StateTree;
	
	UE_API UMetaStoryEditorMode();

	UE_API virtual void Enter() override;
	UE_API virtual void Exit() override;
	UE_API virtual void CreateToolkit() override;
	UE_API virtual void BindCommands() override;

protected:
	UE_API void OnStateTreeChanged();
	UE_API void BindToolkitCommands(const TSharedRef<FUICommandList>& ToolkitCommands);

	UE_API void OnPropertyBindingChanged(const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath);
	UE_API void OnIdentifierChanged(const UMetaStory& InStateTree);
	UE_API void OnSchemaChanged(const UMetaStory& InStateTree);
	UE_API void ForceRefreshDetailsView() const;
	UE_API void OnRefreshDetailsView(const UMetaStory& InStateTree) const;
	UE_API void OnStateParametersChanged(const UMetaStory& InStateTree, const FGuid ChangedStateID) const;

	UE_API void HandleMessageTokenClicked(const TSharedRef<IMessageToken>& InMessageToken) const;

	UE_API void OnAssetFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) const;
	UE_API void OnSelectionFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);

	UE_API void Compile();
	UE_API bool CanCompile() const;
	UE_API bool IsCompileVisible() const;

	UE_API bool HasValidStateTree() const;
	
	UE_API void HandleModelAssetChanged();
	UE_API void HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UMetaStoryState>>& SelectedStates) const;
	UE_API void HandleModelBringNodeToFocus(const UMetaStoryState* State, const FGuid NodeID) const;

	void HandleStateAdded(UMetaStoryState* , UMetaStoryState*)
	{
		UpdateAsset();
	}

	void HandleStatesRemoved(const TSet<UMetaStoryState*>&)
	{
		UpdateAsset();
	}

	void HandleOnStatesMoved(const TSet<UMetaStoryState*>&, const TSet<UMetaStoryState*>&)
	{
		UpdateAsset();
	}

	void HandleOnStateNodesChanged(const UMetaStoryState*)
	{
		UpdateAsset();

		// when node is being changed from the view model, details view might have already been opened and in that case, we need to rebuild
		ForceRefreshDetailsView();
	}

	/** Resolve the internal editor data and fixup the MetaStory nodes. */
	UE_API void UpdateAsset();

	UE_API TSharedPtr<IDetailsView> GetDetailsView() const;
	UE_API TSharedPtr<IDetailsView> GetAssetDetailsView() const;
	
	UE_API TSharedPtr<IMessageLogListing> GetMessageLogListing() const;	
	UE_API void ShowCompilerTab() const;

	UE_API UMetaStory* GetStateTree() const;
	
	friend class FMetaStoryEditorModeToolkit;

protected:
	uint32 EditorDataHash = 0;
	bool bLastCompileSucceeded = true;
	bool bForceAssetDetailViewToRefresh = false;

	mutable FTimerHandle SetObjectTimerHandle;
	mutable FTimerHandle HighlightTimerHandle;

	TWeakObjectPtr<UMetaStory> CachedStateTree = nullptr;

	TSharedPtr<FMetaStoryBindingExtension> DetailsViewExtensionHandler;
	TSharedPtr<FMetaStoryBindingsChildrenCustomization> DetailsViewChildrenCustomizationHandler;
};

#undef UE_API
