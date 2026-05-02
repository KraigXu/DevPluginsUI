// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryCompilerManager.h"
#include "MetaStory.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryEditingSubsystem.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorModule.h"
#include "MetaStoryEditorPropertyBindings.h"

#include "Editor.h"
#include "StructUtilsDelegates.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/ObjectKey.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

namespace UE::MetaStory::Compiler::Private
{
FAutoConsoleVariable CVarLogMetaStoryDependencies(
	TEXT("MetaStory.Compiler.LogDependenciesOnCompilation"),
	false,
	TEXT("After a MetaStory compiles, log the dependencies that will be required for the asset to recompile.")
);

bool bUseDependenciesToTriggerCompilation = true;
FAutoConsoleVariableRef CVarLogMetaStoryUseDependenciesToTriggerCompilation(
	TEXT("MetaStory.Compiler.UseDependenciesToTriggerCompilation"),
	bUseDependenciesToTriggerCompilation,
	TEXT("Use the build dependencies to detect when a MetaStory needs to be linked or compiled.")
);

struct FMetaStoryDependencies
{
	enum EDependencyType
	{
		DT_None = 0,
		DT_Link = 1 << 0,
		DT_Internal = 1<< 1,
		DT_Public = 1 << 2,
	};
	struct FItem
	{
		FObjectKey Key;
		EDependencyType Type = EDependencyType::DT_None;
	};
	TArray<FItem> Dependencies;
};
ENUM_CLASS_FLAGS(FMetaStoryDependencies::EDependencyType);

/** Find the references that are needed by the asset. */
class FArchiveReferencingProperties : public FArchiveUObject
{
public:
	FArchiveReferencingProperties(TNotNull<const UObject*> InReferencingObject)
		: ReferencingObjectPackage(InReferencingObject->GetPackage())
		, MetaStoryModulePackage(UMetaStory::StaticClass()->GetOutermost())
		, MetaStoryEditorModulePackage(UMetaStoryEditorData::StaticClass()->GetOutermost())
		, CoreUObjectModulePackage(UObject::StaticClass()->GetOutermost())
	{
		ArIsObjectReferenceCollector = true;
		ArIgnoreOuterRef = true;
		ArIgnoreArchetypeRef = true;
		ArIgnoreClassGeneratedByRef = true;
		ArIgnoreClassRef = true;

		SetShouldSkipCompilingAssets(false);
	}

	bool IsSupportedObject(TNotNull<const UStruct*> Struct)
	{
		/** As an optimization, do not include basic structures like FVector and MetaStory internal types. */
		return !Struct->IsInPackage(MetaStoryModulePackage)
			&& !Struct->IsInPackage(MetaStoryEditorModulePackage)
			&& !Struct->IsInPackage(CoreUObjectModulePackage);
	}

	virtual FArchive& operator<<(UObject*& InSerializedObject) override
	{
		if (InSerializedObject)
		{
			if (const UStruct* AsStruct = Cast<const UStruct>(InSerializedObject))
			{
				if (IsSupportedObject(AsStruct))
				{
					Dependencies.Add(AsStruct);
				}
			}
			else
			{
				if (IsSupportedObject(InSerializedObject->GetClass()))
				{
					Dependencies.Add(InSerializedObject->GetClass());
				}
			}

			// Traversing the asset inner dependencies (instanced type).
			if (InSerializedObject->IsInPackage(ReferencingObjectPackage))
			{
				bool bAlreadyExists;
				SerializedObjects.Add(InSerializedObject, &bAlreadyExists);

				if (!bAlreadyExists)
				{
					InSerializedObject->Serialize(*this);
				}
			}
		}

		return *this;
	}

	TSet<TNotNull<const UStruct*>> Dependencies;

private:
	/** Tracks the objects which have been serialized by this archive, to prevent recursion */
	TSet<UObject*> SerializedObjects;

