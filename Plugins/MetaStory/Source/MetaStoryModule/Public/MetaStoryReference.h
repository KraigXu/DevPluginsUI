// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/PropertyBag.h"
#include "GameplayTagContainer.h"
#include "MetaStoryReference.generated.h"

#define UE_API METASTORYMODULE_API

class UMetaStory;

/**
 * Struct to hold reference to a MetaStory asset along with values to parameterized it.
 */
USTRUCT(BlueprintType, meta = (DisableSplitPin, HasNativeMake = "/Script/MetaStoryModule.MetaStoryFunctionLibrary.MakeMetaStoryReference"))
struct FMetaStoryReference
{
	GENERATED_BODY()

	/** @return true if the reference is set. */
	bool IsValid() const
	{
		return MetaStory != nullptr;
	}
	
	/** Sets the MetaStory asset and referenced parameters. */
	void SetMetaStory(UMetaStory* NewMetaStory)
	{
		MetaStory = NewMetaStory;
		SyncParameters();
	}

	/** @return const pointer to the referenced MetaStory asset. */
	const UMetaStory* GetMetaStory() const
	{
		return MetaStory;
	}

	/** @return pointer to the referenced MetaStory asset. */
	UMetaStory* GetMutableMetaStory()
	{
		return MetaStory;
	}

	/** @return reference to the parameters for the referenced MetaStory asset. */
	const FInstancedPropertyBag& GetParameters() const
	{
		ConditionallySyncParameters();
		return Parameters;
	}

	/** @return reference to the parameters for the referenced MetaStory asset. */
	FInstancedPropertyBag& GetMutableParameters()
	{
		ConditionallySyncParameters();
		return Parameters;
	}

	/**
	 * Enforce self parameters to be compatible with those exposed by the selected MetaStory asset.
	 */
	UE_API void SyncParameters();

	/**
	 * Indicates if current parameters are compatible with those available in the selected MetaStory asset.
	 * @return true when parameters requires to be synced to be compatible with those available in the selected MetaStory asset, false otherwise.
	 */
	UE_API bool RequiresParametersSync() const;

	/** Sync parameters to match the asset if required. */
	UE_API void ConditionallySyncParameters() const;

	/** @return true if the property of specified ID is overridden. */
	bool IsPropertyOverridden(const FGuid PropertyID) const
	{
		return PropertyOverrides.Contains(PropertyID);
	}

	/** Sets the override status of specified property by ID. */
	UE_API void SetPropertyOverridden(const FGuid PropertyID, const bool bIsOverridden);

	UE_API bool Serialize(FStructuredArchive::FSlot Slot);
	UE_API void PostSerialize(const FArchive& Ar);

protected:
	UPROPERTY(EditAnywhere, Category = "MetaStory")
	TObjectPtr<UMetaStory> MetaStory = nullptr;

	UPROPERTY(EditAnywhere, Category = "MetaStory", meta = (FixedLayout))
	FInstancedPropertyBag Parameters;

	/** Array of overridden properties. Non-overridden properties will inherit the values from the MetaStory default parameters. */
	UPROPERTY(EditAnywhere, Category = "MetaStory")
	TArray<FGuid> PropertyOverrides;

	friend class FMetaStoryReferenceDetails;
};

template<>
struct TStructOpsTypeTraits<FMetaStoryReference> : public TStructOpsTypeTraitsBase2<FMetaStoryReference>
{
	enum
	{
		WithStructuredSerializer = true,
		WithPostSerialize = true,
	};
};


/**
 * Item describing a MetaStory override for a state with a specific tag.
 */
USTRUCT()
struct FMetaStoryReferenceOverrideItem
{
	GENERATED_BODY()

