// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryAsyncExecutionContext.h"
#include "MetaStoryExecutionContext.h"
#include "MetaStoryIndexTypes.h"
#include "MetaStoryInstanceData.h"
#include "MetaStoryPropertyRefHelpers.h"
#include "Templates/Tuple.h"
#include "MetaStoryPropertyRef.generated.h"

struct FMetaStoryPropertyRef;

namespace UE::MetaStory::PropertyRefHelpers
{
	/**
	 * @param PropertyRef Property's reference to get pointer to.
	 * @param InstanceDataStorage Instance Data Storage.
	 * @param ExecutionFrame Execution frame owning referenced property.
	 * @param ParentExecutionFrame Parent of execution frame owning referenced property.
	 * @param OutSourceProperty On success, returns referenced property.
	 * @return Pointer to referenced property value if succeeded.
	 */
	template <class T>
	static T* GetMutablePtrToProperty(const FMetaStoryPropertyRef& PropertyRef, FMetaStoryInstanceStorage& InstanceDataStorage, const FMetaStoryExecutionFrame& ExecutionFrame, const FMetaStoryExecutionFrame* ParentExecutionFrame, const FProperty** OutSourceProperty = nullptr)
	{
		const FMetaStoryPropertyBindings& PropertyBindings = ExecutionFrame.MetaStory->GetPropertyBindings();
		if (const FMetaStoryPropertyAccess* PropertyAccess = PropertyBindings.GetPropertyAccess(PropertyRef))
		{
			const FMetaStoryDataView SourceView = InstanceData::GetDataViewOrTemporary(InstanceDataStorage, nullptr, ParentExecutionFrame, ExecutionFrame, PropertyAccess->SourceDataHandle);
			
			// The only possibility when PropertyRef references another PropertyRef is when source one is a global or subtree parameter, i.e lives in parent execution frame.
			// If that's the case, referenced PropertyRef is obtained and we recursively take the address where it points to.
			if (IsPropertyRef(*PropertyAccess->SourceLeafProperty))
			{
				check(PropertyAccess->SourceDataHandle.GetSource() == EMetaStoryDataSourceType::GlobalParameterData ||
					PropertyAccess->SourceDataHandle.GetSource() == EMetaStoryDataSourceType::ExternalGlobalParameterData ||
					PropertyAccess->SourceDataHandle.GetSource() == EMetaStoryDataSourceType::SubtreeParameterData);

				if (ParentExecutionFrame == nullptr)
				{
					return nullptr;
				}

				const FMetaStoryPropertyRef* ReferencedPropertyRef = PropertyBindings.GetMutablePropertyPtr<FMetaStoryPropertyRef>(SourceView, *PropertyAccess);
				if (ReferencedPropertyRef == nullptr)
				{
					return nullptr;
				}

				const FMetaStoryExecutionFrame* ParentFrame = nullptr;		
				TConstArrayView<FMetaStoryExecutionFrame> ActiveFrames = InstanceDataStorage.GetExecutionState().ActiveFrames;
				const FMetaStoryExecutionFrame* Frame = FMetaStoryExecutionContext::FindFrame(ParentExecutionFrame->MetaStory, ParentExecutionFrame->RootState, ActiveFrames, ParentFrame);
				
				if (Frame == nullptr)
				{
					return nullptr;
				}

				return GetMutablePtrToProperty<T>(*ReferencedPropertyRef, InstanceDataStorage, *Frame, ParentFrame, OutSourceProperty);
			}
			else
			{
				if (OutSourceProperty)
				{
					*OutSourceProperty = PropertyAccess->SourceLeafProperty;
				}

				return PropertyBindings.GetMutablePropertyPtr<T>(SourceView, *PropertyAccess);
			}
		}

		return nullptr;
	}

	/**
	 * @param PropertyRef Property's reference to get pointer to.
	 * @param InstanceDataStorage Instance Data Storage
	 * @param ExecutionFrame Execution frame owning referenced property
	 * @param ParentExecutionFrame Parent of execution frame owning referenced property
	 * @return A tuple of pointer to referenced property if succeeded.
	 */
	template <class... T>
	static TTuple<T*...> GetMutablePtrTupleToProperty(const FMetaStoryPropertyRef& PropertyRef, FMetaStoryInstanceStorage& InstanceDataStorage, const FMetaStoryExecutionFrame& ExecutionFrame, const FMetaStoryExecutionFrame* ParentExecutionFrame)
	{
		const FMetaStoryPropertyBindings& PropertyBindings = ExecutionFrame.MetaStory->GetPropertyBindings();
		if (const FMetaStoryPropertyAccess* PropertyAccess = PropertyBindings.GetPropertyAccess(PropertyRef))
		{
			// Passing empty ContextAndExternalDataViews, as PropertyRef is not allowed to point to context or external data.
			const FMetaStoryDataView SourceView = InstanceData::GetDataViewOrTemporary(InstanceDataStorage, nullptr, ParentExecutionFrame, ExecutionFrame, PropertyAccess->SourceDataHandle);
			return TTuple<T*...>(PropertyBindings.GetMutablePropertyPtr<T>(SourceView, *PropertyAccess)...);
		}

		return TTuple<T*...>{};
	}
} // namespace UE::MetaStory::PropertyRefHelpers

