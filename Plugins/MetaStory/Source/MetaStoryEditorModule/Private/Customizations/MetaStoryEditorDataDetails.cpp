// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorDataDetails.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PropertyBagDetails.h"
#include "PropertyCustomizationHelpers.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorNodeUtils.h"
#include "MetaStoryEditorSchema.h"
#include "MetaStoryEditorStyle.h"
#include "MetaStorySchema.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

TSharedRef<IDetailCustomization> FMetaStoryEditorDataDetails::MakeInstance()
{
	return MakeShareable(new FMetaStoryEditorDataDetails);
}

void FMetaStoryEditorDataDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const TSharedRef<IPropertyUtilities> PropUtils = DetailBuilder.GetPropertyUtilities();
	WeakPropertyUtilities = PropUtils.ToWeakPtr();

	// Find MetaStoryEditorData associated with this panel.
	const UMetaStoryEditorData* EditorData = nullptr;
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	for (TWeakObjectPtr<UObject>& WeakObject : Objects)
	{
		if (UMetaStoryEditorData* Object = Cast<UMetaStoryEditorData>(WeakObject.Get()))
		{
			EditorData = Object;
			break;
		}
	}
	const UMetaStorySchema* Schema = EditorData ? EditorData->Schema.Get() : nullptr;
	const UMetaStoryEditorSchema* EditorSchema = EditorData ? EditorData->EditorSchema.Get() : nullptr;

	// Common category
	IDetailCategoryBuilder& CommonCategory = DetailBuilder.EditCategory(TEXT("Common"), LOCTEXT("EditorDataDetailsCommon", "Common"));
	CommonCategory.SetSortOrder(0);

	// Context category
	IDetailCategoryBuilder& ContextDataCategory = DetailBuilder.EditCategory(TEXT("Context"), LOCTEXT("EditorDataDetailsContext", "Context"));
	ContextDataCategory.SetSortOrder(1);

	// Theme category
	IDetailCategoryBuilder& ThemeCategory = DetailBuilder.EditCategory(TEXT("Theme"));
	ThemeCategory.InitiallyCollapsed(true);

	if (Schema != nullptr)
	{
		for (const FMetaStoryExternalDataDesc& ContextData : Schema->GetContextDataDescs())
		{
			if (ContextData.Struct == nullptr)
			{
				continue;
			}
			
			FEdGraphPinType PinType;
			PinType.PinSubCategory = NAME_None;
			if (ContextData.Struct->IsA<UScriptStruct>())
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			}
			else if (ContextData.Struct->IsA<UClass>())
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			}
			else
			{
				continue;
			}
			PinType.PinSubCategoryObject = const_cast<UStruct*>(ContextData.Struct.Get());

			const UEdGraphSchema_K2* EdGraphSchema = GetDefault<UEdGraphSchema_K2>();
			const FSlateBrush* Icon = FBlueprintEditorUtils::GetIconFromPin(PinType, true);
			const FLinearColor Color = EdGraphSchema->GetPinTypeColor(PinType);

			const FText DataName = FText::FromName(ContextData.Name);
			const FText DataType = ContextData.Struct != nullptr ? ContextData.Struct->GetDisplayNameText() : FText::GetEmpty();
			
			ContextDataCategory.AddCustomRow(DataName)
				.NameContent()
				[
					SNew(SHorizontalBox)
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(DataName)
					]
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						SNew(SBorder)
						.Padding(FMargin(6, 1))
						.BorderImage(FMetaStoryEditorStyle::Get().GetBrush("MetaStory.Param.Background"))
						[
							SNew(STextBlock)
							.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Param.Label")
							.ColorAndOpacity(FStyleColors::Foreground)
							.Text(LOCTEXT("LabelContext", "CONTEXT"))
							.ToolTipText(LOCTEXT("ContextSourceTooltip", "This is Context Object, it passed in from where the MetaStory is being used."))
						]
					]
				]
				.ValueContent()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						SNew(SImage)
						.Image(Icon)
						.ColorAndOpacity(Color)
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(DataType)
					]
				];
		}
	}

	if (Schema && Schema->AllowGlobalParameters())
	{
		// Parameters category
		const FText ParametersDisplayName = LOCTEXT("EditorDataDetailsParameters", "Parameters");
		IDetailCategoryBuilder& ParametersCategory = DetailBuilder.EditCategory(TEXT("Parameters"), ParametersDisplayName);
		ParametersCategory.SetSortOrder(2);
		{
			// Show parameters as a category.
			TSharedPtr<IPropertyHandle> PropertyBagParametersProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, RootParameterPropertyBag)); // FInstancedPropertyBag
			PropertyBagParametersProperty->MarkHiddenByCustomization();
			check(PropertyBagParametersProperty);

			const TSharedRef<SHorizontalBox> HeaderContentWidget = SNew(SHorizontalBox)
				.IsEnabled(PropUtils, &IPropertyUtilities::IsPropertyEditingEnabled)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
				[
					SNew(SImage)
						.ColorAndOpacity(UE::MetaStory::Colors::Blue)
						.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Parameters"))
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Category")
					.Text(ParametersDisplayName)
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					FPropertyBagDetails::MakeAddPropertyWidget(PropertyBagParametersProperty, PropUtils, EPropertyBagPropertyType::Bool, FLinearColor(UE::MetaStory::Colors::Blue)).ToSharedRef()
				];
			ParametersCategory.HeaderContent(HeaderContentWidget, /*FullRowContent*/true);

			TSharedRef<FPropertyBagInstanceDataDetails> InstanceDetails = MakeShareable(new FPropertyBagInstanceDataDetails(PropertyBagParametersProperty, PropUtils, false));
			ParametersCategory.AddCustomBuilder(InstanceDetails);
		}
	}
	else
	{
		TSharedPtr<IPropertyHandle> PropertyBagParametersProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, RootParameterPropertyBag)); // FInstancedPropertyBag
		PropertyBagParametersProperty->MarkHiddenByCustomization();
	}	

	// Evaluators category
	TSharedPtr<IPropertyHandle> EvaluatorsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, Evaluators));
	check(EvaluatorsProperty.IsValid());
	const FName EvalCategoryName(TEXT("Evaluators"));
	if (Schema && Schema->AllowEvaluators())
	{
		UE::MetaStoryEditor::EditorNodeUtils::MakeArrayCategory(
			DetailBuilder,
			EvaluatorsProperty,
			EvalCategoryName,
			LOCTEXT("EditorDataDetailsEvaluators", "Evaluators"),
			FName("MetaStoryEditor.Evaluators"),
			UE::MetaStory::Colors::Bronze,
			UE::MetaStory::Colors::Bronze.WithAlpha(192),
			LOCTEXT("EditorDataDetailsEvaluatorsAddTooltip", "Add new Evaluator"),
			/*SortOrder*/3);
	}
	else
	{
		DetailBuilder.EditCategory(EvalCategoryName).SetCategoryVisibility(false);
	}

	// Global Tasks category
	TSharedPtr<IPropertyHandle> GlobalTasksProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, GlobalTasks));
	check(GlobalTasksProperty.IsValid());
	const FName GlobalTasksCategoryName(TEXT("Global Tasks"));

	const bool bAllowTasksCompletion = Schema && Schema->AllowTasksCompletion();
	TSharedPtr<IPropertyHandle> GlobalTasksCompletionProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, GlobalTasksCompletion));
	GlobalTasksCompletionProperty->MarkHiddenByCustomization();

	IDetailCategoryBuilder& GlobalTasksCategory = UE::MetaStoryEditor::EditorNodeUtils::MakeArrayCategoryHeader(
		DetailBuilder,
		GlobalTasksProperty,
		GlobalTasksCategoryName,
		LOCTEXT("EditorDataDetailsGlobalTasks", "Global Tasks"),
		FName("MetaStoryEditor.Tasks"),
		UE::MetaStory::Colors::Cyan,
		bAllowTasksCompletion ? GlobalTasksCompletionProperty->CreatePropertyValueWidget(/*bDisplayDefaultPropertyButtons*/false) : TSharedPtr<SWidget>(),
		UE::MetaStory::Colors::Cyan.WithAlpha(192),
		LOCTEXT("EditorDataDetailsGlobalTasksAddTooltip", "Add new Global Task"),
		/*SortOrder*/4);
	UE::MetaStoryEditor::EditorNodeUtils::MakeArrayItems(GlobalTasksCategory, GlobalTasksProperty);

	// Extensions
	if (EditorSchema && !EditorSchema->AllowExtensions())
	{
		TSharedPtr<IPropertyHandle> ExtensionsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, Extensions));
		check(ExtensionsProperty.IsValid());
		DetailBuilder.EditCategory(ExtensionsProperty->GetDefaultCategoryName()).SetCategoryVisibility(false);
	}

	// Refresh the UI when the Schema changes.
	TSharedPtr<IPropertyHandle> SchemaProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryEditorData, Schema));
	check(SchemaProperty.IsValid());
	SchemaProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([WeakPropertyUtilities = WeakPropertyUtilities] ()
		{
			if (const TSharedPtr<IPropertyUtilities> PropertyUtilities = WeakPropertyUtilities.Pin())
			{
				PropertyUtilities->ForceRefresh();
			}
		}));
	SchemaProperty->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda([WeakPropertyUtilities = WeakPropertyUtilities] ()
		{
			if (const TSharedPtr<IPropertyUtilities> PropertyUtilities = WeakPropertyUtilities.Pin())
			{
				PropertyUtilities->ForceRefresh();
			}
		}));
}

