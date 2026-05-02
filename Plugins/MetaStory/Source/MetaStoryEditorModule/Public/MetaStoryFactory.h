// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "MetaStoryFactory.generated.h"

#define UE_API METASTORYEDITORMODULE_API

class UMetaStorySchema;

/**
 * Factory for UMetaStory
 */

UCLASS(MinimalAPI)
class UMetaStoryFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	//~ UFactory interface
	UE_API virtual bool ConfigureProperties() override;
	UE_API virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ End of UFactory interface

	UE_API void SetSchemaClass(const TObjectPtr<UClass>& InSchemaClass);  

protected:
	
	UPROPERTY(Transient)
	TObjectPtr<UClass> MetaStorySchemaClass;
};

#undef UE_API
