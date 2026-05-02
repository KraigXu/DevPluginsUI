// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMetaStorySchemaProvider.h"
#include "MetaStoryNodeBase.h"
#include "MetaStoryEditorNode.h"
#include "MetaStoryEditorTypes.h"
#include "MetaStoryEvents.h"
#include "MetaStoryState.generated.h"

#define UE_API METASTORYEDITORMODULE_API

class UMetaStoryState;
class UMetaStory;

/**
 * Editor representation of an event description.
 */
USTRUCT()
struct FMetaStoryEventDesc
{
	GENERATED_BODY()

	FMetaStoryEventDesc() = default;

	FMetaStoryEventDesc(FGameplayTag InTag)
		: Tag(InTag)
	{}

	/** Event Tag. */
	UPROPERTY(EditDefaultsOnly, Category = "Event")
	FGameplayTag Tag;

	/** Event Payload Struct. */
	UPROPERTY(EditDefaultsOnly, Category = "Event")
	TObjectPtr<const UScriptStruct> PayloadStruct;

	/** If set to true, the event is consumed (later state selection cannot react to it) if state selection can be made. */
	UPROPERTY(EditDefaultsOnly, Category = "Event")
	bool bConsumeEventOnSelect = true;
	
	bool IsValid() const
	{
		return Tag.IsValid() || PayloadStruct;
	}

	FMetaStoryEvent& GetTemporaryEvent()
	{
		TemporaryEvent.Tag = Tag;
		TemporaryEvent.Payload = FInstancedStruct(PayloadStruct);

		return TemporaryEvent;
	}

	bool operator==(const FMetaStoryEventDesc& Other) const
	{
		return Tag == Other.Tag && PayloadStruct == Other.PayloadStruct;
	}

private:
	/** Temporary event used as a source value in bindings. */
	UPROPERTY(Transient)
	FMetaStoryEvent TemporaryEvent;
};

/**
 * MetaStory's internal delegate listener used exclusively in transitions.
 */
USTRUCT()
struct FMetaStoryTransitionDelegateListener
{
	GENERATED_BODY()
};

/**
 * Editor representation of a transition in MetaStory
 */
USTRUCT()
struct FMetaStoryTransition
{
	GENERATED_BODY()

	// Macro needed to avoid deprecation errors with members being copied or created.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryTransition() = default;
	UE_API FMetaStoryTransition(const EMetaStoryTransitionTrigger InTrigger, const EMetaStoryTransitionType InType, const UMetaStoryState* InState = nullptr);
	UE_API FMetaStoryTransition(const EMetaStoryTransitionTrigger InTrigger, const FGameplayTag InEventTag, const EMetaStoryTransitionType InType, const UMetaStoryState* InState = nullptr);
	FMetaStoryTransition(const FMetaStoryTransition&) = default;
	FMetaStoryTransition(FMetaStoryTransition&&) = default;
	FMetaStoryTransition& operator=(const FMetaStoryTransition&) = default;
	FMetaStoryTransition& operator=(FMetaStoryTransition&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	template<typename T, typename... TArgs>
	TMetaStoryTypedEditorNode<T>& AddCondition(TArgs&&... InArgs)
	{
		FMetaStoryEditorNode& CondNode = Conditions.AddDefaulted_GetRef();
		CondNode.ID = FGuid::NewGuid();
		CondNode.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		const FMetaStoryNodeBase& Node = CondNode.Node.GetMutable<FMetaStoryNodeBase>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Node.GetInstanceDataType()))
		{
			CondNode.Instance.InitializeAs(InstanceType);
		}
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Node.GetExecutionRuntimeDataType()))
		{
			CondNode.ExecutionRuntimeData.InitializeAs(InstanceType);
		}
		return static_cast<TMetaStoryTypedEditorNode<T>&>(CondNode);
	}

	FGuid GetEventID() const
	{
		return FGuid::Combine(ID, FGuid::NewDeterministicGuid(TEXT("Event")));
	}

	UE_API void PostSerialize(const FArchive& Ar);

	/** When to try trigger the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	EMetaStoryTransitionTrigger Trigger = EMetaStoryTransitionTrigger::OnStateCompleted;

	/** Defines the event required to be present during state selection for the transition to trigger. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", DisplayName = "Required Event")
	FMetaStoryEventDesc RequiredEvent; 

	/** Transition target state. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta=(DisplayName="Transition To"))
	FMetaStoryStateLink State;

	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	FGuid ID;

	/** Listener to the selected delegate dispatcher. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", DisplayName = "Delegate")
	FMetaStoryTransitionDelegateListener DelegateListener;

	/**
	 * Transition priority when multiple transitions happen at the same time.
	 * During transition handling, the transitions are visited from leaf to root.
	 * The first visited transition, of highest priority, that leads to a state selection, will be activated.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::Normal;

	/** Delay the triggering of the transition. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	bool bDelayTransition = false;

	/** Transition delay duration in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (EditCondition = "bDelayTransition", UIMin = "0", ClampMin = "0", UIMax = "25", ClampMax = "25", ForceUnits="s"))
	float DelayDuration = 0.0f;

	/** Transition delay random variance in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (EditCondition = "bDelayTransition", UIMin = "0", ClampMin = "0", UIMax = "25", ClampMax = "25", ForceUnits="s"))
	float DelayRandomVariance = 0.0f;

	/** Expression of conditions that need to evaluate to true to allow transition to be triggered. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (BaseStruct = "/Script/MetaStoryModule.MetaStoryConditionBase", BaseClass = "/Script/MetaStoryModule.MetaStoryConditionBlueprintBase"))
	TArray<FMetaStoryEditorNode> Conditions;

	/** True if the Transition is Enabled (i.e. not explicitly disabled in the asset). */
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bTransitionEnabled = true;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(all, "Use RequiredEvent.Tag instead.")
	UPROPERTY()
	FGameplayTag EventTag_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};

