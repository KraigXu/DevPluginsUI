// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryStateParametersDetails.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Layout/SBox.h"
#include "PropertyBagDetails.h"
#include "MetaStory.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryPropertyHelpers.h"
#include "MetaStoryBindingExtension.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"


//----------------------------------------------------------------//
// FMetaStoryStateParametersInstanceDataDetails
//----------------------------------------------------------------//
struct FMetaStoryStateOverrideProvider : public FPropertyBagInstanceDataDetails::IPropertyBagOverrideProvider
{
	FMetaStoryStateOverrideProvider(UMetaStoryState& InState)
		: State(InState)
	{
	}
		
	virtual bool IsPropertyOverridden(const FGuid PropertyID) const override
	{
		return State.IsParametersPropertyOverridden(PropertyID);
	}
		
	virtual void SetPropertyOverride(const FGuid PropertyID, const bool bIsOverridden) const override
	{
		State.SetParametersPropertyOverridden(PropertyID, bIsOverridden);
	}

private:
	UMetaStoryState& State;
};

//----------------------------------------------------------------//
// FMetaStoryStateParametersInstanceDataDetails
//----------------------------------------------------------------//

FMetaStoryStateParametersInstanceDataDetails::FMetaStoryStateParametersInstanceDataDetails(
	const TSharedPtr<IPropertyHandle>& InStructProperty,
	const TSharedPtr<IPropertyHandle>& InParametersStructProperty,
	const TSharedPtr<IPropertyUtilities>& InPropUtils,
	const bool bInFixedLayout,
	FGuid InID,
	TWeakObjectPtr<UMetaStoryEditorData> InEditorData,
	TWeakObjectPtr<UMetaStoryState> InState)
	: FPropertyBagInstanceDataDetails(InParametersStructProperty, InPropUtils, bInFixedLayout)
	, StructProperty(InStructProperty)
	, WeakEditorData(InEditorData)
	, WeakState(InState)
	, ID(InID)
{
}
	
void FMetaStoryStateParametersInstanceDataDetails::OnChildRowAdded(IDetailPropertyRow& ChildRow)
{
	FPropertyBagInstanceDataDetails::OnChildRowAdded(ChildRow);

	EMetaStoryStateType Type = EMetaStoryStateType::State;
	if (const UMetaStoryState* State = WeakState.Get())
	{
		Type = State->Type;
	}

	// Subtree parameters cannot be bound to, they are provided from the linked state.
	const bool bAllowBinding = Type != EMetaStoryStateType::Subtree && ID.IsValid(); 

	if (bAllowBinding)
	{
		const TSharedPtr<IPropertyHandle> ChildPropHandle = ChildRow.GetPropertyHandle();
		const FProperty* Property = ChildPropHandle->GetProperty();

		// Set the category to Parameter so that the binding extension will pick it up.
		static const FName CategoryName(TEXT("Category"));
		ChildPropHandle->SetInstanceMetaData(CategoryName, TEXT("Parameter"));
	
		// Conditionally control visibility of the value field of bound properties.

		// Pass the node ID to binding extension. Since the properties are added using AddChildStructure(), we break the hierarchy and cannot access parent.
		ChildPropHandle->SetInstanceMetaData(UE::PropertyBinding::MetaDataStructIDName, LexToString(ID));

		FPropertyBindingPath Path(ID, *Property->GetFName().ToString());
		
		const auto IsValueVisible = TAttribute<EVisibility>::Create([Path, WeakEditorData = WeakEditorData]() -> EVisibility
			{
				bool bHasBinding = false;
				if (UMetaStoryEditorData* EditorData = WeakEditorData.Get())
				{
					if (const FMetaStoryEditorPropertyBindings* EditorPropBindings = EditorData->GetPropertyEditorBindings())
					{
						bHasBinding = EditorPropBindings->HasBinding(Path); 
					}
				}

				return bHasBinding ? EVisibility::Collapsed : EVisibility::Visible;
			});

		FDetailWidgetDecl* ValueWidgetDecl = ChildRow.CustomValueWidget();
		const TSharedRef<SBox> WrappedValueWidget = SNew(SBox)
			.Visibility(IsValueVisible)
			[
				ValueWidgetDecl->Widget
			];
		ValueWidgetDecl->Widget = WrappedValueWidget;
	}
}

