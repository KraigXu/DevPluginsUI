// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryConsiderationBase.h"
#include "MetaStoryTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryConsiderationBase)

FMetaStoryConsiderationBase::FMetaStoryConsiderationBase()
	: Operand(EMetaStoryExpressionOperand::And)
	, DeltaIndent(0)
{
}

float FMetaStoryConsiderationBase::GetNormalizedScore(FMetaStoryExecutionContext& Context) const
{
	return FMath::Clamp(GetScore(Context), 0.f, 1.f);
}
