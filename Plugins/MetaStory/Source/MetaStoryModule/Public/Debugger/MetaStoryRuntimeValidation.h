// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryTypes.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "Debugger/MetaStoryDebug.h"
#endif

class UMetaStory;
class UObject;

#if WITH_METASTORY_DEBUG
namespace UE::MetaStory::Debug
{
	class FRuntimeValidationInstanceData;
}
#endif

namespace UE::MetaStory::Debug
{

/**
 * For debugging purposes, test the actions that are made on the MetaStory instance.
 */
struct FRuntimeValidation
{
public:
	FRuntimeValidation() = default;

#if WITH_METASTORY_DEBUG
	FRuntimeValidation(TNotNull<FRuntimeValidationInstanceData*> Instance);
	FRuntimeValidationInstanceData* GetInstanceData() const;
#endif

	/** Set the Owner of the data */
	void SetContext(const UObject* Owner, const UMetaStory* MetaStory, bool bInstanceDataWriteAccessAcquired) const;

private:
#if WITH_METASTORY_DEBUG
	FRuntimeValidationInstanceData* RuntimeValidationData = nullptr;
#endif // WITH_METASTORY_DEBUG
};

} // UE::MetaStory::Debug
