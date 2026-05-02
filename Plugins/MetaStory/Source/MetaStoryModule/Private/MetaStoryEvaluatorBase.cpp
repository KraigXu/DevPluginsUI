// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEvaluatorBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEvaluatorBase)

#if WITH_GAMEPLAY_DEBUGGER
FString FMetaStoryEvaluatorBase::GetDebugInfo(const FMetaStoryReadOnlyExecutionContext& Context) const
{
	TStringBuilder<256> Builder;
	Builder << TEXT('[');
	Builder << Name;
	Builder << TEXT("]\n");
	return Builder.ToString();
}

// Deprecated
void FMetaStoryEvaluatorBase::AppendDebugInfoString(FString& DebugString, const FMetaStoryExecutionContext& Context) const
{
	DebugString += FString::Printf(TEXT("[%s]\n"), *Name.ToString());
}
#endif // WITH_GAMEPLAY_DEBUGGER