	TNotNull<const UPackage*> ReferencingObjectPackage;
	TNotNull<const UPackage*> MetaStoryModulePackage;
	TNotNull<const UPackage*> MetaStoryEditorModulePackage;
	TNotNull<const UPackage*> CoreUObjectModulePackage;
};

class FCompilerManagerImpl
{
public:
	FCompilerManagerImpl();
	~FCompilerManagerImpl();
	FCompilerManagerImpl(const FCompilerManagerImpl&) = delete;
	FCompilerManagerImpl& operator=(const FCompilerManagerImpl&) = delete;

	bool CompileInternalSynchronously(TNotNull<UMetaStory*> InMetaStory, FMetaStoryCompilerLog& InOutLog);

private:
	bool HandleCompileMetaStory(UMetaStory& MetaStory);
	void UpdateBindingsInstanceStructsIfNeeded(TSet<const UStruct*>& Structs, TNotNull<UMetaStoryEditorData*> EditorData);
	void GatherDependencies(TNotNull<UMetaStory*> MetaStory);
	void LogDependencies(TNotNull<UMetaStory*> MetaStory) const;

	using FReplacementObjectMap = TMap<UObject*, UObject*>;
	void HandleObjectsReinstanced(const FReplacementObjectMap& ObjectMap);
	void HandlePreBeginPIE(bool bIsSimulating);
	void HandleUserDefinedStructReinstanced(const UUserDefinedStruct& UserDefinedStruct);

private:
	FDelegateHandle ObjectsReinstancedHandle;
	FDelegateHandle UserDefinedStructReinstancedHandle;
	FDelegateHandle PreBeginPIEHandle;

