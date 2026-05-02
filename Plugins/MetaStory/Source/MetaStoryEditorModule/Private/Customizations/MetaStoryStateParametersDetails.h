// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyBagDetails.h"

class IDetailPropertyRow;
class IPropertyHandle;
class IPropertyUtilities;
class UMetaStory;
class UMetaStoryState;
class UMetaStoryEditorData;

/**
* Type customization for FMetaStoryStateParameters.
*/

class FMetaStoryStateParametersDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	void FindOuterObjects();

	TSharedPtr<IPropertyUtilities> PropUtils;

	TSharedPtr<IPropertyHandle> ParametersProperty;
	TSharedPtr<IPropertyHandle> FixedLayoutProperty;
	TSharedPtr<IPropertyHandle> IDProperty;
	TSharedPtr<IPropertyHandle> StructProperty;

	bool bFixedLayout = false;

	TWeakObjectPtr<UMetaStoryEditorData> WeakEditorData = nullptr;
	TWeakObjectPtr<UMetaStory> WeakStateTree = nullptr;
	TWeakObjectPtr<UMetaStoryState> WeakState = nullptr;
};


class FMetaStoryStateParametersInstanceDataDetails : public FPropertyBagInstanceDataDetails
{
public:
	FMetaStoryStateParametersInstanceDataDetails(
		const TSharedPtr<IPropertyHandle>& InStructProperty,
		const TSharedPtr<IPropertyHandle>& InParametersStructProperty,
		const TSharedPtr<IPropertyUtilities>& InPropUtils,
		const bool bInFixedLayout,
		FGuid InID,
		TWeakObjectPtr<UMetaStoryEditorData> InEditorData,
		TWeakObjectPtr<UMetaStoryState> InState);
	
	virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override;

	virtual bool HasPropertyOverrides() const override;
	virtual void PreChangeOverrides() override;
	virtual void PostChangeOverrides() override;
	virtual void EnumeratePropertyBags(TSharedPtr<IPropertyHandle> PropertyBagHandle, const EnumeratePropertyBagFuncRef& Func) const override;

private:
	TSharedPtr<IPropertyHandle> StructProperty;
	TWeakObjectPtr<UMetaStoryEditorData> WeakEditorData;
	TWeakObjectPtr<UMetaStoryState> WeakState;
	FGuid ID;
};
