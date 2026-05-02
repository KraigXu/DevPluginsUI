// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStory.h"
#include "MetaStoryCustomVersions.h"
#include "Misc/PackageName.h"
#include "MetaStoryLinker.h"
#include "MetaStoryTaskBase.h"
#include "MetaStoryEvaluatorBase.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryConsiderationBase.h"
#include "MetaStoryExtension.h"
#include "MetaStoryModuleImpl.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/ScopeRWLock.h"
#include "MetaStoryDelegates.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "Misc/DataValidation.h"
#include "Misc/EnumerateRange.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "MetaStoryPropertyFunctionBase.h"
#include "AutoRTFM.h"

#include <atomic>

#if WITH_EDITOR
#include "Editor.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Templates/GuardValueAccessors.h"
#include "UObject/LinkerLoad.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStory)

PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Unique GUID for MetaStory (must not match Engine StateTree's FStateTreeCustomVersion or duplicate registration asserts at startup).
const FGuid FMetaStoryCustomVersion::GUID(0x8A3F1D9E, 0x6C2B4E70, 0xB5D8A1F3, 0x2E7C9B04);
namespace UE::MetaStory::Private
{
FCustomVersionRegistration GRegisterMetaStoryAssetCustomVersion(FMetaStoryCustomVersion::GUID, FMetaStoryCustomVersion::LatestVersion, TEXT("MetaStoryAsset"));
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

namespace UE::MetaStory::CustomVersions
{
	// Pre-fork / StateTree-identical asset custom version key (Engine StateTree still registers this when its plugin is enabled).
	static const FGuid GLegacyCompatible_AssetCustomVersionGUID(0x28E21331, 0x501F4723, 0x8110FA64, 0xEA10DA1E);

	int32 GetEffectiveAssetLinkerVersion(const UObject* Object)
	{
		if (Object == nullptr)
		{
			return -1;
		}
		const int32 VNew = Object->GetLinkerCustomVersion(FMetaStoryCustomVersion::GUID);
		const int32 VLegacy = Object->GetLinkerCustomVersion(GLegacyCompatible_AssetCustomVersionGUID);
		int32 Best = -1;
		if (VNew >= 0)
		{
			Best = FMath::Max(Best, VNew);
		}
		if (VLegacy >= 0)
		{
			Best = FMath::Max(Best, VLegacy);
		}
		return Best;
	}

	int32 GetEffectiveAssetArchiveVersion(const FArchive& Ar)
	{
		int32 Best = -1;

		// Saving only registers the current MetaStory GUID via UsingCustomVersion; querying unknown GUIDs asserts (Archive.cpp).
		const int32 VNew = Ar.CustomVer(FMetaStoryCustomVersion::GUID);
		if (VNew >= 0)
		{
			Best = FMath::Max(Best, VNew);
		}

		if (Ar.IsLoading())
		{
			const int32 VLegacy = Ar.CustomVer(GLegacyCompatible_AssetCustomVersionGUID);
			if (VLegacy >= 0)
			{
				Best = FMath::Max(Best, VLegacy);
			}
		}

		return Best;
	}
}

bool UMetaStory::IsReadyToRun() const
{
	// Valid tree must have at least one state and valid instance data.
	return States.Num() > 0 && bIsLinked && PropertyBindings.IsValid();
}

FConstStructView UMetaStory::GetNode(const int32 NodeIndex) const
{
	return Nodes.IsValidIndex(NodeIndex) ? Nodes[NodeIndex] : FConstStructView();
}

FMetaStoryIndex16 UMetaStory::GetNodeIndexFromId(const FGuid Id) const
{
	const FMetaStoryNodeIdToIndex* Entry = IDToNodeMappings.FindByPredicate([Id](const FMetaStoryNodeIdToIndex& Entry){ return Entry.Id == Id; });
	return Entry != nullptr ? Entry->Index : FMetaStoryIndex16::Invalid;
}

FGuid UMetaStory::GetNodeIdFromIndex(const FMetaStoryIndex16 NodeIndex) const
{
	const FMetaStoryNodeIdToIndex* Entry = NodeIndex.IsValid()
		? IDToNodeMappings.FindByPredicate([NodeIndex](const FMetaStoryNodeIdToIndex& Entry){ return Entry.Index == NodeIndex; })
		: nullptr;
	return Entry != nullptr ? Entry->Id : FGuid();
}

const FMetaStoryCompactFrame* UMetaStory::GetFrameFromHandle(const FMetaStoryStateHandle StateHandle) const
{
	return Frames.FindByPredicate([StateHandle](const FMetaStoryCompactFrame& Frame)
		{
			return Frame.RootState == StateHandle;
		});
}

const FMetaStoryCompactState* UMetaStory::GetStateFromHandle(const FMetaStoryStateHandle StateHandle) const
{
	return States.IsValidIndex(StateHandle.Index) ? &States[StateHandle.Index] : nullptr;
}

FMetaStoryStateHandle UMetaStory::GetStateHandleFromId(const FGuid Id) const
{
	const FMetaStoryStateIdToHandle* Entry = IDToStateMappings.FindByPredicate([Id](const FMetaStoryStateIdToHandle& Entry){ return Entry.Id == Id; });
	return Entry != nullptr ? Entry->Handle : FMetaStoryStateHandle::Invalid;
}

FGuid UMetaStory::GetStateIdFromHandle(const FMetaStoryStateHandle Handle) const
{
	const FMetaStoryStateIdToHandle* Entry = IDToStateMappings.FindByPredicate([Handle](const FMetaStoryStateIdToHandle& Entry){ return Entry.Handle == Handle; });
	return Entry != nullptr ? Entry->Id : FGuid();
}

const FMetaStoryCompactStateTransition* UMetaStory::GetTransitionFromIndex(const FMetaStoryIndex16 TransitionIndex) const
{
	return TransitionIndex.IsValid() && Transitions.IsValidIndex(TransitionIndex.Get()) ? &Transitions[TransitionIndex.Get()] : nullptr;
}

FMetaStoryIndex16 UMetaStory::GetTransitionIndexFromId(const FGuid Id) const
{
	const FMetaStoryTransitionIdToIndex* Entry = IDToTransitionMappings.FindByPredicate([Id](const FMetaStoryTransitionIdToIndex& Entry){ return Entry.Id == Id; });
	return Entry != nullptr ? Entry->Index : FMetaStoryIndex16::Invalid;
}

FGuid UMetaStory::GetTransitionIdFromIndex(const FMetaStoryIndex16 Index) const
{
	const FMetaStoryTransitionIdToIndex* Entry = IDToTransitionMappings.FindByPredicate([Index](const FMetaStoryTransitionIdToIndex& Entry){ return Entry.Index == Index; });
	return Entry != nullptr ? Entry->Id : FGuid();
}

const UMetaStoryExtension* UMetaStory::K2_GetExtension(TSubclassOf<UMetaStoryExtension> InExtensionType) const
{
	for (const UMetaStoryExtension* Extension : Extensions)
	{
		if (ensureMsgf(Extension, TEXT("The extension is invalid. Make sure it's not created in an editor only module.")))
		{
			if (Extension->IsA(InExtensionType))
			{
				return Extension;
			}
		}
	}
	return nullptr;
}

UE_AUTORTFM_ALWAYS_OPEN
static int32 GetThreadIndexForSharedInstanceData()
{
	// Create a unique index for each thread.
	static std::atomic_int ThreadIndexCounter {0};
	static thread_local int32 ThreadIndex = INDEX_NONE; // Cannot init directly on WinRT
	if (ThreadIndex == INDEX_NONE)
	{
		ThreadIndex = ThreadIndexCounter.fetch_add(1);
	}

	return ThreadIndex;
}

TSharedPtr<FMetaStoryInstanceData> UMetaStory::GetSharedInstanceData() const
{
	int32 ThreadIndex = GetThreadIndexForSharedInstanceData();

	// If shared instance data for this thread exists, return it.
	{
		UE::TReadScopeLock ReadLock(PerThreadSharedInstanceDataLock);
		if (ThreadIndex < PerThreadSharedInstanceData.Num())
		{
			return PerThreadSharedInstanceData[ThreadIndex];
		}
	}

	// Not initialized yet, create new instances up to the index.
	UE::TWriteScopeLock WriteLock(PerThreadSharedInstanceDataLock);

	// It is possible that multiple threads are waiting for the write lock,
	// which means that execution may get here so that 'ThreadIndex' is already in valid range.
	// The loop below is organized to handle that too.
	
	const int32 NewNum = ThreadIndex + 1;
	PerThreadSharedInstanceData.Reserve(NewNum);
	UMetaStory* NonConstThis = const_cast<UMetaStory*>(this); 
	
	for (int32 Index = PerThreadSharedInstanceData.Num(); Index < NewNum; Index++)
	{
		TSharedPtr<FMetaStoryInstanceData> SharedData = MakeShared<FMetaStoryInstanceData>();
		SharedData->CopyFrom(*NonConstThis, SharedInstanceData);
		PerThreadSharedInstanceData.Add(SharedData);
	}

	return PerThreadSharedInstanceData[ThreadIndex];
}

bool UMetaStory::HasCompatibleContextData(const UMetaStory& Other) const
{
	return HasCompatibleContextData(&Other);
}

bool UMetaStory::HasCompatibleContextData(TNotNull<const UMetaStory*> Other) const
{
	if (ContextDataDescs.Num() != Other->ContextDataDescs.Num())
	{
		return false;
	}

	const int32 Num = ContextDataDescs.Num();
	for (int32 Index = 0; Index < Num; Index++)
	{
		const FMetaStoryExternalDataDesc& Desc = ContextDataDescs[Index];
		const FMetaStoryExternalDataDesc& OtherDesc = Other->ContextDataDescs[Index];
		
		if (!OtherDesc.Struct 
			|| !OtherDesc.Struct->IsChildOf(Desc.Struct))
		{
			return false;
		}
	}
	
	return true;
}

#if WITH_METASTORY_DEBUG
void UMetaStory::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_METASTORY_DEBUG
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		PreGCHandle = FMetaStoryModule::OnPreRuntimeValidationInstanceData.AddUObject(this, &UMetaStory::HandleRuntimeValidationPreGC);
		PostGCHandle = FMetaStoryModule::OnPostRuntimeValidationInstanceData.AddUObject(this, &UMetaStory::HandleRuntimeValidationPostGC);
	}
#endif
}

