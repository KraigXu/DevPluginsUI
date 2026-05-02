// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryBlueprintPropertyRefDetails.h"
#include "DetailWidgetRow.h"
#include "SPinTypeSelector.h"
#include "MetaStoryPropertyRef.h"
#include "MetaStoryEditorNode.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryBlueprintPropertyRefDetails)

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

namespace UE::MetaStory::BlueprintPropertyRef
{
	static bool IsInMetaStoryNode(TSharedRef<IPropertyHandle> PropertyHandle)
	{
		TSharedPtr<IPropertyHandle> TestPropertyHandle = PropertyHandle->GetParentHandle();
		while (TestPropertyHandle)
		{
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(TestPropertyHandle->GetProperty()))
			{
				if (StructProperty->Struct == FMetaStoryEditorNode::StaticStruct())
				{
					return true;
				}
			}

			TestPropertyHandle = TestPropertyHandle->GetParentHandle();
		}

		return false;
	}
}

TSharedRef<IPropertyTypeCustomization> FMetaStoryBlueprintPropertyRefDetails::MakeInstance()
{
	return MakeShared<FMetaStoryBlueprintPropertyRefDetails>();
}

void FMetaStoryBlueprintPropertyRefDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	// Don't allow to modify property ref internal type if it's a part of MetaStory node.
	if (UE::MetaStory::BlueprintPropertyRef::IsInMetaStoryNode(InPropertyHandle))
	{
		HeaderRow.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget()
		];

		return;
	}

	auto GetPinInfo = [WeakPropertyHandle = TWeakPtr<IPropertyHandle>(InPropertyHandle)]()
	{
		FEdGraphPinType Result;
		if (TSharedPtr<IPropertyHandle> PropertyHandle = WeakPropertyHandle.Pin())
		{
			TSharedPtr<IPropertyHandle> RefTypePropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STRING_CHECKED(FMetaStoryBlueprintPropertyRef, RefType));
			TSharedPtr<IPropertyHandle> IsRefToArrayPropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STRING_CHECKED(FMetaStoryBlueprintPropertyRef, bIsRefToArray));
			TSharedPtr<IPropertyHandle> TypeObjectPropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STRING_CHECKED(FMetaStoryBlueprintPropertyRef, TypeObject));

			if (RefTypePropertyHandle && IsRefToArrayPropertyHandle && TypeObjectPropertyHandle)
			{
				uint8 RefType = 0u;
				bool bIsRefToArray = false;
				UObject* TypeObject = nullptr;

				RefTypePropertyHandle->GetValue(RefType);
				IsRefToArrayPropertyHandle->GetValue(bIsRefToArray);
				TypeObjectPropertyHandle->GetValue(TypeObject);

				FMetaStoryBlueprintPropertyRef PropertyRef;
				PropertyRef.RefType = static_cast<EMetaStoryPropertyRefType>(RefType);
				PropertyRef.bIsRefToArray = bIsRefToArray;
				PropertyRef.TypeObject = TypeObject;
				Result = UE::MetaStory::PropertyRefHelpers::GetBlueprintPropertyRefInternalTypeAsPin(PropertyRef);
			}	
		}

		return Result;
	};

	auto PinInfoChanged = [WeakPropertyHandle = TWeakPtr<IPropertyHandle>(InPropertyHandle)](const FEdGraphPinType& PinType)
	{
		if (TSharedPtr<IPropertyHandle> PropertyHandle = WeakPropertyHandle.Pin())
		{
			EMetaStoryPropertyRefType RefType;
			bool bIsRefToArray = false;
			UObject* TypeObject = nullptr;

			UE::MetaStory::PropertyRefHelpers::GetBlueprintPropertyRefInternalTypeFromPin(PinType, RefType, bIsRefToArray, TypeObject);

			TSharedPtr<IPropertyHandle> RefTypePropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STRING_CHECKED(FMetaStoryBlueprintPropertyRef, RefType));
			TSharedPtr<IPropertyHandle> IsRefToArrayPropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STRING_CHECKED(FMetaStoryBlueprintPropertyRef, bIsRefToArray));
			TSharedPtr<IPropertyHandle> TypeObjectPropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STRING_CHECKED(FMetaStoryBlueprintPropertyRef, TypeObject));

			if (RefTypePropertyHandle && IsRefToArrayPropertyHandle && TypeObjectPropertyHandle)
			{
				RefTypePropertyHandle->SetValue(static_cast<uint8>(RefType));
				IsRefToArrayPropertyHandle->SetValue(bIsRefToArray);
				TypeObjectPropertyHandle->SetValue(TypeObject);
			}
		}
	};

	HeaderRow.NameContent()
	[
		InPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(FMargin(0, 0, 6, 0))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ReferenceTo", "Reference to"))
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.ToolTipText(LOCTEXT("ReferenceTo_Tooltip", "Specifies the type of the referenced property. The referenced property is bound using property binding in the MetaStory."))
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(GetDefault<UMetaStoryPropertyRefSchema>(), &UEdGraphSchema_K2::GetVariableTypeTree))
				.OnPinTypeChanged_Lambda(PinInfoChanged)
				.TargetPinType_Lambda(GetPinInfo)
				.Schema(GetDefault<UMetaStoryPropertyRefSchema>())
				.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
				.bAllowArrays(true)
		]
	];
}

void FMetaStoryBlueprintPropertyRefDetails::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	if(!UE::MetaStory::BlueprintPropertyRef::IsInMetaStoryNode(InPropertyHandle))
	{
		TSharedPtr<IPropertyHandle> IsOptionalPropertyHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_STRING_CHECKED(FMetaStoryBlueprintPropertyRef, bIsOptional));
		ChildBuilder.AddProperty(IsOptionalPropertyHandle.ToSharedRef());
	}
}

bool UMetaStoryPropertyRefSchema::SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction, const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const
{
	return ContainerType == EPinContainerType::None || ContainerType == EPinContainerType::Array;
}

#undef LOCTEXT_NAMESPACE
