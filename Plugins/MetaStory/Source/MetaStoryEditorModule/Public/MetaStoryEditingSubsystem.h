// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStory.h"
#include "MetaStoryViewModel.h"
#include "EditorSubsystem.h"
#include "UObject/ObjectKey.h"

#include "MetaStoryEditingSubsystem.generated.h"

#define UE_API METASTORYEDITORMODULE_API

class SWidget;
class FMetaStoryViewModel;
class FUICommandList;
struct FMetaStoryCompilerLog;

UCLASS(MinimalAPI)
class UMetaStoryEditingSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
public:
	UE_API UMetaStoryEditingSubsystem();
	UE_API virtual void BeginDestroy() override;
	
	UE_API TSharedPtr<FMetaStoryViewModel> FindViewModel(TNotNull<const UMetaStory*> InMetaStory) const;
	UE_API TSharedRef<FMetaStoryViewModel> FindOrAddViewModel(TNotNull<UMetaStory*> InMetaStory);
	
	static UE_API bool CompileMetaStory(TNotNull<UMetaStory*> InMetaStory,  FMetaStoryCompilerLog& InOutLog);
	
	/** Create a MetaStoryView widget for the viewmodel. */
	static UE_API TSharedRef<SWidget> GetMetaStoryDocumentView(TSharedRef<FMetaStoryViewModel> InViewModel, const TSharedRef<FUICommandList>& TreeViewCommandList);
	
	/**
	 * Validates and applies the schema restrictions on the MetaStory.
	 * Also serves as the "safety net" of fixing up editor data following an editor operation.
	 * Updates state's link, removes the unused node while validating the MetaStory asset.
	 */
	static UE_API void ValidateMetaStory(TNotNull<UMetaStory*> InMetaStory);

	/** Calculates editor data hash of the asset. */
	static UE_API uint32 CalculateMetaStoryHash(TNotNull<const UMetaStory*> InMetaStory);
	
private:
	void HandlePostGarbageCollect();
	void HandlePostCompile(const UMetaStory& InMetaStory);

protected:
	TMap<FObjectKey, TSharedPtr<FMetaStoryViewModel>> MetaStoryViewModels;
	FDelegateHandle PostGarbageCollectHandle;
	FDelegateHandle PostCompileHandle;
};

#undef UE_API