void UMetaStory::BeginDestroy()
{
#if WITH_METASTORY_DEBUG
	FMetaStoryModule::OnPreRuntimeValidationInstanceData.Remove(PreGCHandle);
	FMetaStoryModule::OnPostRuntimeValidationInstanceData.Remove(PostGCHandle);
#endif

	Super::BeginDestroy();
}
#endif //WITH_METASTORY_DEBUG

#if WITH_EDITOR
namespace UE::MetaStory::Compiler
{
	void RenameObjectToTransientPackage(UObject* ObjectToRename)
	{
		const ERenameFlags RenFlags = REN_DoNotDirty | REN_DontCreateRedirectors;

		ObjectToRename->SetFlags(RF_Transient);
		ObjectToRename->ClearFlags(RF_Public | RF_Standalone | RF_ArchetypeObject);

		// Rename will remove the renamed object's linker when moving to a new package so invalidate the export beforehand
		FLinkerLoad::InvalidateExport(ObjectToRename);
		ObjectToRename->Rename(nullptr, GetTransientPackage(), RenFlags);
	}
}

void UMetaStory::ResetCompiled()
{
	Schema = nullptr;
	Frames.Reset();
	States.Reset();
	Transitions.Reset();
	Nodes.Reset();
	DefaultInstanceData.Reset();
	DefaultEvaluationScopeInstanceData.Reset();
	DefaultExecutionRuntimeData.Reset();
	SharedInstanceData.Reset();
	ContextDataDescs.Reset();
	PropertyBindings.Reset();
	PropertyFunctionEvaluationScopeMemoryRequirements.Reset();
	Extensions.Reset();
	Parameters.Reset();
	ParameterDataType = EMetaStoryParameterDataType::GlobalParameterData;
	IDToStateMappings.Reset();
	IDToNodeMappings.Reset();
	IDToTransitionMappings.Reset();
	
	EvaluatorsBegin = 0;
	EvaluatorsNum = 0;

	GlobalTasksBegin = 0;
	GlobalTasksNum = 0;
	bHasGlobalTransitionTasks = false;
	bHasGlobalTickTasks = false;
	bHasGlobalTickTasksOnlyOnEvents = false;
	bCachedRequestGlobalTick = false;
	bCachedRequestGlobalTickOnlyOnEvents = false;
	bScheduledTickAllowed = false;
	
	ResetLinked();

	// Remove objects created from last compilation.
	{
		TArray<UObject*, TInlineAllocator<32>> Children;
		const bool bIncludeNestedObjects = false;
		ForEachObjectWithOuter(this, [&Children, EditorData = EditorData.Get()](UObject* Child)
			{
				if (Child != EditorData)
				{
					Children.Add(Child);
				}
			}, bIncludeNestedObjects);

		for (UObject* Child : Children)
		{
			UE::MetaStory::Compiler::RenameObjectToTransientPackage(Child);
		}
	}
}

void UMetaStory::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	ResetCompiled();
}

void UMetaStory::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	const FString SchemaClassName = Schema ? Schema->GetClass()->GetPathName() : TEXT("");
	Context.AddTag(FAssetRegistryTag(UE::MetaStory::SchemaTag, SchemaClassName, FAssetRegistryTag::TT_Alphabetical));

	if (Schema)
	{
		Schema->GetAssetRegistryTags(Context);
	}

	Super::GetAssetRegistryTags(Context);
}

void UMetaStory::ThreadedPostLoadAssetRegistryTagsOverride(FPostLoadAssetRegistryTagsContext& Context) const
{
	Super::ThreadedPostLoadAssetRegistryTagsOverride(Context);

	static const FName SchemaTag(TEXT("Schema"));
	const FString SchemaTagValue = Context.GetAssetData().GetTagValueRef<FString>(SchemaTag);
	if (!SchemaTagValue.IsEmpty() && FPackageName::IsShortPackageName(SchemaTagValue))
	{
		const FTopLevelAssetPath SchemaTagClassPathName = UClass::TryConvertShortTypeNameToPathName<UStruct>(SchemaTagValue, ELogVerbosity::Warning, TEXT("UMetaStory::ThreadedPostLoadAssetRegistryTagsOverride"));
		if (!SchemaTagClassPathName.IsNull())
		{
			Context.AddTagToUpdate(FAssetRegistryTag(SchemaTag, SchemaTagClassPathName.ToString(), FAssetRegistryTag::TT_Alphabetical));
		}
	}
}

EDataValidationResult UMetaStory::IsDataValid(FDataValidationContext& Context) const
{
	// Don't warn user that the tree they just saved is not compiled. Only for submit or manual validation
	if (Context.GetValidationUsecase() != EDataValidationUsecase::Save)
	{
		if (UE::MetaStory::Delegates::OnRequestEditorHash.IsBound())
		{
			const uint32 CurrentHash = UE::MetaStory::Delegates::OnRequestEditorHash.Execute(*this);
			if (CurrentHash != LastCompiledEditorDataHash)
			{
				Context.AddWarning(FText::FromString(FString::Printf(TEXT("%s is not compiled. Please recompile the MetaStory."), *GetPathName())));
				return EDataValidationResult::Invalid;
			}
		}
	}

	if (!const_cast<UMetaStory*>(this)->Link())
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s failed to link. Please recompile the MetaStory for more details errors."), *GetPathName())));
		return EDataValidationResult::Invalid;
	}

	return Super::IsDataValid(Context);
}

#endif // WITH_EDITOR

void UMetaStory::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	const UMetaStory* MetaStory = Cast<UMetaStory>(InThis);
	check(MetaStory);
	
	UE::TReadScopeLock ReadLock(MetaStory->PerThreadSharedInstanceDataLock);

	for (const TSharedPtr<FMetaStoryInstanceData>& InstanceData : MetaStory->PerThreadSharedInstanceData)
	{
		if (InstanceData.IsValid())
		{
			Collector.AddPropertyReferencesWithStructARO(FMetaStoryInstanceData::StaticStruct(), InstanceData.Get(), MetaStory);
		}
	}
}

void UMetaStory::PostLoad()
{
	Super::PostLoad();

	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		FStructView NodeView = Nodes[NodeIndex];
		if (FMetaStoryNodeBase* Node = NodeView.GetPtr<FMetaStoryNodeBase>())
		{
			auto PostLoadInstance = [&Node]<typename T>(T & Container, FMetaStoryIndex16 Index)
			{
				if (Container.IsObject(Index.Get()))
				{
					Node->PostLoad(Container.GetMutableObject(Index.Get()));
				}
				else
				{
					Node->PostLoad(Container.GetMutableStruct(Index.Get()));
				}
			};
			if (Node->InstanceTemplateIndex.IsValid())
			{
				const bool bIsUsingSharedInstanceData = Node->InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::SharedInstanceData
					|| Node->InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::SharedInstanceDataObject;
				const bool bIsUsingEvaluationScopeInstanceData = Node->InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceData
					|| Node->InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject;
				if (bIsUsingEvaluationScopeInstanceData)
				{
					PostLoadInstance(DefaultEvaluationScopeInstanceData, Node->InstanceTemplateIndex);
				}
				else if (bIsUsingSharedInstanceData)
				{
					PostLoadInstance(SharedInstanceData, Node->InstanceTemplateIndex);
				}
				else
				{
					PostLoadInstance(DefaultInstanceData, Node->InstanceTemplateIndex);
				}
			}
		}
	}

#if WITH_EDITOR
	if (EditorData)
	{
		// Make sure all the fix up logic in the editor data has had chance to happen.
		EditorData->ConditionalPostLoad();

		TGuardValueAccessors<bool> IsEditorLoadingPackageGuard(UE::GetIsEditorLoadingPackage, UE::SetIsEditorLoadingPackage, true);
		Compile();
	}
#else
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const int32 CurrentVersion = UE::MetaStory::CustomVersions::GetEffectiveAssetLinkerVersion(this);
	if (CurrentVersion < FMetaStoryCustomVersion::LatestVersion)
	{		
		UE_LOG(LogMetaStory, Error, TEXT("%s: compiled data is in older format. Please recompile the MetaStory asset."), *GetPathName());
		return;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

	if (Schema)
	{
		Schema->ConditionalPostLoad();
	}

	for (UMetaStoryExtension* Extension : Extensions)
	{
		if (Extension)
		{
			Extension->ConditionalPostLoad();
		}
	}

	if (!Link())
	{
		UE_LOG(LogMetaStory, Log, TEXT("%s failed to link. Asset will not be usable at runtime."), *GetPathName());
	}
}

#if WITH_EDITORONLY_DATA
void UMetaStory::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	TArray<UClass*> SchemaClasses;
	GetDerivedClasses(UMetaStorySchema::StaticClass(), SchemaClasses);
	for (UClass* SchemaClass : SchemaClasses)
	{
		if (!SchemaClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Transient))
		{
			OutConstructClasses.Add(FTopLevelAssetPath(SchemaClass));
		}
	}
}
#endif

void UMetaStory::Serialize(const FStructuredArchiveRecord Record)
{
	Super::Serialize(Record);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const FGuid MetaStoryCustomVersion = FMetaStoryCustomVersion::GUID;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	Record.GetUnderlyingArchive().UsingCustomVersion(MetaStoryCustomVersion);
	
	// We need to link and rebind property bindings each time a BP is compiled,
	// because property bindings may get invalid, and instance data potentially needs refreshed.
	if (Record.GetUnderlyingArchive().IsModifyingWeakAndStrongReferences())
	{
		if (!Link() && !HasAnyFlags(RF_ClassDefaultObject))
		{
			UE_LOG(LogMetaStory, Log, TEXT("%s failed to link. Asset will not be usable at runtime."), *GetName());	
		}
	}
}

