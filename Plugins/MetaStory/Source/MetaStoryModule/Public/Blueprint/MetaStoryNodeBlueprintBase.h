// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryExecutionTypes.h"
#include "MetaStoryNodeBase.h"
#include "UObject/ObjectMacros.h"
#include "MetaStoryAsyncExecutionContext.h"
#include "MetaStoryNodeBlueprintBase.generated.h"

#define UE_API METASTORYMODULE_API

struct FMetaStoryEvent;
struct FMetaStoryEventQueue;
struct FMetaStoryInstanceStorage;
struct FMetaStoryLinker;
struct FMetaStoryExecutionContext;
struct FMetaStoryBlueprintPropertyRef;
class UMetaStory;

UENUM()
enum class EMetaStoryBlueprintPropertyCategory : uint8
{
	NotSet,
	Input,	
	Parameter,
	Output,
	ContextObject,
};


/** Struct use to copy external data to the Blueprint item instance, resolved during MetaStory linking. */
struct FMetaStoryBlueprintExternalDataHandle
{
	const FProperty* Property = nullptr;
	FMetaStoryExternalDataHandle Handle;
};


UCLASS(MinimalAPI, Abstract, meta = (DisallowLevelActorReference = true))
class UMetaStoryNodeBlueprintBase : public UObject
{
	GENERATED_BODY()

public:
	/** Sends event to the MetaStory. */
	UFUNCTION(BlueprintCallable, Category = "MetaStory", meta = (HideSelfPin = "true", DisplayName = "MetaStory Send Event"))
	UE_API void SendEvent(const FMetaStoryEvent& Event);

	/** Request state transition. */
	UFUNCTION(BlueprintCallable, Category = "MetaStory", meta = (HideSelfPin = "true", DisplayName = "MetaStory Request Transition"))
	UE_API void RequestTransition(const FMetaStoryStateLink& TargetState, const EMetaStoryTransitionPriority Priority = EMetaStoryTransitionPriority::Normal);

	/** Returns a reference to selected property in State Tree. */
	UFUNCTION(CustomThunk)
	UE_API void GetPropertyReference(const FMetaStoryBlueprintPropertyRef& PropertyRef) const;

	/** Returns true if reference to selected property in State Tree is accessible. */
	UFUNCTION()
	UE_API bool IsPropertyRefValid(const FMetaStoryBlueprintPropertyRef& PropertyRef) const;

	/** @return text describing the property, either direct value or binding description. Used internally. */
	UFUNCTION(BlueprintCallable, Category = "MetaStory", meta=( BlueprintInternalUseOnly="true" ))
	UE_API FText GetPropertyDescriptionByPropertyName(FName PropertyName) const;

#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting) const;

	FName GetIconName() const
	{
		return IconName;
	}
	
	FColor GetIconColor() const
	{
		return IconColor;
	}
#endif
	
protected:

	/** Event to implement to get node description. */
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Get Description"))
	UE_API FText ReceiveGetDescription(EMetaStoryNodeFormatting Formatting) const;

	UE_API virtual UWorld* GetWorld() const override;
	UE_API AActor* GetOwnerActor(const FMetaStoryExecutionContext& Context) const;

	/** These methods are const as they set mutable variables and need to be called from a const method. */
	UE_API void SetCachedInstanceDataFromContext(const FMetaStoryExecutionContext& Context) const;
	UE_API void ClearCachedInstanceData() const;

	FMetaStoryWeakExecutionContext GetWeakExecutionContext() const
	{
		return WeakExecutionContext;
	}

private:
	DECLARE_FUNCTION(execGetPropertyReference);

	UE_API void* GetMutablePtrToProperty(const FMetaStoryBlueprintPropertyRef& PropertyRef, FProperty*& OutSourceProperty) const;
	
	/** Cached execution context while the node is active for async nodes. */
	mutable FMetaStoryWeakExecutionContext WeakExecutionContext;

#if WITH_EDITORONLY_DATA

	UE_DEPRECATED(5.6, "WeakInstanceStorage is deprecated. Use WeakExecutionContext")
	/** Cached instance data while the node is active for async nodes. */
	mutable TWeakPtr<FMetaStoryInstanceStorage> WeakInstanceStorage;

	UE_DEPRECATED(5.6, "CachedFrameStateTree is deprecated.")
	/** Cached State Tree of owning execution frame. */
	UPROPERTY()
	mutable TObjectPtr<const UMetaStory> CachedFrameStateTree = nullptr;

	UE_DEPRECATED(5.6, "CachedFrameRootState is deprecated.")
	/** Cached root state of owning execution frame. */
	mutable FMetaStoryStateHandle CachedFrameRootState;

	/** Description of the node. */
	UPROPERTY(EditDefaultsOnly, Category="Description")
	FText Description;

	/**
	 * Name of the icon in format:
	 *		StyleSetName | StyleName [ | SmallStyleName | StatusOverlayStyleName]
	 *		SmallStyleName and StatusOverlayStyleName are optional.
	 *		Example: "MetaStoryEditorStyle|Node.Animation"
	 */
	UPROPERTY(EditDefaultsOnly, Category="Description")
	FName IconName;

	/** Color of the icon. */
	UPROPERTY(EditDefaultsOnly, Category="Description")
	FColor IconColor = UE::MetaStory::Colors::Grey;
#endif // 	WITH_EDITORONLY_DATA

#if WITH_EDITOR
	/** Cached values used during editor to make some BP nodes simpler to use. */
	static UE_API FGuid CachedNodeID;
	static UE_API const IMetaStoryBindingLookup* CachedBindingLookup;
#endif	
};

#undef UE_API
