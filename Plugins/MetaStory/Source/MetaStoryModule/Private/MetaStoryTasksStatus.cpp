// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryTasksStatus.h"
#include "MetaStory.h"
#include "MetaStoryTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryTasksStatus)

namespace UE::MetaStory::Private
{
	inline uint32* GetInlinedBufferPtr(FMetaStoryTasksCompletionStatus::FMaskType*& Buffer)
	{
		return reinterpret_cast<uint32*>(&Buffer);
	}
}

FMetaStoryTasksCompletionStatus::FMetaStoryTasksCompletionStatus(const FMetaStoryCompactFrame& Frame)
{
	BufferNum = Frame.NumberOfTasksStatusMasks;
	MallocBufferIfNeeded();
}

FMetaStoryTasksCompletionStatus::~FMetaStoryTasksCompletionStatus()
{
	if (!UseInlineBuffer())
	{
		FMemory::Free(Buffer);
	}
	Buffer = nullptr;
	BufferNum = 0;
}

FMetaStoryTasksCompletionStatus::FMetaStoryTasksCompletionStatus(const FMetaStoryTasksCompletionStatus& Other)
	: BufferNum(Other.BufferNum)
{
	CopyBuffer(Other);
}

FMetaStoryTasksCompletionStatus::FMetaStoryTasksCompletionStatus(FMetaStoryTasksCompletionStatus&& Other)
	: Buffer(Other.Buffer)
	, BufferNum(Other.BufferNum)
{
	Other.Buffer = nullptr;
	Other.BufferNum = 0;
}

FMetaStoryTasksCompletionStatus& FMetaStoryTasksCompletionStatus::operator=(const FMetaStoryTasksCompletionStatus& Other)
{
	check(this != &Other);
	this->~FMetaStoryTasksCompletionStatus();
	BufferNum = Other.BufferNum;
	CopyBuffer(Other);
	return *this;
}

FMetaStoryTasksCompletionStatus& FMetaStoryTasksCompletionStatus::operator=(FMetaStoryTasksCompletionStatus&& Other)
{
	check(this != &Other);
	this->~FMetaStoryTasksCompletionStatus();
	BufferNum = Other.BufferNum;
	Buffer = Other.Buffer;
	Other.Buffer = nullptr;
	Other.BufferNum = 0;
	return *this;
}

void FMetaStoryTasksCompletionStatus::MallocBufferIfNeeded()
{
	if (!UseInlineBuffer())
	{
		const int32 NumberOfBytes = sizeof(FMetaStoryTasksCompletionStatus::FMaskType) * BufferNum * 2;
		Buffer = (FMaskType*)FMemory::Malloc(sizeof(FMaskType) * NumberOfBytes);
		FMemory::Memzero(Buffer, NumberOfBytes);
	}
}

void FMetaStoryTasksCompletionStatus::CopyBuffer(const FMetaStoryTasksCompletionStatus& Other)
{
	if (!Other.UseInlineBuffer())
	{
		const int32 NumberOfBytes = sizeof(FMetaStoryTasksCompletionStatus::FMaskType) * BufferNum * 2;
		Buffer = (FMetaStoryTasksCompletionStatus::FMaskType*)FMemory::Malloc(NumberOfBytes);
		FMemory::Memcpy(Buffer, Other.Buffer, NumberOfBytes);
	}
	else
	{
		Buffer = Other.Buffer;
	}
}

template<typename TTasksCompletionStatusType>
TTasksCompletionStatusType FMetaStoryTasksCompletionStatus::GetStatusInternal(FMaskType Mask, uint8 BufferIndex, uint8 BitsOffset, EMetaStoryTaskCompletionType Control)
{
	const bool bIsValid = BufferNum > BufferIndex;
	if (bIsValid && UseInlineBuffer())
	{
		check(BufferIndex == 0);

		return TTasksCompletionStatusType(
			UE::MetaStory::Private::GetInlinedBufferPtr(Buffer),
			UE::MetaStory::Private::GetInlinedBufferPtr(Buffer) + 1,
			Mask,
			BitsOffset,
			Control
		);
	}

	if (!bIsValid)
	{
		check(false);
		// In case of invalid data (and the check continues), we prefer to not set any task completion than writing in random memory.
		// Because the mask is 0, no bit will be tested or set. The state tree will never complete.
		BufferNum = 1;
		return TTasksCompletionStatusType(
			UE::MetaStory::Private::GetInlinedBufferPtr(Buffer),
			UE::MetaStory::Private::GetInlinedBufferPtr(Buffer) + 1,
			0,
			0,
			EMetaStoryTaskCompletionType::Any
		);
	}

	constexpr int32 NumberOfBuffers = 2;
	return TTasksCompletionStatusType(
		Buffer + (BufferIndex * NumberOfBuffers),
		Buffer + (BufferIndex * NumberOfBuffers) + 1,
		Mask,
		BitsOffset,
		Control
	);
}