void FMetaStoryEditorDataDetails::PendingDelete()
{
	Super::PendingDelete();
	bPendingDelete = true;
}

void FMetaStoryEditorDataDetails::MakeArrayCategory(IDetailLayoutBuilder& DetailBuilder, FName CategoryName, const FText& DisplayName, int32 SortOrder, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, DisplayName);
	Category.SetSortOrder(SortOrder);

	TSharedRef<SHorizontalBox> HeaderContentWidget = SNew(SHorizontalBox)
		.IsEnabled(DetailBuilder.GetPropertyUtilities(), &IPropertyUtilities::IsPropertyEditingEnabled);

	HeaderContentWidget->AddSlot()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Center)
	[
		PropertyHandle->CreateDefaultPropertyButtonWidgets()
	];
	Category.HeaderContent(HeaderContentWidget);

	// Add items inline
	TSharedRef<FDetailArrayBuilder> Builder = MakeShareable(new FDetailArrayBuilder(PropertyHandle.ToSharedRef(), /*InGenerateHeader*/ false, /*InDisplayResetToDefault*/ true, /*InDisplayElementNum*/ false));
	Builder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda([](TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
		{
			ChildrenBuilder.AddProperty(PropertyHandle);
		}));
	Category.AddCustomBuilder(Builder, /*bForAdvanced*/ false);
}

void FMetaStoryEditorDataDetails::PostUndo(bool bSuccess)
{
	if (bPendingDelete)
	{
		return;
	}

	// Refresh view on undo or redo so that the customization based on e.g. Global tasks bindings, parameters, etc. will be reflected correctly.
	if (const TSharedPtr<IPropertyUtilities> PropertyUtilities = WeakPropertyUtilities.Pin())
	{
		PropertyUtilities->ForceRefresh();
	}
}

void FMetaStoryEditorDataDetails::PostRedo(bool bSuccess)
{
	if (bPendingDelete)
	{
		return;
	}

	// Refresh view on undo or redo so that the customization based on e.g. Global tasks bindings, parameters, etc. will be reflected correctly.
	if (const TSharedPtr<IPropertyUtilities> PropertyUtilities = WeakPropertyUtilities.Pin())
	{
		PropertyUtilities->ForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
