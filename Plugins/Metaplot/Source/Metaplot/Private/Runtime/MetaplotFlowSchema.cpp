// Fill out your copyright notice in the Description page of Project Settings.

#include "Runtime/MetaplotFlowSchema.h"
#include "Runtime/MetaplotStoryTask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaplotFlowSchema)

bool UMetaplotFlowSchema::IsChildOfBlueprintBase(const UClass* InClass)
{
	return InClass->IsChildOf<UMetaplotStoryTask>();
}