	FMetaStoryReferenceOverrideItem() = default;
	FMetaStoryReferenceOverrideItem(const FGameplayTag InStateTag, const FMetaStoryReference& InMetaStoryReference)
		: StateTag(InStateTag)
		, MetaStoryReference(InMetaStoryReference)
	{
	}
	FMetaStoryReferenceOverrideItem(const FGameplayTag InStateTag, FMetaStoryReference&& InMetaStoryReference)
		: StateTag(InStateTag)
		, MetaStoryReference(MoveTemp(InMetaStoryReference))
	{
	}
	
	FGameplayTag GetStateTag() const
	{
		return StateTag;
	}

	const FMetaStoryReference& GetMetaStoryReference() const
	{
		return MetaStoryReference;
	}

	friend uint32 GetTypeHash(const FMetaStoryReferenceOverrideItem& Item)
	{
		return HashCombine(GetTypeHash(Item.StateTag), GetTypeHash(Item.MetaStoryReference.GetMetaStory()));
	}
	
private:
	/** Exact tag used to match against a tag on a linked MetaStory state. */
	UPROPERTY(EditAnywhere, Category = "MetaStory")
	FGameplayTag StateTag;

	/** MetaStory and parameters to replace the linked state asset with. */
	UPROPERTY(EditAnywhere, Category = "MetaStory", meta=(SchemaCanBeOverriden))
	FMetaStoryReference MetaStoryReference;
	
	friend class FMetaStoryReferenceOverridesDetails;
};

/**
 * Overrides for linked MetaStorys. This table is used to override MetaStory references on linked states.
 * If a linked state's tag is exact match of the tag specified on the table, the reference from the table is used instead.
 */
USTRUCT()
struct FMetaStoryReferenceOverrides
{
	GENERATED_BODY()

	/** Removes all overrides. */
	void Reset()
	{
		OverrideItems.Empty();
	}

	/** Adds or replaces override for a selected tag. */
	void AddOverride(const FGameplayTag StateTag, const FMetaStoryReference& MetaStoryReference)
	{
		FMetaStoryReferenceOverrideItem* FoundOverride = OverrideItems.FindByPredicate([StateTag](const FMetaStoryReferenceOverrideItem& Override)
		{
			return Override.GetStateTag() == StateTag;
		});

		if (FoundOverride)
		{
			*FoundOverride = FMetaStoryReferenceOverrideItem(StateTag, MetaStoryReference);
		}
		else
		{
			OverrideItems.Emplace(StateTag, MetaStoryReference);
		}
	}

	/** Adds or replaces override for a selected tag. */
	void AddOverride(FMetaStoryReferenceOverrideItem OverrideItem)
	{
		FMetaStoryReferenceOverrideItem* FoundOverride = OverrideItems.FindByPredicate([&OverrideItem](const FMetaStoryReferenceOverrideItem& Override)
			{
				return Override.GetStateTag() == OverrideItem.GetStateTag();
			});

		if (FoundOverride)
		{
			*FoundOverride = MoveTemp(OverrideItem);
		}
		else
		{
			OverrideItems.Emplace(MoveTemp(OverrideItem));
		}
	}

	/** Returns true if removing an override succeeded. */
	bool RemoveOverride(const FGameplayTag StateTag)
	{
		const int32 Index = OverrideItems.IndexOfByPredicate([StateTag](const FMetaStoryReferenceOverrideItem& Override)
		{
			return Override.GetStateTag() == StateTag;
		});

		if (Index != INDEX_NONE)
		{
			OverrideItems.RemoveAtSwap(Index);
			return true;
		}

		return false;
	}

	TConstArrayView<FMetaStoryReferenceOverrideItem> GetOverrideItems() const
	{
		return OverrideItems;
	}

	friend uint32 GetTypeHash(const FMetaStoryReferenceOverrides& Overrides)
	{
		return GetTypeHash(Overrides.OverrideItems);
	}

private:
	UPROPERTY(EditAnywhere, Category = "MetaStory")
	TArray<FMetaStoryReferenceOverrideItem> OverrideItems;

	friend class FMetaStoryReferenceOverridesDetails;
};

#undef UE_API
