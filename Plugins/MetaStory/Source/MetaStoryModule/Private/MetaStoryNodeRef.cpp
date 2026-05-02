// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryNodeRef.h"
#include "MetaStory.h"
#include "MetaStoryExecutionContext.h"
#include "MetaStoryInstanceData.h"
#include "MetaStoryTaskBase.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if WITH_METASTORY_DEBUG
FMetaStoryStrongTaskRef::FMetaStoryStrongTaskRef(TStrongObjectPtr<const UMetaStory> InMetaStory, const FMetaStoryTaskBase* InTask, FMetaStoryIndex16 InNodeIndex, FGuid InNodeId)
	: MetaStory(InMetaStory)
	, Task(InTask)
	, NodeIndex(InNodeIndex)
	, NodeId(InNodeId)
{
}
#else
FMetaStoryStrongTaskRef::FMetaStoryStrongTaskRef(TStrongObjectPtr<const UMetaStory> InMetaStory, const FMetaStoryTaskBase* InTask, FMetaStoryIndex16 InNodeIndex)
	: MetaStory(InMetaStory)
	, Task(InTask)
	, NodeIndex(InNodeIndex)
	{
	}
#endif

const UMetaStory* FMetaStoryStrongTaskRef::GetMetaStory() const
{
	return IsValid() ? MetaStory.Get() : nullptr;
}
	
const FMetaStoryTaskBase* FMetaStoryStrongTaskRef::GetTask() const
{
	return IsValid() ? Task : nullptr;
}

bool FMetaStoryStrongTaskRef::IsValid() const
{
	bool bIsValid = Task != nullptr && MetaStory.Get() != nullptr;
#if WITH_METASTORY_DEBUG
	if (bIsValid)
	{
		bIsValid = NodeId == MetaStory->GetNodeIdFromIndex(NodeIndex);
		ensureMsgf(bIsValid, TEXT("The node id changed from the last use. Did the MetaStory asset recompiled?"));
	}
#endif
	return bIsValid;
}

FMetaStoryWeakTaskRef::FMetaStoryWeakTaskRef(TNotNull<const UMetaStory*> InMetaStory, FMetaStoryIndex16 InTaskIndex)
	: MetaStory(InMetaStory)
	, NodeIndex(InTaskIndex)
{
#if WITH_METASTORY_DEBUG
	NodeId = InMetaStory->GetNodeIdFromIndex(NodeIndex);
#endif
}

FMetaStoryStrongTaskRef FMetaStoryWeakTaskRef::Pin() const
{
	TStrongObjectPtr<const UMetaStory> MetaStoryPinned = MetaStory.Pin();
	const FMetaStoryTaskBase* Task = nullptr;
	if (MetaStoryPinned && MetaStoryPinned->GetNodes().IsValidIndex(NodeIndex.AsInt32()))
	{
#if WITH_METASTORY_DEBUG
		ensureMsgf(NodeId == MetaStoryPinned->GetNodeIdFromIndex(NodeIndex), TEXT("The node id changed from the last use. Did the MetaStory asset recompiled?"));
#endif
		Task = MetaStoryPinned->GetNodes()[NodeIndex.AsInt32()].GetPtr<const FMetaStoryTaskBase>();
	}

#if WITH_METASTORY_DEBUG
	return Task ? FMetaStoryStrongTaskRef(MetaStoryPinned, Task, NodeIndex, NodeId) : FMetaStoryStrongTaskRef();
#else
	return Task ? FMetaStoryStrongTaskRef(MetaStoryPinned, Task, NodeIndex) : FMetaStoryStrongTaskRef();
#endif
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS