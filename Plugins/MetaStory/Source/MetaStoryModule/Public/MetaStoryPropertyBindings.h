// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBindingBindableStructDescriptor.h"
#include "PropertyBindingPath.h"
#include "PropertyBindingBinding.h"
#include "PropertyBindingBindingCollection.h"
#include "MetaStoryNodeBase.h"
#include "MetaStoryTypes.h"
#include "MetaStoryPropertyRefHelpers.h"
#include "StructUtils/StructView.h"
#include "MetaStoryIndexTypes.h"
#include "MetaStoryPropertyBindings.generated.h"

#define UE_API METASTORYMODULE_API

class FProperty;
struct FMetaStoryPropertyBindingCompiler;
struct FMetaStoryPropertyRef;
class UMetaStory;
enum class EMetaStoryNodeFormatting : uint8;

UENUM()
enum class EMetaStoryBindableStructSource : uint8
{
	/** Source is MetaStory context object */
	Context,
	/** Source is MetaStory parameter */
	Parameter,
	/** Source is MetaStory evaluator */
	Evaluator,
	/** Source is MetaStory global task */
	GlobalTask,
	/** Source is State parameter */
	StateParameter,
	/** Source is State task */
	Task,
	/** Source is State condition */
	Condition,
	/** Source is State utility consideration */
	Consideration,
	/** Source is MetaStory event used by transition */
	TransitionEvent,
	/** Source is MetaStory event used by state selection */
	StateEvent,
	/** Source is Property Function */
	PropertyFunction,
	/** Source is Transition */
	Transition,
};

namespace UE::MetaStory
{
	/** Can that binding type accept a task instance data for a source. */
	[[nodiscard]] UE_API bool AcceptTaskInstanceData(EMetaStoryBindableStructSource Target);
}


/**
 * Describes how the copy should be performed.
 */
enum class UE_DEPRECATED(5.6, "Use EPropertyCopyType instead")  EMetaStoryPropertyCopyType : uint8
{
	None,						// No copying
	
	CopyPlain,					// For plain old data types, we do a simple memcpy.
	CopyComplex,				// For more complex data types, we need to call the properties copy function
	CopyBool,					// Read and write properties using bool property helpers, as source/dest could be bitfield or boolean
	CopyStruct,					// Use struct copy operation, as this needs to correctly handle CPP struct ops
	CopyObject,					// Read and write properties using object property helpers, as source/dest could be regular/weak/soft etc.
	CopyName,					// FName needs special case because its size changes between editor/compiler and runtime.
	CopyFixedArray,				// Array needs special handling for fixed size TArrays

	StructReference,			// Copies pointer to a source struct into a FMetaStoryStructRef.

	/* Promote the type during the copy */

	/* Bool promotions */
	PromoteBoolToByte,
	PromoteBoolToInt32,
	PromoteBoolToUInt32,
	PromoteBoolToInt64,
	PromoteBoolToFloat,
	PromoteBoolToDouble,

	/* Byte promotions */
	PromoteByteToInt32,
	PromoteByteToUInt32,
	PromoteByteToInt64,
	PromoteByteToFloat,
	PromoteByteToDouble,

	/* Int32 promotions */
	PromoteInt32ToInt64,
	PromoteInt32ToFloat,	// This is strictly sketchy because of potential data loss, but it is usually OK in the general case
	PromoteInt32ToDouble,

	/* UInt32 promotions */
	PromoteUInt32ToInt64,
	PromoteUInt32ToFloat,	// This is strictly sketchy because of potential data loss, but it is usually OK in the general case
	PromoteUInt32ToDouble,

	/* Float promotions */
	PromoteFloatToInt32,
	PromoteFloatToInt64,
	PromoteFloatToDouble,

	/* Double promotions */
	DemoteDoubleToInt32,
	DemoteDoubleToInt64,
	DemoteDoubleToFloat,
};

/** Enum describing property compatibility */
enum class UE_DEPRECATED(5.6, "Use UE::PropertyBinding::EPropertyCompatibility instead") EMetaStoryPropertyAccessCompatibility : uint8
{
	/** Properties are incompatible */
	Incompatible,
	/** Properties are directly compatible */
	Compatible,	
	/** Properties can be copied with a simple type promotion */
	Promotable,
};

