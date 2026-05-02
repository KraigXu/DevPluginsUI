// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Math/Color.h"
#include "Misc/Guid.h"
#include "PropertyBindingPath.h"
#include "MetaStoryDelegate.h"
#include "UObject/Class.h"
#include "MetaStoryEditorTypes.generated.h"

/**
 * Id Struct to uniquely identify an FMetaStoryEditorColor instance.
 * An existing FMetaStoryEditorColor instance can be found via UMetaStoryEditorData::FindColor
 */
USTRUCT()
struct FMetaStoryEditorColorRef
{
	GENERATED_BODY()

	FMetaStoryEditorColorRef() = default;

	explicit FMetaStoryEditorColorRef(const FGuid& ID)
		: ID(ID)
	{
	}

	bool operator==(const FMetaStoryEditorColorRef& Other) const
	{
		return ID == Other.ID;
	}

	friend uint32 GetTypeHash(const FMetaStoryEditorColorRef& ColorRef)
	{
		return GetTypeHash(ColorRef.ID);
	}

	UPROPERTY(EditDefaultsOnly, Category = "Theme")
	FGuid ID;
};

/**
 * Struct describing a Color, its display name and a unique identifier to get an instance via UMetaStoryEditorData::FindColor
 */
USTRUCT()
struct FMetaStoryEditorColor
{
	GENERATED_BODY()

	FMetaStoryEditorColor()
		: ColorRef(FGuid::NewGuid())
	{
	}

	explicit FMetaStoryEditorColor(const FMetaStoryEditorColorRef& ColorRef)
		: ColorRef(ColorRef)
	{
	}

	/**
	 * Export Text Item override where properties marked with meta-data "StructExportTransient" are excluded from the exported string
	 * This is so that copy/pasting MetaStory Color entries don't have the effect of also copying over these properties into a new entry.
	 * Since it works with meta-data, it's editor-only.
	 * Side note: The existing "TextExportTransient" / "DuplicateTransient" specifiers apply for uclass properties only
	 */ 
	METASTORYEDITORMODULE_API bool ExportTextItem(FString& OutValueString, const FMetaStoryEditorColor& DefaultValue, UObject* OwnerObject, int32 PortFlags, UObject* ExportRootScope) const;

	bool operator==(const FMetaStoryEditorColor& Other) const
	{
		return ColorRef == Other.ColorRef;
	}

	friend uint32 GetTypeHash(const FMetaStoryEditorColor& InColor)
	{
		return GetTypeHash(InColor.ColorRef);
	}

	/** ID unique per MetaStory Color Entry. Marked as struct export transient so that copy-pasting this entry does not result in the same repeating ID */
	UPROPERTY(meta=(StructExportTransient))
	FMetaStoryEditorColorRef ColorRef;

	UPROPERTY(EditDefaultsOnly, Category = "Theme")
	FString DisplayName;

	UPROPERTY(EditDefaultsOnly, Category = "Theme", meta=(HideAlphaChannel))
	FLinearColor Color = FLinearColor(0.4f, 0.4f, 0.4f);
};

template<>
struct TStructOpsTypeTraits<FMetaStoryEditorColor> : TStructOpsTypeTraitsBase2<FMetaStoryEditorColor>
{
	enum
	{
		WithExportTextItem = true,
	};
};

/**
 * Struct describing a compiled delegate dispatcher.
 * To help with delta cooking/compilation. We want the same generated ID every time the asset compiles.
 */
USTRUCT()
struct FMetaStoryEditorDelegateDispatcherCompiledBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FPropertyBindingPath DispatcherPath;

	UPROPERTY()
	FMetaStoryDelegateDispatcher ID;
};