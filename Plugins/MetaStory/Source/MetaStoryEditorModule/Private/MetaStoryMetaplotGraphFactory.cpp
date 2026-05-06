// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryMetaplotGraphFactory.h"

#include "MetaStory.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryEditingSubsystem.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorModule.h"
#include "MetaStoryEditorSchema.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryMetaplotGraphFactory)

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

UMetaStoryMetaplotGraphFactory::UMetaStoryMetaplotGraphFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMetaStory::StaticClass();
}

UObject* UMetaStoryMetaplotGraphFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (MetaStorySchemaClass == nullptr)
	{
		return nullptr;
	}

	UMetaStory* NewMetaStory = NewObject<UMetaStory>(InParent, Class, Name, Flags);

	TNonNullSubclassOf<UMetaStoryEditorData> EditorDataClass = FMetaStoryEditorModule::GetModule().GetEditorDataClass(MetaStorySchemaClass.Get());
	UMetaStoryEditorData* EditorData = NewObject<UMetaStoryEditorData>(NewMetaStory, EditorDataClass, FName(), RF_Transactional);
	NewMetaStory->EditorData = EditorData;

	EditorData->Schema = NewObject<UMetaStorySchema>(EditorData, MetaStorySchemaClass, FName(), RF_Transactional);
	EditorData->bUseMetaplotFlowTopology = true;
	EditorData->EnsureEmbeddedMetaplotFlow();

	TNonNullSubclassOf<UMetaStoryEditorSchema> EditorSchemaClass = FMetaStoryEditorModule::GetModule().GetEditorSchemaClass(MetaStorySchemaClass.Get());
	EditorData->EditorSchema = NewObject<UMetaStoryEditorSchema>(EditorData, EditorSchemaClass, FName(), RF_Transactional);

	FMetaStoryCompilerLog Log;
	const bool bSuccess = UMetaStoryEditingSubsystem::CompileMetaStory(NewMetaStory, Log);

	if (!bSuccess)
	{
		NewMetaStory = nullptr;
	}

	return NewMetaStory;
}

#undef LOCTEXT_NAMESPACE
