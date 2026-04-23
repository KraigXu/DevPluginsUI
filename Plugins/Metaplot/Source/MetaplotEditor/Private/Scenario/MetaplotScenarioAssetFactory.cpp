#include "Scenario/MetaplotScenarioAssetFactory.h"

#include "Flow/MetaplotFlow.h"

UMetaplotScenarioAssetFactory::UMetaplotScenarioAssetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMetaplotFlow::StaticClass();
}

UObject* UMetaplotScenarioAssetFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn)
{
	return NewObject<UMetaplotFlow>(InParent, Class, Name, Flags | RF_Transactional);
}