/**
 * Property ref allows to get a pointer to selected property in MetaStory.
 * The expected type of the reference should be set in "RefType" meta specifier.
 *
 * Meta specifiers for the type:
 *  - RefType = "<type>"
 *		- Specifies a comma separated list of type of property to reference
 *		- Supported types are: bool, byte, int32, int64, float, double, Name, String, Text, UObject pointers, and structs
 *		- Structs and Objects must use full path name
 *		- If multiple types are specified, GetMutablePtrTuple can be used to access the correct type
 *  - IsRefToArray
 *		- If specified, the reference is to an TArray<RefType>
 *	- CanRefToArray
 *		- If specified, the reference can bind to a Reftype or TArray<RefType>
 *  - Optional
 *		- If specified, the reference can be left unbound, otherwise the compiler report error if the reference is not bound
 *
 * Example:
 *
 *  // Reference to float
 *	UPROPERTY(EditAnywhere, meta = (RefType = "float"))
 *	FMetaStoryPropertyRef RefToFloat;
 *
 *  // Reference to FTestStructBase
 *	UPROPERTY(EditAnywhere, meta = (RefType = "/Script/ModuleName.TestStructBase"))
 *	FMetaStoryPropertyRef RefToTest;
 *
 *  // Reference to TArray<FTestStructBase>
 *	UPROPERTY(EditAnywhere, meta = (RefType = "/Script/ModuleName.TestStructBase", IsRefToArray))
 *	FMetaStoryPropertyRef RefToArrayOfTests;
 *
 *  // Reference to Vector, TArray<FVector>, AActor*, TArray<AActor*>
 *	UPROPERTY(EditAnywhere, meta = (RefType = "/Script/CoreUObject.Vector, /Script/Engine.Actor", CanRefToArray))
 *	FMetaStoryPropertyRef RefToLocationLikeTypes;
 */
USTRUCT()
struct FMetaStoryPropertyRef
{
	GENERATED_BODY()

	FMetaStoryPropertyRef() = default;

	/** @return pointer to the property if possible, nullptr otherwise. */
	template <class T>
	T* GetMutablePtr(const FMetaStoryExecutionContext& Context) const
	{
		const FMetaStoryExecutionFrame* CurrentlyProcessedFrame = Context.GetCurrentlyProcessedFrame();
		check(CurrentlyProcessedFrame);

		return UE::MetaStory::PropertyRefHelpers::GetMutablePtrToProperty<T>(*this, Context.GetMutableInstanceData()->GetMutableStorage(), *CurrentlyProcessedFrame, Context.GetCurrentlyProcessedParentFrame());
	}

	/** @return pointer to the property if possible, nullptr otherwise. */
	template<class T, bool bWithWriteAccess>
	std::conditional_t<bWithWriteAccess, T*, const T*> GetPtrFromStrongExecutionContext(const TMetaStoryStrongExecutionContext<bWithWriteAccess>& Context)
	{
		UE::MetaStory::Async::FActivePathInfo ActivePath = Context.GetActivePathInfo();
		if (ActivePath.IsValid())
		{
			return UE::MetaStory::PropertyRefHelpers::GetMutablePtrToProperty<T>(*this, *Context.Storage, *ActivePath.Frame, ActivePath.ParentFrame);
		}

		return nullptr;
	}

	/** @return a tuple of pointers of the given types to the property if possible, nullptr otherwise. */
	template <class... T>
	TTuple<T*...> GetMutablePtrTuple(const FMetaStoryExecutionContext& Context) const
	{
		const FMetaStoryExecutionFrame* CurrentlyProcessedFrame = Context.GetCurrentlyProcessedFrame();
		check(CurrentlyProcessedFrame);

		return UE::MetaStory::PropertyRefHelpers::GetMutablePtrTupleToProperty<T...>(*this, Context.GetMutableInstanceData()->GetMutableStorage(), *CurrentlyProcessedFrame, Context.GetCurrentlyProcessedParentFrame());
	}

