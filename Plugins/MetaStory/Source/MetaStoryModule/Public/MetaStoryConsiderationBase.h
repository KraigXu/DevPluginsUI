// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryNodeBase.h"
#include "MetaStoryConsiderationBase.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;
enum class EMetaStoryExpressionOperand : uint8;

/**
 * This feature is experimental and the API is expected to change. 
 * Base struct for all utility considerations.
 */
USTRUCT(meta = (Hidden))
struct FMetaStoryConsiderationBase : public FMetaStoryNodeBase
{
	GENERATED_BODY()

	UE_API FMetaStoryConsiderationBase();

public:
	UE_API float GetNormalizedScore(FMetaStoryExecutionContext& Context) const;

protected:
	virtual float GetScore(FMetaStoryExecutionContext& Context) const { return 0.f; };

public:
	UPROPERTY()
	EMetaStoryExpressionOperand Operand;

	UPROPERTY()
	int8 DeltaIndent;
};

/**
 * Base class (namespace) for all common Utility Considerations that are generally applicable.
 * This allows schemas to safely include all considerations child of this struct.
 */
USTRUCT(meta = (Hidden))
struct FMetaStoryConsiderationCommonBase : public FMetaStoryConsiderationBase
{
	GENERATED_BODY()
};

#undef UE_API
