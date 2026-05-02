// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "PropertyBindingExtension.h"
#include "PropertyBindingBindingCollectionOwner.h"
#include "MetaStoryPropertyBindings.h"
#include "MetaStoryEditorNode.h"
#include "MetaStoryEditorPropertyBindings.generated.h"

#define UE_API METASTORYEDITORMODULE_API

enum class EMetaStoryNodeFormatting : uint8;
class IMetaStoryEditorPropertyBindingsOwner;
enum class EMetaStoryVisitor : uint8;

/**
 * Editor representation of all property bindings in a MetaStory
 */
USTRUCT()
struct FMetaStoryEditorPropertyBindings : public FPropertyBindingBindingCollection
{
	GENERATED_BODY()

	/** @return const array view to all bindings. */
	TConstArrayView<FMetaStoryPropertyPathBinding> GetBindings() const
	{
		return PropertyBindings;
	}

	/** @return array view to all bindings. */
	TArrayView<FMetaStoryPropertyPathBinding> GetMutableBindings()
	{
		return PropertyBindings;
	}

	void AddMetaStoryBinding(FMetaStoryPropertyPathBinding&& InBinding)
	{
		RemoveBindings(InBinding.GetTargetPath(), ESearchMode::Exact);
		PropertyBindings.Add(MoveTemp(InBinding));
	}

	//~ Begin FPropertyBindingBindingCollection overrides
	UE_API virtual int32 GetNumBindableStructDescriptors() const override;
	UE_API virtual const FPropertyBindingBindableStructDescriptor* GetBindableStructDescriptorFromHandle(FConstStructView InSourceHandleView) const override;

	UE_API virtual int32 GetNumBindings() const override;
	UE_API virtual void ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const override;
	UE_API virtual void ForEachBinding(FPropertyBindingIndex16 InBegin, FPropertyBindingIndex16 InEnd, TFunctionRef<void(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const override;
	UE_API virtual void ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction) override;
	UE_API virtual void VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const override;
	UE_API virtual void VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction) override;
	//~ End FPropertyBindingBindingCollection overrides

	/**
	 * Adds binding between PropertyFunction of the provided type and destination path.
	 * @param InPropertyFunctionNodeStruct Struct of PropertyFunction.
	 * @param InSourcePathSegments Binding source property path segments.
	 * @param InTargetPath Binding target property path.
	 * @return Constructed binding source property path.
	 */
	UE_API FPropertyBindingPath AddFunctionBinding(const UScriptStruct* InPropertyFunctionNodeStruct, TConstArrayView<FPropertyBindingPathSegment> InSourcePathSegments, const FPropertyBindingPath& InTargetPath);