	TMap<TObjectKey<UMetaStory>, TSharedPtr<FMetaStoryDependencies>> MetaStoryToDependencies;
	TMap<FObjectKey, TArray<TObjectKey<UMetaStory>>> DependenciesToMetaStory;
};
static TUniquePtr<FCompilerManagerImpl> CompilerManagerImpl;

FCompilerManagerImpl::FCompilerManagerImpl()
{
	UE::MetaStory::Delegates::OnRequestCompile.BindRaw(this, &FCompilerManagerImpl::HandleCompileMetaStory);
	ObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddRaw(this, &FCompilerManagerImpl::HandleObjectsReinstanced);
	UserDefinedStructReinstancedHandle = UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.AddRaw(this, &FCompilerManagerImpl::HandleUserDefinedStructReinstanced);
	PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(this, &FCompilerManagerImpl::HandlePreBeginPIE);
}

FCompilerManagerImpl::~FCompilerManagerImpl()
{
	FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
	UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.Remove(UserDefinedStructReinstancedHandle);
	FCoreUObjectDelegates::OnObjectsReinstanced.Remove(ObjectsReinstancedHandle);
	UE::MetaStory::Delegates::OnRequestCompile.Unbind();
}

bool FCompilerManagerImpl::CompileInternalSynchronously(TNotNull<UMetaStory*> MetaStory, FMetaStoryCompilerLog& Log)
{
	UMetaStoryEditingSubsystem::ValidateMetaStory(MetaStory);
	FMetaStoryCompiler Compiler(Log);
	const bool bCompilationResult = Compiler.Compile(MetaStory);
	if (bCompilationResult)
	{
		const uint32 EditorDataHash = UMetaStoryEditingSubsystem::CalculateMetaStoryHash(MetaStory);

		// Success
		MetaStory->LastCompiledEditorDataHash = EditorDataHash;
		UE_LOG(LogMetaStoryEditor, Log, TEXT("Compile MetaStory '%s' succeeded."), *MetaStory->GetFullName());
	}
	else
	{
		// Make sure not to leave stale data on failed compile.
		MetaStory->ResetCompiled();
		MetaStory->LastCompiledEditorDataHash = 0;

		UE_LOG(LogMetaStoryEditor, Error, TEXT("Failed to compile '%s', errors follow."), *MetaStory->GetFullName());
		Log.DumpToLog(LogMetaStoryEditor);
	}

	UE::MetaStory::Delegates::OnPostCompile.Broadcast(*MetaStory);

	GatherDependencies(MetaStory);

	if (CVarLogMetaStoryDependencies->GetBool())
	{
		LogDependencies(MetaStory);
	}

	return bCompilationResult;
}

void FCompilerManagerImpl::UpdateBindingsInstanceStructsIfNeeded(TSet<const UStruct*>& Structs, TNotNull<UMetaStoryEditorData*> EditorData)
{
	bool bShouldUpdate = false;
	EditorData->VisitAllNodes([&Structs, &bShouldUpdate](const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)
		{
			if (Structs.Contains(Value.GetStruct()))
			{
				bShouldUpdate = true;
				return EMetaStoryVisitor::Break;
			}
			return EMetaStoryVisitor::Continue;
		});

	if (!bShouldUpdate)
	{
		bShouldUpdate = EditorData->GetEditorPropertyBindings()->ContainsAnyStruct(Structs);
	}

	if (bShouldUpdate)
	{
		EditorData->UpdateBindingsInstanceStructs();
	}
}

bool FCompilerManagerImpl::HandleCompileMetaStory(UMetaStory& MetaStory)
{
	FMetaStoryCompilerLog Log;
	return CompileInternalSynchronously(&MetaStory, Log);
}

void FCompilerManagerImpl::HandlePreBeginPIE(const bool bIsSimulating)
{
	for (TObjectIterator<UMetaStory> It; It; ++It)
	{
		check(!It->HasAnyFlags(RF_ClassDefaultObject));
		It->CompileIfChanged();
	}
}

void FCompilerManagerImpl::HandleObjectsReinstanced(const FReplacementObjectMap& ObjectMap)
{
	if (ObjectMap.IsEmpty())
	{
		return;
	}

	TArray<const UObject*> ObjectsToBeReplaced;
	ObjectsToBeReplaced.Reserve(ObjectMap.Num());
	for (TMap<UObject*, UObject*>::TConstIterator It(ObjectMap); It; ++It)
	{
		if (const UObject* ObjectToBeReplaced = It->Value)
		{
			ObjectsToBeReplaced.Add(ObjectToBeReplaced);
		}
	}

	TSet<const UStruct*> StructsToBeReplaced;
	StructsToBeReplaced.Reserve(ObjectsToBeReplaced.Num());
	for (const UObject* ObjectToBeReplaced : ObjectsToBeReplaced)
	{
		// It's a UClass or a UScriptStruct
		if (const UStruct* StructToBeReplaced = Cast<const UStruct>(ObjectToBeReplaced))
		{
			StructsToBeReplaced.Add(StructToBeReplaced);
		}
		else
		{
			StructsToBeReplaced.Add(ObjectToBeReplaced->GetClass());
		}
	}

	if (UE::MetaStory::Compiler::Private::bUseDependenciesToTriggerCompilation)
	{
		TArray<TNotNull<UMetaStory*>> MetaStoryToLink;
		for (const UStruct* StructToBeReplaced : StructsToBeReplaced)
		{
			const FObjectKey StructToReplacedKey = StructToBeReplaced;
			TArray<TObjectKey<UMetaStory>>* Dependencies = DependenciesToMetaStory.Find(StructToReplacedKey);
			if (Dependencies)
			{
				for (const TObjectKey<UMetaStory>& MetaStoryKey : *Dependencies)
				{
					if (UMetaStory* MetaStory = MetaStoryKey.ResolveObjectPtr())
					{
						MetaStoryToLink.AddUnique(MetaStory);
					}
				}
			}
		}

		for (UMetaStory* MetaStory : MetaStoryToLink)
		{
			if (!MetaStory->Link())
			{
				UE_LOG(LogMetaStory, Error, TEXT("%s failed to link after Object reinstantiation. Take a look at the asset for any errors. Asset will not be usable at runtime."), *MetaStory->GetPathName());
			}
		}
	}
	else
	{
		for (TObjectIterator<UMetaStoryEditorData> It; It; ++It)
		{
			UMetaStoryEditorData* MetaStoryEditorData = *It;
			UpdateBindingsInstanceStructsIfNeeded(StructsToBeReplaced, MetaStoryEditorData);
		}

		for (TObjectIterator<UMetaStory> It; It; ++It)
		{
			UMetaStory* MetaStory = *It;
			check(!MetaStory->HasAnyFlags(RF_ClassDefaultObject));
			bool bShouldRelink = false;

			// Relink if one of the out of date objects got reinstanced.
			if (MetaStory->OutOfDateStructs.Num() > 0)
			{
				for (const FObjectKey& OutOfDateObjectKey : MetaStory->OutOfDateStructs)
				{
					if (const UObject* OutOfDateObject = OutOfDateObjectKey.ResolveObjectPtr())
					{
						if (ObjectMap.Contains(OutOfDateObject))
						{
							bShouldRelink = true;
							break;
						}
					}
				}
			}

			// If the asset is not linked yet (or has failed), no need to link.
			if (!bShouldRelink && !MetaStory->bIsLinked)
			{
				continue;
			}

			// Relink only if the reinstantiated object belongs to this asset,
			// or anything from the property binding refers to the classes of the reinstantiated object.
			if (!bShouldRelink)
			{
				for (const UObject* ObjectToBeReplaced : ObjectsToBeReplaced)
				{
					if (ObjectToBeReplaced->IsInOuter(MetaStory))
					{
						bShouldRelink = true;
						break;
					}
				}
			}

			if (!bShouldRelink)
			{
				bShouldRelink |= MetaStory->PropertyBindings.ContainsAnyStruct(StructsToBeReplaced);
			}

			if (bShouldRelink)
			{
				if (!MetaStory->Link())
				{
					UE_LOG(LogMetaStory, Error, TEXT("%s failed to link after Object reinstantiation. Take a look at the asset for any errors. Asset will not be usable at runtime."), *MetaStory->GetPathName());
				}
			}
		}
	}
}

void FCompilerManagerImpl::HandleUserDefinedStructReinstanced(const UUserDefinedStruct& UserDefinedStruct)
{
	if (UE::MetaStory::Compiler::Private::bUseDependenciesToTriggerCompilation)
	{
		TSet<TNotNull<UMetaStory*>> MetaStoryToLink;
		const FObjectKey StructToReplacedKey = &UserDefinedStruct;
		TArray<TObjectKey<UMetaStory>>* Dependencies = DependenciesToMetaStory.Find(StructToReplacedKey);
		if (Dependencies)
		{
			for (const TObjectKey<UMetaStory>& MetaStoryKey : *Dependencies)
			{
				if (UMetaStory* MetaStory = MetaStoryKey.ResolveObjectPtr())
				{
					MetaStoryToLink.Add(MetaStory);
				}
			}
		}

		for (UMetaStory* MetaStory : MetaStoryToLink)
		{
			if (!MetaStory->Link())
			{
				UE_LOG(LogMetaStory, Error, TEXT("%s failed to link after Object reinstantiation. Take a look at the asset for any errors. Asset will not be usable at runtime."), *MetaStory->GetPathName());
			}
		}
	}
	else
	{
		TSet<const UStruct*> Structs;
		Structs.Add(&UserDefinedStruct);

		for (TObjectIterator<UMetaStoryEditorData> It; It; ++It)
		{
			UMetaStoryEditorData* MetaStoryEditorData = *It;
			UpdateBindingsInstanceStructsIfNeeded(Structs, MetaStoryEditorData);
		}

		for (TObjectIterator<UMetaStory> It; It; ++It)
		{
			UMetaStory* MetaStory = *It;
			if (MetaStory->PropertyBindings.ContainsAnyStruct(Structs))
			{
				if (!MetaStory->Link())
				{
					UE_LOG(LogMetaStory, Error, TEXT("%s failed to link after Struct reinstantiation. Take a look at the asset for any errors. Asset will not be usable at runtime."), *MetaStory->GetPathName());
				}
			}
		}
	}
}

void FCompilerManagerImpl::GatherDependencies(TNotNull<UMetaStory*> MetaStory)
{
	// Find the tree in the MetaStoryToDependencies
	const TObjectKey<UMetaStory> MetaStoryKey = MetaStory;
	TSharedPtr<FMetaStoryDependencies>& FoundDependencies = MetaStoryToDependencies.FindOrAdd(MetaStoryKey);
	
	// Remove all from DependenciesToMetaStory
	if (FoundDependencies)
	{
		for (FMetaStoryDependencies::FItem& Item : FoundDependencies->Dependencies)
		{
			TArray<TObjectKey<UMetaStory>>* FoundKey = DependenciesToMetaStory.Find(Item.Key);
			if (FoundKey)
			{
				FoundKey->RemoveSingleSwap(MetaStoryKey);
			}
		}
		FoundDependencies->Dependencies.Reset();
	}
	else
	{
		FoundDependencies = MakeShared<FMetaStoryDependencies>();
	}

	auto AddDependency = [this, MetaStoryKey, FoundDependencies](TNotNull<const UStruct*> Object, FMetaStoryDependencies::EDependencyType DependencyType)
		{
			const FObjectKey ObjectKey = Object;
			DependenciesToMetaStory.FindOrAdd(ObjectKey).AddUnique(MetaStoryKey);

			if (FMetaStoryDependencies::FItem* FoundItem = FoundDependencies->Dependencies.FindByPredicate([ObjectKey](const FMetaStoryDependencies::FItem& Other)
				{
					return Other.Key == ObjectKey;
				}))
			{
				FoundItem->Type |= DependencyType;
			}
			else
			{
				FoundDependencies->Dependencies.Add(FMetaStoryDependencies::FItem{.Key = ObjectKey, .Type = DependencyType});
			}
		};

	// Gather new inner dependencies
	UMetaStoryEditorData* EditorData = CastChecked<UMetaStoryEditorData>(MetaStory->EditorData);
	if (EditorData)
	{
		// Internal
		{
			FArchiveReferencingProperties DependencyArchive(MetaStory);
			EditorData->Serialize(DependencyArchive);
			for (const UStruct* Dependency : DependencyArchive.Dependencies)
			{
				AddDependency(Dependency, FMetaStoryDependencies::EDependencyType::DT_Internal);
			}
		}
		// Public
		{
			FArchiveReferencingProperties DependencyArchive(MetaStory);
			const_cast<FInstancedPropertyBag&>(EditorData->GetRootParametersPropertyBag()).Serialize(DependencyArchive);
			for (const UStruct* Dependency : DependencyArchive.Dependencies)
			{
				AddDependency(Dependency, FMetaStoryDependencies::EDependencyType::DT_Public);
			}
			if (EditorData->Schema)
			{
				AddDependency(EditorData->Schema->GetClass(), FMetaStoryDependencies::EDependencyType::DT_Public);
			}
		}
		// Link
		{
			TMap<FGuid, const FPropertyBindingDataView> AllStructValues;
			EditorData->GetAllStructValues(AllStructValues);
			auto AddBindingPathDependencies = [&AddDependency , &AllStructValues](const FPropertyBindingPath& PropertyPath)
				{
					const FPropertyBindingDataView* FoundStruct = AllStructValues.Find(PropertyPath.GetStructID());
					if (FoundStruct)
					{
						FString Error;
						TArray<FPropertyBindingPathIndirection> Indirections;
						if (PropertyPath.ResolveIndirectionsWithValue(*FoundStruct, Indirections, &Error))
						{
							for (const FPropertyBindingPathIndirection& Indirection : Indirections)
							{
								if (Indirection.GetInstanceStruct())
								{
									AddDependency(Indirection.GetInstanceStruct(), FMetaStoryDependencies::EDependencyType::DT_Link);
								}
								else if (Indirection.GetContainerStruct())
								{
									AddDependency(Indirection.GetContainerStruct(), FMetaStoryDependencies::EDependencyType::DT_Link);
								}
							}
						}
					}
				};

			EditorData->GetEditorPropertyBindings()->VisitBindings([&AddBindingPathDependencies](const FPropertyBindingBinding& Binding)
				{
					AddBindingPathDependencies(Binding.GetSourcePath());
					AddBindingPathDependencies(Binding.GetTargetPath());
					return FPropertyBindingBindingCollection::EVisitResult::Continue;
				});
		}
	}
}

void FCompilerManagerImpl::LogDependencies(TNotNull<UMetaStory*> MetaStory) const
{
	FStringBuilderBase LogString;
	LogString << TEXT("MetaStory Dependencies (asset: '");
	MetaStory->GetFullName(LogString);
	LogString << TEXT("')\n");
	
	const TObjectKey<UMetaStory> MetaStoryKey = MetaStory;
	const TSharedPtr<FMetaStoryDependencies>* FoundDependencies = MetaStoryToDependencies.Find(MetaStoryKey);
	if (FoundDependencies != nullptr && FoundDependencies->IsValid())
	{
		auto PrintType = [&LogString](FMetaStoryDependencies::EDependencyType Type)
			{
				bool bPrinted = false;
				auto PrintSeparator = [&bPrinted, &LogString]()
					{
						if (bPrinted)
						{
							LogString << TEXT(" | ");
						}
						bPrinted = true;
					};
				if (EnumHasAnyFlags(Type, FMetaStoryDependencies::EDependencyType::DT_Public))
				{
					PrintSeparator();
					LogString << TEXT("Public");
				}
				if (EnumHasAnyFlags(Type, FMetaStoryDependencies::EDependencyType::DT_Internal))
				{
					PrintSeparator();
					LogString << TEXT("Internal");
				}
				if (EnumHasAnyFlags(Type, FMetaStoryDependencies::EDependencyType::DT_Link))
				{
					PrintSeparator();
					LogString << TEXT("Link");
				}
			};
		for (const FMetaStoryDependencies::FItem& Item : (*FoundDependencies)->Dependencies)
		{
			LogString << TEXT("  ");
			if (const UObject* Object = Item.Key.ResolveObjectPtr())
			{
				Object->GetFullName(LogString);
			}
			else
			{
				LogString << TEXT(" [None]");
			}

			LogString << TEXT(" [");
			PrintType(Item.Type);
			LogString << TEXT("]\n");
		}
	}
	else
	{
		LogString << TEXT("  No Dependency");
	}

	UE_LOG(LogMetaStoryEditor, Log, TEXT("%s"), LogString.ToString());
}


} // namespace UE::MetaStory::Compiler::Private


namespace UE::MetaStory::Compiler
{

void FCompilerManager::Startup()
{
	Private::CompilerManagerImpl = MakeUnique<Private::FCompilerManagerImpl>();
}

void FCompilerManager::Shutdown()
{
	Private::CompilerManagerImpl.Reset();
}

bool FCompilerManager::CompileSynchronously(TNotNull<UMetaStory*> MetaStory)
{
	FMetaStoryCompilerLog Log;
	return CompileSynchronously(MetaStory, Log);
}

bool FCompilerManager::CompileSynchronously(TNotNull<UMetaStory*> MetaStory, FMetaStoryCompilerLog& Log)
{
	if (ensureMsgf(Private::CompilerManagerImpl.IsValid(), TEXT("Can't compile the asset when the module is not available.")))
	{
		return Private::CompilerManagerImpl->CompileInternalSynchronously(MetaStory, Log);
	}
	return false;
}

} // UE::MetaStory::Compiler

