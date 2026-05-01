#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MetaplotFlowEditorData.generated.h"

class UMetaplotFlow;
struct FMetaplotNode;
struct FMetaplotNodeState;
struct FMetaplotTransition;

/**
 * Lightweight editor-session data for Metaplot flow editing.
 * This intentionally stays focused on selection and synchronization helpers.
 */
UCLASS(BlueprintType, Transient)
class METAPLOTEDITOR_API UMetaplotFlowEditorData : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UMetaplotFlow* InFlowAsset);
	void ResetSelection();

	void SetSelectedNode(const FGuid& InNodeId);
	void SetSelectedTransitionIndex(int32 InTransitionIndex);

	UMetaplotFlow* GetFlowAsset() const { return FlowAsset.Get(); }
	const FGuid& GetSelectedNodeId() const { return SelectedNodeId; }
	int32 GetSelectedTransitionIndex() const { return SelectedTransitionIndex; }

	bool IsNodeSelected() const { return SelectedNodeId.IsValid(); }
	bool IsTransitionSelected() const { return SelectedTransitionIndex != INDEX_NONE; }

	const FMetaplotNode* FindSelectedNode() const;
	FMetaplotNode* FindMutableSelectedNode();

	const FMetaplotNodeState* FindSelectedNodeState() const;
	FMetaplotNodeState* FindMutableSelectedNodeState();

	const FMetaplotTransition* FindSelectedTransition() const;
	FMetaplotTransition* FindMutableSelectedTransition();

	// Keep NodeStates aligned with flow nodes.
	void SyncAuxiliaryData();
	bool EnsureSelectedNodeState();

private:
	UPROPERTY(Transient)
	TObjectPtr<UMetaplotFlow> FlowAsset = nullptr;

	UPROPERTY(Transient)
	FGuid SelectedNodeId;

	UPROPERTY(Transient)
	int32 SelectedTransitionIndex = INDEX_NONE;
};
