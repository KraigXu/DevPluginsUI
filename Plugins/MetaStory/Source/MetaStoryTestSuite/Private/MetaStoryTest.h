// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStorySchema.h"
#include "MetaStoryTypes.h"
#include "MetaStoryTest.generated.h"

UCLASS(HideDropdown)
class UMetaStoryTestSchema : public UMetaStorySchema
{
	GENERATED_BODY()

public:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override
	{
		return true;
	}
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override
	{
		return true;
	}
	virtual bool IsScheduledTickAllowed() const
	{
		return true;
	}
	virtual EMetaStoryStateSelectionRules GetStateSelectionRules() const
	{
		return DefaultRules;
	}
	void SetStateSelectionRules(EMetaStoryStateSelectionRules Rules)
	{
		DefaultRules = Rules;
	}

private:
	UPROPERTY()
	EMetaStoryStateSelectionRules DefaultRules = EMetaStoryStateSelectionRules::Default;
};

UCLASS(HideDropdown)
class UMetaStoryTestSchema2 : public UMetaStorySchema
{
	GENERATED_BODY()
};

#define IMPLEMENT_METASTORY_INSTANT_TEST(TestClass, PrettyName) \
	IMPLEMENT_AI_INSTANT_TEST_WITH_FLAGS(TestClass, PrettyName, EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::SupportsAutoRTFM)