	/**
	 * Adds an output binding between source and target path.
	 * Output Binding will copy value from target to source
	 * @param InSourcePath Binding source property path segments.
	 * @param InTargetPath Binding target property path.
	 * @return Constructed Binding.
	 */
	UE_API const FMetaStoryPropertyPathBinding* AddOutputBinding(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/**
	 * Adds binding between source and destination paths. Removes any bindings to TargetPath before adding the new one.
	 * @param SourcePath Binding source property path.
	 * @param TargetPath Binding target property path.
	 */
	UE_DEPRECATED(5.6, "Use AddBinding taking FPropertyBindingPath instead")
	void AddPropertyBinding(const FMetaStoryPropertyPath& SourcePath, const FMetaStoryPropertyPath& TargetPath)
	{
		AddBinding(SourcePath, TargetPath);
	}
	
	/**
	 * Adds binding.
	 * @param Binding Binding to be added.
	*/
	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	void AddPropertyBinding(const FMetaStoryPropertyPathBinding& Binding)
	{
	}

	
	UE_DEPRECATED(5.6, "Use AddFunctionBinding taking FPropertyBindingPath instead")
	FMetaStoryPropertyPath AddFunctionPropertyBinding(const UScriptStruct* InPropertyFunctionNodeStruct, TConstArrayView<FPropertyBindingPathSegment> InSourcePathSegments, const FMetaStoryPropertyPath& InTargetPath)
	{
		return AddFunctionBinding(InPropertyFunctionNodeStruct, InSourcePathSegments, InTargetPath);
	}

	/**
	 * Removes all bindings to target path.
	 * @param TargetPath Target property path.
	 */ 
	UE_DEPRECATED(5.6, "Use RemoveBinding taking FPropertyBindingPath instead")
	void RemovePropertyBindings(const FMetaStoryPropertyPath& TargetPath, ESearchMode SearchMode = ESearchMode::Exact)
	{
		RemoveBindings(TargetPath, SearchMode);
	}
	
	/**
	 * Has any binding to the target path.
	 * @return True of the target path has any bindings.
	 */
	UE_DEPRECATED(5.6, "Use HasBinding taking FPropertyBindingPath instead")
	bool HasPropertyBinding(const FMetaStoryPropertyPath& TargetPath, ESearchMode SearchMode = ESearchMode::Exact) const
	{
		return HasBinding(TargetPath, SearchMode);
	}

	UE_DEPRECATED(5.6, "Use FindBinding taking FPropertyBindingPath instead")
	UE_API const FMetaStoryPropertyPathBinding* FindPropertyBinding(const FMetaStoryPropertyPath& TargetPath, ESearchMode SearchMode = ESearchMode::Exact) const
	{
		return static_cast<const FMetaStoryPropertyPathBinding*>(FindBinding(TargetPath, SearchMode));
	}

	/**
	 * @return Source path for given target path, or null if binding does not exists.
	 */
	UE_DEPRECATED(5.6, "Use GetBindingSource taking FPropertyBindingPath instead")
	const FMetaStoryPropertyPath* GetPropertyBindingSource(const FMetaStoryPropertyPath& TargetPath) const
	{
		return nullptr;
	}

	/**
	 * Returns all pointers to bindings for a specified structs based in struct ID.
	 * @param StructID ID of the struct to find bindings for.
	 * @param OutBindings Bindings for specified struct.
	 */
	UE_DEPRECATED(5.6, "Use GetBindingsFor taking FPropertyBindingPath instead")
	void GetPropertyBindingsFor(const FGuid StructID, TArray<const FMetaStoryPropertyPathBinding*>& OutBindings) const
	{
	}

	/**
	 * Removes bindings which do not point to valid structs IDs.
	 * @param ValidStructs Set of struct IDs that are currently valid.
	 */
	UE_DEPRECATED(5.6, "Use RemoveInvalidBindings taking FPropertyBindingDataView instead")
	void RemoveUnusedBindings(const TMap<FGuid, const FMetaStoryDataView>& ValidStructs)
	{
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

protected:
	//~ Begin FPropertyBindingBindingCollection overrides
	virtual FPropertyBindingBinding* AddBindingInternal(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath) override;
	UE_API virtual void CopyBindingsInternal(const FGuid InFromStructID, const FGuid InToStructID) override;
	UE_API virtual void RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate) override;
	UE_API virtual bool HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
	UE_API virtual const FPropertyBindingBinding* FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
	//~ Begin FPropertyBindingBindingCollection overrides

private:
	UPROPERTY()
	TArray<FMetaStoryPropertyPathBinding> PropertyBindings;
};


UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMetaStoryEditorPropertyBindingsOwner : public UPropertyBindingBindingCollectionOwner
{
	GENERATED_UINTERFACE_BODY()
};

/** Struct of Parameters used to Create a Property */
struct UE_DEPRECATED(5.6, "Use FPropertyCreationDesc instead") FMetaStoryEditorPropertyCreationDesc
{
	/** Property Bag Description of the Property to Create */
	FPropertyBagPropertyDesc PropertyDesc;

	/** Optional: property to copy into the new created property */
	const FProperty* SourceProperty = nullptr;

	/** Optional: container address of the property to copy */
	const void* SourceContainerAddress = nullptr;
};

class IMetaStoryEditorPropertyBindingsOwner : public IPropertyBindingBindingCollectionOwner
{
	GENERATED_BODY()

public:

	/**
	 * Returns structs within the owner that are visible for target struct.
	 * @param TargetStructID Target struct ID
	 * @param OutStructDescs Result descriptors of the visible structs.
	 */
	UE_DEPRECATED(5.6, "Use version taking FPropertyBindingBindableStructDescriptor instead")
	virtual void GetAccessibleStructs(const FGuid TargetStructID, TArray<FMetaStoryBindableStructDesc>& OutStructDescs) const final
	{
	}