	/** @return a tuple of pointers of the given types to the property if possible, nullptr otherwise. */
	template <class... T, bool bWithWriteAccess>
	std::conditional_t<bWithWriteAccess, TTuple<T*...>, TTuple<const T*...>> GetPtrTupleFromStrongExecutionContext(const TMetaStoryStrongExecutionContext<bWithWriteAccess>& Context) const
	{
		UE::MetaStory::Async::FActivePathInfo ActivePath = Context.GetActivePathInfo();
		if (ActivePath.IsValid())
		{
			return UE::MetaStory::PropertyRefHelpers::GetMutablePtrTupleToProperty<T...>(*this, *Context.Storage, *ActivePath.Frame, ActivePath.ParentFrame);
		}

		return {};
	}

	/**
	 * Used internally.
	 * @return index to referenced property access
	 */
	FMetaStoryIndex16 GetRefAccessIndex() const
	{
		return RefAccessIndex;
	}

	//~ Begin TStructOpsTypeTraits interface
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
	{
		static const FName MetaStoryStructRefName("MetaStoryStructRef");
		if (Tag.GetType().IsStruct(MetaStoryStructRefName))
		{
			// Serialize the data, but we don't have anything to do with it.
			// StructRef and PropertyRef are used for input and existing bindings will set them if needed.
			FMetaStoryStructRef TempStructRef;
			FMetaStoryStructRef::StaticStruct()->SerializeItem(Slot, &TempStructRef, nullptr);
			return true;
		}

		return false;
	}
	//~ End TStructOpsTypeTraits interface

private:
	UPROPERTY()
	FMetaStoryIndex16 RefAccessIndex;

	friend FMetaStoryPropertyBindingCompiler;
};

template <>
struct TStructOpsTypeTraits<FMetaStoryPropertyRef> : public TStructOpsTypeTraitsBase2<FMetaStoryPropertyRef>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};

/**
 * TMetaStoryTypedPropertyRef is a type-safe FMetaStoryPropertyRef wrapper against a single given type.
 * @note When used as a property, this automatically defines PropertyRef property meta-data.
 *
 * Example:
 *
 *  // Reference to float
 *	UPROPERTY(EditAnywhere)
 *	TMetaStoryTypedPropertyRef<float> RefToFloat;
 *
 *  // Reference to FTestStructBase
 *	UPROPERTY(EditAnywhere)
 *	TMetaStoryTypedPropertyRef<FTestStructBase> RefToTest;
 *
 *  // Reference to TArray<FTestStructBase>
 *	UPROPERTY(EditAnywhere)
 *	TMetaStoryTypedPropertyRef<TArray<FTestStructBase>> RefToArrayOfTests;
 *
 *  // Reference to FTestStructBase or TArray<FTestStructBase>
 *	UPROPERTY(EditAnywhere, meta = (CanRefToArray))
 *	TMetaStoryTypedPropertyRef<FTestStructBase> RefToSingleOrArrayOfTests;
 */
template <class TRef>
struct TMetaStoryTypedPropertyRef
{
	/** @return pointer to the property if possible, nullptr otherwise. */
	TRef* GetMutablePtr(FMetaStoryExecutionContext& Context) const
	{
		return PropertyRef.GetMutablePtr<TRef>(Context);
	}

	/** @return a tuple of pointer to the property of the type or array of type, nullptr otherwise. */
	TTuple<TRef*, TArray<TRef>*> GetMutablePtrTuple(FMetaStoryExecutionContext& Context) const
	{
		return PropertyRef.GetMutablePtrTuple<TRef, TArray<TRef>>(Context);
	}

	/**
	 * Used internally.
	 * @return internal property ref
	 */
	FMetaStoryPropertyRef GetInternalPropertyRef() const
	{
		return PropertyRef;
	}

private:
	FMetaStoryPropertyRef PropertyRef;
};

/**
 * External Handle allows to wrap-up property reference to make it accessible without having an access to MetaStoryExecutionContext. Useful for capturing property reference in callbacks.
 */
struct FMetaStoryPropertyRefExternalHandle
{
	FMetaStoryPropertyRefExternalHandle(FMetaStoryPropertyRef InPropertyRef, FMetaStoryExecutionContext& InContext)
		: WeakInstanceStorage(InContext.GetMutableInstanceData()->GetWeakMutableStorage())
		, WeakMetaStory(InContext.GetCurrentlyProcessedFrame()->MetaStory)
		, RootState(InContext.GetCurrentlyProcessedFrame()->RootState)
		, PropertyRef(InPropertyRef)
	{
	}

	/** @return pointer to the property if possible, nullptr otherwise. */
	template <class TRef>
	TRef* GetMutablePtr() const
	{
		return GetMutablePtrTuple<TRef>().template Get<0>();
	}

