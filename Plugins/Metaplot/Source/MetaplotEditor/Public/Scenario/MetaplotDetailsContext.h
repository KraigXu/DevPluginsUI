#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MetaplotDetailsContext.generated.h"

class UMetaplotFlow;

UCLASS(Transient)
class METAPLOTEDITOR_API UMetaplotDetailsContext : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UMetaplotFlow* InEditingFlowAsset, const FGuid& InSelectedNodeId);
	static UMetaplotDetailsContext* GetActiveContext();

	UPROPERTY(Transient, VisibleAnywhere, Category = "Context")
	TObjectPtr<UMetaplotFlow> EditingFlowAsset = nullptr;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Context")
	FGuid SelectedNodeId;
};
