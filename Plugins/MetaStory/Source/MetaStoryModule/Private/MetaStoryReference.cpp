// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryReference.h"
#include "MetaStory.h"
#include "MetaStoryCustomVersions.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryReference)

void FMetaStoryReference::SyncParameters()
{
	if (MetaStory == nullptr)
	{
		Parameters.Reset();
	}
	else
	{
		// In editor builds, sync with overrides.
		Parameters.MigrateToNewBagInstanceWithOverrides(MetaStory->GetDefaultParameters(), PropertyOverrides);
		
		// Remove overrides that do not exists anymore
		if (!PropertyOverrides.IsEmpty())
		{
			if (const UPropertyBag* Bag = Parameters.GetPropertyBagStruct())
			{
				for (TArray<FGuid>::TIterator It = PropertyOverrides.CreateIterator(); It; ++It)
				{
					if (!Bag->FindPropertyDescByID(*It))
					{
						It.RemoveCurrentSwap();
					}
				}
			}
		}
	}
}

bool FMetaStoryReference::RequiresParametersSync() const
{
	bool bShouldSync = false;
	
	if (MetaStory)
	{
		const FInstancedPropertyBag& DefaultParameters = MetaStory->GetDefaultParameters();
		const UPropertyBag* DefaultParametersBag = DefaultParameters.GetPropertyBagStruct();
		const UPropertyBag* ParametersBag = Parameters.GetPropertyBagStruct();
		
		// Mismatching property bags, needs sync.
		if (DefaultParametersBag != ParametersBag)
		{
			bShouldSync = true;
		}
		else if (ParametersBag && DefaultParametersBag)
		{
			// Check if non-overridden parameters are not identical, needs sync.
			const uint8* SourceAddress = DefaultParameters.GetValue().GetMemory();
			const uint8* TargetAddress = Parameters.GetValue().GetMemory();
			check(SourceAddress);
			check(TargetAddress);

			for (const FPropertyBagPropertyDesc& Desc : ParametersBag->GetPropertyDescs())
			{
				// Skip overridden
				if (PropertyOverrides.Contains(Desc.ID))
				{
					continue;
				}

				const uint8* SourceValueAddress = SourceAddress + Desc.CachedProperty->GetOffset_ForInternal();
				const uint8* TargetValueAddress = TargetAddress + Desc.CachedProperty->GetOffset_ForInternal();
				if (!Desc.CachedProperty->Identical(SourceValueAddress, TargetValueAddress))
				{
					// Mismatching values, should sync.
					bShouldSync = true;
					break;
				}
			}
		}
	}
	else
	{
		// Empty MetaStory should not have parameters
		bShouldSync = Parameters.IsValid();
	}
	
	return bShouldSync;
}

void FMetaStoryReference::ConditionallySyncParameters() const
{
	if (RequiresParametersSync())
	{
		FMetaStoryReference* NonConstThis = const_cast<FMetaStoryReference*>(this);
		NonConstThis->SyncParameters();
		UE_LOG(LogMetaStory, Warning, TEXT("Parameters for '%s' stored in MetaStoryReference were auto-fixed to be usable at runtime."), *GetNameSafe(MetaStory));	
	}
}

void FMetaStoryReference::SetPropertyOverridden(const FGuid PropertyID, const bool bIsOverridden)
{
	if (bIsOverridden)
	{
		PropertyOverrides.AddUnique(PropertyID);
	}
	else
	{
		PropertyOverrides.Remove(PropertyID);
		ConditionallySyncParameters();
	}
}

bool FMetaStoryReference::Serialize(FStructuredArchive::FSlot Slot)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const FGuid MetaStoryCustomVersion = FMetaStoryCustomVersion::GUID;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	Slot.GetUnderlyingArchive().UsingCustomVersion(MetaStoryCustomVersion);
	return false; // Let the default serializer handle serializing.
}

void FMetaStoryReference::PostSerialize(const FArchive& Ar)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const int32 CurrentMetaStoryCustomVersion = UE::MetaStory::CustomVersions::GetEffectiveAssetArchiveVersion(Ar);
	constexpr int32 OverridableParametersVersion = FMetaStoryCustomVersion::OverridableParameters;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (CurrentMetaStoryCustomVersion < OverridableParametersVersion)
	{
		// In earlier versions, all parameters were overwritten.
		if (const UPropertyBag* Bag = Parameters.GetPropertyBagStruct())
		{
			for (const FPropertyBagPropertyDesc& Desc : Bag->GetPropertyDescs())
			{
				PropertyOverrides.Add(Desc.ID);
			}
		}
	}
}
