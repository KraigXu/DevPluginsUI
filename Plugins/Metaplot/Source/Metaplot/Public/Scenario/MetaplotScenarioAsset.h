#pragma once

#include "CoreMinimal.h"
#include "Flow/MetaplotFlow.h"
#include "MetaplotScenarioAsset.generated.h"

// Backward compatibility shim: keep legacy asset class name while internally
// aligning to the new UMetaplotFlow data model.
UCLASS(BlueprintType)
class METAPLOT_API UMetaplotScenarioAsset : public UMetaplotFlow
{
	GENERATED_BODY()
};
