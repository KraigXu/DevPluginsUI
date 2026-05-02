// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryStateDetails.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "IPropertyUtilities.h"
#include "PropertyBagDetails.h"
#include "PropertyRestriction.h"
#include "MetaStory.h"
#include "MetaStoryEditingSubsystem.h"
#include "MetaStoryEditor.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorNodeUtils.h"
#include "MetaStoryEditorStyle.h"
#include "MetaStoryEditorDataExtension.h"
#include "MetaStoryPropertyHelpers.h"
#include "MetaStorySchema.h"
#include "MetaStoryStateParametersDetails.h"
#include "Debugger/MetaStoryDebuggerUIExtensions.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"


TSharedRef<IDetailCustomization> FMetaStoryStateDetails::MakeInstance()
{
	return MakeShareable(new FMetaStoryStateDetails);
}

void FMetaStoryStateDetails::PendingDelete()
{
	Super::PendingDelete();
	bIsPendingDelete = true;
}

void FMetaStoryStateDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const TSharedRef<IPropertyUtilities> PropUtils = DetailBuilder.GetPropertyUtilities();
	WeakPropertyUtilities = PropUtils.ToWeakPtr();

	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	// Find MetaStoryEditorData associated with this panel.
	UMetaStoryEditorData* EditorData = nullptr;
	for (TWeakObjectPtr<UObject>& WeakObject : Objects)
	{
		if (UObject* Object = WeakObject.Get())
		{
			if (UMetaStoryEditorData* OuterEditorData = Object->GetTypedOuter<UMetaStoryEditorData>())
			{
				EditorData = OuterEditorData;
				break;
			}
		}
	}

	// Find MetaStoryState associated with this panel.
	TWeakObjectPtr<UMetaStoryState> WeakState;
	for (TWeakObjectPtr<UObject>& WeakObject : Objects)
	{
		if (UObject* Object = WeakObject.Get())
		{
			if (UMetaStoryState* State = Cast<UMetaStoryState>(Object))
			{
				WeakState = State;
				break;
			}
		}
	}

	const UMetaStorySchema* Schema = EditorData ? EditorData->Schema.Get() : nullptr;
	const FString SchemaPath = Schema ? Schema->GetClass()->GetPathName() : FString();
	TWeakObjectPtr<UMetaStoryEditorData> WeakEditorData = EditorData;

	TSharedPtr<FMetaStoryViewModel> ViewModel;
	if (UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = EditorData != nullptr ? GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>() : nullptr)
	{
		ViewModel = MetaStoryEditingSubsystem->FindOrAddViewModel(EditorData->GetTypedOuter<UMetaStory>());
	}

	const TSharedPtr<IPropertyHandle> IDProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, ID));
	const TSharedPtr<IPropertyHandle> NameProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, Name));
	const TSharedPtr<IPropertyHandle> TagProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, Tag));
	const TSharedPtr<IPropertyHandle> ColorRefProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, ColorRef));
	const TSharedPtr<IPropertyHandle> EnabledProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, bEnabled));
	const TSharedPtr<IPropertyHandle> TasksProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, Tasks));
	const TSharedPtr<IPropertyHandle> SingleTaskProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, SingleTask));
	const TSharedPtr<IPropertyHandle> EnterConditionsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, EnterConditions));
	const TSharedPtr<IPropertyHandle> ConsiderationsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, Considerations));
	const TSharedPtr<IPropertyHandle> TransitionsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, Transitions));
	const TSharedPtr<IPropertyHandle> TypeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, Type));
	const TSharedPtr<IPropertyHandle> LinkedSubtreeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, LinkedSubtree));
	const TSharedPtr<IPropertyHandle> LinkedAssetProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, LinkedAsset));
	const TSharedPtr<IPropertyHandle> CustomTickRateProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, CustomTickRate));
	const TSharedPtr<IPropertyHandle> ParametersProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, Parameters));
	const TSharedPtr<IPropertyHandle> SelectionBehaviorProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, SelectionBehavior));
	const TSharedPtr<IPropertyHandle> TasksCompletionProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, TasksCompletion));
	const TSharedPtr<IPropertyHandle> RequiredEventToEnterProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, RequiredEventToEnter));
	const TSharedPtr<IPropertyHandle> CheckPrerequisitesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, bCheckPrerequisitesWhenActivatingChildDirectly));
	const TSharedPtr<IPropertyHandle> WeightProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaStoryState, Weight));

	// Never show enabled
	EnabledProperty->MarkHiddenByCustomization();

	// Show ID only for debugging
	if (UE::MetaStory::Editor::GbDisplayItemIds == false)
	{
		IDProperty->MarkHiddenByCustomization();
	}

	uint8 StateTypeValue = 0;
	TypeProperty->GetValue(StateTypeValue);
	const EMetaStoryStateType StateType = (EMetaStoryStateType)StateTypeValue;

	IDetailCategoryBuilder& StateCategory = DetailBuilder.EditCategory(TEXT("State"), LOCTEXT("StateDetailsState", "State"));
	StateCategory.SetSortOrder(0);
	{
		TSharedRef<SHorizontalBox> HeaderContent = SNew(SHorizontalBox)
			.IsEnabled(PropUtils, &IPropertyUtilities::IsPropertyEditingEnabled)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				// Debugger labels
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					UE::MetaStoryEditor::DebuggerExtensions::CreateStateWidget(EnabledProperty, ViewModel)
				]
				// Options
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnGetMenuContent_Lambda([EnabledProperty, ViewModel]()
					{
						FMenuBuilder MenuBuilder(/*ShouldCloseWindowAfterMenuSelection*/true, /*CommandList*/nullptr);
						// Append debugger items.
						UE::MetaStoryEditor::DebuggerExtensions::AppendStateMenuItems(MenuBuilder, EnabledProperty, ViewModel);
						return MenuBuilder.MakeWidget();
					})
					.ToolTipText(LOCTEXT("ItemActions", "Item actions"))
					.HasDownArrow(false)
					.ContentPadding(FMargin(4.f, 2.f))
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.ChevronDown"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			];
		StateCategory.HeaderContent(HeaderContent);
	}

	// Name
	NameProperty->MarkHiddenByCustomization();
	StateCategory.AddProperty(NameProperty);

	// Tag
	TagProperty->MarkHiddenByCustomization();
	StateCategory.AddProperty(TagProperty);

	// Color
	ColorRefProperty->MarkHiddenByCustomization();
	StateCategory.AddProperty(ColorRefProperty);

	// Custom Tick Rate
	CustomTickRateProperty->MarkHiddenByCustomization();
	if (Schema && Schema->IsScheduledTickAllowed())
	{
		StateCategory.AddProperty(CustomTickRateProperty);
	}

	// Type
	TypeProperty->MarkHiddenByCustomization();
	if (Schema)
	{
		// Restrict IsStateTypeAllowed
		TSharedRef<FPropertyRestriction> SchemaSupported = MakeShared<FPropertyRestriction>(LOCTEXT("StateTypeAllowedRestriction", "The schema restricts the state type."));
		const UEnum* EnumPtr = StaticEnum<EMetaStoryStateType>();
		for (int32 Index = 0; Index < EnumPtr->NumEnums(); ++Index)
		{
			const EMetaStoryStateType EnumValue = static_cast<EMetaStoryStateType>(EnumPtr->GetValueByIndex(Index));
			if (!Schema->IsStateTypeAllowed(EnumValue))
			{
				SchemaSupported->AddHiddenValue(EnumPtr->GetNameStringByIndex(Index));
			}
		}
		TypeProperty->AddRestriction(SchemaSupported);
	}
	StateCategory.AddProperty(TypeProperty);

	// Per state type properties
	SelectionBehaviorProperty->MarkHiddenByCustomization();
	TasksCompletionProperty->MarkHiddenByCustomization();
	LinkedSubtreeProperty->MarkHiddenByCustomization();
	LinkedAssetProperty->MarkHiddenByCustomization();

	if (StateType == EMetaStoryStateType::State || StateType == EMetaStoryStateType::Subtree)
	{
		// Restrict IsStateSelectionAllowed
		if (Schema)
		{
			TSharedRef<FPropertyRestriction> SchemaSupported = MakeShared<FPropertyRestriction>(LOCTEXT("StateSelectionAllowedRestriction", "The schema restricts the selection behavior."));
			const UEnum* EnumPtr = StaticEnum<EMetaStoryStateSelectionBehavior>();
			for (int32 Index = 0; Index < EnumPtr->NumEnums(); ++Index)
			{
				const EMetaStoryStateSelectionBehavior EnumValue = static_cast<EMetaStoryStateSelectionBehavior>(EnumPtr->GetValueByIndex(Index));
				if (!Schema->IsStateSelectionAllowed(EnumValue))
				{
					SchemaSupported->AddHiddenValue(EnumPtr->GetNameStringByIndex(Index));
				}
			}
			SelectionBehaviorProperty->AddRestriction(SchemaSupported);
		}
		StateCategory.AddProperty(SelectionBehaviorProperty);
	}
	else if (StateType == EMetaStoryStateType::Linked)
	{
		StateCategory.AddProperty(LinkedSubtreeProperty);
	}
	else if (StateType == EMetaStoryStateType::LinkedAsset)
	{
		// Custom widget for the linked asset, to add filtering to the assets.
		IDetailPropertyRow& Row = StateCategory.AddProperty(LinkedAssetProperty);
		Row.CustomWidget()
		.NameContent()
		[
			LinkedAssetProperty->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SObjectPropertyEntryBox)
			.PropertyHandle(LinkedAssetProperty)
			.AllowedClass(UMetaStory::StaticClass())
			.ThumbnailPool(DetailBuilder.GetPropertyUtilities()->GetThumbnailPool())
			.OnShouldFilterAsset_Lambda([SchemaPath](const FAssetData& InAssetData)
			{
				return !SchemaPath.IsEmpty() && !InAssetData.TagsAndValues.ContainsKeyValue(UE::MetaStory::SchemaTag, SchemaPath);
			})
		];
	}

	// Parameters category
	const FText ParametersDisplayName = LOCTEXT("EditorStateDetailsParameters", "Parameters");
	IDetailCategoryBuilder& ParametersCategory = DetailBuilder.EditCategory(TEXT("Parameters"), ParametersDisplayName);
	ParametersCategory.SetSortOrder(1);
	{
		// Show parameters as a category.
		ParametersProperty->MarkHiddenByCustomization();

		TSharedPtr<IPropertyHandle> ParametersParametersProperty = ParametersProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryStateParameters, Parameters)); // FInstancedPropertyBag
		check(ParametersParametersProperty);
		TSharedPtr<IPropertyHandle> ParametersFixedLayoutProperty = ParametersProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryStateParameters, bFixedLayout));
		check(ParametersFixedLayoutProperty);
		TSharedPtr<IPropertyHandle> ParametersIDProperty = ParametersProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryStateParameters, ID));
		check(ParametersIDProperty);

		bool bFixedLayout = false;
		ParametersFixedLayoutProperty->GetValue(bFixedLayout);

		const TSharedRef<SHorizontalBox> HeaderContentWidget = SNew(SHorizontalBox)
			.IsEnabled(PropUtils, &IPropertyUtilities::IsPropertyEditingEnabled);

		HeaderContentWidget->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
			[
				SNew(SImage)
					.ColorAndOpacity(UE::MetaStory::Colors::Blue)
					.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Parameters"))
			];
		HeaderContentWidget->AddSlot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Category")
				.Text(ParametersDisplayName)
			];

		if (!bFixedLayout)
		{
			HeaderContentWidget->AddSlot()
				.FillWidth(1.f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					FPropertyBagDetails::MakeAddPropertyWidget(
						ParametersParametersProperty,
						PropUtils,
						EPropertyBagPropertyType::Bool,
						FLinearColor(UE::MetaStory::Colors::Blue)).ToSharedRef()
				];
		}

		ParametersCategory.HeaderContent(HeaderContentWidget, /*FullRowContent*/true);

		FGuid ID;
		UE::MetaStory::PropertyHelpers::GetStructValue<FGuid>(ParametersIDProperty, ID);

		// Show the Value (FInstancedStruct) as child rows.
		TSharedRef<FMetaStoryStateParametersInstanceDataDetails> InstanceDetails =
			MakeShareable(new FMetaStoryStateParametersInstanceDataDetails(ParametersProperty, ParametersProperty, PropUtils, bFixedLayout, ID, WeakEditorData, WeakState));
		ParametersCategory.AddCustomBuilder(InstanceDetails);
	}

	// Enter conditions
	const FName EnterConditionsCategoryName(TEXT("Enter Conditions"));
	if (Schema && Schema->AllowEnterConditions())
	{

		IDetailCategoryBuilder& EnterConditionsCategory = UE::MetaStoryEditor::EditorNodeUtils::MakeArrayCategory(
			DetailBuilder,
			EnterConditionsProperty,
			EnterConditionsCategoryName,
			LOCTEXT("StateDetailsEnterConditions", "Enter Conditions"),
			FName("MetaStoryEditor.Conditions"),
			UE::MetaStory::Colors::Yellow,
			UE::MetaStory::Colors::Yellow.WithAlpha(192),
			LOCTEXT("EnterConditionsAddTooltip", "Add new Enter Condition"),
			/*SortOrder*/2);
		EnterConditionsProperty->MarkHiddenByCustomization();

		// Event
		RequiredEventToEnterProperty->MarkHiddenByCustomization();
		EnterConditionsCategory.AddProperty(RequiredEventToEnterProperty);

		// Check Prerequisites
		CheckPrerequisitesProperty->MarkHiddenByCustomization();
		EnterConditionsCategory.AddProperty(CheckPrerequisitesProperty);
	}
	else
	{
		DetailBuilder.EditCategory(EnterConditionsCategoryName).SetCategoryVisibility(false);
	}

	// Utility
	WeightProperty->MarkHiddenByCustomization();
	ConsiderationsProperty->MarkHiddenByCustomization();
	if (Schema && Schema->AllowUtilityConsiderations())
	{
		const FName UtilityCategoryName(TEXT("Selection Utility"));
		IDetailCategoryBuilder& UtilityConsiderationsCategory = UE::MetaStoryEditor::EditorNodeUtils::MakeArrayCategory(
			DetailBuilder,
			ConsiderationsProperty,
			UtilityCategoryName,
			LOCTEXT("StateDetailsSelectionUtility", "Selection Utility"),
			FName("MetaStoryEditor.Utility"),
			UE::MetaStory::Colors::Orange,
			UE::MetaStory::Colors::Orange.WithAlpha(192),
			LOCTEXT("UtilityAddTooltip", "Add new Utility Consideration"),
			/*SortOrder*/3);

		// Weight
		UtilityConsiderationsCategory.AddProperty(WeightProperty);
	}

	// Tasks
	if ((StateType == EMetaStoryStateType::State || StateType == EMetaStoryStateType::Subtree))
	{
		if (Schema && Schema->AllowMultipleTasks())
		{
			const bool bAllowTasksCompletion = Schema->AllowTasksCompletion();
			const FName TasksCategoryName(TEXT("Tasks"));
			IDetailCategoryBuilder& TasksCategory = UE::MetaStoryEditor::EditorNodeUtils::MakeArrayCategoryHeader(
				DetailBuilder,
				TasksProperty,
				TasksCategoryName,
				LOCTEXT("StateDetailsTasks", "Tasks"),
				FName("MetaStoryEditor.Tasks"),
				UE::MetaStory::Colors::Cyan,
				bAllowTasksCompletion ? TasksCompletionProperty->CreatePropertyValueWidget(/*bDisplayDefaultPropertyButtons*/false) : TSharedPtr<SWidget>(),
				UE::MetaStory::Colors::Cyan.WithAlpha(192),
				LOCTEXT("StateDetailsTasksAddTooltip", "Add new Task"),
				/*SortOrder*/4);
			SingleTaskProperty->MarkHiddenByCustomization();
			UE::MetaStoryEditor::EditorNodeUtils::MakeArrayItems(TasksCategory, TasksProperty);
		}
		else
		{
			const FName TaskCategoryName(TEXT("Task"));
			IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TaskCategoryName);
			Category.SetSortOrder(4);

			IDetailPropertyRow& Row = Category.AddProperty(SingleTaskProperty);
			Row.ShouldAutoExpand(true);

			TasksProperty->MarkHiddenByCustomization();
		}
	}
	else
	{
		SingleTaskProperty->MarkHiddenByCustomization();
		TasksProperty->MarkHiddenByCustomization();
	}

	// Transitions
	UE::MetaStoryEditor::EditorNodeUtils::MakeArrayCategory(
		DetailBuilder,
		TransitionsProperty,
		"Transitions",
		LOCTEXT("StateDetailsTransitions", "Transitions"),
		FName("MetaStoryEditor.Transitions"),
		UE::MetaStory::Colors::Magenta,
		UE::MetaStory::Colors::Magenta.WithAlpha(192),
		LOCTEXT("StateDetailsTransitionsAddTooltip", "Add new Transition"),
		/*SortOrder*/5);

	// Refresh the UI when the type changes.
	TypeProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([WeakPropertyUtilities = WeakPropertyUtilities]
		{
			if (const TSharedPtr<IPropertyUtilities> PropertyUtilities = WeakPropertyUtilities.Pin())
			{
				PropertyUtilities->ForceRefresh();
			}
		}));

	for (UMetaStoryEditorDataExtension* Extension : EditorData->Extensions)
	{
		if (Extension)
		{
			Extension->CustomizeDetails(WeakState.Get(), DetailBuilder);
		}
	}
}

void FMetaStoryStateDetails::PostUndo(bool bSuccess)
{
	// Prevent redundant calls to force refresh which doubles the number of DetailCustomization 
	if (bIsPendingDelete)
	{
		return;
	}

	// Refresh view on undo or redo so that the customization based on e.g. State type will be reflected correctly.
	if (const TSharedPtr<IPropertyUtilities> PropertyUtilities = WeakPropertyUtilities.Pin())
	{
		PropertyUtilities->ForceRefresh();
	}
}

void FMetaStoryStateDetails::PostRedo(bool bSuccess)
{
	// Prevent redundant calls to force refresh which doubles the number of DetailCustomization
	if (bIsPendingDelete)
	{
		return;
	}

	// Refresh view on undo or redo so that the customization based on e.g. State type will be reflected correctly.
	if (const TSharedPtr<IPropertyUtilities> PropertyUtilities = WeakPropertyUtilities.Pin())
	{
		PropertyUtilities->ForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
