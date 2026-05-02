// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryDelegate.h"
#include "MetaStoryEditorTypes.h"
#include "MetaStoryPropertyBindings.h"
#include "MetaStoryPropertyBindingCompiler.generated.h"

#define UE_API METASTORYEDITORMODULE_API

enum class EPropertyAccessCompatibility;
struct FMetaStoryCompilerLog;
struct FMetaStoryPropertyPathBinding;

/**
 * Helper class to compile editor representation of property bindings into runtime representation.
 * TODO: Better error reporting, something that can be shown in the UI.
 */
USTRUCT()
struct FMetaStoryPropertyBindingCompiler
{
	GENERATED_BODY()

	/**
	  * Initializes the compiler to compile copies to specified Property Bindings.
	  * @param PropertyBindings - Reference to the Property Bindings where all the batches will be stored.
	  * @return true on success.
	  */
	[[nodiscard]] UE_API bool Init(FMetaStoryPropertyBindings& InPropertyBindings, FMetaStoryCompilerLog& InLog);

	/**
	  * Compiles a batch of property copies.
	  * @param TargetStruct - Description of the structs which contains the target properties.
	  * @param PropertyBindings - Array of bindings to compile, all bindings that point to TargetStructs will be added to the batch.
	  * @param PropertyFuncsBegin - Index of the first PropertyFunction belonging to this batch.
	  * @param PropertyFuncsEnd - Index of the last PropertyFunction belonging to this batch.
	  * @param OutBatchIndex - Resulting batch index, if index is INDEX_NONE, no bindings were found and no batch was generated.
	  * @return True on success, false on failure.
	 */
	[[nodiscard]] UE_API bool CompileBatch(const FMetaStoryBindableStructDesc& TargetStruct, TConstArrayView<FMetaStoryPropertyPathBinding> PropertyBindings, FMetaStoryIndex16 PropertyFuncsBegin, FMetaStoryIndex16 PropertyFuncsEnd, int32& OutBatchIndex);

	 /**
	  * Compiles delegates dispatcher for selected struct.
	  * @param SourceStruct - Description of the structs which contains the delegate dispatchers.
	  * @param PreviousDistpatchers - Array of previous dispatchers IDs.
	  * @param DelegateSourceBindings - Array of bindings dispatcher to compile.
	  * @param InstanceDataView - View to the instance data
	  * @return True on success, false on failure.
	 */
	[[nodiscard]] UE_API bool CompileDelegateDispatchers(const FMetaStoryBindableStructDesc& SourceStruct, TConstArrayView<FMetaStoryEditorDelegateDispatcherCompiledBinding> PreviousDistpatchers, TConstArrayView<FMetaStoryPropertyPathBinding> DelegateSourceBindings, FMetaStoryDataView InstanceDataView);

	 /**
	  * Compiles delegates listener for selected struct.
	  * @param TargetStruct - Description of the structs which contains the delegate listeners.
	  * @param DelegateTargetBindings - Array of bindings listeners to compile.
	  * @param InstanceDataView - View to the instance data
	  * @return True on success, false on failure.
	 */
	[[nodiscard]] UE_API bool CompileDelegateListeners(const FMetaStoryBindableStructDesc& TargetStruct, TConstArrayView<FMetaStoryPropertyPathBinding> DelegateSourceBindings, FMetaStoryDataView InstanceDataView);

	/**
	  * Compiles references for selected struct
	  * @param TargetStruct - Description of the structs which contains the target properties.
	  * @param PropertyReferenceBindings - Array of bindings to compile, all bindings that point to TargetStructs will be added.
	  * @param InstanceDataView - view to the instance data
	  * @return True on success, false on failure.
	 */
	[[nodiscard]] UE_API bool CompileReferences(const FMetaStoryBindableStructDesc& TargetStruct, TConstArrayView<FMetaStoryPropertyPathBinding> PropertyReferenceBindings, FMetaStoryDataView InstanceDataView, const TMap<FGuid, const FMetaStoryDataView>& IDToStructValue);

	/** Finalizes compilation, should be called once all batches are compiled. */
	UE_API void Finalize();

	/**
	  * Adds source struct. When compiling a batch, the bindings can be between any of the defined source structs, and the target struct.
	  * Source structs can be added between calls to Compilebatch().
	  * @param SourceStruct - Description of the source struct to add.
	  * @return Source struct index.
	  */
	UE_API int32 AddSourceStruct(const FMetaStoryBindableStructDesc& SourceStruct);

	/** @return delegate dispatcher residing behind given path. */
	UE_API FMetaStoryDelegateDispatcher GetDispatcherFromPath(const FPropertyBindingPath& PathToDispatcher) const;

	/** @return the list of compiled delegate dispatcher. */
	UE_API TArray<FMetaStoryEditorDelegateDispatcherCompiledBinding> GetCompiledDelegateDispatchers() const;

	const FMetaStoryBindableStructDesc* GetSourceStructDescByID(const FGuid& ID) const
	{
		return SourceStructs.FindByPredicate([ID](const FMetaStoryBindableStructDesc& Structs) { return (Structs.ID == ID); });
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	UE_API FMetaStoryIndex16 GetDispatcherIDFromPath(const FMetaStoryPropertyPath& PathToDispatcher) const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

protected:

	void StoreSourceStructs();

	UPROPERTY()
	TArray<FMetaStoryBindableStructDesc> SourceStructs;

	/** Representation of compiled reference. */
	struct FCompiledReference
	{
		FPropertyBindingPath Path;
		FMetaStoryIndex16 Index;
	};

	TArray<FCompiledReference> CompiledReferences;

	/** Dispatcher used by bindings. */
	TArray<FMetaStoryEditorDelegateDispatcherCompiledBinding> CompiledDelegateDispatchers;
	int32 ListenersNum = 0;

	FMetaStoryPropertyBindings* PropertyBindings = nullptr;

	FMetaStoryCompilerLog* Log = nullptr;
};

#undef UE_API
