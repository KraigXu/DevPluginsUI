// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryTypes.h"

class FArchive;
class UObject;

namespace UE::MetaStory::CustomVersions
{
	/**
	 * MetaStory asset packages historically shared GUIDs with Engine StateTree; MetaStory now registers unique GUIDs.
	 * These helpers combine the current MetaStory GUID with the legacy StateTree-compatible GUID when reading packages/archives.
	 */
	METASTORYMODULE_API int32 GetEffectiveAssetLinkerVersion(const UObject* Object);
	METASTORYMODULE_API int32 GetEffectiveAssetArchiveVersion(const FArchive& Ar);
	METASTORYMODULE_API int32 GetEffectiveInstanceStorageArchiveVersion(const FArchive& Ar);
}
