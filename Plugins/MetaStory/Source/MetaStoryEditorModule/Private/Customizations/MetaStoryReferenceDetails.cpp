// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryReferenceDetails.h"

#include "MetaStory.h"
#include "MetaStoryDelegates.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "IMetaStorySchemaProvider.h"
#include "PropertyBagDetails.h"
#include "PropertyCustomizationHelpers.h"
#include "MetaStoryReference.h"
#include "MetaStoryPropertyHelpers.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

class FMetaStoryRefParametersDetails : public FPropertyBagInstanceDataDetails
{
public:
	FMetaStoryRefParametersDetails(const TSharedPtr<IPropertyHandle> InStateTreeRefStructProperty, const TSharedPtr<IPropertyHandle> InParametersStructProperty, const TSharedPtr<IPropertyUtilities>& InPropUtils)
		: FPropertyBagInstanceDataDetails(InParametersStructProperty, InPropUtils, /*bInFixedLayout*/true)
		, MetaStoryRefStructProperty(InStateTreeRefStructProperty)
	{
		check(UE::MetaStory::PropertyHelpers::IsScriptStruct<FMetaStoryReference>(MetaStoryRefStructProperty));
	}

protected:
	struct FMetaStoryReferenceOverrideProvider : public IPropertyBagOverrideProvider
	{
		FMetaStoryReferenceOverrideProvider(FMetaStoryReference& InStateTreeRef)
			: MetaStoryRef(InStateTreeRef)
		{
		}
		
		virtual bool IsPropertyOverridden(const FGuid PropertyID) const override
		{
			return MetaStoryRef.IsPropertyOverridden(PropertyID);
		}
		
		virtual void SetPropertyOverride(const FGuid PropertyID, const bool bIsOverridden) const override
		{
			MetaStoryRef.SetPropertyOverridden(PropertyID, bIsOverridden);
		}

	private:
		FMetaStoryReference& MetaStoryRef;
	};

	virtual bool HasPropertyOverrides() const override
	{
		return true;
	}

	virtual void PreChangeOverrides() override
	{
		check(MetaStoryRefStructProperty);
		MetaStoryRefStructProperty->NotifyPreChange();
	}

	virtual void PostChangeOverrides() override
	{
		check(MetaStoryRefStructProperty);
		MetaStoryRefStructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		MetaStoryRefStructProperty->NotifyFinishedChangingProperties();
	}

	virtual void EnumeratePropertyBags(TSharedPtr<IPropertyHandle> PropertyBagHandle, const EnumeratePropertyBagFuncRef& Func) const override
	{
		check(MetaStoryRefStructProperty);
		MetaStoryRefStructProperty->EnumerateRawData([Func](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (FMetaStoryReference* MetaStoryRef = static_cast<FMetaStoryReference*>(RawData))
			{
				if (const UMetaStory* MetaStory = MetaStoryRef->GetStateTree())
				{
					const FInstancedPropertyBag& DefaultParameters = MetaStory->GetDefaultParameters();
					FInstancedPropertyBag& Parameters = MetaStoryRef->GetMutableParameters();
					FMetaStoryReferenceOverrideProvider OverrideProvider(*MetaStoryRef);
					if (!Func(DefaultParameters, Parameters, OverrideProvider))
					{
						return false;
					}
				}
			}
			return true;
		});
	}

private:
	
	TSharedPtr<IPropertyHandle> MetaStoryRefStructProperty;
};


TSharedRef<IPropertyTypeCustomization> FMetaStoryReferenceDetails::MakeInstance()
{
	return MakeShareable(new FMetaStoryReferenceDetails);
}

