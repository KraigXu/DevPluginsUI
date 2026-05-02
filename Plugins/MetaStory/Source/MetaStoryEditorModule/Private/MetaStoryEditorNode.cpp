// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorNode.h"

#include "Blueprint/MetaStoryConditionBlueprintBase.h"
#include "Blueprint/MetaStoryEvaluatorBlueprintBase.h"
#include "Blueprint/MetaStoryTaskBlueprintBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEditorNode)

FName FMetaStoryEditorNode::GetName() const
{
	const UScriptStruct* NodeType = Node.GetScriptStruct();
	if (!NodeType)
	{
		return FName();
	}

	if (const FMetaStoryNodeBase* NodePtr = Node.GetPtr<FMetaStoryNodeBase>())
	{
		if (NodePtr->Name.IsNone())
		{
			if (InstanceObject &&
					(NodeType->IsChildOf(TBaseStructure<FMetaStoryBlueprintTaskWrapper>::Get())
					|| NodeType->IsChildOf(TBaseStructure<FMetaStoryBlueprintEvaluatorWrapper>::Get())
					|| NodeType->IsChildOf(TBaseStructure<FMetaStoryBlueprintConditionWrapper>::Get())))
			{
				return FName(InstanceObject->GetClass()->GetDisplayNameText().ToString());
			}
			return FName(NodeType->GetDisplayNameText().ToString());
		}
		return NodePtr->Name;
	}
	
	return FName();
}
