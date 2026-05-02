// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryConsiderationBase.h"
#include "MetaStoryNodeBlueprintBase.h"
#include "Templates/SubclassOf.h"
#include "MetaStoryConsiderationBlueprintBase.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryExecutionContext;

/*
 * Base class for Blueprint based Considerations.
 */
UCLASS(MinimalAPI, Abstract, Blueprintable)
class UMetaStoryConsiderationBlueprintBase : public UMetaStoryNodeBlueprintBase
{
	GENERATED_BODY()

public:
	UE_API UMetaStoryConsiderationBlueprintBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "GetScore"))
	UE_API float ReceiveGetScore() const;

protected:
	UE_API virtual float GetScore(FMetaStoryExecutionContext& Context) const;

	friend struct FMetaStoryBlueprintConsiderationWrapper;

	uint8 bHasGetScore : 1;
};

/**
 * Wrapper for Blueprint based Considerations.
 */
USTRUCT()
struct FMetaStoryBlueprintConsiderationWrapper : public FMetaStoryConsiderationBase
{
	GENERATED_BODY()

	//~ Begin FMetaStoryNodeBase Interface
	virtual const UStruct* GetInstanceDataType() const override { return ConsiderationClass; };
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
	UE_API virtual FName GetIconName() const override;
	UE_API virtual FColor GetIconColor() const override;
#endif //WITH_EDITOR
	//~ End FMetaStoryNodeBase Interface

protected:
	//~ Begin FMetaStoryConsiderationBase Interface
	UE_API virtual float GetScore(FMetaStoryExecutionContext& Context) const override;
	//~ End FMetaStoryConsiderationBase Interface

public:
	UPROPERTY()
	TSubclassOf<UMetaStoryConsiderationBlueprintBase> ConsiderationClass;
};

#undef UE_API
