#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "MetaplotScenarioAssetFactory.generated.h"

UCLASS()
class METAPLOTEDITOR_API UMetaplotScenarioAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UMetaplotScenarioAssetFactory();

	virtual UObject* FactoryCreateNew(
		UClass* Class,
		UObject* InParent,
		FName Name,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn) override;
};
