// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryFactory.h"
#include "MetaStory.h"
#include "Kismet2/SClassPickerDialog.h"
#include "ClassViewerFilter.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryCompiler.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryEditor.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorModule.h"
#include "MetaStoryEditorSchema.h"
#include "MetaStoryEditingSubsystem.h"
#include "MetaStoryNodeClassCache.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryFactory)

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

/////////////////////////////////////////////////////
// FMetaStoryClassFilter

class FMetaStoryClassFilter : public IClassViewerFilter
{
public:
	/** All children of these classes will be included unless filtered out by another setting. */
	TSet<const UClass*> AllowedChildrenOfClasses;

	/** Disallowed class flags. */
	EClassFlags DisallowedClassFlags = CLASS_None;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return !InClass->HasAnyClassFlags(DisallowedClassFlags)
			&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
	}
	
	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return !InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags)
			&& InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
	}

};

/////////////////////////////////////////////////////
// UMetaStoryFactory

UMetaStoryFactory::UMetaStoryFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMetaStory::StaticClass();
}

bool UMetaStoryFactory::ConfigureProperties()
{
	// Load the classviewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Load MetaStory class cache to get all schema classes
	FMetaStoryEditorModule& EditorModule = FModuleManager::GetModuleChecked<FMetaStoryEditorModule>(TEXT("MetaStoryEditorModule"));
	FMetaStoryNodeClassCache* ClassCache = EditorModule.GetNodeClassCache().Get();
	check(ClassCache);

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.DisplayMode = EClassViewerDisplayMode::Type::TreeView;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.bShowNoneOption = false;
	Options.bExpandAllNodes = true;

	TSharedPtr<FMetaStoryClassFilter> Filter = MakeShareable(new FMetaStoryClassFilter);
	Options.ClassFilters.Add(Filter.ToSharedRef());

	// Add all schemas which are tagged as "CommonSchema" to the common section.
	TArray<TSharedPtr<FMetaStoryNodeClassData>> AvailableClasses;
	ClassCache->GetClasses(UMetaStorySchema::StaticClass(), AvailableClasses);
	for (const TSharedPtr<FMetaStoryNodeClassData>& ClassData : AvailableClasses)
	{
		if (FMetaStoryNodeClassData* Data = ClassData.Get())
		{
			if (UClass* Class = Data->GetClass())
			{
				if (Class->HasMetaData("CommonSchema"))
				{
					Options.ExtraPickerCommonClasses.Add(Class);					
				}
			}
		}
	}

	Filter->DisallowedClassFlags = CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Abstract | CLASS_HideDropDown;
	Filter->AllowedChildrenOfClasses.Add(UMetaStorySchema::StaticClass());

	const FText TitleText = LOCTEXT("CreateMetaStory", "Pick Schema for MetaStory");

	UClass* ChosenClass = nullptr;
	const bool bResult = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UMetaStorySchema::StaticClass());
	MetaStorySchemaClass = ChosenClass;

	return bResult;
}

void UMetaStoryFactory::SetSchemaClass(const TObjectPtr<UClass>& InSchemaClass)
{
	MetaStorySchemaClass = InSchemaClass;
}

UObject* UMetaStoryFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (MetaStorySchemaClass == nullptr)
	{
		return nullptr;
	}

	// Create new asset
	UMetaStory* NewMetaStory = NewObject<UMetaStory>(InParent, Class, Name, Flags);

	// Create and init new editor data.
	TNonNullSubclassOf<UMetaStoryEditorData> EditorDataClass = FMetaStoryEditorModule::GetModule().GetEditorDataClass(MetaStorySchemaClass.Get());
	UMetaStoryEditorData* EditorData = NewObject<UMetaStoryEditorData>(NewMetaStory, EditorDataClass, FName(), RF_Transactional);
	NewMetaStory->EditorData = EditorData;

	EditorData->Schema = NewObject<UMetaStorySchema>(EditorData, MetaStorySchemaClass, FName(), RF_Transactional);
	EditorData->AddRootState();

	TNonNullSubclassOf<UMetaStoryEditorSchema> EditorSchemaClass = FMetaStoryEditorModule::GetModule().GetEditorSchemaClass(MetaStorySchemaClass.Get());
	EditorData->EditorSchema = NewObject<UMetaStoryEditorSchema>(EditorData, EditorSchemaClass, FName(), RF_Transactional);

	FMetaStoryCompilerLog Log;
	const bool bSuccess = UMetaStoryEditingSubsystem::CompileStateTree(NewMetaStory, Log);

	if (!bSuccess)
	{
		NewMetaStory = nullptr;
	}
	
	return NewMetaStory;
}

#undef LOCTEXT_NAMESPACE