void FMetaStoryReferenceDetails::CustomizeHeader(const TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;
	PropUtils = InCustomizationUtils.GetPropertyUtilities();

	ParametersHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryReference, Parameters));
	check(ParametersHandle);
	
	// Make sure parameters are synced when displaying MetaStoryReference.
	// Associated MetaStory asset might have been recompiled after the MetaStoryReference was loaded.
	// Note: SyncParameters() will create an undoable transaction, do not try to sync when called during Undo/redo as it would overwrite the undo.   
	if (!GIsTransacting)
	{
		SyncParameters();
	}

	const TSharedPtr<IPropertyHandle> MetaStoryProperty = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryReference, MetaStory));
	check(MetaStoryProperty.IsValid());

	const FString SchemaMetaDataValue = GetSchemaPath(*StructPropertyHandle);

	InHeaderRow
	.NameContent()
	[
		MetaStoryProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SObjectPropertyEntryBox)
		.PropertyHandle(MetaStoryProperty)
		.AllowedClass(UMetaStory::StaticClass())
		.ThumbnailPool(InCustomizationUtils.GetThumbnailPool())
		.OnShouldFilterAsset_Lambda([SchemaMetaDataValue](const FAssetData& InAssetData)
		{
			return !SchemaMetaDataValue.IsEmpty() && !InAssetData.TagsAndValues.ContainsKeyValue(UE::MetaStory::SchemaTag, SchemaMetaDataValue);
		})
	]
	.ShouldAutoExpand(true);
	
	MetaStoryProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([this]()
		{
			SyncParameters();
		}));

	// Registers a delegate to be notified when the associated MetaStory asset get successfully recompiled
	// to make sure that the parameters in the MetaStoryReference are still valid.
	UE::MetaStory::Delegates::OnPostCompile.AddSP(this, &FMetaStoryReferenceDetails::OnTreeCompiled);
}

void FMetaStoryReferenceDetails::CustomizeChildren(const TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InChildrenBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	// Show parameter values.
	const TSharedRef<FMetaStoryRefParametersDetails> ParameterInstanceDetails = MakeShareable(new FMetaStoryRefParametersDetails(StructPropertyHandle, ParametersHandle, PropUtils));
	InChildrenBuilder.AddCustomBuilder(ParameterInstanceDetails);
}

void FMetaStoryReferenceDetails::OnTreeCompiled(const UMetaStory& MetaStory) const
{
	SyncParameters(&MetaStory);
}

void FMetaStoryReferenceDetails::SyncParameters(const UMetaStory* MetaStoryToSync) const
{
	check(StructPropertyHandle.IsValid());

	// This function will get called in 3 situations:
	// - auto update when the property customization is created
	// - auto update when a state tree is compiled.
	// - when the associate state tree asset is changed
	
	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);

	TArray<void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);

	bool bDidSync = false;
	
	for (int32 i = 0; i < RawData.Num(); i++)
	{
		if (FMetaStoryReference* MetaStoryReference = static_cast<FMetaStoryReference*>(RawData[i]))
		{
			const UMetaStory* MetaStoryAsset = MetaStoryReference->GetStateTree();

			if ((MetaStoryToSync == nullptr || MetaStoryAsset == MetaStoryToSync) 
				&& MetaStoryReference->RequiresParametersSync())
			{
				// Changing the data without Modify().
				// When called on property row creation, we don't expect to dirty the owner.
				// In other cases we expect the outer to already been modified.
				MetaStoryReference->SyncParameters();
				bDidSync = true;
			}
		}
	}

	if (bDidSync && PropUtils)
	{
		PropUtils->RequestRefresh();
	}
}

FString FMetaStoryReferenceDetails::GetSchemaPath(IPropertyHandle& StructPropertyHandle)
{
	if (StructPropertyHandle.HasMetaData(UE::MetaStory::SchemaCanBeOverridenTag))
	{
		TArray<UObject*> OuterObjects;
		StructPropertyHandle.GetOuterObjects(OuterObjects);

		for (const UObject* OuterObject : OuterObjects)
		{
			if (const IMetaStorySchemaProvider* SchemaProvider = Cast<IMetaStorySchemaProvider>(OuterObject))
			{
				if (const UClass* SchemaClass = SchemaProvider->GetSchema().Get())
				{
					return SchemaClass->GetPathName();
				}
			}
		}
	}

	return StructPropertyHandle.GetMetaData(UE::MetaStory::SchemaTag);
}

#undef LOCTEXT_NAMESPACE
