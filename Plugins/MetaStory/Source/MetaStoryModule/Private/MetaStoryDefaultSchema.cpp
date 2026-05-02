// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryDefaultSchema.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryConsiderationBase.h"
#include "MetaStoryEvaluatorBase.h"
#include "MetaStoryPropertyFunctionBase.h"
#include "MetaStoryTaskBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryDefaultSchema)

UMetaStoryDefaultSchema::UMetaStoryDefaultSchema() = default;

bool UMetaStoryDefaultSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FMetaStoryConditionBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FMetaStoryEvaluatorBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FMetaStoryTaskBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FMetaStoryConsiderationBase::StaticStruct())
		|| InScriptStruct->IsChildOf(FMetaStoryPropertyFunctionBase::StaticStruct());
}

bool UMetaStoryDefaultSchema::IsClassAllowed(const UClass* InClass) const
{
	return IsChildOfBlueprintBase(InClass);
}

bool UMetaStoryDefaultSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(UObject::StaticClass());
}

bool UMetaStoryDefaultSchema::IsScheduledTickAllowed() const
{
	return true;
}
