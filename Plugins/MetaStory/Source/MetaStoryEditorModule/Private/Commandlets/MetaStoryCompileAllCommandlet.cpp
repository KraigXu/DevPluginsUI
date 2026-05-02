// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/MetaStoryCompileAllCommandlet.h"
#include "Modules/ModuleManager.h"
#include "MetaStory.h"
#include "PackageHelperFunctions.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryEditor.h"
#include "MetaStoryCompiler.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryEditingSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryCompileAllCommandlet)

DEFINE_LOG_CATEGORY_STATIC(LogMetaStoryCompile, Log, Log);

UMetaStoryCompileAllCommandlet::UMetaStoryCompileAllCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UMetaStoryCompileAllCommandlet::Main(const FString& Params)
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	// want everything in upper case, it's a mess otherwise
	const FString ParamsUpperCase = Params.ToUpper();
	const TCHAR* Parms = *ParamsUpperCase;
	ParseCommandLine(Parms, Tokens, Switches);

	// Source control
	bool bNoSourceControl = Switches.Contains(TEXT("nosourcecontrol"));
	FScopedSourceControl SourceControl;
	SourceControlProvider = bNoSourceControl ? nullptr : &SourceControl.GetProvider();

	// Load assets
	UE_LOG(LogMetaStoryCompile, Display, TEXT("Loading Asset Registry..."));
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
	AssetRegistryModule.Get().SearchAllAssets(/*bSynchronousSearch =*/true);
	UE_LOG(LogMetaStoryCompile, Display, TEXT("Finished Loading Asset Registry."));
	
	UE_LOG(LogMetaStoryCompile, Display, TEXT("Gathering All MetaStorys From Asset Registry..."));
	TArray<FAssetData> MetaStoryAssetList;
	AssetRegistryModule.Get().GetAssetsByClass(UMetaStory::StaticClass()->GetClassPathName(), MetaStoryAssetList, /*bSearchSubClasses*/false);

	int32 Counter = 0;
	for (const FAssetData& Asset : MetaStoryAssetList)
	{
		const FString ObjectPath = Asset.GetObjectPathString();
		UE_LOG(LogMetaStoryCompile, Display, TEXT("Loading and Compiling: '%s' [%d/%d]..."), *ObjectPath, Counter+1, MetaStoryAssetList.Num());

		UMetaStory* MetaStory = Cast<UMetaStory>(StaticLoadObject(Asset.GetClass(), /*Outer*/nullptr, *ObjectPath, /*FileName*/nullptr, LOAD_NoWarn));
		if (MetaStory == nullptr)
		{
			UE_LOG(LogMetaStoryCompile, Error, TEXT("Failed to Load: '%s'."), *ObjectPath);
		}
		else
		{
			CompileAndSaveMetaStory(MetaStory);
		}
		Counter++;
	}
		
	return 0;
}

bool UMetaStoryCompileAllCommandlet::CompileAndSaveMetaStory(TNonNullPtr<UMetaStory> MetaStory) const
{
	UPackage* Package = MetaStory->GetPackage();
	const FString PackageFileName = SourceControlHelpers::PackageFilename(Package);

	// Compile the MetaStory asset.
	UMetaStoryEditingSubsystem::ValidateMetaStory(MetaStory);
	const uint32 EditorDataHash = UMetaStoryEditingSubsystem::CalculateMetaStoryHash(MetaStory);

	FMetaStoryCompilerLog Log;
	const bool bSuccess = UMetaStoryEditingSubsystem::CompileMetaStory(MetaStory, Log);

	if (!bSuccess)
	{
		return false;
	}

	// Check out the MetaStory asset
	if (SourceControlProvider != nullptr)
	{
		const FSourceControlStatePtr SourceControlState = SourceControlProvider->GetState(PackageFileName, EStateCacheUsage::ForceUpdate);

		if (SourceControlState.IsValid())
		{
			FString OtherCheckedOutUser;
			if (SourceControlState->IsCheckedOutOther(&OtherCheckedOutUser))
			{
				UE_LOG(LogMetaStoryCompile, Error, TEXT("Overwriting package %s already checked out by %s, will not submit"), *PackageFileName, *OtherCheckedOutUser);
				return false;
			}
			else if (!SourceControlState->IsCurrent())
			{
				UE_LOG(LogMetaStoryCompile, Error, TEXT("Overwriting package %s (not at head revision), will not submit"), *PackageFileName);
				return false;
			}
			else if (SourceControlState->IsCheckedOut() || SourceControlState->IsAdded())
			{
				UE_LOG(LogMetaStoryCompile, Log, TEXT("Package %s already checked out"), *PackageFileName);
			}
			else if (SourceControlState->IsSourceControlled())
			{
				UE_LOG(LogMetaStoryCompile, Log, TEXT("Checking out package %s from revision control"), *PackageFileName);
				if (SourceControlProvider->Execute(ISourceControlOperation::Create<FCheckOut>(), PackageFileName) != ECommandResult::Succeeded)
				{
					UE_LOG(LogMetaStoryCompile, Log, TEXT("Failed to check out package %s from revision control"), *PackageFileName);
					return false;
				}
			}
		}
	}

	// Save MetaStory asset.
	if (!SavePackageHelper(Package, PackageFileName))
	{
		UE_LOG(LogMetaStoryCompile, Error, TEXT("Failed to save %s."), *PackageFileName);
		return false;
	}

	UE_LOG(LogMetaStoryCompile, Log, TEXT("Compile and save %s succeeded."), *PackageFileName);

	return true;
}