template<>
struct TStructOpsTypeTraits<FMetaStoryTransition> : public TStructOpsTypeTraitsBase2<FMetaStoryTransition>
{
	enum 
	{
		WithPostSerialize = true,
	};
};


USTRUCT()
struct FMetaStoryStateParameters
{
	GENERATED_BODY()

	void ResetParametersAndOverrides()
	{
		// Reset just the parameters, keep the bFixedLayout intact.
		Parameters.Reset();
		PropertyOverrides.Reset();
	}

	/** Removes overrides that do appear in Parameters. */
	UE_API void RemoveUnusedOverrides();
	
	UPROPERTY(EditDefaultsOnly, Category = Parameters)
	FInstancedPropertyBag Parameters;

	/** Overrides for parameters. */
	UPROPERTY()
	TArray<FGuid> PropertyOverrides;

	UPROPERTY(EditDefaultsOnly, Category = Parameters)
	bool bFixedLayout = false;

	UPROPERTY(EditDefaultsOnly, Category = Parameters, meta = (IgnoreForMemberInitializationTest))
	FGuid ID;
};

/**
 * Editor representation of a state in MetaStory
 */
UCLASS(MinimalAPI, BlueprintType, EditInlineNew, CollapseCategories, meta = (DisallowLevelActorReference = true))
class UMetaStoryState : public UObject, public IMetaStorySchemaProvider
{
	GENERATED_BODY()

public:
	UE_API UMetaStoryState(const FObjectInitializer& ObjectInitializer);
	UE_API virtual ~UMetaStoryState() override;

	UE_API virtual void PostInitProperties() override;
	UE_API virtual void PreEditChange(FEditPropertyChain& PropertyAboutToChange) override;
	UE_API virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	UE_API virtual void PostLoad() override;;
	UE_API void UpdateParametersFromLinkedSubtree();
	UE_API void OnTreeCompiled(const UMetaStory& MetaStory);

	UE_API const UMetaStoryState* GetRootState() const;
	UE_API const UMetaStoryState* GetNextSiblingState() const;
	UE_API const UMetaStoryState* GetNextSelectableSiblingState() const;

	/** @return the path of the state as string. */
	UE_API FString GetPath() const;
	
	/** @return true if the property of specified ID is overridden. */
	bool IsParametersPropertyOverridden(const FGuid PropertyID) const
	{
		return Parameters.PropertyOverrides.Contains(PropertyID);
	}

	/** Sets the override status of specified property by ID. */
	UE_API void SetParametersPropertyOverridden(const FGuid PropertyID, const bool bIsOverridden);

	/** @returns Default parameters from linked state or asset). */
	UE_API const FInstancedPropertyBag* GetDefaultParameters() const;
	
	//~ MetaStory Builder API
	/** @return state link to this state. */
	UE_API FMetaStoryStateLink GetLinkToState() const;
	