void UMetaStory::ResetLinked()
{
	bIsLinked = false;
	ExternalDataDescs.Reset();

#if WITH_EDITOR
	OutOfDateStructs.Reset();
#endif

	UE::TWriteScopeLock WriteLock(PerThreadSharedInstanceDataLock);
	PerThreadSharedInstanceData.Reset();
}

bool UMetaStory::ValidateInstanceData()
{
	bool bResult = true;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FConstStructView& NodeView = Nodes[Index];
		const FMetaStoryNodeBase* Node = NodeView.GetPtr<const FMetaStoryNodeBase>();
		if (Node && Node->InstanceTemplateIndex.IsValid())
		{
			auto TestInstanceData = [this, Node, Index, &bResult](const UStruct* CurrentInstanceDataType, const UStruct* DesiredInstanceDataType)
				{
					if (CurrentInstanceDataType == nullptr)
					{
						UE_LOG(LogMetaStory, Error, TEXT("%s: node (%d) '%s' with name '%s' failed. Missing instance value, possibly due to Blueprint class or C++ class/struct template deletion.")
							, *GetPathName()
							, Index
							, *WriteToString<64>(Node->StaticStruct()->GetFName())
							, *WriteToString<128>(Node->Name)
							);

						bResult = false;
						return;
					}

					auto HasNewerVersionExists = [](TNotNull<const UStruct*> InstanceDataType)
					{
						// Is the class/scriptstruct a blueprint that got replaced by another class.
						bool bHasNewerVersionExists = InstanceDataType->HasAnyFlags(RF_NewerVersionExists);
						if (!bHasNewerVersionExists)
						{
							if (const UClass* InstanceDataClass = Cast<UClass>(InstanceDataType))
							{
								bHasNewerVersionExists = InstanceDataClass->HasAnyClassFlags(CLASS_NewerVersionExists);
							}
							else if (const UScriptStruct* InstanceDataStruct = Cast<UScriptStruct>(InstanceDataType))
							{
								bHasNewerVersionExists = (InstanceDataStruct->StructFlags & STRUCT_NewerVersionExists) != 0;
							}
						}
						return bHasNewerVersionExists;
					};

					if (HasNewerVersionExists(CurrentInstanceDataType))
					{
						bool bLogError = true;
#if WITH_EDITOR
						OutOfDateStructs.Add(CurrentInstanceDataType);
						bLogError = false;
#endif

						if (bLogError)
						{
							UE_LOG(LogMetaStory, Error, TEXT("%s: node '%s' failed. The source Instance Data type '%s' has a newer version."), *GetPathName(), *WriteToString<64>(Node->StaticStruct()->GetFName()), *WriteToString<64>(CurrentInstanceDataType->GetFName()));
						}

						bResult = false;
					}

					{
						// The FMyInstance::StaticStruct doesn't get a notification like the other objects when reinstanced.
						const bool bDesiredHasNewerVersion = HasNewerVersionExists(DesiredInstanceDataType);

						// Use strict testing so that the users will have option to initialize data mismatch if the type changes (even if potentially compatible).
						if (CurrentInstanceDataType != DesiredInstanceDataType
							&& !bDesiredHasNewerVersion)
						{
							bool bLogError = true;
#if WITH_EDITOR
							const UClass* CurrentInstanceDataClass = Cast<UClass>(CurrentInstanceDataType);
							const UClass* DesiredInstanceDataClass = Cast<UClass>(DesiredInstanceDataType);
							if (CurrentInstanceDataClass && DesiredInstanceDataClass)
							{
								// Because of the loading order. It's possible that the OnObjectsReinstanced did not completed.
								if (CurrentInstanceDataClass->ClassGeneratedBy == DesiredInstanceDataClass->ClassGeneratedBy)
								{
									OutOfDateStructs.Add(CurrentInstanceDataType);
									bLogError = false;
								}
							}
#endif
							if (bLogError)
							{
								UE_LOG(LogMetaStory, Error, TEXT("%s: node '%s' failed. The source Instance Data type '%s' does not match '%s'"), *GetPathName(), *WriteToString<64>(Node->StaticStruct()->GetFName()), *GetNameSafe(CurrentInstanceDataType), *GetNameSafe(DesiredInstanceDataType));
							}
							bResult = false;
						}
					}
				};

			{
				const UStruct* CurrentInstanceDataType = nullptr;
				{
					const bool bIsUsingSharedInstanceData = Node->InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::SharedInstanceData
						|| Node->InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::SharedInstanceDataObject;
					const bool bIsUsingEvaluationScopeInstanceData = Node->InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceData
						|| Node->InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject;
					if (bIsUsingEvaluationScopeInstanceData)
					{
						if (DefaultEvaluationScopeInstanceData.IsObject(Node->InstanceTemplateIndex.Get()))
						{
							const UObject* InstanceObject = DefaultEvaluationScopeInstanceData.GetObject(Node->InstanceTemplateIndex.Get());
							CurrentInstanceDataType = InstanceObject ? InstanceObject->GetClass() : nullptr;
						}
						else
						{
							CurrentInstanceDataType = DefaultEvaluationScopeInstanceData.GetStruct(Node->InstanceTemplateIndex.Get()).GetScriptStruct();
						}
					}
					else
					{
						const FMetaStoryInstanceData& SourceInstanceData = bIsUsingSharedInstanceData ? SharedInstanceData : DefaultInstanceData;
						if (SourceInstanceData.IsObject(Node->InstanceTemplateIndex.Get()))
						{
							const UObject* InstanceObject = SourceInstanceData.GetObject(Node->InstanceTemplateIndex.Get());
							CurrentInstanceDataType = InstanceObject ? InstanceObject->GetClass() : nullptr;
						}
						else
						{
							CurrentInstanceDataType = SourceInstanceData.GetStruct(Node->InstanceTemplateIndex.Get()).GetScriptStruct();
						}
					}
				}
				TestInstanceData(CurrentInstanceDataType, Node->GetInstanceDataType());
			}

			if (Node->ExecutionRuntimeTemplateIndex.IsValid())
			{
				const UStruct* CurrentInstanceDataType = nullptr;
				if (GetDefaultExecutionRuntimeData().IsObject(Node->ExecutionRuntimeTemplateIndex.Get()))
				{
					const UObject* InstanceObject = GetDefaultExecutionRuntimeData().GetObject(Node->ExecutionRuntimeTemplateIndex.Get());
					CurrentInstanceDataType = InstanceObject ? InstanceObject->GetClass() : nullptr;
				}
				else
				{
					CurrentInstanceDataType = GetDefaultExecutionRuntimeData().GetStruct(Node->ExecutionRuntimeTemplateIndex.Get()).GetScriptStruct();
				}
				TestInstanceData(CurrentInstanceDataType, Node->GetExecutionRuntimeDataType());
			}
		}
	}

	return bResult;
}

bool UMetaStory::Link()
{
	// Initialize the instance data default value.
	// This data will be used to allocate runtime instance on all MetaStory users.
	ResetLinked();

	// Validate that all the source instance data types matches the node instance data types
	if (!ValidateInstanceData())
	{
		return false;
	}

	if (States.Num() > 0 && Nodes.Num() > 0)
	{
		// Check that all nodes are valid.
		for (FConstStructView Node : Nodes)
		{
			if (!Node.IsValid())
			{
				UE_LOG(LogMetaStory, Error, TEXT("%s: MetaStory asset was not properly loaded (missing node). See log for loading failures, or recompile the MetaStory asset."), *GetPathName());
				return false;
			}
		}
	}

	// Resolves nodes references to other MetaStory data
	{
		FMetaStoryLinker Linker(this);

		for (int32 Index = 0; Index < Nodes.Num(); Index++)
		{
			FStructView Node = Nodes[Index];
			FMetaStoryNodeBase* NodePtr = Node.GetPtr<FMetaStoryNodeBase>();
			if (ensure(NodePtr))
			{
				const bool bLinkSucceeded = NodePtr->Link(Linker);
				if (!bLinkSucceeded || Linker.GetStatus() == EMetaStoryLinkerStatus::Failed)
				{
					UE_LOG(LogMetaStory, Error, TEXT("%s: node '%s' failed to resolve its references."), *GetPathName(), *NodePtr->StaticStruct()->GetName());
					return false;
				}
			}
		}

		// Schema
		if (Schema)
		{
			const bool bSchemaLinkSucceeded = Schema->Link(Linker);
			if (!bSchemaLinkSucceeded || Linker.GetStatus() == EMetaStoryLinkerStatus::Failed)
			{
				UE_LOG(LogMetaStory, Error, TEXT("%s: schema failed to resolve its references."), *GetPathName());
				return false;
			}
		}

		// Extensions
		if (Extensions.Num())
		{
			for (UMetaStoryExtension* Extension : Extensions)
			{
				if (Extension)
				{
					const bool bLinkSucceeded = Extension->Link(Linker);
					if (!bLinkSucceeded || Linker.GetStatus() == EMetaStoryLinkerStatus::Failed)
					{
						UE_LOG(LogMetaStory, Error, TEXT("%s: extension failed to resolve its references."), *GetPathName());
						return false;
					}
				}
			}
		}

		ExternalDataDescs = Linker.GetExternalDataDescs();
	}

	UpdateRuntimeFlags();

	if (!DefaultInstanceData.AreAllInstancesValid())
	{
		UE_LOG(LogMetaStory, Error, TEXT("%s: MetaStory asset was not properly loaded (missing instance data). See log for loading failures, or recompile the MetaStory asset."), *GetPathName());
		return false;
	}

	if (!SharedInstanceData.AreAllInstancesValid())
	{
		UE_LOG(LogMetaStory, Error, TEXT("%s: MetaStory asset was not properly loaded (missing shared instance data). See log for loading failures, or recompile the MetaStory asset."), *GetPathName());
		return false;
	}

	if (!DefaultEvaluationScopeInstanceData.AreAllInstancesValid())
	{
		UE_LOG(LogMetaStory, Error, TEXT("%s: MetaStory asset was not properly loaded (missing evaluation scope instance data). See log for loading failures, or recompile the MetaStory asset."), *GetPathName());
		return false;
	}

	if (!GetDefaultExecutionRuntimeData().AreAllInstancesValid())
	{
		UE_LOG(LogMetaStory, Error, TEXT("%s: MetaStory asset was not properly loaded (missing execution runtime data). See log for loading failures, or recompile the MetaStory asset."), *GetPathName());
		return false;
	}
	
	if (!PatchBindings())
	{
		return false;
	}

	// Resolves property paths used by bindings a store property pointers
	if (!PropertyBindings.ResolvePaths())
	{
		return false;
	}

	// Link succeeded, setup tree to be ready to run
	bIsLinked = true;
	
	return true;
}

