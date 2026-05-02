// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "Templates/SubclassOf.h"

class UMetaStorySchema;

#include "IMetaStorySchemaProvider.generated.h"

UINTERFACE(MinimalAPI)
class UMetaStorySchemaProvider : public UInterface
{
	GENERATED_BODY()
};

/**
* Implementing this interface allows derived class to override the schema used to filter valid state trees for a FMetaStoryReference.
* The state tree reference property needs to be marked with SchemaCanBeOverriden metatag.
* Ex:
*	UPROPERTY(EditAnywhere, Category = AI, meta=(Schema="/Script/GameplayStateTreeModule.MetaStoryComponentSchema", SchemaCanBeOverriden))
*	FMetaStoryReference MetaStoryRef;
*/
class IMetaStorySchemaProvider
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UMetaStorySchema> GetSchema() const PURE_VIRTUAL(IMetaStorySchemaProvider::GetSchema(), return nullptr;)
};