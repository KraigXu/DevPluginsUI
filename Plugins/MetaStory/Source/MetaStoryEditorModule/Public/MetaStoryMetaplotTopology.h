// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMetaStoryEditorData;
struct FMetaStoryCompilerLog;

namespace UE::MetaStory::MetaplotTopology
{
	/**
	 * Rebuilds UMetaStoryEditorData::SubTrees from UMetaplotFlow for compilation.
	 * Metaplot is the source of truth; shadow states exist so the existing MetaStory compiler can run unchanged.
	 *
	 * Current constraints (v1):
	 * - At most one incoming transition per node (no merge/join); arborescence from Start.
	 * - FMetaplotCondition entries are not yet lowered to MetaStory condition nodes (warnings only).
	 * - Tasks on Metaplot nodes are not copied (UMetaplotStoryTask vs FMetaStoryEditorNode); add bridges later.
	 */
	METASTORYEDITORMODULE_API bool RebuildShadowStates(UMetaStoryEditorData& EditorData, FMetaStoryCompilerLog* Log);
}