	/** Adds child state with specified name. */
	UMetaStoryState& AddChildState(const FName ChildName, const EMetaStoryStateType StateType = EMetaStoryStateType::State)
	{
		UMetaStoryState* ChildState = NewObject<UMetaStoryState>(this, FName(), RF_Transactional);
		check(ChildState);
		ChildState->Name = ChildName;
		ChildState->Parent = this;
		ChildState->Type = StateType;
		Children.Add(ChildState);
		return *ChildState;
	}

	/**
	 * Adds enter condition of specified type.
	 * @return reference to the new condition.
	 */
	template<typename T, typename... TArgs>
	TMetaStoryTypedEditorNode<T>& AddEnterCondition(TArgs&&... InArgs)
	{
		FMetaStoryEditorNode& CondNode = EnterConditions.AddDefaulted_GetRef();
		CondNode.ID = FGuid::NewGuid();
		CondNode.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		const FMetaStoryNodeBase& Node = CondNode.Node.GetMutable<FMetaStoryNodeBase>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Node.GetInstanceDataType()))
		{
			CondNode.Instance.InitializeAs(InstanceType);
		}
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Node.GetExecutionRuntimeDataType()))
		{
			CondNode.ExecutionRuntimeData.InitializeAs(InstanceType);
		}
		return static_cast<TMetaStoryTypedEditorNode<T>&>(CondNode);
	}

	/**
	 * Adds Task of specified type.
	 * @return reference to the new Task.
	 */
	template<typename T, typename... TArgs>
	TMetaStoryTypedEditorNode<T>& AddTask(TArgs&&... InArgs)
	{
		FMetaStoryEditorNode& TaskItem = Tasks.AddDefaulted_GetRef();
		TaskItem.ID = FGuid::NewGuid();
		TaskItem.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		const FMetaStoryNodeBase& Node = TaskItem.Node.GetMutable<FMetaStoryNodeBase>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Node.GetInstanceDataType()))
		{
			TaskItem.Instance.InitializeAs(InstanceType);
		}
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Node.GetExecutionRuntimeDataType()))
		{
			TaskItem.ExecutionRuntimeData.InitializeAs(InstanceType);
		}
		return static_cast<TMetaStoryTypedEditorNode<T>&>(TaskItem);
	}

	/** Sets linked state and updates parameters to match the linked state. */
	UE_API void SetLinkedState(FMetaStoryStateLink InStateLink);

	/** Sets linked asset and updates parameters to match the linked asset. */
	UE_API void SetLinkedStateAsset(UMetaStory* InLinkedAsset);
	
	/**
	 * Adds Transition.
	 * @return reference to the new Transition.
	 */
	FMetaStoryTransition& AddTransition(const EMetaStoryTransitionTrigger InTrigger, const EMetaStoryTransitionType InType, const UMetaStoryState* InState = nullptr)
	{
		FMetaStoryTransition& Transition = Transitions.Emplace_GetRef(InTrigger, InType, InState);
		Transition.ID = FGuid::NewGuid();
		return Transition;
	}

	FMetaStoryTransition& AddTransition(const EMetaStoryTransitionTrigger InTrigger, const FGameplayTag InEventTag, const EMetaStoryTransitionType InType, const UMetaStoryState* InState = nullptr)
	{
		FMetaStoryTransition& Transition = Transitions.Emplace_GetRef(InTrigger, InEventTag, InType, InState);
		Transition.ID = FGuid::NewGuid();
		return Transition;
	}

	FGuid GetEventID() const
	{
		return FGuid::Combine(ID, FGuid::NewDeterministicGuid(TEXT("Event")));
	}

	//~ IMetaStorySchemaProvider API
	/**  @return Class of schema used by the MetaStory containing this state. */
	UE_API virtual TSubclassOf<UMetaStorySchema> GetSchema() const override;

	//~IMetaStorySchemaProvider API

	//~ Note: these properties are customized out in FMetaStoryStateDetails, adding a new property might require to adjust the customization.
	
	/** Display name of the State */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	FName Name;

	/** Description of the State */
	UPROPERTY(EditDefaultsOnly, Category = "State", meta=(MultiLine))
	FString Description;

	/** GameplayTag describing the State */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	FGameplayTag Tag;

	/** Display color of the State */
	UPROPERTY(EditDefaultsOnly, Category = "State", DisplayName = "Color")
	FMetaStoryEditorColorRef ColorRef;

	/** Type the State, allows e.g. states to be linked to other States. */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	EMetaStoryStateType Type = EMetaStoryStateType::State;

	/** How to treat child states when this State is selected.  */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	EMetaStoryStateSelectionBehavior SelectionBehavior = EMetaStoryStateSelectionBehavior::TrySelectChildrenInOrder;
	
	/** How tasks will complete the state. Only tasks that are considered for completion can complete the state. */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	EMetaStoryTaskCompletionType TasksCompletion = EMetaStoryTaskCompletionType::Any;

	/** Subtree to run as extension of this State. */
	UPROPERTY(EditDefaultsOnly, Category = "State", Meta=(DirectStatesOnly, SubtreesOnly))
	FMetaStoryStateLink LinkedSubtree;

	/** Another MetaStory asset to run as extension of this State. */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	TObjectPtr<UMetaStory> LinkedAsset = nullptr;

	/** 
	 * Tick rate in seconds the state tasks and transitions should tick.
	 * If set the state cannot sleep.
	 * If set all the other states (children or parents) will also tick at that rate.
	 * If more than one active states has a custom tick rate then the smallest custom tick rate wins.
	 * If not set, the state will tick every frame unless the MetaStory is allowed to sleep.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "State", meta = (EditCondition = "bHasCustomTickRate", ClampMin = 0.0f))
	float CustomTickRate = 0.0f;

	/** Activate the CustomTickRate. */
	UPROPERTY(EditDefaultsOnly, Category = "State", meta=(InlineEditConditionToggle))
	bool bHasCustomTickRate = false;

	/** Parameters of this state. If the state is linked to another state or asset, the parameters are for the linked state. */
	UPROPERTY(EditDefaultsOnly, Category = "State")
	FMetaStoryStateParameters Parameters;

	/** Should state's required event and enter conditions be evaluated when transition leads directly to it's child. */
	UPROPERTY(EditDefaultsOnly, Category = "Enter Conditions")
	bool bCheckPrerequisitesWhenActivatingChildDirectly = true;

	UPROPERTY(EditDefaultsOnly, Category = "Enter Conditions", meta=(InlineEditConditionToggle))
	bool bHasRequiredEventToEnter = false;

	/** Defines the event required to be present during state selection for the state to be selected. */
	UPROPERTY(EditDefaultsOnly, Category = "Enter Conditions", meta = (EditCondition = "bHasRequiredEventToEnter"))
	FMetaStoryEventDesc RequiredEventToEnter;
	
	/** Weight used to scale the normalized final utility score for this state */
	UPROPERTY(EditDefaultsOnly, Category = "Utility", meta=(ClampMin=0))
	float Weight = 1.f;

	/** Expression of enter conditions that needs to evaluate true to allow the state to be selected. */
	UPROPERTY(EditDefaultsOnly, Category = "Enter Conditions", meta = (BaseStruct = "/Script/MetaStoryModule.MetaStoryConditionBase", BaseClass = "/Script/MetaStoryModule.MetaStoryConditionBlueprintBase"))
	TArray<FMetaStoryEditorNode> EnterConditions;

	UPROPERTY(EditDefaultsOnly, Category = "Tasks", meta = (BaseStruct = "/Script/MetaStoryModule.MetaStoryTaskBase", BaseClass = "/Script/MetaStoryModule.MetaStoryTaskBlueprintBase"))
	TArray<FMetaStoryEditorNode> Tasks;

	/** Expression of enter conditions that needs to evaluate true to allow the state to be selected. */
	UPROPERTY(EditDefaultsOnly, Category = "Utility", meta = (BaseStruct = "/Script/MetaStoryModule.MetaStoryConsiderationBase", BaseClass = "/Script/MetaStoryModule.MetaStoryConsiderationBlueprintBase"))
	TArray<FMetaStoryEditorNode> Considerations;

	// Single item used when schema calls for single task per state.
	UPROPERTY(EditDefaultsOnly, Category = "Task", meta = (BaseStruct = "/Script/MetaStoryModule.MetaStoryTaskBase", BaseClass = "/Script/MetaStoryModule.MetaStoryTaskBlueprintBase"))
	FMetaStoryEditorNode SingleTask;

	UPROPERTY(EditDefaultsOnly, Category = "Transitions")
	TArray<FMetaStoryTransition> Transitions;

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UMetaStoryState>> Children;

	UPROPERTY(EditDefaultsOnly, Category = "State", meta = (IgnoreForMemberInitializationTest))
	FGuid ID;

	UPROPERTY(meta = (ExcludeFromHash))
	bool bExpanded = true;

	UPROPERTY(EditDefaultsOnly, Category = "State")
	bool bEnabled = true;

	UPROPERTY(meta = (ExcludeFromHash))
	TObjectPtr<UMetaStoryState> Parent = nullptr;
};

#undef UE_API
