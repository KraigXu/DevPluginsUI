#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Flow/MetaplotFlow.h"
#include "MetaplotDetailsProxy.generated.h"

class UMetaplotFlow;

UCLASS(Transient)
class METAPLOTEDITOR_API UMetaplotNodeDetailsProxy : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UMetaplotFlow* InFlowAsset, const FGuid& InNodeId);

	UPROPERTY(VisibleAnywhere, Category = "Metaplot|Node")
	FGuid NodeId;

	UPROPERTY(EditAnywhere, Category = "Metaplot|Node")
	FText NodeName;

	UPROPERTY(EditAnywhere, Category = "Metaplot|Node", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, Category = "Metaplot|Node")
	EMetaplotNodeType NodeType = EMetaplotNodeType::Normal;

	UPROPERTY(EditAnywhere, Category = "Metaplot|Node", meta = (ClampMin = "0"))
	int32 StageIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Metaplot|Node", meta = (ClampMin = "0"))
	int32 LayerIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Metaplot|Node")
	EMetaplotNodeCompletionPolicy CompletionPolicy = EMetaplotNodeCompletionPolicy::AllTasksFinished;

	UPROPERTY(EditAnywhere, Category = "Metaplot|Node")
	EMetaplotNodeResultPolicy ResultPolicy = EMetaplotNodeResultPolicy::AnyFailedIsFailed;

	UPROPERTY(VisibleAnywhere, Category = "Metaplot|Node")
	EMetaplotNodeResult RuntimeResult = EMetaplotNodeResult::None;

	UPROPERTY(EditAnywhere, Category = "Metaplot|Tasks")
	TArray<FMetaplotStoryTaskSpec> StoryTasks;

protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
	FMetaplotNode* FindNodeMutable() const;
	const FMetaplotNode* FindNode() const;
	FMetaplotNodeStoryTasks* FindTaskSetMutable() const;
	const FMetaplotNodeStoryTasks* FindTaskSet() const;
	FMetaplotNodeStoryTasks& FindOrAddTaskSetMutable() const;
	void PullFromFlow();
	void PushToFlow();

	UPROPERTY(Transient)
	TObjectPtr<UMetaplotFlow> FlowAsset;
};

UCLASS(Transient)
class METAPLOTEDITOR_API UMetaplotTransitionDetailsProxy : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UMetaplotFlow* InFlowAsset, const FGuid& InSourceNodeId, const FGuid& InTargetNodeId);
	bool ResolveBlackboardType(const FName& KeyName, EMetaplotBlackboardType& OutType) const;

	UPROPERTY(VisibleAnywhere, Category = "Metaplot|Transition")
	FGuid SourceNodeId;

	UPROPERTY(VisibleAnywhere, Category = "Metaplot|Transition")
	FGuid TargetNodeId;

	UPROPERTY(EditAnywhere, Category = "Metaplot|Transition")
	TArray<FMetaplotCondition> Conditions;

protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
	FMetaplotTransition* FindTransitionMutable() const;
	const FMetaplotTransition* FindTransition() const;
	void PullFromFlow();
	void PushToFlow();

	UPROPERTY(Transient)
	TObjectPtr<UMetaplotFlow> FlowAsset;
};