void UMetaStory::UpdateRuntimeFlags()
{
	// Set the tick flags at runtime instead of compilation.
	//This is to support hotfix (when we only modify cpp code).

	for (FMetaStoryCompactState& State : States)
	{
		// Update the state task flags.
		State.bHasTickTasks = false;
		State.bHasTickTasksOnlyOnEvents = false;
		State.bHasTransitionTasks = false;
		State.bCachedRequestTick = false;
		State.bCachedRequestTickOnlyOnEvents = false;
		for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); ++TaskIndex)
		{
			const FMetaStoryTaskBase& Task = Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
			if (Task.bTaskEnabled)
			{
				State.bHasTickTasks |= Task.bShouldCallTick;
				State.bHasTickTasksOnlyOnEvents |= Task.bShouldCallTickOnlyOnEvents;
				State.bHasTransitionTasks |= Task.bShouldAffectTransitions;
				if (Task.bConsideredForScheduling)
				{
					State.bCachedRequestTick |= Task.bShouldCallTick || Task.bShouldAffectTransitions;
					State.bCachedRequestTickOnlyOnEvents |= Task.bShouldCallTickOnlyOnEvents;
				}
			}
		}

		// Cache the amount of memory needed to execute the conditions.
		{
			UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer::FMemoryRequirementBuilder Requirement;
			for (int32 Index = 0; Index < State.EnterConditionsNum; ++Index)
			{
				const int32 NodeIndex = State.EnterConditionsBegin + Index;
				const FMetaStoryConditionBase& Cond = Nodes[NodeIndex].Get<const FMetaStoryConditionBase>();
				if (Cond.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceData
					|| Cond.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject)
				{
					const FConstStructView DefaultInstanceView = DefaultEvaluationScopeInstanceData.GetStruct(Cond.InstanceTemplateIndex.Get());
					Requirement.Add(DefaultInstanceView.GetScriptStruct());
				}
			}
			State.EnterConditionEvaluationScopeMemoryRequirement = Requirement.Build();
		}
		// Cache the amount of memory needed to execute the considerations.
		{
			UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer::FMemoryRequirementBuilder Requirement;
			for (int32 Index = 0; Index < State.UtilityConsiderationsNum; ++Index)
			{
				const int32 NodeIndex = State.UtilityConsiderationsBegin + Index;
				const FMetaStoryConsiderationBase& Consideration = Nodes[NodeIndex].Get<const FMetaStoryConsiderationBase>();
				if (Consideration.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceData
					|| Consideration.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject)
				{
					const FConstStructView DefaultInstanceView = DefaultEvaluationScopeInstanceData.GetStruct(Consideration.InstanceTemplateIndex.Get());
					Requirement.Add(DefaultInstanceView.GetScriptStruct());
				}
			}
			State.ConsiderationEvaluationScopeMemoryRequirement = Requirement.Build();
		}
	}

	// Update the global task flags.
	{
		bHasGlobalTickTasks = false;
		bHasGlobalTickTasksOnlyOnEvents = false;
		bHasGlobalTransitionTasks = false;
		bCachedRequestGlobalTick = false;
		bCachedRequestGlobalTickOnlyOnEvents = false;
		for (int32 TaskIndex = GlobalTasksBegin; TaskIndex < (GlobalTasksBegin + GlobalTasksNum); ++TaskIndex)
		{
			const FMetaStoryTaskBase& Task = Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
			if (Task.bTaskEnabled)
			{
				bHasGlobalTickTasks |= Task.bShouldCallTick;
				bHasGlobalTickTasksOnlyOnEvents |= Task.bShouldCallTickOnlyOnEvents;
				bHasGlobalTransitionTasks |= Task.bShouldAffectTransitions;
				if (Task.bConsideredForScheduling)
				{
					bCachedRequestGlobalTick |= Task.bShouldCallTick || Task.bShouldAffectTransitions;
					bCachedRequestGlobalTickOnlyOnEvents |= Task.bShouldCallTickOnlyOnEvents;
				}
			}
		}
	}

	// Cache the amount of memory needed to execute the transition's condition.
	for (FMetaStoryCompactStateTransition& Transition : Transitions)
	{
		UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer::FMemoryRequirementBuilder Requirement;
		for (int32 Index = 0; Index < Transition.ConditionsNum; ++Index)
		{
			const int32 NodeIndex = Transition.ConditionsBegin + Index;
			const FMetaStoryConditionBase& Cond = Nodes[NodeIndex].Get<const FMetaStoryConditionBase>();
			if (Cond.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceData
				|| Cond.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject)
			{
				const FConstStructView DefaultInstanceView = DefaultEvaluationScopeInstanceData.GetStruct(Cond.InstanceTemplateIndex.Get());
				Requirement.Add(DefaultInstanceView.GetScriptStruct());
			}
		}
		Transition.ConditionEvaluationScopeMemoryRequirement = Requirement.Build();
	}

	bScheduledTickAllowed = Schema ? Schema->IsScheduledTickAllowed() : false;
	StateSelectionRules = Schema ? Schema->GetStateSelectionRules() : EMetaStoryStateSelectionRules::Default;
}