/**
 * Descriptor for a struct or class that can be a binding source or target.
 * Each struct has unique identifier, which is used to distinguish them, and name that is mostly for debugging and UI.
 */
USTRUCT()
struct FMetaStoryBindableStructDesc : public FPropertyBindingBindableStructDescriptor
{
	GENERATED_BODY()

	FMetaStoryBindableStructDesc() = default;

#if WITH_EDITORONLY_DATA
	FMetaStoryBindableStructDesc(const FString& InStatePath, const FName InName, const UStruct* InStruct, const FMetaStoryDataHandle InDataHandle, const EMetaStoryBindableStructSource InDataSource, const FGuid InGuid)
		: FPropertyBindingBindableStructDescriptor(InName, InStruct, InGuid)
		, DataHandle(InDataHandle)
		, DataSource(InDataSource)
		, StatePath(InStatePath)
	{
	}

	virtual FString GetSection() const override
	{
		return StatePath;
	}
#endif // WITH_EDITORONLY_DATA

	UE_API virtual FString ToString() const override;

	/** Runtime data the struct represents. */
	UPROPERTY()
	FMetaStoryDataHandle DataHandle = FMetaStoryDataHandle::Invalid;

	/** Type of the source. */
	UPROPERTY()
	EMetaStoryBindableStructSource DataSource = EMetaStoryBindableStructSource::Context;

#if WITH_EDITORONLY_DATA
	/** In Editor path to State containing the data. */
	UPROPERTY(Transient)
	FString StatePath;
#endif
};

struct UE_DEPRECATED(5.6, "Use FPropertyBindingPathIndirection instead") FMetaStoryPropertyPathIndirection : FPropertyBindingPathIndirection
{
};

/**
 * Representation of a property path used for property binding in MetaStory.
 */
struct UE_DEPRECATED(5.6, "Use FPropertyBindingPath instead") METASTORYMODULE_API FMetaStoryPropertyPath final : FPropertyBindingPath
{
	using FPropertyBindingPath::FPropertyBindingPath;

	//~ Intentionally not explicit to facilitate introduction of base class FPropertyBindingPath
	FMetaStoryPropertyPath(const FPropertyBindingPath& Other)
		: FPropertyBindingPath(Other)
	{
	}
};


USTRUCT()
struct UE_DEPRECATED(all, "Use FPropertyBindingPath instead.") FMetaStoryEditorPropertyPath
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	/** Handle of the struct this property path is relative to. */
	UPROPERTY()
	FGuid StructID;

	/** Property path segments */
	UPROPERTY()
	TArray<FString> Path;

	bool IsValid() const
	{
		return StructID.IsValid();
	}
#endif // WITH_EDITORONLY_DATA
};

/**
 * Representation of a property binding in MetaStory
 */
