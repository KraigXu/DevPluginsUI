// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryTaskBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryTaskBase)


#if WITH_GAMEPLAY_DEBUGGER
FString FMetaStoryTaskBase::GetDebugInfo(const FMetaStoryReadOnlyExecutionContext& Context) const
{
	// @todo: this needs to include more info
	TStringBuilder<256> Builder;
	Builder << TEXT('[');
	Builder << Name;
	Builder << TEXT("]\n");
	return Builder.ToString();
}

//Deprecated
void FMetaStoryTaskBase::AppendDebugInfoString(FString& DebugString, const FMetaStoryExecutionContext& Context) const
{
	DebugString += FString::Printf(TEXT("[%s]\n"), *Name.ToString());
}
#endif