	/**
	 * Returns struct descriptor based on struct ID.
	 * @param StructID Target struct ID
	 * @param OutStructDesc Result descriptor.
	 * @return True if struct found.
	 */
	UE_DEPRECATED(5.6, "Use version taking FPropertyBindingBindableStructDescriptor instead")
	virtual bool GetStructByID(const FGuid StructID, FMetaStoryBindableStructDesc& OutStructDesc) const final
	{
		return false;
	}

	/**
	 * Finds a bindable context struct based on name and type.
	 * @param ObjectType Object type to match
	 * @param ObjectNameHint Name to use if multiple context objects of same type are found. 
	 */
	virtual FMetaStoryBindableStructDesc FindContextData(const UStruct* ObjectType, const FString ObjectNameHint) const PURE_VIRTUAL(IMetaStoryEditorPropertyBindingsOwner::FindContextData, return {}; );

	/**
	 * Returns data view based on struct ID.
	 * @param StructID Target struct ID
	 * @param OutDataView Result data view.
	 * @return True if struct found.
	 */
	UE_DEPRECATED(5.6, "Use version taking FPropertyBindingDataView instead")
	virtual bool GetDataViewByID(const FGuid StructID, FMetaStoryDataView& OutDataView) const final
	{
		return false;
	}

	/** @return Pointer to editor property bindings. */
	virtual FMetaStoryEditorPropertyBindings* GetPropertyEditorBindings() PURE_VIRTUAL(IMetaStoryEditorPropertyBindingsOwner::GetPropertyEditorBindings, return nullptr; );

	/** @return Pointer to editor property bindings. */
	virtual const FMetaStoryEditorPropertyBindings* GetPropertyEditorBindings() const PURE_VIRTUAL(IMetaStoryEditorPropertyBindingsOwner::GetPropertyEditorBindings, return nullptr; );

	virtual EMetaStoryVisitor EnumerateBindablePropertyFunctionNodes(TFunctionRef<EMetaStoryVisitor(const UScriptStruct* NodeStruct, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const PURE_VIRTUAL(IMetaStoryEditorPropertyBindingsOwner::EnumerateBindablePropertyFunctionNodes, return static_cast<EMetaStoryVisitor>(0); );

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/**
	 * Creates the given properties in the property bag of the struct matching the given struct ID
	 * @param StructID Target struct ID
	 * @param InOutCreationDescs the descriptions of the properties to create. This is modified to update the property names that actually got created
	 */
	UE_DEPRECATED(5.6, "Use version taking FPropertyCreationDesc instead")
	virtual void CreateParameters(const FGuid StructID, TArrayView<FMetaStoryEditorPropertyCreationDesc> InOutCreationDescs) final
	{
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
};

// TODO: We should merge this with IMetaStoryEditorPropertyBindingsOwner and FMetaStoryEditorPropertyBindings.
// Currently FMetaStoryEditorPropertyBindings is meant to be used as a member for just to store things,
// IMetaStoryEditorPropertyBindingsOwner is meant return model specific stuff,
// and IMetaStoryBindingLookup is used in non-editor code and it cannot be in FMetaStoryEditorPropertyBindings because bindings don't know about the owner.
struct FMetaStoryBindingLookup : public IMetaStoryBindingLookup
{
	UE_API FMetaStoryBindingLookup(const IMetaStoryEditorPropertyBindingsOwner* InBindingOwner);

	const IMetaStoryEditorPropertyBindingsOwner* BindingOwner = nullptr;

	UE_API virtual const FPropertyBindingPath* GetPropertyBindingSource(const FPropertyBindingPath& InTargetPath) const override;
	UE_API virtual FText GetPropertyPathDisplayName(const FPropertyBindingPath& InTargetPath, EMetaStoryNodeFormatting Formatting) const override;
	UE_API virtual FText GetBindingSourceDisplayName(const FPropertyBindingPath& InTargetPath, EMetaStoryNodeFormatting Formatting) const override;
	UE_API virtual const FProperty* GetPropertyPathLeafProperty(const FPropertyBindingPath& InPath) const override;

};

#undef UE_API
