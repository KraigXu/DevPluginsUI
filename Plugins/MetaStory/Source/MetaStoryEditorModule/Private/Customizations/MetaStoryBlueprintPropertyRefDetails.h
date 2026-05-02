// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "EdGraphSchema_K2.h"
#include "MetaStoryBlueprintPropertyRefDetails.generated.h"

#define UE_API METASTORYEDITORMODULE_API

class SWidget;
class IPropertyHandle;

/**
 * Type customization for FMetaStoryBlueprintPropertyRef.
 */
class FMetaStoryBlueprintPropertyRefDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

private:

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;
};

/**
 * Specific property ref schema to allow customizing the requirements (e.g. supported containers).
 */
UCLASS(MinimalAPI)
class UMetaStoryPropertyRefSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()
public:
	UE_API virtual bool SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction, const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const override;
};

#undef UE_API
