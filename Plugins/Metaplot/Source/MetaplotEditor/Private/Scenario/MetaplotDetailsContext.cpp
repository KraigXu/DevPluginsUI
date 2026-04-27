#include "Scenario/MetaplotDetailsContext.h"

namespace
{
	TWeakObjectPtr<UMetaplotDetailsContext> GActiveMetaplotDetailsContext;
}

void UMetaplotDetailsContext::Initialize(UMetaplotFlow* InEditingFlowAsset, const FGuid& InSelectedNodeId)
{
	EditingFlowAsset = InEditingFlowAsset;
	SelectedNodeId = InSelectedNodeId;
	GActiveMetaplotDetailsContext = this;
}

UMetaplotDetailsContext* UMetaplotDetailsContext::GetActiveContext()
{
	return GActiveMetaplotDetailsContext.Get();
}
