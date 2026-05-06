// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMetaStoryEditorData;
struct FMetaStoryCompilerLog;

namespace UE::MetaStory::FlowTopology
{
	/**
	 * Rebuilds UMetaStoryEditorData::SubTrees from embedded UMetaStoryFlow for compilation.
	 * The flow graph is the source of truth for topology; shadow states carry per-node editor data (tasks, Color, Tag, conditions, …).
	 * Before SubTrees are cleared, each flow-node shadow state's editor fields are snapshotted and reapplied after rebuild so save/load and PostLoad do not drop user edits.
	 */
	METASTORYEDITORMODULE_API bool RebuildShadowStates(UMetaStoryEditorData& EditorData, FMetaStoryCompilerLog* Log);
}
