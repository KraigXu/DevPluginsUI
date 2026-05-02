// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryInstanceContainer.h"
#include "MetaStoryInstanceDataHelpers.h"

#include "Experimental/ConcurrentLinearAllocator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryInstanceContainer)

namespace UE::MetaStory::InstanceData
{

	void FMetaStoryInstanceContainer::Init(TNotNull<UObject*> InOwner, const FMetaStoryInstanceContainer& InStructs, FAddArgs Args)
	{
		InstanceStructs.Reset();
		InstanceStructs = InStructs.InstanceStructs;
		Private::PostAppendToInstanceStructContainer(InstanceStructs, InOwner, Args.bDuplicateWrappedObject, 0);
	}

	void FMetaStoryInstanceContainer::Init(TNotNull<UObject*> InOwner, TConstArrayView<FInstancedStruct> InStructs, FAddArgs Args)
	{
		InstanceStructs.Reset();
		Private::AppendToInstanceStructContainer(InstanceStructs, InOwner, InStructs, Args.bDuplicateWrappedObject);
	}

	void FMetaStoryInstanceContainer::Init(TNotNull<UObject*> InOwner, TConstArrayView<FConstStructView> InStructs, FAddArgs Args)
	{
		InstanceStructs.Reset();
		Private::AppendToInstanceStructContainer(InstanceStructs, InOwner, InStructs, Args.bDuplicateWrappedObject);
	}

	int32 FMetaStoryInstanceContainer::Append(TNotNull<UObject*> InOwner, const FMetaStoryInstanceContainer& InStructs, FAddArgs Args)
	{
		TArray<FConstStructView, FNonconcurrentLinearArrayAllocator> ToAppend;
		ToAppend.Reserve(InStructs.InstanceStructs.Num());
		for (const FConstStructView& Struct : InStructs.InstanceStructs)
		{
			ToAppend.Add(Struct);
		}
		return Append(InOwner, MakeConstArrayView(ToAppend), Args);
	}

	int32 FMetaStoryInstanceContainer::Append(TNotNull<UObject*> InOwner, TConstArrayView<FInstancedStruct> InStructs, FAddArgs Args)
	{
		return Private::AppendToInstanceStructContainer(InstanceStructs, InOwner, InStructs, Args.bDuplicateWrappedObject);
	}

	int32 FMetaStoryInstanceContainer::Append(TNotNull<UObject*> InOwner, TConstArrayView<FConstStructView> InStructs, FAddArgs Args)
	{
		return Private::AppendToInstanceStructContainer(InstanceStructs, InOwner, InStructs, Args.bDuplicateWrappedObject);
	}

	bool FMetaStoryInstanceContainer::AreAllInstancesValid() const
	{
		return Private::AreAllInstancesValid(InstanceStructs);
	}

	int32 FMetaStoryInstanceContainer::GetAllocatedMemory() const
	{
		return Private::GetAllocatedMemory(InstanceStructs);
	}

} // namespace UE::MetaStory::InstanceData
