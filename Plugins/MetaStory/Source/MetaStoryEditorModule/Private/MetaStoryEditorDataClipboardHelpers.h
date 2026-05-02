// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryEditorDataClipboardHelpers.generated.h"

namespace UE::MetaStoryEditor
{
struct FMetaStoryClipboardEditorData;
}

struct FPropertyBindingBinding;
struct FMetaStoryTransition;
struct FMetaStoryEditorNode;
struct FMetaStoryPropertyPathBinding;

class UMetaStoryEditorData;

namespace UE::MetaStoryEditor
{
void ExportTextAsClipboardEditorData(const FMetaStoryClipboardEditorData& InClipboardEditorData);

bool ImportTextAsClipboardEditorData(const UScriptStruct* InTargetType, TNotNull<UMetaStoryEditorData*> InTargetTree, TNotNull<UObject*> InOwner,
                                     FMetaStoryClipboardEditorData& OutClipboardEditorData, bool bProcessBuffer = true);

void RemoveInvalidBindings(TNotNull<UMetaStoryEditorData*> InEditorData);

void AddErrorNotification(const FText& InText, float InExpiredDuration = 5.f);

USTRUCT()
struct FMetaStoryClipboardEditorData
{
	GENERATED_BODY()

	void ProcessBuffer(const UScriptStruct* InTargetType, TNotNull<UMetaStoryEditorData*> InEditorData, TNotNull<UObject*> InOwner);

	bool IsValid() const
	{
		return bBufferProcessed;
	}

	void Append(TNotNull<const UMetaStoryEditorData*> InStateTree, TConstArrayView<FMetaStoryEditorNode> InEditorNodes);
	void Append(TNotNull<const UMetaStoryEditorData*> InStateTree, TConstArrayView<FMetaStoryTransition> InTransitions);
	void Append(TNotNull<const UMetaStoryEditorData*> InStateTree, TConstArrayView<const FPropertyBindingBinding*> InBindingPtrs);

	void Reset();

	TArrayView<FMetaStoryEditorNode> GetEditorNodesInBuffer()
	{
		return EditorNodesBuffer;
	}

	TConstArrayView<FMetaStoryEditorNode> GetEditorNodesInBuffer() const
	{
		return EditorNodesBuffer;
	}

	TArrayView<FMetaStoryTransition> GetTransitionsInBuffer()
	{
		return TransitionsBuffer;
	}

	TConstArrayView<FMetaStoryTransition> GetTransitionsInBuffer() const
	{
		return TransitionsBuffer;
	}

	TArrayView<FMetaStoryPropertyPathBinding> GetBindingsInBuffer()
	{
		return BindingsBuffer;
	}

	TConstArrayView<FMetaStoryPropertyPathBinding> GetBindingsInBuffer() const
	{
		return BindingsBuffer;
	}

private:
	void CollectBindingsForEditorNodes(TNotNull<const UMetaStoryEditorData*> InStateTree, TConstArrayView<FMetaStoryEditorNode> InEditorNodes);

	UPROPERTY()
	TArray<FMetaStoryEditorNode> EditorNodesBuffer;

	UPROPERTY()
	TArray<FMetaStoryTransition> TransitionsBuffer;

	UPROPERTY()
	TArray<FMetaStoryPropertyPathBinding> BindingsBuffer;

	bool bBufferProcessed = false;
};
}