bool FMetaStoryStateParametersInstanceDataDetails::HasPropertyOverrides() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		return State->Type == EMetaStoryStateType::Linked || State->Type == EMetaStoryStateType::LinkedAsset;
	}
	return false;
}

void FMetaStoryStateParametersInstanceDataDetails::PreChangeOverrides()
{
	check(StructProperty);
	StructProperty->NotifyPreChange();
}

void FMetaStoryStateParametersInstanceDataDetails::PostChangeOverrides()
{
	check(StructProperty);
	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();
}

void FMetaStoryStateParametersInstanceDataDetails::EnumeratePropertyBags(TSharedPtr<IPropertyHandle> PropertyBagHandle, const EnumeratePropertyBagFuncRef& Func) const
{
	if (UMetaStoryState* State = WeakState.Get())
	{
		if (const FInstancedPropertyBag* DefaultParameters = State->GetDefaultParameters())
		{
			FInstancedPropertyBag& Parameters = State->Parameters.Parameters;
			FMetaStoryStateOverrideProvider OverrideProvider(*State);
			Func(*DefaultParameters, Parameters, OverrideProvider);
		}
	}
}


//----------------------------------------------------------------//
// FMetaStoryStateParametersDetails
//----------------------------------------------------------------//

TSharedRef<IPropertyTypeCustomization> FMetaStoryStateParametersDetails::MakeInstance()
{
	return MakeShareable(new FMetaStoryStateParametersDetails);
}

void FMetaStoryStateParametersDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities();

	ParametersProperty = StructProperty->GetChildHandle(TEXT("Parameters"));
	FixedLayoutProperty = StructProperty->GetChildHandle(TEXT("bFixedLayout"));
	IDProperty = StructProperty->GetChildHandle(TEXT("ID"));
	check(ParametersProperty.IsValid());
	check(FixedLayoutProperty.IsValid());
	check(IDProperty.IsValid());

	FindOuterObjects();
	
	bFixedLayout = false;
	FixedLayoutProperty->GetValue(bFixedLayout);

	TSharedPtr<SWidget> ValueWidget = SNullWidget::NullWidget;
	if (!bFixedLayout)
	{
		ValueWidget = FPropertyBagDetails::MakeAddPropertyWidget(ParametersProperty, PropUtils);
	}
	
	HeaderRow
		.NameContent()
		[
			ParametersProperty->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			ValueWidget.ToSharedRef()
		]
		.ShouldAutoExpand(true);
}

void FMetaStoryStateParametersDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FGuid ID;
	UE::MetaStory::PropertyHelpers::GetStructValue<FGuid>(IDProperty, ID);

	// Show the Value (FInstancedStruct) as child rows.
	TSharedRef<FMetaStoryStateParametersInstanceDataDetails> InstanceDetails = MakeShareable(new FMetaStoryStateParametersInstanceDataDetails(StructProperty, ParametersProperty, PropUtils, bFixedLayout, ID, WeakEditorData, WeakState));
	StructBuilder.AddCustomBuilder(InstanceDetails);
}

void FMetaStoryStateParametersDetails::FindOuterObjects()
{
	check(StructProperty);
	
	WeakEditorData = nullptr;
	WeakStateTree = nullptr;
	WeakState = nullptr;

	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (UObject* Outer : OuterObjects)
	{
		UMetaStoryState* OuterState = Cast<UMetaStoryState>(Outer);
		UMetaStoryEditorData* OuterEditorData = Outer->GetTypedOuter<UMetaStoryEditorData>();
		UMetaStory* OuterStateTree = OuterEditorData ? OuterEditorData->GetTypedOuter<UMetaStory>() : nullptr;
		if (OuterEditorData && OuterStateTree && OuterState)
		{
			WeakStateTree = OuterStateTree;
			WeakEditorData = OuterEditorData;
			WeakState = OuterState;
			break;
		}
	}
}


#undef LOCTEXT_NAMESPACE
