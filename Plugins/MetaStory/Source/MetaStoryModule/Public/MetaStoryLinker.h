// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStory.h"
#include "MetaStorySchema.h"
#include "MetaStoryExecutionTypes.h"
#include "Templates/Casts.h"
#include "MetaStoryLinker.generated.h"

#define UE_API METASTORYMODULE_API

UENUM()
enum class EMetaStoryLinkerStatus : uint8
{
	Succeeded,
	Failed,
};

/**
 * The MetaStory linker is used to resolved references to various MetaStory data at load time.
 * @see TMetaStoryExternalDataHandle<> for example usage.
 */
struct FMetaStoryLinker
{
	UE_API explicit FMetaStoryLinker(TNotNull<const UMetaStory*> InMetaStory);

	UE_DEPRECATED(5.7, "Use the constructor with the MetaStory pointer.")
	explicit FMetaStoryLinker(const UMetaStorySchema* InSchema)
		: Schema(InSchema)
	{}
	
	/** @returns the linking status. */
	EMetaStoryLinkerStatus GetStatus() const
	{
		return Status;
	}

	/** @returns the MetaStory asset. */
	const UMetaStory* GetMetaStory() const
	{
		return MetaStory.Get();
	}
	
	/**
	 * Links reference to an external UObject.
	 * @param Handle Reference to TMetaStoryExternalDataHandle<> with UOBJECT type to link to.
	 */
	template <typename T>
	typename TEnableIf<TIsDerivedFrom<typename T::DataType, UObject>::IsDerived, void>::Type LinkExternalData(T& Handle)
	{
		LinkExternalData(Handle, T::DataType::StaticClass(), T::DataRequirement);
	}

	/**
	 * Links reference to an external UStruct.
	 * @param Handle Reference to TMetaStoryExternalDataHandle<> with USTRUCT type to link to.
	 */
	template <typename T>
	typename TEnableIf<!TIsDerivedFrom<typename T::DataType, UObject>::IsDerived && !TIsIInterface<typename T::DataType>::Value, void>::Type LinkExternalData(T& Handle)
	{
		LinkExternalData(Handle, T::DataType::StaticStruct(), T::DataRequirement);
	}

	/**
	 * Links reference to an external IInterface.
	 * @param Handle Reference to TMetaStoryExternalDataHandle<> with IINTERFACE type to link to.
	 */
	template <typename T>
	typename TEnableIf<TIsIInterface<typename T::DataType>::Value, void>::Type LinkExternalData(T& Handle)
	{
		LinkExternalData(Handle, T::DataType::UClassType::StaticClass(), T::DataRequirement);
	}

	/**
	 * Links reference to an external Object or Struct.
	 * This function should only be used when TMetaStoryExternalDataHandle<> cannot be used, i.e. the Struct is based on some data.
	 * @param Handle Reference to link to.
	 * @param Struct Expected type of the Object or Struct to link to.
	 * @param Requirement Describes if the external data is expected to be required or optional.
	 */
	UE_API void LinkExternalData(FMetaStoryExternalDataHandle& Handle, const UStruct* Struct, const EMetaStoryExternalDataRequirement Requirement);

	/** @return linked external data descriptors. */
	const TArrayView<const FMetaStoryExternalDataDesc> GetExternalDataDescs() const
	{
		return ExternalDataDescs;
	}

protected:
	TStrongObjectPtr<const UMetaStory> MetaStory;
	TStrongObjectPtr<const UMetaStorySchema> Schema;
	EMetaStoryLinkerStatus Status = EMetaStoryLinkerStatus::Succeeded;
	TArray<FMetaStoryExternalDataDesc> ExternalDataDescs;
};

#undef UE_API
