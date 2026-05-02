// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/CurveFloat.h"
#include "MetaStoryAnyEnum.h"
#include "MetaStoryConsiderationBase.h"
#include "MetaStoryCommonConsiderations.generated.h"

#define UE_API METASTORYMODULE_API

USTRUCT()
struct FMetaStoryConstantConsiderationInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0, ClampMax = 1, UIMin = 0, UIMax = 1), Category = "Default")
	float Constant = 0.f;
};

/**
 * Consideration using a constant as its score.
 */
USTRUCT(DisplayName = "Constant")
struct FMetaStoryConstantConsideration: public FMetaStoryConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryConstantConsiderationInstanceData;

	//~ Begin FMetaStoryNodeBase Interface
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
	//~ End FMetaStoryNodeBase Interface
#endif

protected:
	//~ Begin FMetaStoryConsiderationBase Interface
	UE_API virtual float GetScore(FMetaStoryExecutionContext& Context) const override;
	//~ End FMetaStoryConsiderationBase Interface
};

USTRUCT()
struct FMetaStoryConsiderationResponseCurve
{
	GENERATED_BODY()

	/**
	 * Evaluate the output value from curve
	 * @param NormalizedInput the normalized input value to the response curve
	 * @return The output value. If the curve is not set, will simply return NormalizedInput.
	 */
	float Evaluate(float NormalizedInput) const
	{
		if (const FRichCurve* Curve = CurveInfo.GetRichCurveConst())
		{
			if (!Curve->IsEmpty())
			{
				return Curve->Eval(NormalizedInput);
			}
		}

		return NormalizedInput;
	}

	/* Curve used to output the final score for the Consideration. */
	UPROPERTY(EditAnywhere, Category = Default, DisplayName = "Curve")
	FRuntimeFloatCurve CurveInfo;
};

USTRUCT()
struct FMetaStoryFloatInputConsiderationInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	float Input = 0.f;

	UPROPERTY(EditAnywhere, DisplayName = "InputRange", Category = "Parameter")
	FFloatInterval Interval = FFloatInterval(0.f, 1.f);
};

/**
 * Consideration using a Float as input to the response curve.
 */
USTRUCT(DisplayName = "Float Input")
struct FMetaStoryFloatInputConsideration : public FMetaStoryConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryFloatInputConsiderationInstanceData;
	
	//~ Begin FMetaStoryNodeBase Interface
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
#if WITH_EDITOR
	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
	//~ End FMetaStoryNodeBase Interface
#endif

protected:
	//~ Begin FMetaStoryConsiderationBase Interface
	UE_API virtual float GetScore(FMetaStoryExecutionContext& Context) const override;
	//~ End FMetaStoryConsiderationBase Interface

public:
	UPROPERTY(EditAnywhere, Category = "Default")
	FMetaStoryConsiderationResponseCurve ResponseCurve;
};

USTRUCT()
struct FMetaStoryEnumValueScorePair
{
	GENERATED_BODY()

	bool operator==(const FMetaStoryEnumValueScorePair& RHS) const
	{
		return EnumValue == RHS.EnumValue && Score == RHS.Score;
	}

	bool operator!=(const FMetaStoryEnumValueScorePair& RHS) const
	{
		return EnumValue != RHS.EnumValue || Score != RHS.Score;
	}

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Default)
	FName EnumName;
#endif

	UPROPERTY(EditAnywhere, Category = Default)
	int64 EnumValue = 0;

	UPROPERTY(EditAnywhere, Category = Default, meta = (UIMin = 0.0, ClampMin = 0.0, UIMax = 1.0, ClampMax = 1.0))
	float Score = 0.f;
};

USTRUCT()
struct FMetaStoryEnumValueScorePairs
{
	GENERATED_BODY()
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Default)
	TObjectPtr<const UEnum> Enum;
#endif //WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, Category = Default, DisplayName = EnumValueScorePairs)
	TArray<FMetaStoryEnumValueScorePair> Data;

#if WITH_EDITORONLY_DATA
	/** Initializes the class to specific enum.*/
	void Initialize(const UEnum* NewEnum)
	{
		if (Enum != NewEnum || NewEnum == nullptr)
		{
			Data.Empty();
		}

		Enum = NewEnum;
	}
#endif //WITH_EDITORONLY_DATA
};

USTRUCT()
struct FMetaStoryEnumInputConsiderationInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta=(AllowAnyBinding))
	FMetaStoryAnyEnum Input;
};

USTRUCT(DisplayName = "Enum Input")
struct FMetaStoryEnumInputConsideration : public FMetaStoryConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMetaStoryEnumInputConsiderationInstanceData;

	//~ Begin FMetaStoryNodeBase Interface
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
#if WITH_EDITOR
	UE_API virtual EDataValidationResult Compile(UE::MetaStory::ICompileNodeContext& Context) override;
	UE_API virtual void OnBindingChanged(const FGuid& ID, FMetaStoryDataView InstanceData, const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath, const IMetaStoryBindingLookup& BindingLookup) override;

	UE_API virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const override;
	//~ End FMetaStoryNodeBase Interface
#endif //WITH_EDITOR

protected:
	//~ Begin FMetaStoryConsiderationBase Interface
	UE_API virtual float GetScore(FMetaStoryExecutionContext& Context) const override;
	//~ End FMetaStoryConsiderationBase Interface
	
	UPROPERTY(EditAnywhere, Category = "Default")
	FMetaStoryEnumValueScorePairs EnumValueScorePairs;
};

#undef UE_API