UE::MetaStory::FTasksCompletionStatus FMetaStoryTasksCompletionStatus::GetStatus(const FMetaStoryCompactState& State)
{
	return GetStatusInternal<UE::MetaStory::FTasksCompletionStatus>(
		State.CompletionTasksMask,
		State.CompletionTasksMaskBufferIndex,
		State.CompletionTasksMaskBitsOffset,
		State.CompletionTasksControl
		);
}

UE::MetaStory::FConstTasksCompletionStatus FMetaStoryTasksCompletionStatus::GetStatus(const FMetaStoryCompactState& State) const
{
	return const_cast<FMetaStoryTasksCompletionStatus*>(this)->GetStatusInternal<UE::MetaStory::FConstTasksCompletionStatus>(
		State.CompletionTasksMask,
		State.CompletionTasksMaskBufferIndex,
		State.CompletionTasksMaskBitsOffset,
		State.CompletionTasksControl
		);
}

UE::MetaStory::FTasksCompletionStatus FMetaStoryTasksCompletionStatus::GetStatus(TNotNull<const UMetaStory*> MetaStory)
{
	constexpr int32 BufferIndex = 0;
	constexpr int32 BitOffset = 0;
	return GetStatusInternal<UE::MetaStory::FTasksCompletionStatus>(
		MetaStory->CompletionGlobalTasksMask,
		BufferIndex,
		BitOffset,
		MetaStory->CompletionGlobalTasksControl
		);
}

UE::MetaStory::FConstTasksCompletionStatus FMetaStoryTasksCompletionStatus::GetStatus(TNotNull<const UMetaStory*> MetaStory) const
{
	constexpr int32 BufferIndex = 0;
	constexpr int32 BitOffset = 0;
	return const_cast<FMetaStoryTasksCompletionStatus*>(this)->GetStatusInternal<UE::MetaStory::FConstTasksCompletionStatus>(
		MetaStory->CompletionGlobalTasksMask,
		BufferIndex,
		BitOffset,
		MetaStory->CompletionGlobalTasksControl)
		;
}

void FMetaStoryTasksCompletionStatus::Push(const FMetaStoryCompactState& State)
{
	check(BufferNum > State.CompletionTasksMaskBufferIndex);
	GetStatus(State).ResetStatus(State.TasksNum);
}

bool FMetaStoryTasksCompletionStatus::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		int8 NewBufferNum = 0;
		Ar << NewBufferNum;
		if (NewBufferNum != BufferNum)
		{
			this->~FMetaStoryTasksCompletionStatus();
			BufferNum = NewBufferNum;
			MallocBufferIfNeeded();
		}

		if (UseInlineBuffer())
		{
			Ar << *(UE::MetaStory::Private::GetInlinedBufferPtr(Buffer));
			Ar << *(UE::MetaStory::Private::GetInlinedBufferPtr(Buffer) + 1);
		}
		else
		{
			for (int32 Index = 0; Index < BufferNum*2; ++Index)
			{
				Ar << Buffer[Index];
			}
		}
	}
	else if (Ar.IsSaving())
	{
		Ar << BufferNum;
		if (UseInlineBuffer())
		{
			Ar << *(UE::MetaStory::Private::GetInlinedBufferPtr(Buffer));
			Ar << *(UE::MetaStory::Private::GetInlinedBufferPtr(Buffer) + 1);
		}
		else
		{
			for (int32 Index = 0; Index < BufferNum*2; ++Index)
			{
				Ar << Buffer[Index];
			}
		}
	}
	return true;
}

bool FMetaStoryTasksCompletionStatus::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;
	return Serialize(Ar);
}