bool UMetaStory::PatchBindings()
{
	const TArrayView<FMetaStoryBindableStructDesc> SourceStructs = PropertyBindings.SourceStructs;
	TArrayView<FPropertyBindingCopyInfoBatch> CopyBatches = PropertyBindings.GetMutableCopyBatches();
	const TArrayView<FMetaStoryPropertyPathBinding> PropertyPathBindings = PropertyBindings.PropertyPathBindings;

	// Make mapping from data handle to source struct.
	TMap<FMetaStoryDataHandle, int32> SourceStructByHandle;
	for (TConstEnumerateRef<FMetaStoryBindableStructDesc> SourceStruct : EnumerateRange(SourceStructs))
	{
		SourceStructByHandle.Add(SourceStruct->DataHandle, SourceStruct.GetIndex());
	}

	auto GetSourceStructByHandle = [&SourceStructByHandle, &SourceStructs](const FMetaStoryDataHandle DataHandle) -> FMetaStoryBindableStructDesc*
	{
		if (int32* Index = SourceStructByHandle.Find(DataHandle))
		{
			return &SourceStructs[*Index];
		}
		return nullptr;
	};
	
	// Reconcile out of date classes.
	for (FMetaStoryBindableStructDesc& SourceStruct : SourceStructs)
	{
		if (const UClass* SourceClass = Cast<UClass>(SourceStruct.Struct))
		{
			if (SourceClass->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				SourceStruct.Struct = SourceClass->GetAuthoritativeClass();
			}
		}
	}

	for (FPropertyBindingCopyInfoBatch& CopyBatch : CopyBatches)
	{
		if (const UClass* TargetClass = Cast<UClass>(CopyBatch.TargetStruct.Get().Struct))
		{
			if (TargetClass->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				CopyBatch.TargetStruct.GetMutable<>().Struct = TargetClass->GetAuthoritativeClass();
			}
		}
	}

	auto PatchPropertyPath = [](FPropertyBindingPath& PropertyPath)
	{
		for (FPropertyBindingPathSegment& Segment : PropertyPath.GetMutableSegments())
		{
			if (const UClass* InstanceStruct = Cast<UClass>(Segment.GetInstanceStruct()))
			{
				if (InstanceStruct->HasAnyClassFlags(CLASS_NewerVersionExists))
				{
					Segment.SetInstanceStruct(InstanceStruct->GetAuthoritativeClass());
				}
			}
		}
	};

	for (FMetaStoryPropertyPathBinding& PropertyPathBinding : PropertyPathBindings)
	{
		PatchPropertyPath(PropertyPathBinding.GetMutableSourcePath());
		PatchPropertyPath(PropertyPathBinding.GetMutableTargetPath());
	}

	// Update property bag structs before resolving binding.
	const EMetaStoryDataSourceType GlobalParameterDataType = UE::MetaStory::CastToDataSourceType(ParameterDataType);
	if (FMetaStoryBindableStructDesc* RootParamsDesc = GetSourceStructByHandle(FMetaStoryDataHandle(GlobalParameterDataType)))
	{
		RootParamsDesc->Struct = Parameters.GetPropertyBagStruct();
	}

	// Refresh state parameter descs and bindings batches.
	for (const FMetaStoryCompactState& State : States)
	{
		// For subtrees and linked states, the parameters must exists.
		if (State.Type == EMetaStoryStateType::Subtree
			|| State.Type == EMetaStoryStateType::Linked
			|| State.Type == EMetaStoryStateType::LinkedAsset)
		{
			if (!State.ParameterTemplateIndex.IsValid())
			{
				UE_LOG(LogMetaStory, Error, TEXT("%s: Data for state '%s' is malformed. Please recompile the MetaStory asset."), *GetPathName(), *State.Name.ToString());
				return false;
			}
		}

		if (State.ParameterTemplateIndex.IsValid())
		{
			// Subtree is a bind source, update bag struct.
			const FMetaStoryCompactParameters& Params = DefaultInstanceData.GetMutableStruct(State.ParameterTemplateIndex.Get()).Get<FMetaStoryCompactParameters>();
			FMetaStoryBindableStructDesc* Desc = GetSourceStructByHandle(State.ParameterDataHandle);
			if (!Desc)
			{
				UE_LOG(LogMetaStory, Error, TEXT("%s: Data for state '%s' is malformed. Please recompile the MetaStory asset."), *GetPathName(), *State.Name.ToString());
				return false;
			}
			Desc->Struct = Params.Parameters.GetPropertyBagStruct();

			if (State.ParameterBindingsBatch.IsValid())
			{
				FPropertyBindingCopyInfoBatch& Batch = CopyBatches[State.ParameterBindingsBatch.Get()];
				Batch.TargetStruct.GetMutable().Struct = Params.Parameters.GetPropertyBagStruct();
			}
		}
	}

	// Check linked state property bags consistency
	for (const FMetaStoryCompactState& State : States)
	{
		if (State.Type == EMetaStoryStateType::Linked && State.LinkedState.IsValid())
		{
			const FMetaStoryCompactState& LinkedState = States[State.LinkedState.Index];

			if (State.ParameterTemplateIndex.IsValid() == false
				|| LinkedState.ParameterTemplateIndex.IsValid() == false)
			{
				UE_LOG(LogMetaStory, Error, TEXT("%s: Data for state '%s' is malformed. Please recompile the MetaStory asset."), *GetPathName(), *State.Name.ToString());
				return false;
			}

			// Check that the bag in linked state matches.
			const FMetaStoryCompactParameters& Params = DefaultInstanceData.GetMutableStruct(State.ParameterTemplateIndex.Get()).Get<FMetaStoryCompactParameters>();
			const FMetaStoryCompactParameters& LinkedStateParams = DefaultInstanceData.GetMutableStruct(LinkedState.ParameterTemplateIndex.Get()).Get<FMetaStoryCompactParameters>();

			if (LinkedStateParams.Parameters.GetPropertyBagStruct() != Params.Parameters.GetPropertyBagStruct())
			{
				UE_LOG(LogMetaStory, Error, TEXT("%s: The parameters on state '%s' does not match the linked state parameters in state '%s'. Please recompile the MetaStory asset."), *GetPathName(), *State.Name.ToString(), *LinkedState.Name.ToString());
				return false;
			}
		}
		else if (State.Type == EMetaStoryStateType::LinkedAsset && State.LinkedAsset)
		{
			// Check that the bag in linked state matches.
			const FInstancedPropertyBag& TargetTreeParameters = State.LinkedAsset->Parameters;
			const FMetaStoryCompactParameters& Params = DefaultInstanceData.GetMutableStruct(State.ParameterTemplateIndex.Get()).Get<FMetaStoryCompactParameters>();

			if (TargetTreeParameters.GetPropertyBagStruct() != Params.Parameters.GetPropertyBagStruct())
			{
				UE_LOG(LogMetaStory, Error, TEXT("%s: The parameters on state '%s' does not match the linked asset parameters '%s'. Please recompile the MetaStory asset."),
					*GetPathName(), *State.Name.ToString(), *State.LinkedAsset->GetPathName());
				return false;
			}
		}
	}

	TMap<FMetaStoryDataHandle, FMetaStoryDataView> DataViews;
	TMap<FMetaStoryIndex16, FMetaStoryDataView> BindingBatchDataView;

	// Tree parameters
	DataViews.Add(FMetaStoryDataHandle(GlobalParameterDataType), Parameters.GetMutableValue());

	// Setup data views for context data. Since the external data is passed at runtime, we can only provide the type.
	for (const FMetaStoryExternalDataDesc& DataDesc : ContextDataDescs)
	{
		DataViews.Add(DataDesc.Handle.DataHandle, FMetaStoryDataView(DataDesc.Struct, nullptr));
	}
	
	// Setup data views for state parameters.
	for (FMetaStoryCompactState& State : States)
	{
		if (State.ParameterDataHandle.IsValid())
		{
			FMetaStoryCompactParameters& Params = DefaultInstanceData.GetMutableStruct(State.ParameterTemplateIndex.Get()).Get<FMetaStoryCompactParameters>();
			DataViews.Add(State.ParameterDataHandle, Params.Parameters.GetMutableValue());
			if (State.ParameterBindingsBatch.IsValid())
			{
				BindingBatchDataView.Add(State.ParameterBindingsBatch, Params.Parameters.GetMutableValue());
			}
		}
	}

	// Setup data views for all nodes.
	for (FConstStructView NodeView : Nodes)
	{
		const FMetaStoryNodeBase& Node = NodeView.Get<const FMetaStoryNodeBase>();

		FMetaStoryDataView NodeDataView;
		if (Node.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::SharedInstanceData
			|| Node.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::SharedInstanceDataObject)
		{
			NodeDataView = Node.InstanceDataHandle.IsObjectSource()
				? FMetaStoryDataView(SharedInstanceData.GetMutableObject(Node.InstanceTemplateIndex.Get()))
				: FMetaStoryDataView(SharedInstanceData.GetMutableStruct(Node.InstanceTemplateIndex.Get()));
		}
		else if (Node.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceData
			|| Node.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject)
		{
			NodeDataView = Node.InstanceDataHandle.IsObjectSource()
				? FMetaStoryDataView(DefaultEvaluationScopeInstanceData.GetMutableObject(Node.InstanceTemplateIndex.Get()))
				: FMetaStoryDataView(DefaultEvaluationScopeInstanceData.GetMutableStruct(Node.InstanceTemplateIndex.Get()));
		}
		else
		{
			NodeDataView = Node.InstanceDataHandle.IsObjectSource()
				? FMetaStoryDataView(DefaultInstanceData.GetMutableObject(Node.InstanceTemplateIndex.Get()))
				: FMetaStoryDataView(DefaultInstanceData.GetMutableStruct(Node.InstanceTemplateIndex.Get()));
		}
		DataViews.Add(Node.InstanceDataHandle, NodeDataView);

		if (Node.BindingsBatch.IsValid())
		{
			BindingBatchDataView.Add(Node.BindingsBatch, NodeDataView);
		}

		if (Node.OutputBindingsBatch.IsValid())
		{
			BindingBatchDataView.Add(Node.OutputBindingsBatch, NodeDataView);
		}
	}
	
	auto GetDataSourceView = [&DataViews](const FMetaStoryDataHandle Handle) -> FMetaStoryDataView
	{
		if (const FMetaStoryDataView* ViewPtr = DataViews.Find(Handle))
		{
			return *ViewPtr;
		}
		return FMetaStoryDataView();
	};

	auto GetBindingBatchDataView = [&BindingBatchDataView](const FMetaStoryIndex16 Index) -> FMetaStoryDataView
	{
		if (const FMetaStoryDataView* ViewPtr = BindingBatchDataView.Find(Index))
		{
			return *ViewPtr;
		}
		return FMetaStoryDataView();
	};


	for (int32 BatchIndex = 0; BatchIndex < CopyBatches.Num(); ++BatchIndex)
	{
		const FPropertyBindingCopyInfoBatch& Batch = CopyBatches[BatchIndex];

		// Find data view for the binding target.
		FMetaStoryDataView TargetView = GetBindingBatchDataView(FMetaStoryIndex16(BatchIndex));
		if (!TargetView.IsValid())
		{
			UE_LOG(LogMetaStory, Error, TEXT("%hs: '%s' Invalid target struct when trying to bind to '%s'")
				, __FUNCTION__
				, *GetPathName()
				, *Batch.TargetStruct.Get().Name.ToString());
			return false;
		}

		FString ErrorMsg;
		for (int32 Index = Batch.BindingsBegin.Get(); Index != Batch.BindingsEnd.Get(); Index++)
		{
			FMetaStoryPropertyPathBinding& Binding = PropertyPathBindings[Index];

			const EMetaStoryDataSourceType Source = Binding.GetSourceDataHandle().GetSource();
			const bool bIsSourceEvent = Source == EMetaStoryDataSourceType::TransitionEvent || Source == EMetaStoryDataSourceType::StateEvent;

			if(!bIsSourceEvent)
			{
				FMetaStoryDataView SourceView = GetDataSourceView(Binding.GetSourceDataHandle());

				if (!Binding.GetMutableSourcePath().UpdateSegmentsFromValue(SourceView, &ErrorMsg))
				{
					UE_LOG(LogMetaStory, Error, TEXT("%hs: '%s' Failed to update source instance structs for property binding '%s'. Reason: %s")
						, __FUNCTION__
						, *GetPathName()
						, *Binding.GetTargetPath().ToString()
						, *ErrorMsg);
					return false;
				}
			}

			if (!Binding.GetMutableTargetPath().UpdateSegmentsFromValue(TargetView, &ErrorMsg))
			{
				UE_LOG(LogMetaStory, Error, TEXT("%hs: '%s' Failed to update target instance structs for property binding '%s'. Reason: %s")
					, __FUNCTION__
					, *GetPathName()
					, *Binding.GetTargetPath().ToString()
					, *ErrorMsg);
				return false;
			}
		}
	}

	// Cache the amount of memory needed to execute property functions for each batch.
	PropertyFunctionEvaluationScopeMemoryRequirements.Reset(CopyBatches.Num());
	for (FPropertyBindingCopyInfoBatch& CopyBatch : CopyBatches)
	{
		UE::MetaStory::InstanceData::FEvaluationScopeInstanceContainer::FMemoryRequirementBuilder CopyBatchFunctionRequirement;

		if (CopyBatch.PropertyFunctionsBegin != CopyBatch.PropertyFunctionsEnd)
		{
			const int32 FuncsBegin = CopyBatch.PropertyFunctionsBegin.Get();
			const int32 FuncsEnd = CopyBatch.PropertyFunctionsEnd.Get();
			for (int32 FuncIndex = FuncsBegin; FuncIndex < FuncsEnd; ++FuncIndex)
			{
				const FMetaStoryPropertyFunctionBase& Func = Nodes[FuncIndex].Get<const FMetaStoryPropertyFunctionBase>();
				if (Func.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceData
					|| Func.InstanceDataHandle.GetSource() == EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject)
				{
					const FConstStructView DefaultInstanceView = DefaultEvaluationScopeInstanceData.GetStruct(Func.InstanceTemplateIndex.Get());
					CopyBatchFunctionRequirement.Add(DefaultInstanceView.GetScriptStruct());
				}
			}
		}

		PropertyFunctionEvaluationScopeMemoryRequirements.Add(CopyBatchFunctionRequirement.Build());
	}
	check(CopyBatches.Num() == PropertyFunctionEvaluationScopeMemoryRequirements.Num());

	return true;
}

