// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryAsyncExecutionContext.h"
#include "MetaStoryInstanceData.h"
#include "MetaStoryReference.h"
#include "MetaStoryTaskBase.h"
#include "MetaStoryRunParallelMetaStoryTask.generated.h"

#define UE_API METASTORYMODULE_API

USTRUCT()
struct FMetaStoryRunParallelStateTreeTaskInstanceData
{
	GENERATED_BODY()

	/** State tree and parameters that will be run when this task is started. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta=(SchemaCanBeOverriden))
	FMetaStoryReference MetaStory;

	UPROPERTY(Transient)
	FMetaStoryInstanceData TreeInstanceData;

	UPROPERTY(Transient)
	TObjectPtr<const UMetaStory> RunningStateTree = nullptr;

	/** The handle of the scheduled tick. */
	UE::MetaStory::FScheduledTickHandle ScheduledTickHandle;
};

USTRUCT()
struct FMetaStoryRunParallelStateTreeExecutionExtension : public FMetaStoryExecutionExtension
{
	GENERATED_BODY()

public:
	virtual void ScheduleNextTick(const FContextParameters& Context, const FNextTickArguments& Args) override;

	FMetaStoryWeakExecutionContext WeakExecutionContext;
	UE::MetaStory::FScheduledTickHandle ScheduledTickHandle;
};

/**
* Task that will run another state tree in the current state while allowing the current tree to continue selection and process of child state.
* It will succeed, fail or run depending on the result of the parallel tree.
* Less efficient then Linked Asset state, it has the advantage of allowing multiple trees to run in parallel.
*/
USTRUCT(meta = (DisplayName = "Run Parallel Tree", Category = "Common"))
struct FMetaStoryRunParallelStateTreeTask : public FMetaStoryTaskCommonBase
{
	GENERATED_BODY()
	using FInstanceDataType = FMetaStoryRunParallelStateTreeTaskInstanceData;
	
	UE_API FMetaStoryRunParallelStateTreeTask();

#if WITH_EDITORONLY_DATA
	// Sets event handling priority
	void SetEventHandlingPriority(const EMetaStoryTransitionPriority NewPriority)
	{
		EventHandlingPriority = NewPriority;
	}
#endif	
	
protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transitions) const override;
	UE_API virtual EMetaStoryRunStatus Tick(FMetaStoryExecutionContext& Context, const float DeltaTime) const override;
	UE_API virtual void TriggerTransitions(FMetaStoryExecutionContext& Context) const override;
	UE_API virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override;

#if WITH_EDITOR
	UE_API virtual EDataValidationResult Compile(UE::MetaStory::ICompileNodeContext& Context) override;
	UE_API virtual void PostEditInstanceDataChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FMetaStoryDataView InstanceDataView) override;
	UE_API virtual void PostLoad(FMetaStoryDataView InstanceDataView) override;
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
	virtual FName GetIconName() const override
	{
		return FName("MetaStoryEditorStyle|Node.RunParallel");
	}
	virtual FColor GetIconColor() const override
	{
		return UE::MetaStory::Colors::Grey;
	}
#endif // WITH_EDITOR

	UE_API const FMetaStoryReference& GetStateTreeToRun(FMetaStoryExecutionContext& Context, FInstanceDataType& InstanceData) const;

	/** If set the task will look at the linked state tree override to replace the state tree it's running. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag MetaStoryOverrideTag;

#if WITH_EDITORONLY_DATA
	/**
	 * At what priority the events should be handled in the parallel State Tree.
	 * If set to 'Normal' the order of the States in the State Tree will define the handling order.
	 * If the priority is set to Low, the main tree is let to handle the transitions first.
	 * If set to High or above, the parallel tree has change to handle events first.
	 * If multiple tasks has same priority, the State order of the States defines the handling order.
	 * The tree handling order is: States and handle from leaf to root, tasks before and handled before transitions per State.
	 */
	UPROPERTY(EditAnywhere, Category = Parameter)
	EMetaStoryTransitionPriority EventHandlingPriority = EMetaStoryTransitionPriority::Normal;
#endif // WITH_EDITORONLY_DATA	
};

#undef UE_API
