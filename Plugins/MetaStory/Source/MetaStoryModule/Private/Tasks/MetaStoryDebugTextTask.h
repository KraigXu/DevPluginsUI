// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryTaskBase.h"
#include "MetaStoryDebugTextTask.generated.h"

#define UE_API METASTORYMODULE_API

enum class EMetaStoryRunStatus : uint8;
struct FMetaStoryTransitionResult;

USTRUCT()
struct FMetaStoryDebugTextTaskInstanceData
{
	GENERATED_BODY()

	/** Optional actor where to draw the text at. */
	UPROPERTY(EditAnywhere, Category = "Input", meta=(Optional))
	TObjectPtr<AActor> ReferenceActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FString BindableText;
};

/**
 * Draws debug text on the HUD associated to the player controller.
 */
USTRUCT(meta = (DisplayName = "Debug Text Task"))
struct FMetaStoryDebugTextTask : public FMetaStoryTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryDebugTextTaskInstanceData;
	
	UE_API FMetaStoryDebugTextTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	UE_API virtual EMetaStoryRunStatus EnterState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override;
	UE_API virtual void ExitState(FMetaStoryExecutionContext& Context, const FMetaStoryTransitionResult& Transition) const override;
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
	virtual FName GetIconName() const override
	{
		return FName("MetaStoryEditorStyle|Node.Text");
	}
	virtual FColor GetIconColor() const override
	{
		return UE::MetaStory::Colors::Grey;
	}
#endif
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FString Text;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FColor TextColor = FColor::White;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta=(ClampMin = 0, UIMin = 0))
	float FontScale = 1.0f;
	
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bEnabled = true;
};

#undef UE_API