#if WITH_EDITOR

void FMetaStoryMemoryUsage::AddUsage(const FConstStructView View)
{
	if (const UScriptStruct* ScriptStruct = View.GetScriptStruct())
	{
		EstimatedMemoryUsage = Align(EstimatedMemoryUsage, ScriptStruct->GetMinAlignment());
		EstimatedMemoryUsage += ScriptStruct->GetStructureSize();
	}
}

void FMetaStoryMemoryUsage::AddUsage(const UObject* Object)
{
	if (Object != nullptr)
	{
		check(Object->GetClass());
		EstimatedMemoryUsage += Object->GetClass()->GetStructureSize();
	}
}

TArray<FMetaStoryMemoryUsage> UMetaStory::CalculateEstimatedMemoryUsage() const
{
	TArray<FMetaStoryMemoryUsage> MemoryUsages;
	TArray<TPair<int32, int32>> StateLinks;

	if (!bIsLinked
		|| States.IsEmpty()
		|| !Nodes.IsValid())
	{
		return MemoryUsages;
	}

	const int32 TreeMemUsageIndex = MemoryUsages.Emplace(TEXT("MetaStory Max"));
	const int32 InstanceMemUsageIndex = MemoryUsages.Emplace(TEXT("Instance Overhead"));
	const int32 EvalMemUsageIndex = MemoryUsages.Emplace(TEXT("Evaluators"));
	const int32 GlobalTaskMemUsageIndex = MemoryUsages.Emplace(TEXT("GlobalTask"));
	const int32 SharedMemUsageIndex = MemoryUsages.Emplace(TEXT("Shared Data"));
	const int32 ExtensionMemUsageIndex = MemoryUsages.Emplace(TEXT("Extensions"));

	auto GetRootStateHandle = [this](const FMetaStoryStateHandle InState) -> FMetaStoryStateHandle
	{
		FMetaStoryStateHandle Result = InState;
		while (Result.IsValid() && States[Result.Index].Parent.IsValid())
		{
			Result = States[Result.Index].Parent;
		}
		return Result;		
	};

	auto GetUsageIndexForState = [&MemoryUsages, this](const FMetaStoryStateHandle InStateHandle) -> int32
	{
		check(InStateHandle.IsValid());
		
		const int32 FoundMemUsage = MemoryUsages.IndexOfByPredicate([InStateHandle](const FMetaStoryMemoryUsage& MemUsage) { return MemUsage.Handle == InStateHandle; });
		if (FoundMemUsage != INDEX_NONE)
		{
			return FoundMemUsage;
		}

		const FMetaStoryCompactState& CompactState = States[InStateHandle.Index];
		
		return MemoryUsages.Emplace(TEXT("State ") + CompactState.Name.ToString(), InStateHandle);
	};

	// Calculate memory usage per state.
	TArray<FMetaStoryMemoryUsage> TempStateMemoryUsages;
	TempStateMemoryUsages.SetNum(States.Num());

	for (int32 Index = 0; Index < States.Num(); Index++)
	{
		const FMetaStoryStateHandle StateHandle((uint16)Index);
		const FMetaStoryCompactState& CompactState = States[Index];
		const FMetaStoryStateHandle ParentHandle = GetRootStateHandle(StateHandle);
		const int32 ParentUsageIndex = GetUsageIndexForState(ParentHandle);
		
		FMetaStoryMemoryUsage& MemUsage = CompactState.Parent.IsValid() ? TempStateMemoryUsages[Index] : MemoryUsages[GetUsageIndexForState(StateHandle)];
		
		MemUsage.NodeCount += CompactState.TasksNum;

		if (CompactState.Type == EMetaStoryStateType::Linked)
		{
			const int32 LinkedUsageIndex = GetUsageIndexForState(CompactState.LinkedState);
			StateLinks.Emplace(ParentUsageIndex, LinkedUsageIndex);
		}
		
		if (CompactState.ParameterTemplateIndex.IsValid())
		{
			MemUsage.NodeCount++;
			MemUsage.AddUsage(DefaultInstanceData.GetStruct(CompactState.ParameterTemplateIndex.Get()));
		}
		
		for (int32 TaskIndex = CompactState.TasksBegin; TaskIndex < (CompactState.TasksBegin + CompactState.TasksNum); TaskIndex++)
		{
			if (const FMetaStoryTaskBase* Task = Nodes[TaskIndex].GetPtr<const FMetaStoryTaskBase>())
			{
				if (Task->InstanceDataHandle.IsObjectSource())
				{
					MemUsage.NodeCount++;
					MemUsage.AddUsage(DefaultInstanceData.GetObject(Task->InstanceTemplateIndex.Get()));
				}
				else
				{
					MemUsage.NodeCount++;
					MemUsage.AddUsage(DefaultInstanceData.GetStruct(Task->InstanceTemplateIndex.Get()));
				}
			}
		}
	}

	// Combine max child usage to parents. Iterate backwards to update children first.
	for (int32 Index = States.Num() - 1; Index >= 0; Index--)
	{
		const FMetaStoryStateHandle StateHandle((uint16)Index);
		const FMetaStoryCompactState& CompactState = States[Index];

		FMetaStoryMemoryUsage& MemUsage = CompactState.Parent.IsValid() ? TempStateMemoryUsages[Index] : MemoryUsages[GetUsageIndexForState(StateHandle)];

		int32 MaxChildStateMem = 0;
		int32 MaxChildStateNodes = 0;
		
		for (uint16 ChildState = CompactState.ChildrenBegin; ChildState < CompactState.ChildrenEnd; ChildState = States[ChildState].GetNextSibling())
		{
			const FMetaStoryMemoryUsage& ChildMemUsage = TempStateMemoryUsages[ChildState];
			if (ChildMemUsage.EstimatedMemoryUsage > MaxChildStateMem)
			{
				MaxChildStateMem = ChildMemUsage.EstimatedMemoryUsage;
				MaxChildStateNodes = ChildMemUsage.NodeCount;
			}
		}

		MemUsage.EstimatedMemoryUsage += MaxChildStateMem;
		MemUsage.NodeCount += MaxChildStateNodes;
	}

	// Accumulate linked states.
	for (int32 Index = StateLinks.Num() - 1; Index >= 0; Index--)
	{
		FMetaStoryMemoryUsage& ParentUsage = MemoryUsages[StateLinks[Index].Get<0>()];
		const FMetaStoryMemoryUsage& LinkedUsage = MemoryUsages[StateLinks[Index].Get<1>()];
		const int32 LinkedTotalUsage = LinkedUsage.EstimatedMemoryUsage + LinkedUsage.EstimatedChildMemoryUsage;
		if (LinkedTotalUsage > ParentUsage.EstimatedChildMemoryUsage)
		{
			ParentUsage.EstimatedChildMemoryUsage = LinkedTotalUsage;
			ParentUsage.ChildNodeCount = LinkedUsage.NodeCount + LinkedUsage.ChildNodeCount;
		}
	}

	// Evaluators
	FMetaStoryMemoryUsage& EvalMemUsage = MemoryUsages[EvalMemUsageIndex];
	for (int32 EvalIndex = EvaluatorsBegin; EvalIndex < (EvaluatorsBegin + EvaluatorsNum); EvalIndex++)
	{
		const FMetaStoryEvaluatorBase& Eval = Nodes[EvalIndex].Get<const FMetaStoryEvaluatorBase>();
		if (Eval.InstanceDataHandle.IsObjectSource())
		{
			EvalMemUsage.AddUsage(DefaultInstanceData.GetObject(Eval.InstanceTemplateIndex.Get()));
		}
		else
		{
			EvalMemUsage.AddUsage(DefaultInstanceData.GetStruct(Eval.InstanceTemplateIndex.Get()));
		}
		EvalMemUsage.NodeCount++;
	}

	// Global Tasks
	FMetaStoryMemoryUsage& GlobalTaskMemUsage = MemoryUsages[GlobalTaskMemUsageIndex];
	for (int32 TaskIndex = GlobalTasksBegin; TaskIndex < (GlobalTasksBegin + GlobalTasksNum); TaskIndex++)
	{
		const FMetaStoryTaskBase& Task = Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
		if (Task.InstanceDataHandle.IsObjectSource())
		{
			GlobalTaskMemUsage.AddUsage(DefaultInstanceData.GetObject(Task.InstanceTemplateIndex.Get()));
		}
		else
		{
			GlobalTaskMemUsage.AddUsage(DefaultInstanceData.GetStruct(Task.InstanceTemplateIndex.Get()));
		}
		GlobalTaskMemUsage.NodeCount++;
	}

	// Estimate highest combined usage.
	FMetaStoryMemoryUsage& TreeMemUsage = MemoryUsages[TreeMemUsageIndex];

	// Exec state
	TreeMemUsage.AddUsage(DefaultInstanceData.GetStruct(0));
	TreeMemUsage.NodeCount++;

	TreeMemUsage.EstimatedMemoryUsage += EvalMemUsage.EstimatedMemoryUsage;
	TreeMemUsage.NodeCount += EvalMemUsage.NodeCount;

	TreeMemUsage.EstimatedMemoryUsage += GlobalTaskMemUsage.EstimatedMemoryUsage;
	TreeMemUsage.NodeCount += GlobalTaskMemUsage.NodeCount;

	FMetaStoryMemoryUsage& InstanceMemUsage = MemoryUsages[InstanceMemUsageIndex];
	// FMetaStoryInstanceData overhead.
	InstanceMemUsage.EstimatedMemoryUsage += sizeof(FMetaStoryInstanceData);
	// FInstancedStructContainer overhead.
	InstanceMemUsage.EstimatedMemoryUsage += TreeMemUsage.NodeCount * FInstancedStructContainer::OverheadPerItem;

	TreeMemUsage.EstimatedMemoryUsage += InstanceMemUsage.EstimatedMemoryUsage;
	
	int32 MaxSubtreeUsage = 0;
	int32 MaxSubtreeNodeCount = 0;
	
	for (const FMetaStoryMemoryUsage& MemUsage : MemoryUsages)
	{
		if (MemUsage.Handle.IsValid())
		{
			const int32 TotalUsage = MemUsage.EstimatedMemoryUsage + MemUsage.EstimatedChildMemoryUsage;
			if (TotalUsage > MaxSubtreeUsage)
			{
				MaxSubtreeUsage = TotalUsage;
				MaxSubtreeNodeCount = MemUsage.NodeCount + MemUsage.ChildNodeCount;
			}
		}
	}

	TreeMemUsage.EstimatedMemoryUsage += MaxSubtreeUsage;
	TreeMemUsage.NodeCount += MaxSubtreeNodeCount;

	FMetaStoryMemoryUsage& SharedMemUsage = MemoryUsages[SharedMemUsageIndex];
	SharedMemUsage.NodeCount = SharedInstanceData.Num();
	SharedMemUsage.EstimatedMemoryUsage = SharedInstanceData.GetEstimatedMemoryUsage();

	// Extensions
	FMetaStoryMemoryUsage& ExtensionMemUsage = MemoryUsages[ExtensionMemUsageIndex];
	for (UMetaStoryExtension* Extension : Extensions)
	{
		if (Extension)
		{
			ExtensionMemUsage.AddUsage(Extension);
			++ExtensionMemUsage.NodeCount;
		}
	}

	return MemoryUsages;
}

