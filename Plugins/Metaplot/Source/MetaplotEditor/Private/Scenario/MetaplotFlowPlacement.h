#pragma once

#include "CoreMinimal.h"

class UMetaplotFlow;

namespace MetaplotFlowPlacement
{
	/** 将节点移动到 (NewStage, NewLayer) 后，现有连线是否仍满足 Stage/Layer 规则，且不与其它节点占格冲突。 */
	bool IsValidCellForNodeMove(const UMetaplotFlow* Flow, const FGuid& MovingNodeId, int32 NewStage, int32 NewLayer);
}
