// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryIndexTypes.h"
#include "MetaStoryTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryIndexTypes)

bool FMetaStoryIndex16::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	if (Tag.Type == NAME_UInt16Property)
	{
		// Support loading from uint16.
		// Note: 0xffff is silently read as invalid value.
		uint16 OldValue = 0;
		Slot << OldValue;

		*this = FMetaStoryIndex16(OldValue);
		return true;
	}
	else if (Tag.GetType().IsStruct(FMetaStoryIndex8::StaticStruct()->GetFName()))
	{
		// Support loading from Index8.
		FMetaStoryIndex8 OldValue;
		FMetaStoryIndex8::StaticStruct()->SerializeItem(Slot, &OldValue, nullptr);

		int32 NewValue = OldValue.AsInt32();
		if (!IsValidIndex(NewValue))
		{
			NewValue = INDEX_NONE;
		}
		
		*this = FMetaStoryIndex16(NewValue);
		
		return true;
	}
	
	return false;
}


bool FMetaStoryIndex8::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	// Support loading from Index16.
	if (Tag.GetType().IsStruct(FMetaStoryIndex16::StaticStruct()->GetFName()))
	{
		FMetaStoryIndex16 OldValue;
		FMetaStoryIndex16::StaticStruct()->SerializeItem(Slot, &OldValue, nullptr);

		int32 NewValue = OldValue.AsInt32();
		if (!IsValidIndex(NewValue))
		{
			NewValue = INDEX_NONE;
		}
		
		*this = FMetaStoryIndex8(NewValue);
		
		return true;
	}
	
	return false;
}
