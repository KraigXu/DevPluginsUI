// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryStatePath.h"

class UMetaStory;
class UObject;

#if WITH_METASTORY_DEBUG

namespace UE::MetaStory::Debug
{
/**
 * For debugging purposes. Data used for runtime check.
*/
class FRuntimeValidationInstanceData
{
public:
	FRuntimeValidationInstanceData() = default;
	~FRuntimeValidationInstanceData();

	void SetContext(const UObject* InOwner, const UMetaStory* InMetaStory, bool bInInstanceDataWriteAccessAcquired);

	void NodeEnterState(FGuid NodeID, FActiveFrameID FrameID);
	void NodeExitState(FGuid NodeID, FActiveFrameID FrameID);

private:
	void ValidateTreeNodes(const UMetaStory* NewMetaStory) const;
	void ValidateInstanceData(const UMetaStory* NewMetaStory);

private:
	enum class EState : uint8
	{
		None = 0x00,
		BetweenEnterExitState = 0x01,
	};
	FRIEND_ENUM_CLASS_FLAGS(EState);

	struct FNodeStatePair
	{
		FGuid NodeID;
		FActiveFrameID FrameID;
		EState State = EState::None;
	};
	TArray<FNodeStatePair> NodeStates;
	TWeakObjectPtr<const UMetaStory> MetaStory;
	TWeakObjectPtr<const UObject> Owner;

	/** an ExecContext with write access to the Instance Storage data this is debugging on has been created */
	bool bInstanceDataWriteAccessAcquired = false;
};

ENUM_CLASS_FLAGS(FRuntimeValidationInstanceData::EState);

} // UE::MetaStory::Debug

#endif // WITH_METASTORY_DEBUG