void UMetaStory::CompileIfChanged()
{
	if (UE::MetaStory::Delegates::OnRequestCompile.IsBound() && UE::MetaStory::Delegates::OnRequestEditorHash.IsBound())
	{
		const uint32 CurrentHash = UE::MetaStory::Delegates::OnRequestEditorHash.Execute(*this);
		if (LastCompiledEditorDataHash != CurrentHash)
		{
			UE_LOG(LogMetaStory, Log, TEXT("%s: Editor data has changed. Recompiling MetaStory."), *GetPathName());
			UE::MetaStory::Delegates::OnRequestCompile.Execute(*this);
		}
	}
	else
	{
		ResetCompiled();
		UE_LOG(LogMetaStory, Warning, TEXT("%s: could not compile. Please resave the MetaStory asset."), *GetPathName());
	}
}

void UMetaStory::Compile()
{
	if (UE::MetaStory::Delegates::OnRequestCompile.IsBound())
	{
		UE_LOG(LogMetaStory, Log, TEXT("%s: Editor data has changed. Recompiling MetaStory."), *GetPathName());
		UE::MetaStory::Delegates::OnRequestCompile.Execute(*this);
	}
	else
	{
		ResetCompiled();
		UE_LOG(LogMetaStory, Warning, TEXT("%s: could not compile. Please resave the MetaStory asset."), *GetPathName());
	}
}
#endif //WITH_EDITOR

#if WITH_EDITOR || WITH_METASTORY_DEBUG
FString UMetaStory::DebugInternalLayoutAsString() const
{
	FStringBuilderBase DebugString;
	DebugString << TEXT("MetaStory (asset: '");
	GetFullName(DebugString);
	DebugString << TEXT("')\n");

	auto PrintObjectNameSafe = [&DebugString](int32 Index, const UObject* Obj)
		{
			DebugString << TEXT("  (");
			DebugString << Index;
			DebugString << TEXT(")");
			if (Obj)
			{
				DebugString << Obj->GetFName();
			}
			else
			{
				DebugString << TEXT("null");
			}
			DebugString << TEXT('\n');
		};
	auto PrintViewNameSafe = [&DebugString](int32 Index, const FConstStructView& View)
		{
			DebugString << TEXT("  (");
			DebugString << Index;
			DebugString << TEXT(")");
			if (View.IsValid())
			{
				DebugString << View.GetScriptStruct()->GetFName();
			}
			else
			{
				DebugString << TEXT("null");
			}
			DebugString << TEXT('\n');
		};

	if (Schema)
	{
		DebugString.Appendf(TEXT("Schema: %s\n"), *WriteToString<128>(Schema->GetFName()));
	}
	else
	{
		DebugString.Append(TEXT("Schema: [None]\n"));
	}

	// Tree items (e.g. tasks, evaluators, conditions)
	DebugString.Appendf(TEXT("\nNodes(%d)\n"), Nodes.Num());
	for (int32 Index = 0; Index < Nodes.Num(); Index++)
	{
		const FConstStructView Node = Nodes[Index];
		PrintViewNameSafe(Index, Node);
	}

	auto PrintInstanceData = [&DebugString, &PrintObjectNameSafe, &PrintViewNameSafe]<typename T>(T& Container, const FStringView Name)
		{
			DebugString.Appendf(TEXT("\n%s(%d)\n"), Name.GetData(), Container.Num());
			for (int32 Index = 0; Index < Container.Num(); Index++)
			{
				if (Container.IsObject(Index))
				{
					const UObject* Data = Container.GetObject(Index);
					PrintObjectNameSafe(Index, Data);
				}
				else
				{
					const FConstStructView Data = Container.GetStruct(Index);
					PrintViewNameSafe(Index, Data);
				}
			}
		};

	// Instance InstanceData data (e.g. tasks)
	PrintInstanceData(DefaultInstanceData, TEXT("Instance Data"));

	// Shared Instance data (e.g. conditions/evaluators)
	PrintInstanceData(SharedInstanceData, TEXT("Shared Instance Data"));

	// Evaluation Scope InstanceData data (e.g. conditions/evaluators)
	PrintInstanceData(DefaultEvaluationScopeInstanceData, TEXT("Evaluation Scope Instance Data"));

	// Execution Runtime InstanceData data (e.g. tasks)
	PrintInstanceData(GetDefaultExecutionRuntimeData(), TEXT("Execution Runtime Instance Data"));

	// External data (e.g. fragments, subsystems)
	DebugString.Appendf(TEXT("\nExternal Data(%d)\n"), ExternalDataDescs.Num());
	if (ExternalDataDescs.Num())
	{
		DebugString.Appendf(TEXT("  [ (Idx) | %-40s | %-8s | %15s ]\n"), TEXT("Name"), TEXT("Optional"), TEXT("Handle"));
		for (int32 DataDescIndex = 0; DataDescIndex < ExternalDataDescs.Num(); ++DataDescIndex)
		{
			const FMetaStoryExternalDataDesc& Desc = ExternalDataDescs[DataDescIndex];
			DebugString.Appendf(TEXT("  | (%3d) | %-40s | %8s | %15s |\n"),
				DataDescIndex,
				Desc.Struct ? *Desc.Struct->GetName() : TEXT("null"),
				*UEnum::GetDisplayValueAsText(Desc.Requirement).ToString(),
				*Desc.Handle.DataHandle.Describe());
		}
	}

	// Bindings
#if WITH_PROPERTYBINDINGUTILS_DEBUG
	DebugString << PropertyBindings.DebugAsString();
#endif

	// Frames
	DebugString.Appendf(TEXT("\nFrames(%d)\n"), Frames.Num());
	if (Frames.Num())
	{
		DebugString.Appendf(TEXT("  [ (Idx) | %-4s ]\n"), TEXT("Root"));
		for (int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex)
		{
			const FMetaStoryCompactFrame& Frame = Frames[FrameIndex];
			DebugString.Appendf(TEXT("  | (%3d) | %-4d |\n"),
				FrameIndex,
				Frame.RootState.Index
			);
		}
	}

	// States
	DebugString.Appendf(TEXT("\nStates(%d)\n"), States.Num());
	if (States.Num())
	{
		DebugString.Appendf(TEXT("  [ (Idx) | %-30s | %15s | %5s [%3s:%-3s[ | Begin Idx : %4s %4s %4s %4s | Num : %4s %4s %4s %4s ]\n"),
			TEXT("Name"), TEXT("Parent"), TEXT("Child"), TEXT("Beg"), TEXT("End"),
			TEXT("Cond"), TEXT("Tr"), TEXT("Tsk"), TEXT("Uti"), TEXT("Cond"), TEXT("Tr"), TEXT("Tsk"), TEXT("Uti"));
		for (int32 StateIndex = 0; StateIndex < States.Num(); ++StateIndex)
		{
			const FMetaStoryCompactState& State = States[StateIndex];
			DebugString.Appendf(TEXT("  | (%3d) | %-30s | %15s | %5s [%3d:%-3d[ | %9s   %4d %4d %4d %4d | %3s   %4d %4d %4d %4d |\n"),
				StateIndex,
				*State.Name.ToString(),
				*State.Parent.Describe(),
				TEXT(" "), State.ChildrenBegin, State.ChildrenEnd,
				TEXT(" "), State.EnterConditionsBegin, State.TransitionsBegin, State.TasksBegin, State.UtilityConsiderationsBegin,
				TEXT(" "), State.EnterConditionsNum, State.TransitionsNum, State.TasksNum, State.UtilityConsiderationsNum
			);
		}

		auto AppendBinary = [&DebugString](const uint32 Mask)
			{
				for (int32 Index = (sizeof(uint32) * 8) - 1; Index >= 0; --Index)
				{
					const TCHAR BinaryValue = ((Mask >> Index) & 1U) ? TEXT('1') : TEXT('0');
					DebugString << BinaryValue;
				}
			};

		DebugString.Appendf(TEXT("\n  [ (Idx) | %32s | %8s ]\n"),
			TEXT("CompMask"), TEXT("CustTick"));
		for (int32 StateIndex = 0; StateIndex < States.Num(); ++StateIndex)
		{
			const FMetaStoryCompactState& State = States[StateIndex];
			DebugString.Appendf(TEXT("  | (%3d) | "), StateIndex);
			AppendBinary(State.CompletionTasksMask);
			DebugString.Appendf(TEXT(" | %3f |\n"), State.CustomTickRate);
		}
	}

	// Transitions
	DebugString.Appendf(TEXT("\nTransitions(%d)\n"), Transitions.Num());
	if (Transitions.Num())
	{
		DebugString.Appendf(TEXT("  [ (Idx) | %-3s | %15s | %-20s | %-40s | %-40s | %-8s ]\n")
			, TEXT("Idx"), TEXT("State"), TEXT("Transition Trigger"), TEXT("Transition Event Tag"), TEXT("Transition Event Payload"), TEXT("Cond:Num"));
		for (int32 TransitionIndex = 0; TransitionIndex < Transitions.Num(); ++TransitionIndex)
		{
			const FMetaStoryCompactStateTransition& Transition = Transitions[TransitionIndex];
			DebugString.Appendf(TEXT("  | (%3d) | %3d | %15s | %-20s | %-40s | %-40s | %4d:%3d |\n"),
				TransitionIndex,
				Transition.ConditionsBegin,
				*Transition.State.Describe(),
				*UEnum::GetDisplayValueAsText(Transition.Trigger).ToString(),
				*Transition.RequiredEvent.Tag.ToString(),
				Transition.RequiredEvent.PayloadStruct ? *Transition.RequiredEvent.PayloadStruct->GetName() : TEXT("None"),
				Transition.ConditionsBegin,
				Transition.ConditionsNum);
		}
	}

	// @todo: add output binding batch index info

	// Evaluators
	DebugString.Appendf(TEXT("\nEvaluators(%d)\n"), EvaluatorsNum);
	if (EvaluatorsNum)
	{
		DebugString.Appendf(TEXT("  [ (Idx) | %-30s | %8s | %14s ]\n"),
			TEXT("Name"), TEXT("Bindings"), TEXT("Struct Idx"));
		for (int32 EvalIndex = EvaluatorsBegin; EvalIndex < (EvaluatorsBegin + EvaluatorsNum); EvalIndex++)
		{
			const FMetaStoryEvaluatorBase& Eval = Nodes[EvalIndex].Get<const FMetaStoryEvaluatorBase>();
			DebugString.Appendf(TEXT("  | (%3d) | %-30s | %8d | %14s |\n"),
				EvalIndex,
				*Eval.Name.ToString(),
				Eval.BindingsBatch.Get(),
				*Eval.InstanceDataHandle.Describe());
		}
	}

	// Tasks
	int32 NumberOfTasks = GlobalTasksNum;
	for (const FMetaStoryCompactState& State : States)
	{
		NumberOfTasks += State.TasksNum;
	}

	DebugString.Appendf(TEXT("\nTasks(%d)\n  [ (Idx) | %-30s | %-30s | %8s | %14s ]\n"),
		NumberOfTasks, TEXT("State"), TEXT("Name"), TEXT("Bindings"), TEXT("Struct Idx"));
	for (const FMetaStoryCompactState& State : States)
	{
		if (State.TasksNum)
		{
			for (int32 TaskIndex = State.TasksBegin; TaskIndex < (State.TasksBegin + State.TasksNum); TaskIndex++)
			{
				const FMetaStoryTaskBase& Task = Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
				DebugString.Appendf(TEXT("  | (%3d) | %-30s | %-30s | %8d | %14s |\n"),
					TaskIndex,
					*State.Name.ToString(),
					*Task.Name.ToString(),
					Task.BindingsBatch.Get(),
					*Task.InstanceDataHandle.Describe());
			}
		}
	}
	for (int32 TaskIndex = GlobalTasksBegin; TaskIndex < (GlobalTasksBegin + GlobalTasksNum); TaskIndex++)
	{
		const FMetaStoryTaskBase& Task = Nodes[TaskIndex].Get<const FMetaStoryTaskBase>();
		DebugString.Appendf(TEXT("  | (%3d) | %-30s | %-30s | %8d | %14s |\n"),
			TaskIndex,
			TEXT("Global"),
			*Task.Name.ToString(),
			Task.BindingsBatch.Get(),
			*Task.InstanceDataHandle.Describe());
	}

	// Conditions
	DebugString.Appendf(TEXT("\nConditions\n  [ (Idx) | %-30s | %8s | %12s | %14s ]\n"),
		TEXT("Name"), TEXT("Operand"), TEXT("Evaluation"), TEXT("Struct Idx"));
	{
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if(const FMetaStoryConditionBase* Cond = Nodes[Index].GetPtr<const FMetaStoryConditionBase>())
			{
				DebugString.Appendf(TEXT("  | (%3d) | %-30s | %8s | %12s | %14s |\n"),
					Index,
					*Cond->Name.ToString(),
					*UEnum::GetDisplayValueAsText(Cond->Operand).ToString(),
					*UEnum::GetDisplayValueAsText(Cond->EvaluationMode).ToString(),
					*Cond->InstanceDataHandle.Describe());
			}
		}
	}

	// Extensions
	DebugString.Appendf(TEXT("\nExtensions(%d)\n"), Extensions.Num());
	for (int32 Index = 0; Index < Extensions.Num(); ++Index)
	{
		if (const UMetaStoryExtension* Extension = Extensions[Index])
		{
			if (Extension)
			{
				DebugString.Appendf(TEXT("  %s\n"), *WriteToString<128>(Extension->GetFName()));
			}
			else
			{
				DebugString.Append(TEXT("  [None]\n"));
			}
		}
	}

	return DebugString.ToString();
}
#endif // WITH_EDITOR || WITH_METASTORY_DEBUG

#if WITH_METASTORY_DEBUG
namespace UE::MetaStory::Debug::Private
{
	IConsoleVariable* FindCVarInstanceDataGC()
	{
		IConsoleVariable* FoundVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("MetaStory.RuntimeValidation.InstanceDataGC"));
		check(FoundVariable);
		return FoundVariable;
	}
}

void UMetaStory::HandleRuntimeValidationPreGC()
{
	GCObjectDatas.Reset();
	auto AddInstanceObject = [this](const FMetaStoryInstanceStorage& InstanceData, int32 PerThreadSharedIndex, FDebugInstanceData::EContainer Container)
		{
			const int32 NumObject = InstanceData.Num();
			for (int32 Index = 0; Index < NumObject; ++Index)
			{
				FConstStructView Instance = InstanceData.GetStruct(Index);
				if (!Instance.IsValid())
				{
					const TCHAR* Msg = (Container == FDebugInstanceData::EContainer::SharedInstance) ? TEXT("A shared") : TEXT("An");
					ensureAlwaysMsgf(false, TEXT("%s instance data is invalid. InstanceDataIndex:%d. PerThreadSharedIndex:%d."), Msg, Index, PerThreadSharedIndex);
					UE::MetaStory::Debug::Private::FindCVarInstanceDataGC()->Set(false);
					GCObjectDatas.Reset();
					break;
				}

				FDebugInstanceData& Data = GCObjectDatas.AddDefaulted_GetRef();
				Data.InstanceDataStructIndex = Index;
				Data.SharedInstanceDataIndex = PerThreadSharedIndex;
				Data.Container = Container;

				if (const FMetaStoryInstanceObjectWrapper* Wrapper = Instance.GetPtr<const FMetaStoryInstanceObjectWrapper>())
				{
					if (!IsValid(Wrapper->InstanceObject))
					{
						const TCHAR* Msg = (Container == FDebugInstanceData::EContainer::SharedInstance) ? TEXT("A shared") : TEXT("An");
						ensureAlwaysMsgf(false, TEXT("%s instance data object is invalid. InstanceDataIndex:%d. PerThreadSharedIndex:%d."), Msg, Index, PerThreadSharedIndex);
						UE::MetaStory::Debug::Private::FindCVarInstanceDataGC()->Set(false);
						GCObjectDatas.Reset();
						break;
					}

					Data.Object = FWeakObjectPtr(Wrapper->InstanceObject);
					Data.Type = FDebugInstanceData::EObjectType::ObjectInstance;
				}
				else
				{
					Data.Object = FWeakObjectPtr(Instance.GetScriptStruct());
					Data.Type = FDebugInstanceData::EObjectType::Struct;
				}
			}
		};

	AddInstanceObject(DefaultInstanceData.GetStorage(), INDEX_NONE, FDebugInstanceData::EContainer::DefaultInstance);
	AddInstanceObject(SharedInstanceData.GetStorage(), INDEX_NONE, FDebugInstanceData::EContainer::SharedInstance);

	{
		UE::TReadScopeLock ReadLock(PerThreadSharedInstanceDataLock);
		for (int32 Index = 0; Index < PerThreadSharedInstanceData.Num(); ++Index)
		{
			TSharedPtr<FMetaStoryInstanceData>& SharedInstance = PerThreadSharedInstanceData[Index];
			if (ensureMsgf(SharedInstance, TEXT("The shared instance data is invalid. Index %d"), Index))
			{
				AddInstanceObject(SharedInstance->GetStorage(), Index, FDebugInstanceData::EContainer::SharedInstance);
			}
		}
	}
}

void UMetaStory::HandleRuntimeValidationPostGC()
{
	for (const FDebugInstanceData& Data : GCObjectDatas)
	{
		if (Data.Object.IsStale())
		{
			if (Data.Type == FDebugInstanceData::EObjectType::ObjectInstance)
			{
				const TCHAR* Msg = (Data.Container == FDebugInstanceData::EContainer::SharedInstance) ? TEXT("A shared") : TEXT("An");
				ensureAlwaysMsgf(false, TEXT("%s instance data object is GCed. InstDataIndex:%d. SharedIndex:%d."), Msg, Data.InstanceDataStructIndex, Data.SharedInstanceDataIndex);
			}
			else
			{ //-V523 disabling identical branch warning
				const TCHAR* Msg = (Data.Container == FDebugInstanceData::EContainer::SharedInstance) ? TEXT("A shared") : TEXT("An");
				ensureAlwaysMsgf(false, TEXT("%s instance data struct type is GCed. InstDataIndex:%d. SharedIndex:%d."), Msg, Data.InstanceDataStructIndex, Data.SharedInstanceDataIndex);
			}
			UE::MetaStory::Debug::Private::FindCVarInstanceDataGC()->Set(false);
			break;
		}
	}
	GCObjectDatas.Reset();
}
#endif