USTRUCT()
struct FMetaStoryPropertyPathBinding : public FPropertyBindingBinding
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryPropertyPathBinding() = default;

	FMetaStoryPropertyPathBinding(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath, const bool bInIsOutputBinding)
		: FPropertyBindingBinding(InSourcePath, InTargetPath)
		, bIsOutputBinding(bInIsOutputBinding)
	{
	}

	UE_DEPRECATED(5.7, "Use the version with bInIsOutputBinding instead.")
	FMetaStoryPropertyPathBinding(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
		: FMetaStoryPropertyPathBinding(InSourcePath, InTargetPath, false)
	{
	}

	FMetaStoryPropertyPathBinding(const FMetaStoryDataHandle InSourceDataHandle, const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath, const bool bInIsOutputBinding)
		: FPropertyBindingBinding(InSourcePath, InTargetPath)
		, SourceDataHandle(InSourceDataHandle)
		, bIsOutputBinding(bInIsOutputBinding)
	{
	}

	UE_DEPRECATED(5.7, "Use the version with bInIsOutputBinding instead.")
	FMetaStoryPropertyPathBinding(const FMetaStoryDataHandle InSourceDataHandle, const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
		: FMetaStoryPropertyPathBinding(InSourceDataHandle, InSourcePath, InTargetPath, false)
	{
	}

#if WITH_EDITOR
	FMetaStoryPropertyPathBinding(FConstStructView InFunctionNodeStruct, const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
		: FPropertyBindingBinding(InFunctionNodeStruct, InSourcePath, InTargetPath)
	{
	}
#endif // WITH_EDITOR

UE_API PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if WITH_EDITORONLY_DATA
	void PostSerialize(const FArchive& Ar);
#endif // WITH_EDITORONLY_DATA

	void SetSourceDataHandle(const FMetaStoryDataHandle NewSourceDataHandle)
	{
		SourceDataHandle = NewSourceDataHandle;
	}

	FMetaStoryDataHandle GetSourceDataHandle() const
	{
		return SourceDataHandle;
	}

	void SetIsOutputBinding(const bool bInIsOutputBinding)
	{
		bIsOutputBinding = bInIsOutputBinding;
	}

	bool IsOutputBinding() const
	{
		return bIsOutputBinding;
	}

protected:
	virtual FConstStructView GetSourceDataHandleStruct() const override
	{
		return FConstStructView::Make(SourceDataHandle);
	}

private:
	/** Describes how to get the source data pointer for the binding. */
	UPROPERTY()
	FMetaStoryDataHandle SourceDataHandle = FMetaStoryDataHandle::Invalid;

	/** Whether this binding is reversed(i.e., copying from target to source). */
	UPROPERTY()
	bool bIsOutputBinding = false;

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY()
	FMetaStoryEditorPropertyPath SourcePath_DEPRECATED;

	UPROPERTY()
	FMetaStoryEditorPropertyPath TargetPath_DEPRECATED;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
};

#if WITH_EDITORONLY_DATA
template<>
struct TStructOpsTypeTraits<FMetaStoryPropertyPathBinding> : public TStructOpsTypeTraitsBase2<FMetaStoryPropertyPathBinding>
{
	enum 
	{
		WithPostSerialize = true,
	};
};
#endif // WITH_EDITORONLY_DATA

/**
 * Representation of a property reference binding in MetaStory.
 */
USTRUCT()
struct FMetaStoryPropertyRefPath
{
	GENERATED_BODY()

	FMetaStoryPropertyRefPath() = default;

	FMetaStoryPropertyRefPath(FMetaStoryDataHandle InSourceDataHandle, const FPropertyBindingPath& InSourcePath)
		: SourcePropertyPath(InSourcePath)
		, SourceDataHandle(InSourceDataHandle)
	{
	}

	const FPropertyBindingPath& GetSourcePath() const { return SourcePropertyPath; }

	FPropertyBindingPath& GetMutableSourcePath() { return SourcePropertyPath; }

	void SetSourceDataHandle(const FMetaStoryDataHandle NewSourceDataHandle) { SourceDataHandle = NewSourceDataHandle; }
	FMetaStoryDataHandle GetSourceDataHandle() const { return SourceDataHandle; }

private:
	/** Source property path of the reference */
	UPROPERTY()
	FPropertyBindingPath SourcePropertyPath;

	/** Describes how to get the source data pointer */
	UPROPERTY()
	FMetaStoryDataHandle SourceDataHandle = FMetaStoryDataHandle::Invalid;
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS
/**
 * Used internally.
 * Property indirection is a resolved property path segment, used for accessing properties in structs.
 */
struct UE_DEPRECATED(5.6, "Use FPropertyBindingPropertyIndirection instead") METASTORYMODULE_API FMetaStoryPropertyIndirection
{
	/** Index in the array the property points at. */
	FMetaStoryIndex16 ArrayIndex = FMetaStoryIndex16::Invalid;

	/** Cached offset of the property */
	uint16 Offset = 0;

	/** Cached offset of the property */
	FMetaStoryIndex16 NextIndex = FMetaStoryIndex16::Invalid;

	/** Type of access/indirection. */
	EPropertyBindingPropertyAccessType Type = EPropertyBindingPropertyAccessType::Offset;

	/** Type of the struct or object instance in case the segment is pointing into an instanced data. */
	TObjectPtr<const UStruct> InstanceStruct = nullptr;

	/** Cached array property. */
	const FArrayProperty* ArrayProperty = nullptr;
};


/**
 * Used internally.
 * Describes property copy, the property from source is copied into the property at the target.
 * Copy target struct is described in the property copy batch.
 */
struct UE_DEPRECATED(5.6, "Use FPropertyBindingCopyInfo instead") METASTORYMODULE_API FMetaStoryPropertyCopy
{
	/** Source property access. */
	FMetaStoryPropertyIndirection SourceIndirection;

	/** Target property access. */
	FMetaStoryPropertyIndirection TargetIndirection;

	/** Cached pointer to the leaf property of the access. */
	const FProperty* SourceLeafProperty = nullptr;

	/** Cached pointer to the leaf property of the access. */
	const FProperty* TargetLeafProperty = nullptr;

	/** Type of the source data, used for validation. */
	TObjectPtr<const UStruct> SourceStructType = nullptr;

	/** Cached property element size * dim. */
	int32 CopySize = 0;

	/** Describes how to get the source data pointer for the copy. */
	FMetaStoryDataHandle SourceDataHandle = FMetaStoryDataHandle::Invalid;

	/** Type of the copy */
	EMetaStoryPropertyCopyType Type = EMetaStoryPropertyCopyType::None;
};


/**
 * Describes a batch of property copies from many sources to one target struct.
 * Note: The batch is used to reference both bindings and copies (a binding turns into copy when resolved).
 */
struct UE_DEPRECATED(5.6, "Use FPropertyBindingCopyInfoBatch instead") METASTORYMODULE_API FMetaStoryPropertyCopyBatch
{
	/** Expected target struct */
	FMetaStoryBindableStructDesc TargetStruct;

	/** Index to first binding/copy. */
	FMetaStoryIndex16 BindingsBegin;

	/** Index to one past the last binding/copy. */
	FMetaStoryIndex16 BindingsEnd;

	/** Index to first property function. */
	FMetaStoryIndex16 PropertyFunctionsBegin;

	/** Index to one past the last property function. */
	FMetaStoryIndex16 PropertyFunctionsEnd;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

/**
 * Describes access to referenced property.
 */
USTRUCT()
struct FMetaStoryPropertyAccess
{
	GENERATED_BODY()

	/** Source property access. */
	UPROPERTY()
	FPropertyBindingPropertyIndirection SourceIndirection;

	/** Cached pointer to the leaf property of the access. */
	const FProperty* SourceLeafProperty = nullptr;

	/** Type of the source data, used for validation. */
	UPROPERTY(Transient)
	TObjectPtr<const UStruct> SourceStructType = nullptr;

	/** Describes how to get the source data pointer. */
	UPROPERTY()
	FMetaStoryDataHandle SourceDataHandle = FMetaStoryDataHandle::Invalid;
};

/**
 * Runtime storage and execution of property bindings.
 */
USTRUCT()
struct FMetaStoryPropertyBindings : public FPropertyBindingBindingCollection
{
	GENERATED_BODY()

	UE_API FMetaStoryPropertyBindings();

	/**
	 * Clears all bindings.
	 */
	UE_API virtual void OnReset() override;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/**
	 * @return Number of source structs the copy expects.
	 */
	UE_DEPRECATED(5.6, "Use GetNumBindableStructDescriptors instead")
	UE_API int32 GetSourceStructNum() const;

	/**
	 * Copies a property from Source to Target based on the provided Copy.
	 * @param Copy Describes which parameter and how is copied.
	 * @param SourceStructView Pointer and type for the source containing the property to be copied.
	 * @param TargetStructView Pointer and type for the target containing the property to be copied.
	 * @return true if the property was copied successfully.
	 */
	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingCopyInfo instead")
	UE_API bool CopyProperty(const FMetaStoryPropertyCopy& Copy, FMetaStoryDataView SourceStructView, FMetaStoryDataView TargetStructView) const;

	/** @return copy batch at specified index. */
	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingCopyInfo instead")
	UE_API const FMetaStoryPropertyCopyBatch& GetBatch(const FMetaStoryIndex16 TargetBatchIndex) const;

	/** @return All the property copies for a specific batch. */
	UE_DEPRECATED(5.6, "Use the overload return FPropertyBindingCopyInfo and taking FPropertyBindingIndex16 instead")
	UE_API TConstArrayView<FMetaStoryPropertyCopy> GetBatchCopies(const FMetaStoryIndex16 TargetBatchIndex) const;

	/** @return All the property copies for a specific batch. */
	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingCopyInfoBatch instead")
	UE_API TConstArrayView<FMetaStoryPropertyCopy> GetBatchCopies(const FMetaStoryPropertyCopyBatch& Batch) const;
UE_API PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * @return Referenced property access for provided PropertyRef.
	 */
	const FMetaStoryPropertyAccess* GetPropertyAccess(const FMetaStoryPropertyRef& Reference) const;

	/**
	 * Pointer to referenced property 
	 * @param SourceView Data view to referenced property's owner.
	 * @param PropertyAccess Access to the property for which we want to obtain a pointer.
	 * @return Pointer to referenced property if it's type match, nullptr otherwise.
	 */
	template<class T>
	T* GetMutablePropertyPtr(FMetaStoryDataView SourceView, const FMetaStoryPropertyAccess& PropertyAccess) const;

	//~ Begin FPropertyBindingBindingCollection overrides
	UE_API virtual int32 GetNumBindableStructDescriptors() const override;
	UE_API virtual const FPropertyBindingBindableStructDescriptor* GetBindableStructDescriptorFromHandle(FConstStructView InSourceHandleView) const override;
	UE_API virtual void VisitSourceStructDescriptorInternal(TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor& Descriptor)> InFunction) const override;
	[[nodiscard]] UE_API virtual bool ResolveBindingCopyInfo(const FPropertyBindingBinding& InResolvedBinding, const FPropertyBindingPathIndirection& InBindingSourceLeafIndirection, const FPropertyBindingPathIndirection& InBindingTargetLeafIndirection, FPropertyBindingCopyInfo& OutCopyInfo) override;
	//~ End FPropertyBindingBindingCollection overrides
protected:

	UE_API const FPropertyBindingBindableStructDescriptor* GetBindableStructDescriptorFromHandle(FMetaStoryDataHandle InSourceHandle) const;

	//~ Begin FPropertyBindingBindingCollection overrides
	[[nodiscard]] UE_API virtual bool OnResolvingPaths() override;

	UE_API virtual int32 GetNumBindings() const override;
	UE_API virtual void ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const override;
	UE_API virtual void ForEachBinding(const FPropertyBindingIndex16 InBegin, const FPropertyBindingIndex16 InEnd, const TFunctionRef<void(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const override;
	UE_API virtual void ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction) override;
	UE_API virtual void VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const override;
	UE_API virtual void VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction) override;

#if WITH_EDITOR
	UE_API virtual FPropertyBindingBinding* AddBindingInternal(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath) override;
	UE_API virtual void RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate) override;
	UE_API virtual bool HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
	UE_API virtual const FPropertyBindingBinding* FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
#endif // WITH_EDITOR
	//~ End FPropertyBindingBindingCollection overrides

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingIndex16 and FPropertyBindingDataView instead")
	UE_API bool ResetObjects(const FMetaStoryIndex16 TargetBatchIndex, FMetaStoryDataView TargetStructView) const;

	/**
	 * Resolves what kind of copy type to use between specified property indirections.
	 * @param SourceIndirection Property path indirections of the copy source,
	 * @param TargetIndirection Property path indirections of the copy target.
	 * @param OutCopy Resulting copy type.
	 * @return true if copy was resolved, or false if no copy could be resolved between paths.
	 */
	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingCopyInfo instead")
	[[nodiscard]] static UE_API bool ResolveCopyType(const FPropertyBindingPathIndirection& SourceIndirection, const FPropertyBindingPathIndirection& TargetIndirection, FMetaStoryPropertyCopy& OutCopy);

	UE_DEPRECATED(5.6, "Use UE::PropertyBinding::GetPropertyCompatibility instead")
	static UE_API EMetaStoryPropertyAccessCompatibility GetPropertyCompatibility(const FProperty* FromProperty, const FProperty* ToProperty);

	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingPathIndirection instead")
	[[nodiscard]] static UE_API bool ResolveCopyType(const FMetaStoryPropertyPathIndirection& SourceIndirection, const FMetaStoryPropertyPathIndirection& TargetIndirection, FMetaStoryPropertyCopy& OutCopy);

private:
	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPropertyIndirection instead.")
	[[nodiscard]] UE_API bool ResolvePath(const UStruct* Struct, const FPropertyBindingPath& Path, FMetaStoryPropertyIndirection& OutFirstIndirection, FPropertyBindingPathIndirection& OutLeafIndirection);

	UE_DEPRECATED(5.6, "Use GetBindableStructDescriptorFromHandle instead.")
	UE_API const FMetaStoryBindableStructDesc* GetSourceDescByHandle(const FMetaStoryDataHandle SourceDataHandle);

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingCopyInfo instead.")
	UE_API void PerformCopy(const FMetaStoryPropertyCopy& Copy, uint8* SourceAddress, uint8* TargetAddress) const;

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingCopyInfo instead.")
	UE_API void PerformResetObjects(const FMetaStoryPropertyCopy& Copy, uint8* TargetAddress) const;

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBinding types instead.")
	UE_API uint8* GetAddress(FMetaStoryDataView InStructView, const FMetaStoryPropertyIndirection& FirstIndirection, const FProperty* LeafProperty) const;

private:
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Array of expected source structs. */
	UPROPERTY()
	TArray<FMetaStoryBindableStructDesc> SourceStructs;

	/** Array of property bindings, resolved into arrays of copies before use. */
	UPROPERTY()
	TArray<FMetaStoryPropertyPathBinding> PropertyPathBindings;

	/** Array of referenced property paths */
	UPROPERTY()
	TArray<FMetaStoryPropertyRefPath> PropertyReferencePaths;

	/** Array of individually accessed properties */
	UPROPERTY()
	TArray<FMetaStoryPropertyAccess> PropertyAccesses;

	friend FMetaStoryPropertyBindingCompiler;
	friend UMetaStory;
};

template <class T>
T* FMetaStoryPropertyBindings::GetMutablePropertyPtr(FMetaStoryDataView SourceView, const FMetaStoryPropertyAccess& PropertyAccess) const
{
	check(SourceView.GetStruct() == PropertyAccess.SourceStructType);

	if (!UE::MetaStory::PropertyRefHelpers::Validator<std::remove_cv_t<T>>::IsValid(*PropertyAccess.SourceLeafProperty))
	{
		return nullptr;
	}

	return reinterpret_cast<T*>(Super::GetAddress(SourceView, PropertyAccess.SourceIndirection, PropertyAccess.SourceLeafProperty));
}

/**
 * Helper interface to reason about bound properties. The implementation is in the editor plugin.
 */
struct IMetaStoryBindingLookup
{
	virtual ~IMetaStoryBindingLookup() {} 

	/** @return Source path for given target path, or null if binding does not exists. */
	virtual const FPropertyBindingPath* GetPropertyBindingSource(const FPropertyBindingPath& InTargetPath) const = 0;

	/** @return Display name given property path. */
	virtual FText GetPropertyPathDisplayName(const FPropertyBindingPath& InPath, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const = 0;

	/** @return Leaf property based on property path. */
	virtual const FProperty* GetPropertyPathLeafProperty(const FPropertyBindingPath& InPath) const = 0;

	/** @return Display name of binding source, or empty if binding does not exists. */
	virtual FText GetBindingSourceDisplayName(const FPropertyBindingPath& InTargetPath, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const = 0;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	virtual const FMetaStoryPropertyPath* GetPropertyBindingSource(const FMetaStoryPropertyPath& InTargetPath) const final
	{
		return nullptr;
	}

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	virtual FText GetPropertyPathDisplayName(const FMetaStoryPropertyPath& InPath, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const final
	{
		return FText::GetEmpty();
	}

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	virtual FText GetBindingSourceDisplayName(const FMetaStoryPropertyPath& InTargetPath, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const final
	{
		return FText::GetEmpty();
	}

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	virtual const FProperty* GetPropertyPathLeafProperty(const FMetaStoryPropertyPath& InPath) const final
	{
		return nullptr;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
};


namespace UE::MetaStory
{
	/** @return desc and path as a display string. */
	extern METASTORYMODULE_API FString GetDescAndPathAsString(const FMetaStoryBindableStructDesc& Desc, const FPropertyBindingPath& Path);

#if WITH_EDITOR
	/**
	 * Returns property usage based on the Category metadata of given property.
	 * @param Property Handle to property where value is got from.
	 * @return found usage type, or EMetaStoryPropertyUsage::Invalid if not found.
	 */
	METASTORYMODULE_API EMetaStoryPropertyUsage GetUsageFromMetaData(const FProperty* Property);

	/** @return struct's property which is the only one marked as Output. Returns null otherwise. */
	METASTORYMODULE_API const FProperty* GetStructSingleOutputProperty(const UStruct& InStruct);
#endif
} // UE::MetaStory

#undef UE_API