	/** @return a tuple of pointers of the given types to the property if possible, nullptr otherwise. */
	template <class... TRef>
	TTuple<TRef*...> GetMutablePtrTuple() const
	{
		if (!WeakInstanceStorage.IsValid())
		{
			return TTuple<TRef*...>{};
		}

		FMetaStoryInstanceStorage& InstanceStorage = *WeakInstanceStorage.Pin();
		TConstArrayView<FMetaStoryExecutionFrame> ActiveFrames = InstanceStorage.GetExecutionState().ActiveFrames;
		const FMetaStoryExecutionFrame* ParentFrame = nullptr;
		const FMetaStoryExecutionFrame* Frame = FMetaStoryExecutionContext::FindFrame(WeakMetaStory.Get(), RootState, ActiveFrames, ParentFrame);

		if (Frame == nullptr)
		{
			return TTuple<TRef*...>{};
		}

		return UE::MetaStory::PropertyRefHelpers::GetMutablePtrTupleToProperty<TRef...>(PropertyRef, InstanceStorage, *Frame, ParentFrame);
	}

protected:
	TWeakPtr<FMetaStoryInstanceStorage> WeakInstanceStorage;
	TWeakObjectPtr<const UMetaStory> WeakMetaStory = nullptr;
	FMetaStoryStateHandle RootState = FMetaStoryStateHandle::Invalid;
	FMetaStoryPropertyRef PropertyRef;
};

/**
 * Single type safe external handle allows to wrap-up property reference to make it accessible without having an access to MetaStoryExecutionContext. Useful for capturing property reference in callbacks.
 */
template <class TRef>
struct TMetaStoryTypedPropertyRefExternalHandle : public FMetaStoryPropertyRefExternalHandle
{
	using FMetaStoryPropertyRefExternalHandle::FMetaStoryPropertyRefExternalHandle;
	TMetaStoryTypedPropertyRefExternalHandle(TMetaStoryTypedPropertyRef<TRef> InPropertyRef, FMetaStoryExecutionContext& InContext)
		: FMetaStoryPropertyRefExternalHandle(InPropertyRef.GetInternalPropertyRef(), InContext)
	{
	}

	/** @return pointer to the property if possible, nullptr otherwise. */
	TRef* GetMutablePtr() const
	{
		return FMetaStoryPropertyRefExternalHandle::GetMutablePtr<TRef>();
	}

	/** @return a tuple of pointer to the property of the type or array of type, nullptr otherwise. */
	TTuple<TRef*, TArray<TRef>*> GetMutablePtrTuple() const
	{
		return FMetaStoryPropertyRefExternalHandle::GetMutablePtrTuple<TRef, TArray<TRef>>();
	}

private:
	using FMetaStoryPropertyRefExternalHandle::GetMutablePtr;
	using FMetaStoryPropertyRefExternalHandle::GetMutablePtrTuple;
};

UENUM()
enum class EMetaStoryPropertyRefType : uint8
{
	None,
	Bool,
	Byte,
	Int32,
	Int64,
	Float,
	Double,
	Name,
	String,
	Text,
	Enum,
	Struct,
	Object,
	SoftObject,
	Class,
	SoftClass,
};

/**
 * FMetaStoryBlueprintPropertyRef is a PropertyRef intended to be used in MetaStory Blueprint nodes like tasks, conditions or evaluators, but also as a MetaStory parameter.
 */
USTRUCT(BlueprintType, DisplayName = "MetaStory Property Ref")
struct FMetaStoryBlueprintPropertyRef : public FMetaStoryPropertyRef
{
	GENERATED_BODY()

	FMetaStoryBlueprintPropertyRef() = default;

	/** Returns PropertyRef's type */
	EMetaStoryPropertyRefType GetRefType() const { return RefType; }

	/** Returns true if referenced property is an array. */
	bool IsRefToArray() const { return bIsRefToArray; }

	/** Returns selected ScriptStruct, Class or Enum. */
	UObject* GetTypeObject() const { return TypeObject; }

	/** Returns true if PropertyRef was marked as optional. */
	bool IsOptional() const { return bIsOptional; }

private:
	/** Specifies the type of property to reference */
	UPROPERTY(EditAnywhere, Category = "InternalType")
	EMetaStoryPropertyRefType RefType = EMetaStoryPropertyRefType::None;

	/** If specified, the reference is to an TArray<RefType> */
	UPROPERTY(EditAnywhere, Category = "InternalType")
	uint8 bIsRefToArray : 1 = false;

	/** If specified, the reference can be left unbound, otherwise the MetaStory compiler reports an error if the reference is not bound. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	uint8 bIsOptional : 1 = false;

	/** Specifies the type of property to reference together with RefType, used for Enums, Structs, Objects and Classes. */
	UPROPERTY(EditAnywhere, Category= "InternalType")
	TObjectPtr<UObject> TypeObject = nullptr;

	friend class FMetaStoryBlueprintPropertyRefDetails;
};