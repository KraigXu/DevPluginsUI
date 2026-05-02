// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class UMetaStory;
class IPropertyUtilities;

/**
 * Type customization for FMetaStoryReference.
 */
class FMetaStoryReferenceDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InChildrenBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;

private:
	/**
	 * Callback registered to MetaStory post compilation delegate used to synchronize parameters.
	 * @param MetaStory The MetaStory asset that got compiled.
	 */
	void OnTreeCompiled(const UMetaStory& MetaStory) const;

	/**
	 * Synchronizes parameters with MetaStory asset parameters if needed.
	 * @param MetaStoryToSync Optional MetaStory asset used to filter which parameters should be synced. A null value indicates that all parameters should be synced.   
	 */
	void SyncParameters(const UMetaStory* MetaStoryToSync = nullptr) const;

	static FString GetSchemaPath(IPropertyHandle& StructPropertyHandle);

	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	TSharedPtr<IPropertyHandle> ParametersHandle;
	TSharedPtr<IPropertyUtilities> PropUtils;
};
