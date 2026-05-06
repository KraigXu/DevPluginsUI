// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMetaStoryEditorData;
struct FMetaStoryCompilerLog;

namespace UE::MetaStory::FlowTopology
{
	/**
	 * Rebuilds UMetaStoryEditorData::SubTrees from embedded UMetaStoryFlow for compilation.
	 * The flow graph is the source of truth; shadow states exist so the existing MetaStory compiler can run unchanged.
	 */
	METASTORYEDITORMODULE_API bool RebuildShadowStates(UMetaStoryEditorData& EditorData, FMetaStoryCompilerLog* Log);
}
