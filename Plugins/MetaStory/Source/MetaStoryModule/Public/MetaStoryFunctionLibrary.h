// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetaStoryFunctionLibrary.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryReference;
class UMetaStory;

/**
 * Blueprint helpers for MetaStory references and related utilities.
 */
UCLASS(MinimalAPI, Abstract)
class UMetaStoryFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MetaStory|Reference")
	static UE_API void SetMetaStory(UPARAM(ref)FMetaStoryReference& Reference, UMetaStory* MetaStory);

	UFUNCTION(BlueprintCallable, DisplayName = "Make MetaStory Reference", Category = "MetaStory|Reference", meta = (Keywords = "construct build", NativeMakeFunc, BlueprintInternalUseOnly = "true"))
	static UE_API FMetaStoryReference MakeMetaStoryReference(UMetaStory* MetaStory);

	UFUNCTION(BlueprintCallable, CustomThunk, DisplayName = "Set Parameter Property", Category = "MetaStory|Reference", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "NewValue"))
	static UE_API void K2_SetParametersProperty(UPARAM(ref)FMetaStoryReference& Reference, FGuid PropertyID, UPARAM(ref) const int32& NewValue);

	UFUNCTION(BlueprintCallable, CustomThunk, DisplayName = "Get Parameter Property", Category = "MetaStory|Reference", meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "ReturnValue"))
	static UE_API void K2_GetParametersProperty(UPARAM(ref)const FMetaStoryReference& Reference, FGuid PropertyID, int32& ReturnValue);

private:
	DECLARE_FUNCTION(execK2_SetParametersProperty);
	DECLARE_FUNCTION(execK2_GetParametersProperty);
};

#undef UE_API
