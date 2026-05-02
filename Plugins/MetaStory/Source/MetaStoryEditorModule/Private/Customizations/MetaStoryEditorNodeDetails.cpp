// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorNodeDetails.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "IDetailChildrenBuilder.h"
#include "MetaStory.h"
#include "MetaStoryEditor.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorUserSettings.h"
#include "MetaStoryPropertyFunctionBase.h"
#include "MetaStoryPropertyRef.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SMetaStoryNodeTypePicker.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "InstancedStructDetails.h"
#include "MetaStoryBindingExtension.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryPropertyHelpers.h"
#include "MetaStoryEditorStyle.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Blueprint/MetaStoryEvaluatorBlueprintBase.h"
#include "Blueprint/MetaStoryTaskBlueprintBase.h"
#include "Blueprint/MetaStoryConditionBlueprintBase.h"
#include "Blueprint/MetaStoryConsiderationBlueprintBase.h"
#include "Styling/StyleColors.h"
#include "ScopedTransaction.h"
#include "MetaStoryEditorNodeUtils.h"
#include "Debugger/MetaStoryDebuggerUIExtensions.h"
#include "MetaStoryEditingSubsystem.h"
#include "MetaStoryPropertyBindings.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Styling/SlateTypes.h"
#include "TextStyleDecorator.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "SourceCodeNavigation.h"
#include "MetaStoryDelegate.h"
#include "MetaStoryEditorDataClipboardHelpers.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

namespace UE::MetaStoryEditor::Internal
{
	/* Returns true if provided property is direct or indirect child of PropertyFunction */
	bool IsOwnedByPropertyFunctionNode(TSharedPtr<IPropertyHandle> Property)
	{
		while (Property)
		{
			if (FStructProperty* StructProperty = CastField<FStructProperty>(Property->GetProperty()))
			{
				if (StructProperty->Struct == FMetaStoryEditorNode::StaticStruct())
				{
					if (const FMetaStoryEditorNode* Node = UE::MetaStoryEditor::EditorNodeUtils::GetCommonNode(Property))
					{
						if (const UScriptStruct* ScriptStruct = Node->Node.GetScriptStruct())
						{
							return ScriptStruct->IsChildOf<FMetaStoryPropertyFunctionBase>();
						}
					}
				}	
			}

			Property = Property->GetParentHandle();
		}

		return false;
	}

	/** @return text describing the pin type, matches SPinTypeSelector. */
	FText GetPinTypeText(const FEdGraphPinType& PinType)
	{
		const FName PinSubCategory = PinType.PinSubCategory;
		const UObject* PinSubCategoryObject = PinType.PinSubCategoryObject.Get();
		if (PinSubCategory != UEdGraphSchema_K2::PSC_Bitmask && PinSubCategoryObject)
		{
			if (const UField* Field = Cast<const UField>(PinSubCategoryObject))
			{
				return Field->GetDisplayNameText();
			}
			return FText::FromString(PinSubCategoryObject->GetName());
		}

		return UEdGraphSchema_K2::GetCategoryText(PinType.PinCategory, NAME_None, true);
	}

	/** @return if property is struct property of DelegateDispatcher type. */
	bool IsDelegateDispatcherProperty(const FProperty& Property)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			return StructProperty->Struct == FMetaStoryDelegateDispatcher::StaticStruct();
		}

		return false;
	}

	/** @return UClass or UScriptStruct of class or struct property, nullptr for others. */
	UStruct* GetPropertyStruct(TSharedPtr<IPropertyHandle> PropHandle)
	{
		if (!PropHandle.IsValid())
		{
			return nullptr;
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(PropHandle->GetProperty()))
		{
			return StructProperty->Struct;
		}
		
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PropHandle->GetProperty()))
		{
			return ObjectProperty->PropertyClass;
		}

		return nullptr;
	}

	void ModifyRow(IDetailPropertyRow& ChildRow, const FGuid& ID, UMetaStoryEditorData* EditorData)
	{
		FMetaStoryEditorPropertyBindings* EditorPropBindings = EditorData ? EditorData->GetPropertyEditorBindings() : nullptr;
		if (!EditorPropBindings)
		{
			return;
		}
		
		TSharedPtr<IPropertyHandle> ChildPropHandle = ChildRow.GetPropertyHandle();
		check(ChildPropHandle.IsValid());
		
		const EMetaStoryPropertyUsage Usage = UE::MetaStory::GetUsageFromMetaData(ChildPropHandle->GetProperty());
		const FProperty* Property = ChildPropHandle->GetProperty();
		
		// Hide output properties for PropertyFunctionNode.
		if (Usage == EMetaStoryPropertyUsage::Output && UE::MetaStoryEditor::Internal::IsOwnedByPropertyFunctionNode(ChildPropHandle))
		{
			ChildRow.Visibility(EVisibility::Hidden);
			return;
		}

		// Conditionally control visibility of the value field of bound properties.
		if (Usage != EMetaStoryPropertyUsage::Invalid && ID.IsValid())
		{
			// Pass the node ID to binding extension. Since the properties are added using AddChildStructure(), we break the hierarchy and cannot access parent.
			ChildPropHandle->SetInstanceMetaData(UE::PropertyBinding::MetaDataStructIDName, LexToString(ID));

			FPropertyBindingPath Path(ID, *Property->GetFName().ToString());
			TSharedPtr<SWidget> NameWidget;
			TSharedPtr<SWidget> ValueWidget;
			FDetailWidgetRow Row;
			ChildRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

			const bool bValidUsage = Usage == EMetaStoryPropertyUsage::Input || Usage == EMetaStoryPropertyUsage::Output || Usage == EMetaStoryPropertyUsage::Context;
			const bool bIsDelegateDispatcher = UE::MetaStoryEditor::Internal::IsDelegateDispatcherProperty(*Property);

			if (bValidUsage || bIsDelegateDispatcher)
			{
				FEdGraphPinType PinType;
				const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

				// Show referenced type for property refs.
				if (UE::MetaStory::PropertyRefHelpers::IsPropertyRef(*Property))
				{
					// Use internal type to construct PinType if it's property of PropertyRef type.
					FMetaStoryDataView TargetDataView;
					if (ensure(EditorData->GetBindingDataViewByID(ID, TargetDataView)))
					{
						TArray<FPropertyBindingPathIndirection> TargetIndirections;
						if (ensure(Path.ResolveIndirectionsWithValue(TargetDataView, TargetIndirections)))
						{
							const void* PropertyRef = TargetIndirections.Last().GetPropertyAddress();
							PinType = UE::MetaStory::PropertyRefHelpers::GetPropertyRefInternalTypeAsPin(*Property, PropertyRef);
						}
					}
				}
				else
				{
					Schema->ConvertPropertyToPinType(Property, PinType);
				}

				auto IsValueVisible = TAttribute<EVisibility>::Create([Path, EditorPropBindings]() -> EVisibility
					{
						return EditorPropBindings->HasBinding(Path, FPropertyBindingBindingCollection::ESearchMode::Exact) ? EVisibility::Collapsed : EVisibility::Visible;
					});

				const FSlateBrush* Icon = FBlueprintEditorUtils::GetIconFromPin(PinType, true);
				FText Text = GetPinTypeText(PinType);
				
				FText ToolTip; 
				FLinearColor IconColor = Schema->GetPinTypeColor(PinType);
				FText Label;
				FText LabelToolTip;
				FSlateColor TextColor = FSlateColor::UseForeground();

				if (bIsDelegateDispatcher)
				{
					Label = LOCTEXT("LabelDelegate", "DELEGATE");
					LabelToolTip = LOCTEXT("DelegateToolTip", "This is Delegate Dispatcher. You can bind to it from listeners.");

					FEdGraphPinType DelegatePinType;
					DelegatePinType.PinCategory = UEdGraphSchema_K2::PC_Delegate;
					IconColor = Schema->GetPinTypeColor(DelegatePinType);
				}
				else if (Usage == EMetaStoryPropertyUsage::Input)
				{
					Label = LOCTEXT("LabelInput", "IN");
					LabelToolTip = LOCTEXT("InputToolTip", "This is Input property. It is always expected to be bound to some other property.");
				}
				else if (Usage == EMetaStoryPropertyUsage::Output)
				{
					Label = LOCTEXT("LabelOutput", "OUT");
					LabelToolTip = LOCTEXT("OutputToolTip", "This is Output property. The node will always set it's value. It can bind to another property to push the value. Other nodes can also bind to it to fetch the value.");
				}
				else if (Usage == EMetaStoryPropertyUsage::Context)
				{
					Label = LOCTEXT("LabelContext", "CONTEXT");
					LabelToolTip = LOCTEXT("ContextObjectToolTip", "This is Context property. It is automatically connected to one of the Contex objects, or can be overridden with property binding.");

					if (UStruct* Struct = GetPropertyStruct(ChildPropHandle))
					{
						const FMetaStoryBindableStructDesc Desc = EditorData->FindContextData(Struct, ChildPropHandle->GetProperty()->GetName());
						if (Desc.IsValid())
						{
							// Show as connected.
							Icon = FCoreStyle::Get().GetBrush("Icons.Link");
							Text = FText::FromName(Desc.Name);
							
							ToolTip = FText::Format(
								LOCTEXT("ToolTipConnected", "Connected to Context {0}."),
									FText::FromName(Desc.Name));
						}
						else
						{
							// Show as unconnected.
							Icon = FCoreStyle::Get().GetBrush("Icons.Warning");
							ToolTip = LOCTEXT("ToolTipNotConnected", "Could not connect Context property automatically.");
						}
					}
					else
					{
						// Mismatching type.
						Text = LOCTEXT("ContextObjectInvalidType", "Invalid type");
						ToolTip = LOCTEXT("ContextObjectInvalidTypeTooltip", "Context properties must be Object references or Structs.");
						Icon = FCoreStyle::Get().GetBrush("Icons.ErrorWithColor");
						IconColor = FLinearColor::White;
					}
				}
				
				ChildRow
					.CustomWidget(true)
					.NameContent()
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							NameWidget.ToSharedRef()
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f)
						[
							SNew(SBorder)
							.Padding(FMargin(6.0f, 1.0f))
							.BorderImage(FMetaStoryEditorStyle::Get().GetBrush("MetaStory.Param.Background"))
							.Visibility(Label.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
							[
								SNew(STextBlock)
								.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Param.Label")
								.ColorAndOpacity(FStyleColors::Foreground)
								.Text(Label)
								.ToolTipText(LabelToolTip)
							]
						]

					]
					.ValueContent()
					[
						SNew(SHorizontalBox)
						.Visibility(IsValueVisible)

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f)
						[
							SNew(SImage)
							.Image(Icon)
							.ColorAndOpacity(IconColor)
							.ToolTipText(ToolTip)
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.ColorAndOpacity(TextColor)
							.Text(Text)
							.ToolTipText(ToolTip)
						]
					];
			}
		}
	}

} // UE::MetaStoryEditor::Internal

// Customized version of FInstancedStructDataDetails used to hide bindable properties.
class FBindableNodeInstanceDetails : public FInstancedStructDataDetails
{
public:

	FBindableNodeInstanceDetails(TSharedPtr<IPropertyHandle> InStructProperty, const FGuid& InID, UMetaStoryEditorData* InEditorData)
		: FInstancedStructDataDetails(InStructProperty)
		, ID(InID)
		, EditorData(InEditorData)
	{
	}

	virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override
	{
		UE::MetaStoryEditor::Internal::ModifyRow(ChildRow, ID, EditorData.Get());
	}

	FGuid ID;
	TWeakObjectPtr<UMetaStoryEditorData> EditorData;
};

////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FMetaStoryEditorNodeDetails::MakeInstance()
{
	return MakeShareable(new FMetaStoryEditorNodeDetails);
}

FMetaStoryEditorNodeDetails::~FMetaStoryEditorNodeDetails()
{
	UE::MetaStory::PropertyBinding::OnMetaStoryPropertyBindingChanged.Remove(OnBindingChangedHandle);
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->GetOnAssetChanged().Remove(OnChangedAssetHandle);
	}
}

void FMetaStoryEditorNodeDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	ParentProperty = StructProperty->GetParentHandle();
	ParentArrayProperty = ParentProperty->AsArray();

	PropUtils = StructCustomizationUtils.GetPropertyUtilities();

	NodeProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, Node));
	InstanceProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, Instance));
	InstanceObjectProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, InstanceObject));
	ExecutionRuntimeDataProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, ExecutionRuntimeData));
	ExecutionRuntimeDataObjectProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, ExecutionRuntimeDataObject));
	IDProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, ID));

	IndentProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, ExpressionIndent));
	OperandProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, ExpressionOperand));

	check(NodeProperty.IsValid());
	check(InstanceProperty.IsValid());
	check(InstanceObjectProperty.IsValid());
	check(ExecutionRuntimeDataProperty.IsValid());
	check(ExecutionRuntimeDataObjectProperty.IsValid());
	check(IDProperty.IsValid());
	check(IndentProperty.IsValid());
	check(OperandProperty.IsValid());

	{
		UScriptStruct* BaseScriptStructPtr = nullptr;
		UClass* BaseClassPtr = nullptr;
		UE::MetaStoryEditor::EditorNodeUtils::GetNodeBaseScriptStructAndClass(StructProperty, BaseScriptStructPtr, BaseClassPtr);
		BaseScriptStruct = BaseScriptStructPtr;
		BaseClass = BaseClassPtr;
	}

	UE::MetaStory::Delegates::OnIdentifierChanged.AddSP(this, &FMetaStoryEditorNodeDetails::OnIdentifierChanged);
	OnBindingChangedHandle = UE::MetaStory::PropertyBinding::OnMetaStoryPropertyBindingChanged.AddRaw(this, &FMetaStoryEditorNodeDetails::OnBindingChanged);
	FindOuterObjects();
	if (MetaStoryViewModel)
	{
		OnChangedAssetHandle = MetaStoryViewModel->GetOnAssetChanged().AddSP(this, &FMetaStoryEditorNodeDetails::HandleAssetChanged);
	}

	// Don't draw the header if it's a PropertyFunction.
	if (UE::MetaStoryEditor::Internal::IsOwnedByPropertyFunctionNode(StructProperty))
	{
		return;
	}

	const FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateSP(this, &FMetaStoryEditorNodeDetails::ShouldResetToDefault);
	const FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateSP(this, &FMetaStoryEditorNodeDetails::ResetToDefault);
	const FResetToDefaultOverride ResetOverride = FResetToDefaultOverride::Create(IsResetVisible, ResetHandler);

	auto IndentColor = [this]() -> FSlateColor
	{
		return (RowBorder && RowBorder->IsHovered()) ? FSlateColor::UseForeground() : FSlateColor(FLinearColor::Transparent);
	};

	TSharedPtr<SBorder> FlagBorder;
	TSharedPtr<SHorizontalBox> DescriptionBox;

	HeaderRow
		.WholeRowContent()
		.VAlign(VAlign_Center)
		[
			// Border to capture mouse clicks on the row (used for right click menu).
			SAssignNew(RowBorder, SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.Padding(0.0f)
			.ForegroundColor(this, &FMetaStoryEditorNodeDetails::GetContentRowColor)
			.OnMouseButtonDown(this, &FMetaStoryEditorNodeDetails::OnRowMouseDown)
			.OnMouseButtonUp(this, &FMetaStoryEditorNodeDetails::OnRowMouseUp)
			[
				SNew(SHorizontalBox)
				
				// Indent
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(30.0f)
					.Visibility(this, &FMetaStoryEditorNodeDetails::AreIndentButtonsVisible)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &FMetaStoryEditorNodeDetails::HandleIndentPlus)
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(4.f, 4.f))
						.ToolTipText(LOCTEXT("IncreaseIdentTooltip", "Increment the depth of the expression row controlling parentheses and expression order"))
						[
							SNew(SImage)
							.DesiredSizeOverride(FVector2D(8.f, 8.f))
							.Image(FAppStyle::GetBrush("Icons.Plus"))
							.ColorAndOpacity_Lambda(IndentColor)
						]
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(this, &FMetaStoryEditorNodeDetails::GetIndentSize)
					.Visibility(this, &FMetaStoryEditorNodeDetails::AreIndentButtonsVisible)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnClicked(this, &FMetaStoryEditorNodeDetails::HandleIndentMinus)
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(4.f, 4.f))
						.ToolTipText(LOCTEXT("DecreaseIndentTooltip", "Decrement the depth of the expression row controlling parentheses and expression order"))
						[
							SNew(SImage)
							.DesiredSizeOverride(FVector2D(8.f, 8.f))
							.Image(FAppStyle::GetBrush("Icons.Minus"))
							.ColorAndOpacity_Lambda(IndentColor)
						]
					]
				]

				// Operand
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.MinDesiredWidth(30.0f)
					.Padding(FMargin(2.0f, 4.0f, 2.0f, 3.0f))
					.VAlign(VAlign_Center)
					.Visibility(this, &FMetaStoryEditorNodeDetails::IsOperandVisible)
					[
						SNew(SComboButton)
						.IsEnabled(TAttribute<bool>(this, &FMetaStoryEditorNodeDetails::IsOperandEnabled))
						.ComboButtonStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Node.Operand.ComboBox")
						.ButtonColorAndOpacity(this, &FMetaStoryEditorNodeDetails::GetOperandColor)
						.HasDownArrow(false)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnGetMenuContent(this, &FMetaStoryEditorNodeDetails::OnGetOperandContent)
						.ButtonContent()
						[
							SNew(STextBlock)
							.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Node.Operand")
							.Text(this, &FMetaStoryEditorNodeDetails::GetOperandText)
						]
					]
				]
				// Open parens
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.Padding(FMargin(FMargin(0.0f, 0.0f, 4.0f, 0.0f)))
					.Visibility(this, &FMetaStoryEditorNodeDetails::AreParensVisible)
					[
						SNew(STextBlock)
						.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Node.Parens")
						.Text(this, &FMetaStoryEditorNodeDetails::GetOpenParens)
					]
				]
				// Description
				+ SHorizontalBox::Slot()
				.FillContentWidth(0.0f, 1.0f) // no growing, allow shrink
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SAssignNew(DescriptionBox, SHorizontalBox)
					.Clipping(EWidgetClipping::ClipToBoundsAlways)

					// Icon
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SImage)
						.Image(this, &FMetaStoryEditorNodeDetails::GetIcon)
						.ColorAndOpacity(this, &FMetaStoryEditorNodeDetails::GetIconColor)
						.Visibility(this, &FMetaStoryEditorNodeDetails::IsIconVisible)
					]

					// Rich text description and name edit 
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(0.0f, 0.0f, 0.0f, 0.0f)
					[
						SAssignNew(NameSwitcher, SWidgetSwitcher)
						.WidgetIndex(0)
						+ SWidgetSwitcher::Slot()
						[
							SNew(SBox)
							.Padding(FMargin(1.0f,0.0f, 1.0f, 1.0f))
							[
								SNew(SRichTextBlock)
								.Text(this, &FMetaStoryEditorNodeDetails::GetNodeDescription)
								.TextStyle(&FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Node.Normal"))
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								.Visibility(this, &FMetaStoryEditorNodeDetails::IsNodeDescriptionVisible)
								.ToolTipText(this, &FMetaStoryEditorNodeDetails::GetNodeTooltip)
								+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Node.Normal")))
								+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Node.Bold")))
								+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Node.Subdued")))
							]
						]
						+ SWidgetSwitcher::Slot()
						[
							SAssignNew(NameEdit, SInlineEditableTextBlock)
							.Style(FMetaStoryEditorStyle::Get(), "MetaStory.Node.TitleInlineEditableText")
							.Text(this, &FMetaStoryEditorNodeDetails::GetName)
							.OnTextCommitted(this, &FMetaStoryEditorNodeDetails::HandleNameCommitted)
							.OnVerifyTextChanged(this, &FMetaStoryEditorNodeDetails::HandleVerifyNameChanged)
							.Visibility(this, &FMetaStoryEditorNodeDetails::IsNodeDescriptionVisible)
						]
					]

					// Flags icons
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(4.0f, 0.0f))
					[
						SAssignNew(FlagsContainer, SBorder)
							.BorderImage(FStyleDefaults::GetNoBrush())
							.Visibility(this, &FMetaStoryEditorNodeDetails::AreFlagsVisible)
					]

					// Close parens
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Node.Parens")
						.Text(this, &FMetaStoryEditorNodeDetails::GetCloseParens)
						.Visibility(this, &FMetaStoryEditorNodeDetails::AreParensVisible)
					]
				]

				// Debug and property widgets
				+ SHorizontalBox::Slot()
				.FillContentWidth(1.0f, 0.0f) // grow, no shrinking
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(FMargin(8.0f, 0.0f, 2.0f, 0.0f))
				[
					SNew(SHorizontalBox)

					// Debugger labels
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						UE::MetaStoryEditor::DebuggerExtensions::CreateEditorNodeWidget(StructPropertyHandle, MetaStoryViewModel)
					]

					// Browse To source Button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.Visibility(this, &FMetaStoryEditorNodeDetails::IsBrowseToSourceVisible)
						.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(this, &FMetaStoryEditorNodeDetails::OnBrowseToSource)
							.ToolTipText(FText::Format(LOCTEXT("GoToCode_ToolTip", "Click to open the node source file in {0}"), FSourceCodeNavigation::GetSelectedSourceCodeIDE()))
							.ContentPadding(0.0f)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.OpenSourceLocation"))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
					]

					// Browse To BP Button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.Visibility(this, &FMetaStoryEditorNodeDetails::IsBrowseToNodeBlueprintVisible)
						.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(this, &FMetaStoryEditorNodeDetails::OnBrowseToNodeBlueprint)
							.ToolTipText(LOCTEXT("BrowseToCurrentNodeBP", "Browse to the current node blueprint in Content Browser"))
							.ContentPadding(0.0f)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.BrowseContent"))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
					]
					// Edit BP Button
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.Visibility(this, &FMetaStoryEditorNodeDetails::IsEditNodeBlueprintVisible)
						.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(this, &FMetaStoryEditorNodeDetails::OnEditNodeBlueprint)
							.ToolTipText(LOCTEXT("EditCurrentNodeBP", "Edit the current node blueprint in Editor"))
							.ContentPadding(0.0f)
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.Edit"))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
					]

					// Options
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
					[
						SNew(SComboButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnGetMenuContent(this, &FMetaStoryEditorNodeDetails::GenerateOptionsMenu)
						.ToolTipText(LOCTEXT("ItemActions", "Item actions"))
						.HasDownArrow(false)
						.ContentPadding(FMargin(4.0f, 2.0f))
						.ButtonContent()
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("Icons.ChevronDown"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
					]
				]
			]
		]
		.OverrideResetToDefault(ResetOverride)
		.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::OnCopyNode)))
		.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::OnPasteNodes)));

	// Task completion
	bool bShowCompletion = true;
	if (const UMetaStoryEditorData* EditorDataPtr = EditorData.Get())
	{
		bShowCompletion = EditorDataPtr->Schema ? EditorDataPtr->Schema->AllowTasksCompletion() : true;
	}
	if (bShowCompletion)
	{
		if (const FMetaStoryEditorNode* Node = UE::MetaStoryEditor::EditorNodeUtils::GetCommonNode(StructProperty))
		{
			if (const FMetaStoryTaskBase* TaskBase = Node->Node.GetPtr<FMetaStoryTaskBase>())
			{
				DescriptionBox->InsertSlot(0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 0.0f)
				[
					// Create the toggle favorites button
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &FMetaStoryEditorNodeDetails::HandleToggleCompletionTaskClicked)
					.ToolTipText(this, &FMetaStoryEditorNodeDetails::GetToggleCompletionTaskTooltip)
					[
						SNew(SImage)
						.ColorAndOpacity(this, &FMetaStoryEditorNodeDetails::GetToggleCompletionTaskColor)
						.Image(this, &FMetaStoryEditorNodeDetails::GetToggleCompletionTaskIcon)
					]
					.IsEnabled(UE::MetaStoryEditor::EditorNodeUtils::CanEditTaskConsideredForCompletion(*Node))
					.Visibility(this, &FMetaStoryEditorNodeDetails::GetToggleCompletionTaskVisibility)
				];
			}
		}
	}

	MakeFlagsWidget();
}

void FMetaStoryEditorNodeDetails::MakeFlagsWidget()
{
	if (!FlagsContainer.IsValid())
	{
		return;
	}

	FlagsContainer->SetPadding(FMargin(4.0f));
	FlagsContainer->SetContent(SNullWidget::NullWidget);

	const UMetaStory* MetaStoryPtr = MetaStory.Get();

	TArray<void*> RawNodeDatas;
	StructProperty->AccessRawData(RawNodeDatas);
	bool bShowCallTick = false;
	bool bShouldCallTickOnlyOnEvents = false;
	bool bHasTransitionTick = false;
	for (void* RawNodeData : RawNodeDatas)
	{
		if (const FMetaStoryEditorNode* EditorNode = reinterpret_cast<const FMetaStoryEditorNode*>(RawNodeData))
		{
			bool bUseEditorData = true;
			// Use the compiled version if it exists. It is more accurate (like with BP tasks) but less interactive (the user needs to compile) :(
			if (MetaStoryPtr)
			{
				if (const FMetaStoryTaskBase* CompiledTask = MetaStoryPtr->GetNode(MetaStoryPtr->GetNodeIndexFromId(EditorNode->ID).AsInt32()).GetPtr<const FMetaStoryTaskBase>())
				{
					if (CompiledTask->bConsideredForScheduling)
					{
						bShowCallTick = bShowCallTick || CompiledTask->bShouldCallTick;
						bShouldCallTickOnlyOnEvents = bShouldCallTickOnlyOnEvents || CompiledTask->bShouldCallTickOnlyOnEvents;
						bHasTransitionTick = bHasTransitionTick || CompiledTask->bShouldAffectTransitions;
					}
					bUseEditorData = false;
				}
			}

			if (bUseEditorData)
			{
				if (const FMetaStoryTaskBase* TreeTaskNodePtr = EditorNode->Node.GetPtr<FMetaStoryTaskBase>())
				{
					if (TreeTaskNodePtr->bConsideredForScheduling)
					{
						bShowCallTick = bShowCallTick || TreeTaskNodePtr->bShouldCallTick;
						bShouldCallTickOnlyOnEvents = bShouldCallTickOnlyOnEvents || TreeTaskNodePtr->bShouldCallTickOnlyOnEvents;
						bHasTransitionTick = bHasTransitionTick || TreeTaskNodePtr->bShouldAffectTransitions;
					}
				}
			}
		}
	}

	if (bShowCallTick || bShouldCallTickOnlyOnEvents || bHasTransitionTick)
	{
		TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
		const bool bShowTickIcon = bShowCallTick || bHasTransitionTick;
		if (bShowTickIcon)
		{
			Box->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Flags.Tick"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.ToolTipText(LOCTEXT("TaskTick", "The task ticks at runtime."))
			];
		}

		if (!bShowTickIcon && bShouldCallTickOnlyOnEvents)
		{
			Box->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
					.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Flags.TickOnEvent"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.ToolTipText(LOCTEXT("TaskTickEvent", "The task ticks on event at runtime."))
			];
		}

		FlagsContainer->SetPadding(FMargin(4.0f));
		FlagsContainer->SetContent(Box);
	}
}

FReply FMetaStoryEditorNodeDetails::OnRowMouseDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply FMetaStoryEditorNodeDetails::OnRowMouseUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(
			NameSwitcher.ToSharedRef(),
			WidgetPath,
			GenerateOptionsMenu(),
			MouseEvent.GetScreenSpacePosition(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

void FMetaStoryEditorNodeDetails::OnCopyNode()
{
	UMetaStoryEditorData* EditorDataPtr = EditorData.Get();
	if (!EditorDataPtr)
	{
		return;
	}

	void* EditorNodeAddress = nullptr;
	if (StructProperty->GetValueData(EditorNodeAddress) == FPropertyAccess::Success)
	{
		UE::MetaStoryEditor::FMetaStoryClipboardEditorData Clipboard;
		Clipboard.Append(EditorDataPtr, TConstArrayView<FMetaStoryEditorNode>(static_cast<FMetaStoryEditorNode*>(EditorNodeAddress), 1));
		UE::MetaStoryEditor::ExportTextAsClipboardEditorData(Clipboard);
	}
}

void FMetaStoryEditorNodeDetails::OnCopyAllNodes()
{
	UMetaStoryEditorData* EditorDataPtr = EditorData.Get();
	if (!EditorDataPtr)
	{
		return;
	}

	if (ParentArrayProperty)
	{
		void* EditorNodeArrayAddress = nullptr;
		if (ParentProperty->GetValueData(EditorNodeArrayAddress) == FPropertyAccess::Success)
		{
			UE::MetaStoryEditor::FMetaStoryClipboardEditorData Clipboard;
			Clipboard.Append(EditorDataPtr, *static_cast<TArray<FMetaStoryEditorNode>*>(EditorNodeArrayAddress));
			UE::MetaStoryEditor::ExportTextAsClipboardEditorData(Clipboard);
		}
	}
	else
	{
		OnCopyNode();
	}
}

void FMetaStoryEditorNodeDetails::OnPasteNodes()
{
	using namespace UE::MetaStoryEditor;

	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	if (OuterObjects.IsEmpty())
	{
		return;
	}

	UMetaStoryEditorData* EditorDataPtr = EditorData.Get();
	if (!EditorDataPtr)
	{
		return;
	}

	// In case its multi selected, we need to have a unique copy for each Object
	TArray<FMetaStoryClipboardEditorData, TInlineAllocator<2>> ClipboardEditorDatas;
	ClipboardEditorDatas.AddDefaulted(OuterObjects.Num());

	for (int32 Idx = 0; Idx < OuterObjects.Num(); ++Idx)
	{
		const bool bSuccess = ImportTextAsClipboardEditorData
		(BaseScriptStruct.Get(),
			EditorData.Get(),
			OuterObjects[Idx],
			ClipboardEditorDatas[Idx]);

		if (!bSuccess)
		{
			return;
		}
	}

	// make sure each Clipboard has the same number of nodes
	for (int32 Idx = 0; Idx < ClipboardEditorDatas.Num() - 1; ++Idx)
	{
		check(ClipboardEditorDatas[Idx].GetEditorNodesInBuffer().Num() == ClipboardEditorDatas[Idx + 1].GetEditorNodesInBuffer().Num());
	}

	int32 NumEditorNodesInBuffer = ClipboardEditorDatas[0].GetEditorNodesInBuffer().Num();

	if (NumEditorNodesInBuffer == 0)
	{
		return;
	}

	if (!ParentArrayProperty.IsValid() && NumEditorNodesInBuffer != 1)
	{
		// Node is not in an array. we can't do multi-to-one paste
		return;
	}

	if (OuterObjects.Num() != 1 && NumEditorNodesInBuffer != 1)
	{
		// if multiple selected objects, and we have more than one nodes to paste into
		// Array Handle doesn't support manipulation on multiple objects.
		FNotificationInfo NotificationInfo(FText::GetEmpty());
		NotificationInfo.Text = LOCTEXT("NotSupportedByMultipleObjects", "Operation is not supported for multi-selected objects");
		NotificationInfo.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);

		return;
	}

	{
		FScopedTransaction Transaction(LOCTEXT("PasteNode", "Paste Node"));

		EditorDataPtr->Modify();	// we might modify the bindings on Editor Data
		StructProperty->NotifyPreChange();

		if (ParentArrayProperty)
		{
			// Paste multi nodes into one node
			const int32 StructIndex = StructProperty->GetIndexInArray();
			check(StructIndex != INDEX_NONE);

			uint32 NumArrayElements = 0;
			ParentArrayProperty->GetNumElements(NumArrayElements);
			check(NumArrayElements > 0);	// since we already have at least one element to paste into

			// Insert or append uninitialized elements after the current node to match the number of nodes in the paste buffer and retain the order of elements 
			// The first node in the buffer goes into the current node
			const int32 IndexToInsert = StructIndex + 1;
			const int32 NumElementsToAddOrInsert = ClipboardEditorDatas[0].GetEditorNodesInBuffer().Num() - 1;

			int32 Cnt = 0;
			if (IndexToInsert == NumArrayElements)
			{
				while (Cnt++ < NumElementsToAddOrInsert)
				{
					FPropertyHandleItemAddResult Result = ParentArrayProperty->AddItem();
					if (Result.GetAccessResult() != FPropertyAccess::Success)
					{
						return;
					}
				}
			}
			else
			{
				while (Cnt++ < NumElementsToAddOrInsert)
				{
					FPropertyAccess::Result Result = ParentArrayProperty->Insert(IndexToInsert);
					if (Result != FPropertyAccess::Success)
					{
						return;
					}
				}
			}

			TArray<void*> RawDatasArray;
			ParentProperty->AccessRawData(RawDatasArray);
			check(RawDatasArray.Num() == OuterObjects.Num());
			for (int32 ObjIdx = 0; ObjIdx < OuterObjects.Num(); ++ObjIdx)
			{
				if (TArray<FMetaStoryEditorNode>* EditorNodesPtr = static_cast<TArray<FMetaStoryEditorNode>*>(RawDatasArray[ObjIdx]))
				{
					TArrayView<FMetaStoryEditorNode> EditorNodesClipboardBuffer = ClipboardEditorDatas[ObjIdx].GetEditorNodesInBuffer();
					TArray<FMetaStoryEditorNode>& EditorNodesToPasteInto = *EditorNodesPtr;

					for (int32 Idx = 0; Idx < EditorNodesClipboardBuffer.Num(); ++Idx)
					{
						EditorNodesToPasteInto[StructIndex + Idx] = MoveTemp(EditorNodesClipboardBuffer[Idx]);
					}

					for (FMetaStoryPropertyPathBinding& Binding : ClipboardEditorDatas[ObjIdx].GetBindingsInBuffer())
					{
						EditorDataPtr->GetPropertyEditorBindings()->AddMetaStoryBinding(MoveTemp(Binding));
					}
				}
			}
		}
		else
		{
			// Paste single node to a single Node
			TArray<void*> RawDatas;
			StructProperty->AccessRawData(RawDatas);
			check(RawDatas.Num() == OuterObjects.Num());
			for (int32 Idx = 0; Idx < RawDatas.Num(); ++Idx)
			{
				if (FMetaStoryEditorNode* CurrentEditorNode = static_cast<FMetaStoryEditorNode*>(RawDatas[Idx]))
				{
					*CurrentEditorNode = MoveTemp(ClipboardEditorDatas[Idx].GetEditorNodesInBuffer()[0]);

					for (FMetaStoryPropertyPathBinding& Binding : ClipboardEditorDatas[Idx].GetBindingsInBuffer())
					{
						EditorDataPtr->GetPropertyEditorBindings()->AddMetaStoryBinding(MoveTemp(Binding));
					}
				}
			}
		}

		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();
	}

	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

bool FMetaStoryEditorNodeDetails::ShouldResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle) const
{
	check(StructProperty);
	
	bool bAnyValid = false;
	
	TArray<const void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);
	for (const void* Data : RawNodeData)
	{
		if (const FMetaStoryEditorNode* Node = static_cast<const FMetaStoryEditorNode*>(Data))
		{
			if (Node->Node.IsValid())
			{
				bAnyValid = true;
				break;
			}
		}
	}
	
	// Assume that the default value is empty. Any valid means that some can be reset to empty.
	return bAnyValid;
}


void FMetaStoryEditorNodeDetails::ResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	UE::MetaStoryEditor::EditorNodeUtils::ModifyNodeInTransaction(LOCTEXT("OnTaskEnableToggled", "Toggled Task Enabled"),
		StructProperty,
		[](const TSharedPtr<IPropertyHandle>& StructPropertyHandle)
		{
			TArray<void*> RawNodeData;
			StructPropertyHandle->AccessRawData(RawNodeData);
			for (void* Data : RawNodeData)
			{
				if (FMetaStoryEditorNode* Node = static_cast<FMetaStoryEditorNode*>(Data))
				{
					Node->Reset();
				}
			}
		});

	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

void FMetaStoryEditorNodeDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const FMetaStoryEditorNode* EditorNodePtr = UE::MetaStory::PropertyHelpers::GetStructPtr<FMetaStoryEditorNode>(StructProperty);
	if (EditorNodePtr == nullptr)
	{
		// in case StructPropertyHandle is not valid
		return;
	}

	// ID
	if (UE::MetaStory::Editor::GbDisplayItemIds)
	{
		// ID
		StructBuilder.AddProperty(IDProperty.ToSharedRef());
	}
	
	UMetaStoryEditorData* EditorDataPtr = EditorData.Get();
	auto AddObjectInstance = [this, &StructBuilder, EditorNodePtr, EditorDataPtr](TSharedPtr<IPropertyHandle> ValueProperty)
		{
			if (ValueProperty.IsValid())
			{
				uint32 NumChildren = 0;
				ValueProperty->GetNumChildren(NumChildren);

				// Find visible child properties and sort them so in order: Context, Input, Param, Output.
				struct FSortedChild
				{
					TSharedPtr<IPropertyHandle> PropertyHandle;
					EMetaStoryPropertyUsage Usage = EMetaStoryPropertyUsage::Invalid;
				};

				TArray<FSortedChild> SortedChildren;
				for (uint32 Index = 0; Index < NumChildren; Index++)
				{
					if (TSharedPtr<IPropertyHandle> ChildHandle = ValueProperty->GetChildHandle(Index); ChildHandle.IsValid())
					{
						FSortedChild Child;
						Child.PropertyHandle = ChildHandle;
						Child.Usage = UE::MetaStory::GetUsageFromMetaData(Child.PropertyHandle->GetProperty());

						// If the property is set to one of these usages, display it even if it is not edit on instance.
						// It is a common mistake to forget to set the "eye" on these properties it and wonder why it does not show up.
						const bool bShouldShowByUsage = Child.Usage == EMetaStoryPropertyUsage::Input || Child.Usage == EMetaStoryPropertyUsage::Output || Child.Usage == EMetaStoryPropertyUsage::Context;
        				const bool bIsEditable = !Child.PropertyHandle->GetProperty()->HasAllPropertyFlags(CPF_DisableEditOnInstance);

						if (bShouldShowByUsage || bIsEditable)
						{
							SortedChildren.Add(Child);
						}
					}
				}

				SortedChildren.StableSort([](const FSortedChild& LHS, const FSortedChild& RHS) { return LHS.Usage < RHS.Usage; });

				for (FSortedChild& Child : SortedChildren)
				{
					IDetailPropertyRow& ChildRow = StructBuilder.AddProperty(Child.PropertyHandle.ToSharedRef());
					UE::MetaStoryEditor::Internal::ModifyRow(ChildRow, EditorNodePtr->ID, EditorDataPtr);
				}
			}
		};

	// Node
	TSharedRef<FBindableNodeInstanceDetails> NodeDetails = MakeShareable(new FBindableNodeInstanceDetails(NodeProperty, EditorNodePtr->GetNodeID(), EditorDataPtr));
	StructBuilder.AddCustomBuilder(NodeDetails);

	// Instance
	TSharedRef<FBindableNodeInstanceDetails> InstanceDetails = MakeShareable(new FBindableNodeInstanceDetails(InstanceProperty, EditorNodePtr->ID, EditorDataPtr));
	StructBuilder.AddCustomBuilder(InstanceDetails);

	// InstanceObject
	// Get the actual UObject from the pointer.
	TSharedPtr<IPropertyHandle> InstanceObjectValueProperty = GetInstancedObjectValueHandle(InstanceObjectProperty);
	AddObjectInstance(InstanceObjectValueProperty);

	// ExecutionRuntime Instance
	TSharedRef<FBindableNodeInstanceDetails> ExecutionRuntimeDataDetails = MakeShareable(new FBindableNodeInstanceDetails(ExecutionRuntimeDataProperty, EditorNodePtr->ID, EditorDataPtr));
	ExecutionRuntimeDataProperty->SetInstanceMetaData(UE::PropertyBinding::MetaDataNoBindingName, TEXT("true"));
	StructBuilder.AddCustomBuilder(ExecutionRuntimeDataDetails);

	// ExecutionRuntime Instance Object
	TSharedPtr<IPropertyHandle> ExecutionRuntimeDataObjectValueProperty = GetInstancedObjectValueHandle(ExecutionRuntimeDataObjectProperty);
	ExecutionRuntimeDataObjectProperty->SetInstanceMetaData(UE::PropertyBinding::MetaDataNoBindingName, TEXT("true"));
	AddObjectInstance(ExecutionRuntimeDataObjectValueProperty);
}

TSharedPtr<IPropertyHandle> FMetaStoryEditorNodeDetails::GetInstancedObjectValueHandle(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	TSharedPtr<IPropertyHandle> ChildHandle;

	uint32 NumChildren;
	PropertyHandle->GetNumChildren(NumChildren);

	if (NumChildren > 0)
	{
		// when the property is a (inlined) object property, the first child will be
		// the object instance, and its properties are the children underneath that
		ensure(NumChildren == 1);
		ChildHandle = PropertyHandle->GetChildHandle(0);
	}

	return ChildHandle;
}

void FMetaStoryEditorNodeDetails::OnIdentifierChanged(const UMetaStory& InMetaStory)
{
	if (PropUtils && MetaStory == &InMetaStory)
	{
		PropUtils->ForceRefresh();
	}
}

void FMetaStoryEditorNodeDetails::OnBindingChanged(const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath)
{
	check(StructProperty);

	UMetaStoryEditorData* EditorDataPtr = EditorData.Get();
	if (!EditorDataPtr)
	{
		return;
	}

	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);

	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);

	if (OuterObjects.Num() != RawNodeData.Num())
	{
		return;
	}

	const FMetaStoryBindingLookup BindingLookup(EditorDataPtr);

	for (int32 i = 0; i < OuterObjects.Num(); i++)
	{
		FMetaStoryEditorNode* EditorNode = static_cast<FMetaStoryEditorNode*>(RawNodeData[i]);
		UObject* OuterObject = OuterObjects[i]; // Immediate outer, i.e MetaStoryState
		if (EditorNode && OuterObject && EditorNode->ID == TargetPath.GetStructID())
		{
			FMetaStoryNodeBase* Node = EditorNode->Node.GetMutablePtr<FMetaStoryNodeBase>();
			FMetaStoryDataView InstanceView = EditorNode->GetInstance(); 

			if (Node && InstanceView.IsValid())
			{
				OuterObject->Modify();
				Node->OnBindingChanged(EditorNode->ID, InstanceView, SourcePath, TargetPath, BindingLookup);
			}
		}
	}
}

void FMetaStoryEditorNodeDetails::FindOuterObjects()
{
	check(StructProperty);
	
	EditorData.Reset();
	MetaStory.Reset();
	MetaStoryViewModel.Reset();

	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (UObject* Outer : OuterObjects)
	{
		UMetaStoryEditorData* OuterEditorData = Cast<UMetaStoryEditorData>(Outer);
		if (OuterEditorData == nullptr)
		{
			OuterEditorData = Outer->GetTypedOuter<UMetaStoryEditorData>();
		}
		
		UMetaStory* OuterMetaStory = OuterEditorData ? OuterEditorData->GetTypedOuter<UMetaStory>() : nullptr;
		if (OuterEditorData && OuterMetaStory)
		{
			MetaStory = OuterMetaStory;
			EditorData = OuterEditorData;
			if (UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = GEditor ? GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>() : nullptr)
			{
				MetaStoryViewModel = MetaStoryEditingSubsystem->FindOrAddViewModel(OuterMetaStory);
			}
			break;
		}
	}
}

FOptionalSize FMetaStoryEditorNodeDetails::GetIndentSize() const
{
	return FOptionalSize(static_cast<float>(GetIndent()) * 30.0f);
}

FReply FMetaStoryEditorNodeDetails::HandleIndentPlus()
{
	SetIndent(GetIndent() + 1);
	return FReply::Handled();
}

FReply FMetaStoryEditorNodeDetails::HandleIndentMinus()
{
	SetIndent(GetIndent() - 1);
	return FReply::Handled();
}

int32 FMetaStoryEditorNodeDetails::GetIndent() const
{
	check(IndentProperty);
	
	uint8 Indent = 0;
	IndentProperty->GetValue(Indent);

	return Indent;
}

void FMetaStoryEditorNodeDetails::SetIndent(const int32 Indent) const
{
	check(IndentProperty);
	
	IndentProperty->SetValue((uint8)FMath::Clamp(Indent, 0, UE::MetaStory::MaxExpressionIndent - 1));
}

bool FMetaStoryEditorNodeDetails::IsIndent(const int32 Indent) const
{
	return Indent == GetIndent();
}

bool FMetaStoryEditorNodeDetails::IsFirstItem() const
{
	check(StructProperty);
	return StructProperty->GetIndexInArray() == 0;
}

int32 FMetaStoryEditorNodeDetails::GetCurrIndent() const
{
	// First item needs to be zero indent to make the parentheses counting to work properly.
	return IsFirstItem() ? 0 : (GetIndent() + 1);
}

int32 FMetaStoryEditorNodeDetails::GetNextIndent() const
{
	// Find the intent of the next item by finding the item in the parent array.
	check(StructProperty);
	TSharedPtr<IPropertyHandle> ParentProp = StructProperty->GetParentHandle();
	if (!ParentProp.IsValid())
	{
		return 0;
	}
	TSharedPtr<IPropertyHandleArray> ParentArray = ParentProp->AsArray();
	if (!ParentArray.IsValid())
	{
		return 0;
	}

	uint32 NumElements = 0;
	if (ParentArray->GetNumElements(NumElements) != FPropertyAccess::Success)
	{
		return 0;
	}

	const int32 NextIndex = StructProperty->GetIndexInArray() + 1;
	if (NextIndex >= (int32)NumElements)
	{
		return 0;
	}

	TSharedPtr<IPropertyHandle> NextStructProperty = ParentArray->GetElement(NextIndex);
	if (!NextStructProperty.IsValid())
	{
		return 0;
	}

	TSharedPtr<IPropertyHandle> NextIndentProperty = NextStructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryEditorNode, ExpressionIndent));
	if (!NextIndentProperty.IsValid())
	{
		return 0;
	}

	uint8 Indent = 0;
	NextIndentProperty->GetValue(Indent);

	return Indent + 1;
}

FText FMetaStoryEditorNodeDetails::GetOpenParens() const
{
	check(IndentProperty);

	const int32 CurrIndent = GetCurrIndent();
	const int32 NextIndent = GetNextIndent();
	const int32 DeltaIndent = NextIndent - CurrIndent;
	const int32 OpenParens = FMath::Max(0, DeltaIndent);

	static_assert(UE::MetaStory::MaxExpressionIndent == 4);
	switch (OpenParens)
	{
	case 1: return FText::FromString(TEXT("("));
	case 2: return FText::FromString(TEXT("(("));
	case 3: return FText::FromString(TEXT("((("));
	case 4: return FText::FromString(TEXT("(((("));
	}
	return FText::GetEmpty();
}

FText FMetaStoryEditorNodeDetails::GetCloseParens() const
{
	check(IndentProperty);

	const int32 CurrIndent = GetCurrIndent();
	const int32 NextIndent = GetNextIndent();
	const int32 DeltaIndent = NextIndent - CurrIndent;
	const int32 CloseParens = FMath::Max(0, -DeltaIndent);

	static_assert(UE::MetaStory::MaxExpressionIndent == 4);
	switch (CloseParens)
	{
	case 1: return FText::FromString(TEXT(")"));
	case 2: return FText::FromString(TEXT("))"));
	case 3: return FText::FromString(TEXT(")))"));
	case 4: return FText::FromString(TEXT("))))"));
	}
	return FText::GetEmpty();
}

FSlateColor FMetaStoryEditorNodeDetails::GetContentRowColor() const
{
	return UE::MetaStoryEditor::DebuggerExtensions::IsEditorNodeEnabled(StructProperty)
		? FSlateColor::UseForeground()
		: FSlateColor::UseSubduedForeground();
}

FText FMetaStoryEditorNodeDetails::GetOperandText() const
{
	check(OperandProperty);

	if (IsConditionVisible() == EVisibility::Visible)
	{
		return GetConditionOperandText();
	}
	else if (IsConsiderationVisible() == EVisibility::Visible)
	{
		return GetConsiderationOperandText();
	}

	return FText::GetEmpty();
}

FText FMetaStoryEditorNodeDetails::GetConditionOperandText() const
{
	check(OperandProperty);

	// First item does not relate to anything existing, it could be empty. 
	// return IF to indicate that we're building condition and IS for consideration.
	if (IsFirstItem())
	{
		return LOCTEXT("IfOperand", "IF");
	}

	uint8 Value = 0;
	OperandProperty->GetValue(Value);

	switch (EMetaStoryExpressionOperand Operand = static_cast<EMetaStoryExpressionOperand>(Value))
	{
	case EMetaStoryExpressionOperand::And:
		return LOCTEXT("AndOperand", "AND");
	case EMetaStoryExpressionOperand::Or:
		return LOCTEXT("OrOperand", "OR");
	case EMetaStoryExpressionOperand::Multiply:
	default:
		ensureMsgf(false, TEXT("Unhandled operand %s"), *UEnum::GetValueAsString(Operand));
		return FText::GetEmpty();
	};
}

FText FMetaStoryEditorNodeDetails::GetConsiderationOperandText() const
{
	check(OperandProperty);

	// First item does not relate to anything existing, it could be empty. 
	// return IF to indicate that we're building condition and IS for consideration.
	if (IsFirstItem())
	{
		return LOCTEXT("IsOperand", "IS");
	}

	uint8 Value = 0;
	OperandProperty->GetValue(Value);

	switch (EMetaStoryExpressionOperand Operand = static_cast<EMetaStoryExpressionOperand>(Value))
	{
	case EMetaStoryExpressionOperand::And:
		return LOCTEXT("AndOperand", "AND");
	case EMetaStoryExpressionOperand::Or:
		return LOCTEXT("OrOperand", "OR");
	case EMetaStoryExpressionOperand::Multiply:
		return LOCTEXT("MultiplyOperand", "MULTIPLY");
	default:
		ensureMsgf(false, TEXT("Unhandled operand %s"), *UEnum::GetValueAsString(Operand));
		return FText::GetEmpty();
	};
}

FSlateColor FMetaStoryEditorNodeDetails::GetOperandColor() const
{
	check(OperandProperty);

	if (IsFirstItem())
	{
		return FStyleColors::Transparent;
	}

	uint8 Value = 0; 
	OperandProperty->GetValue(Value);

	switch (EMetaStoryExpressionOperand Operand = static_cast<EMetaStoryExpressionOperand>(Value))
	{
	case EMetaStoryExpressionOperand::And:
		return FStyleColors::AccentPink;
	case EMetaStoryExpressionOperand::Or:
		return FStyleColors::AccentBlue;
	case EMetaStoryExpressionOperand::Multiply:
		return FStyleColors::AccentGreen;
	default:
		ensureMsgf(false, TEXT("Unhandled operand %s"), *UEnum::GetValueAsString(Operand));
		return FStyleColors::Transparent;
	};
}

TSharedRef<SWidget> FMetaStoryEditorNodeDetails::OnGetOperandContent() const
{
	if (IsConditionVisible() == EVisibility::Visible)
	{
		return GetConditionOperandContent();
	}
	else //(IsConsiderationVisible() == EVisibility::Visible)
	{
		return GetConsiderationOperandContent();
	}
}

TSharedRef<SWidget> FMetaStoryEditorNodeDetails::GetConditionOperandContent() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	FUIAction AndAction(
		FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::SetOperand, EMetaStoryExpressionOperand::And),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMetaStoryEditorNodeDetails::IsOperand, EMetaStoryExpressionOperand::And));
	MenuBuilder.AddMenuEntry(LOCTEXT("AndOperand", "AND"), TAttribute<FText>(), FSlateIcon(), AndAction, FName(), EUserInterfaceActionType::Check);

	FUIAction OrAction(FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::SetOperand, EMetaStoryExpressionOperand::Or),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMetaStoryEditorNodeDetails::IsOperand, EMetaStoryExpressionOperand::Or));
	MenuBuilder.AddMenuEntry(LOCTEXT("OrOperand", "OR"), TAttribute<FText>(), FSlateIcon(), OrAction, FName(), EUserInterfaceActionType::Check);

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FMetaStoryEditorNodeDetails::GetConsiderationOperandContent() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	FUIAction AndAction(
		FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::SetOperand, EMetaStoryExpressionOperand::And),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMetaStoryEditorNodeDetails::IsOperand, EMetaStoryExpressionOperand::And));
	MenuBuilder.AddMenuEntry(LOCTEXT("AndOperand", "AND"), TAttribute<FText>(), FSlateIcon(), AndAction, FName(), EUserInterfaceActionType::Check);

	FUIAction OrAction(FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::SetOperand, EMetaStoryExpressionOperand::Or),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMetaStoryEditorNodeDetails::IsOperand, EMetaStoryExpressionOperand::Or));
	MenuBuilder.AddMenuEntry(LOCTEXT("OrOperand", "OR"), TAttribute<FText>(), FSlateIcon(), OrAction, FName(), EUserInterfaceActionType::Check);

	FUIAction MultiplyAction(
		FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::SetOperand, EMetaStoryExpressionOperand::Multiply),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMetaStoryEditorNodeDetails::IsOperand, EMetaStoryExpressionOperand::Multiply));
	MenuBuilder.AddMenuEntry(LOCTEXT("MultiplyOperand", "MULTIPLY"), TAttribute<FText>(), FSlateIcon(), MultiplyAction, FName(), EUserInterfaceActionType::Check);

	return MenuBuilder.MakeWidget();
}

bool FMetaStoryEditorNodeDetails::IsOperandEnabled() const
{
	return !IsFirstItem();
}

bool FMetaStoryEditorNodeDetails::IsOperand(const EMetaStoryExpressionOperand Operand) const
{
	check(OperandProperty);

	uint8 Value = 0; 
	OperandProperty->GetValue(Value);
	const EMetaStoryExpressionOperand CurrOperand = static_cast<EMetaStoryExpressionOperand>(Value);

	return CurrOperand == Operand;
}

void FMetaStoryEditorNodeDetails::SetOperand(const EMetaStoryExpressionOperand Operand) const
{
	check(OperandProperty);

	OperandProperty->SetValue(static_cast<uint8>(Operand));
}

EVisibility FMetaStoryEditorNodeDetails::IsConditionVisible() const
{
	return UE::MetaStoryEditor::EditorNodeUtils::IsConditionVisible(StructProperty);
}

EVisibility FMetaStoryEditorNodeDetails::IsConsiderationVisible() const
{
	return UE::MetaStoryEditor::EditorNodeUtils::IsConsiderationVisible(StructProperty);
}

EVisibility FMetaStoryEditorNodeDetails::IsOperandVisible() const
{
	// Assume the Condition and Consideration's Visibility is either Visible or Collapsed
	if (IsConditionVisible() == EVisibility::Visible || IsConsiderationVisible() == EVisibility::Visible)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility FMetaStoryEditorNodeDetails::AreIndentButtonsVisible() const
{
	if (IsFirstItem())
	{
		return EVisibility::Collapsed;
	}

	// Assume the Condition and Consideration's Visibility is either Visible or Collapsed
	if (IsConditionVisible() == EVisibility::Visible || IsConsiderationVisible() == EVisibility::Visible)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility FMetaStoryEditorNodeDetails::AreParensVisible() const
{
	//Assume the Condition and Consideration's Visibility is either Visible or Collapsed
	if (EVisibility::Visible.Value & (IsConditionVisible().Value | IsConsiderationVisible().Value))
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility FMetaStoryEditorNodeDetails::AreFlagsVisible() const
{
	bool bVisible = EnumHasAllFlags(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewDisplayNodeType(), EMetaStoryEditorUserSettingsNodeType::Flag);
	return bVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FMetaStoryEditorNodeDetails::IsIconVisible() const
{
	return UE::MetaStoryEditor::EditorNodeUtils::IsIconVisible(StructProperty);
}

const FSlateBrush* FMetaStoryEditorNodeDetails::GetIcon() const
{
	return UE::MetaStoryEditor::EditorNodeUtils::GetIcon(StructProperty).GetIcon();
}

FSlateColor FMetaStoryEditorNodeDetails::GetIconColor() const
{
	return UE::MetaStoryEditor::EditorNodeUtils::GetIconColor(StructProperty);
}

FReply FMetaStoryEditorNodeDetails::OnDescriptionClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	if (NameSwitcher && NameEdit)
	{
		if (NameSwitcher->GetActiveWidgetIndex() == 0)
		{
			// Enter edit mode
			NameSwitcher->SetActiveWidgetIndex(1);

			// Focus on name edit.
			FReply Reply = FReply::Handled();
			Reply.SetUserFocus(NameEdit.ToSharedRef());
			NameEdit->EnterEditingMode();
			return Reply;
		}
	}

	return FReply::Unhandled();
}

FText FMetaStoryEditorNodeDetails::GetNodeDescription() const
{
	check(StructProperty);
	const UMetaStoryEditorData* EditorDataPtr = EditorData.Get();
	if (!EditorDataPtr)
	{
		return FText::GetEmpty();
	}
	
	// Multiple names do not make sense, just if only one node is selected.
	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);
	if (RawNodeData.Num() == 1)
	{
		FText Description = LOCTEXT("EmptyNodeRich", "<s>None</>");
		if (const FMetaStoryEditorNode* Node = static_cast<FMetaStoryEditorNode*>(RawNodeData[0]))
		{
			return EditorDataPtr->GetNodeDescription(*Node, EMetaStoryNodeFormatting::RichText);
		}
		return Description;
	}

	return LOCTEXT("MultipleSelectedRich", "<s>Multiple Selected</>");
}

EVisibility FMetaStoryEditorNodeDetails::IsNodeDescriptionVisible() const
{
	const UScriptStruct* ScriptStruct = nullptr;
	if (const FMetaStoryEditorNode* Node = UE::MetaStoryEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		ScriptStruct = Node->Node.GetScriptStruct();
	}

	if (ScriptStruct != nullptr && ScriptStruct->IsChildOf(FMetaStoryTaskBase::StaticStruct()))
	{
		const UMetaStoryEditorData* EditorDataPtr = EditorData.Get();
		const UMetaStorySchema* Schema = EditorDataPtr ? EditorDataPtr->Schema.Get() : nullptr;
		if (Schema && Schema->AllowMultipleTasks() == false)
		{
			// Single task states use the state name as task name.
			return EVisibility::Collapsed;
		}
	}
	
	return EVisibility::Visible;
}

FText FMetaStoryEditorNodeDetails::GetNodeTooltip() const
{
	check(StructProperty);

	const UMetaStoryEditorData* EditorDataPtr = EditorData.Get();
	if (!EditorDataPtr)
	{
		return FText::GetEmpty();
	}
	
	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);
	if (RawNodeData.Num() == 1)
	{
		FText NameText;
		FText PathText;
		FText DescText;

		if (const FMetaStoryEditorNode* Node = static_cast<FMetaStoryEditorNode*>(RawNodeData[0]))
		{
			const UStruct* Struct = Node->GetInstance().GetStruct();
			if (Struct == nullptr || !Struct->IsChildOf<UMetaStoryNodeBlueprintBase>())
			{
				Struct = Node->Node.GetScriptStruct();
			}

			if (Struct)
			{
				static const FName NAME_Tooltip(TEXT("Tooltip"));
				const FText StructToolTipText = Struct->HasMetaData(NAME_Tooltip) ? Struct->GetToolTipText() : FText::GetEmpty();

				FTextBuilder TooltipBuilder;
				TooltipBuilder.AppendLineFormat(LOCTEXT("NodeTooltip", "{0} ({1})"), Struct->GetDisplayNameText(), FText::FromString(Struct->GetPathName()));

				if (!StructToolTipText.IsEmpty())
				{
					TooltipBuilder.AppendLine();
					TooltipBuilder.AppendLine(StructToolTipText);
				}
				return TooltipBuilder.ToText();
			}
		}
	}

	return FText::GetEmpty();
}

FText FMetaStoryEditorNodeDetails::GetName() const
{
	check(StructProperty);

	// Multiple names do not make sense, just if only one node is selected.
	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);
	if (RawNodeData.Num() == 1)
	{
		if (const FMetaStoryEditorNode* Node = static_cast<FMetaStoryEditorNode*>(RawNodeData[0]))
		{
			if (const FMetaStoryNodeBase* BaseNode = Node->Node.GetPtr<FMetaStoryNodeBase>())
			{
				if (!BaseNode->Name.IsNone())
				{
					return FText::FromName(BaseNode->Name);
				}
				const FText Desc = EditorData->GetNodeDescription(*Node, EMetaStoryNodeFormatting::Text);
				if (!Desc.IsEmpty())
				{
					return Desc;
				}
			}
		}

		return FText::GetEmpty();
	}

	return LOCTEXT("MultipleSelected", "Multiple Selected");
}

bool FMetaStoryEditorNodeDetails::HandleVerifyNameChanged(const FText& InText, FText& OutErrorMessage) const
{
	const FString NewName = FText::TrimPrecedingAndTrailing(InText).ToString();
	if (NewName.Len() >= NAME_SIZE)
	{
		OutErrorMessage = LOCTEXT("VerifyNodeLabelFailed_MaxLength", "Max length exceeded");
		return false;
	}
	return NewName.Len() > 0;
}

void FMetaStoryEditorNodeDetails::HandleNameCommitted(const FText& NewText, ETextCommit::Type InTextCommit) const
{
	check(StructProperty);

	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		// Remove excess whitespace and prevent categories with just spaces
		const FString NewName = FText::TrimPrecedingAndTrailing(NewText).ToString();
		if (NewName.Len() > 0 && NewName.Len() < NAME_SIZE)
		{
			if (GEditor)
			{
				GEditor->BeginTransaction(LOCTEXT("SetName", "Set Name"));
			}
			StructProperty->NotifyPreChange();

			TArray<void*> RawNodeData;
			StructProperty->AccessRawData(RawNodeData);

			for (void* Data : RawNodeData)
			{
				// Set Name
				if (FMetaStoryEditorNode* Node = static_cast<FMetaStoryEditorNode*>(Data))
				{
					if (FMetaStoryNodeBase* BaseNode = Node->Node.GetMutablePtr<FMetaStoryNodeBase>())
					{
						BaseNode->Name = FName(NewName);
					}
				}
			}

			StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);

			if (const UMetaStory* MetaStoryPtr = MetaStory.Get())
			{
				UE::MetaStory::Delegates::OnIdentifierChanged.Broadcast(*MetaStoryPtr);
			}

			if (GEditor)
			{
				GEditor->EndTransaction();
			}

			StructProperty->NotifyFinishedChangingProperties();
		}
	}

	// Switch back to rich view.
	NameSwitcher->SetActiveWidgetIndex(0);
}

FReply FMetaStoryEditorNodeDetails::HandleToggleCompletionTaskClicked()
{
	UE::MetaStoryEditor::EditorNodeUtils::ModifyNodeInTransaction(LOCTEXT("OnCompletionTaskToggled", "Toggled Completion Task"),
		StructProperty,
		[](const TSharedPtr<IPropertyHandle>& StructProperty)
		{
			if (FMetaStoryEditorNode* Node = UE::MetaStoryEditor::EditorNodeUtils::GetMutableCommonNode(StructProperty))
			{
				if (UE::MetaStoryEditor::EditorNodeUtils::IsTaskEnabled(*Node))
				{
					const bool bCurrentValue = UE::MetaStoryEditor::EditorNodeUtils::IsTaskConsideredForCompletion(*Node);
					UE::MetaStoryEditor::EditorNodeUtils::SetTaskConsideredForCompletion(*Node, !bCurrentValue);
				}
			}
		});

	return FReply::Handled();
}

FText FMetaStoryEditorNodeDetails::GetToggleCompletionTaskTooltip() const
{
	if (const FMetaStoryEditorNode* Node = UE::MetaStoryEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		if (UE::MetaStoryEditor::EditorNodeUtils::IsTaskConsideredForCompletion(*Node))
		{
			return LOCTEXT("ToggleTaskCompletionEnabled", "Toggle Completion.\n"
				"The task is considered for state completion.\n"
				"When the task completes, it will stop ticking, and the state can be considered for transition.");
		}
		else
		{
			return LOCTEXT("ToggleTaskCompletionDisabled", "Toggle Completion.\n"
				"The task doesn't affect the state completion.\n"
				"When the task completes, it will stop ticking.");
		}
	}
	return FText::GetEmpty();
}

FSlateColor FMetaStoryEditorNodeDetails::GetToggleCompletionTaskColor() const
{
	if (const FMetaStoryEditorNode* Node = UE::MetaStoryEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		if (UE::MetaStoryEditor::EditorNodeUtils::IsTaskConsideredForCompletion(*Node))
		{
			return UE::MetaStory::Colors::Cyan;
		}
	}
	return FSlateColor(EStyleColor::Foreground);
}

const FSlateBrush* FMetaStoryEditorNodeDetails::GetToggleCompletionTaskIcon() const
{
	if (const FMetaStoryEditorNode* Node = UE::MetaStoryEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		if (UE::MetaStoryEditor::EditorNodeUtils::IsTaskConsideredForCompletion(*Node))
		{
			return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.TasksCompletion.Enabled");
		}
		else
		{
			return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.TasksCompletion.Disabled");
		}
	}
	return nullptr;
}

EVisibility FMetaStoryEditorNodeDetails::GetToggleCompletionTaskVisibility() const
{
	if (const FMetaStoryEditorNode* Node = UE::MetaStoryEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		return UE::MetaStoryEditor::EditorNodeUtils::IsTaskEnabled(*Node) ? EVisibility::Visible : EVisibility::Hidden;
	}
	return EVisibility::Collapsed;
}

FText FMetaStoryEditorNodeDetails::GetNodePickerTooltip() const
{
	check(StructProperty);

	const UMetaStoryEditorData* EditorDataPtr = EditorData.Get();
	if (!EditorDataPtr)
	{
		return FText::GetEmpty();
	}

	FTextBuilder TextBuilder;

	// Append full description.
	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);
	if (RawNodeData.Num() == 1)
	{
		FText Description = LOCTEXT("EmptyNodeStyled", "<s>None</>");
		if (const FMetaStoryEditorNode* Node = static_cast<FMetaStoryEditorNode*>(RawNodeData[0]))
		{
			TextBuilder.AppendLine(EditorDataPtr->GetNodeDescription(*Node));
		}
	}

	if (TextBuilder.GetNumLines() > 0)
	{
		TextBuilder.AppendLine(FText::GetEmpty());
	}
	
	// Text describing the type.
	if (const FMetaStoryEditorNode* Node = UE::MetaStoryEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		if (const UScriptStruct* ScriptStruct = Node->Node.GetScriptStruct())
		{
			if (ScriptStruct->IsChildOf(FMetaStoryBlueprintEvaluatorWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FMetaStoryBlueprintTaskWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FMetaStoryBlueprintConditionWrapper::StaticStruct()))
			{
				if (Node->InstanceObject != nullptr
					&& Node->InstanceObject->GetClass() != nullptr)
				{
					TextBuilder.AppendLine(Node->InstanceObject->GetClass()->GetDisplayNameText());
				}
			}
			else
			{
				TextBuilder.AppendLine(ScriptStruct->GetDisplayNameText());
			}
		}
	}

	return TextBuilder.ToText();
}

namespace UE::MetaStory::Editor::Private
{
	const UScriptStruct* GetNodeStruct(const TSharedPtr<IPropertyHandle>& NodeProperty)
	{
		void* Address = nullptr;
		const FPropertyAccess::Result AccessResult = NodeProperty->GetValueData(Address);
		if (AccessResult == FPropertyAccess::Success && Address)
		{
			FInstancedStruct* InstancedStruct = static_cast<FInstancedStruct*>(Address);
			return InstancedStruct->GetScriptStruct();
		}
		return nullptr;
	}

	const UClass* GetInstanceObjectClass(const TSharedPtr<IPropertyHandle>& InstanceObjectProperty)
	{
		const UObject* InstanceObject = nullptr;
		const FPropertyAccess::Result AccessResult = InstanceObjectProperty->GetValue(InstanceObject);
		if (AccessResult == FPropertyAccess::Success && InstanceObject)
		{
			return InstanceObject->GetClass();
		}
		return nullptr;
	}
}

FReply FMetaStoryEditorNodeDetails::OnBrowseToSource() const
{
	using namespace UE::MetaStory::Editor::Private;
	if (const UScriptStruct* Node = GetNodeStruct(NodeProperty))
	{
		FSourceCodeNavigation::NavigateToStruct(Node);
	}

	return FReply::Handled();
}

FReply FMetaStoryEditorNodeDetails::OnBrowseToNodeBlueprint() const
{
	using namespace UE::MetaStory::Editor::Private;
	if (const UBlueprintGeneratedClass* BlueprintGeneratedClass = Cast<const UBlueprintGeneratedClass>(GetInstanceObjectClass(InstanceObjectProperty)))
	{
		//If the blueprint asset has been cooked, UBlueprint Object will be set to null and we need to browse to its BlueprintGeneratedClass
		GEditor->SyncBrowserToObject(BlueprintGeneratedClass->ClassGeneratedBy ? BlueprintGeneratedClass->ClassGeneratedBy.Get() : BlueprintGeneratedClass);
	}

	return FReply::Handled();
}

FReply FMetaStoryEditorNodeDetails::OnEditNodeBlueprint() const
{
	//Cooked blueprint asset is not editable.
	using namespace UE::MetaStory::Editor::Private;
	const UBlueprintGeneratedClass* BlueprintGeneratedClass = Cast<const UBlueprintGeneratedClass>(GetInstanceObjectClass(InstanceObjectProperty));
	if (BlueprintGeneratedClass && BlueprintGeneratedClass->ClassGeneratedBy)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(BlueprintGeneratedClass->ClassGeneratedBy);
	}

	return FReply::Handled();
}

EVisibility FMetaStoryEditorNodeDetails::IsBrowseToSourceVisible() const
{
	using namespace UE::MetaStory::Editor::Private;
	if (!Cast<const UBlueprintGeneratedClass>(GetInstanceObjectClass(InstanceObjectProperty)))
	{
		if (const UScriptStruct* Node = GetNodeStruct(NodeProperty))
		{
			if (FSourceCodeNavigation::CanNavigateToStruct(Node))
			{
				return EVisibility::Visible;
			}
		}
	}
	return EVisibility::Collapsed;
}

EVisibility FMetaStoryEditorNodeDetails::IsBrowseToNodeBlueprintVisible() const
{
	using namespace UE::MetaStory::Editor::Private;
	const UBlueprintGeneratedClass* BlueprintGeneratedClass = Cast<const UBlueprintGeneratedClass>(GetInstanceObjectClass(InstanceObjectProperty));
	return BlueprintGeneratedClass != nullptr ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FMetaStoryEditorNodeDetails::IsEditNodeBlueprintVisible() const
{
	//Cooked blueprint asset is not editable
	using namespace UE::MetaStory::Editor::Private;
	const UBlueprintGeneratedClass* BlueprintGeneratedClass = Cast<const UBlueprintGeneratedClass>(GetInstanceObjectClass(InstanceObjectProperty));
	return BlueprintGeneratedClass != nullptr && BlueprintGeneratedClass->ClassGeneratedBy ? EVisibility::Visible : EVisibility::Collapsed;
}

void FMetaStoryEditorNodeDetails::GeneratePickerMenu(class FMenuBuilder& InMenuBuilder)
{
	// Expand and select currently selected item.
	const UStruct* CommonStruct  = nullptr;
	if (const FMetaStoryEditorNode* Node = UE::MetaStoryEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		if (const UScriptStruct* ScriptStruct = Node->Node.GetScriptStruct())
		{
			if (ScriptStruct->IsChildOf(FMetaStoryBlueprintEvaluatorWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FMetaStoryBlueprintTaskWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FMetaStoryBlueprintConditionWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FMetaStoryBlueprintConsiderationWrapper::StaticStruct()))
			{
				if (Node->InstanceObject != nullptr)
				{
					CommonStruct = Node->InstanceObject->GetClass();
				}
			}
			else
			{
				CommonStruct = ScriptStruct;
			}
		}
	}

	TSharedRef<SMetaStoryNodeTypePicker> Picker = SNew(SMetaStoryNodeTypePicker)
		.Schema(EditorData->Schema)
		.BaseScriptStruct(BaseScriptStruct.Get())
		.BaseClass(BaseClass.Get())
		.CurrentStruct(CommonStruct)
		.OnNodeTypePicked(SMetaStoryNodeTypePicker::FOnNodeStructPicked::CreateSP(this, &FMetaStoryEditorNodeDetails::OnNodePicked));
	
	InMenuBuilder.AddWidget(SNew(SBox)
		.MinDesiredWidth(400.f)
		.MinDesiredHeight(300.f)
		.MaxDesiredHeight(300.f)
		.Padding(2)	
		[
			Picker
		],
		FText::GetEmpty(), /*bNoIdent*/true);
}
	
TSharedRef<SWidget> FMetaStoryEditorNodeDetails::GenerateOptionsMenu()
{
	FMenuBuilder MenuBuilder(/*ShouldCloseWindowAfterMenuSelection*/true, /*CommandList*/nullptr);

	MenuBuilder.BeginSection(FName("Type"), LOCTEXT("Type", "Type"));

	// Change type
	MenuBuilder.AddSubMenu(
		LOCTEXT("ReplaceWith", "Replace With"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateSP(this, &FMetaStoryEditorNodeDetails::GeneratePickerMenu));

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(FName("Edit"), LOCTEXT("Edit", "Edit"));

	// Copy
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CopyItem", "Copy"),
		LOCTEXT("CopyItemTooltip", "Copy this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::OnCopyNode),
			FCanExecuteAction()
		));

	// Copy all
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CopyAllItems", "Copy all"),
		LOCTEXT("CopyAllItemsTooltip", "Copy all items"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::OnCopyAllNodes),
			FCanExecuteAction()
		));

	// Paste
	MenuBuilder.AddMenuEntry(
		LOCTEXT("PasteItem", "Paste"),
		LOCTEXT("PasteItemTooltip", "Paste into this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::OnPasteNodes),
			FCanExecuteAction()
		));

	// Duplicate
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DuplicateItem", "Duplicate"),
		LOCTEXT("DuplicateItemTooltip", "Duplicate this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Duplicate"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::OnDuplicateNode),
			FCanExecuteAction()
		));

	// Delete
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeleteItem", "Delete"),
		LOCTEXT("DeleteItemTooltip", "Delete this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::OnDeleteNode),
			FCanExecuteAction()
		));

	// Delete All
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeleteAllItems", "Delete all"),
		LOCTEXT("DeleteAllItemsTooltip", "Delete all items"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::OnDeleteAllNodes),
			FCanExecuteAction()
		));

	// Rename
	MenuBuilder.AddMenuEntry(
		LOCTEXT("RenameNode", "Rename"),
		LOCTEXT("RenameNodeTooltip", "Rename this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Rename"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryEditorNodeDetails::OnRenameNode),
			FCanExecuteAction()
		));

	MenuBuilder.EndSection();

	// Append debugger items.
	UE::MetaStoryEditor::DebuggerExtensions::AppendEditorNodeMenuItems(MenuBuilder, StructProperty, MetaStoryViewModel);

	return MenuBuilder.MakeWidget();
}

void FMetaStoryEditorNodeDetails::OnDeleteNode() const
{
	const int32 Index = StructProperty->GetArrayIndex();
	if (ParentArrayProperty)
	{
		if (UMetaStoryEditorData* EditorDataPtr = EditorData.Get())
		{
			FScopedTransaction Transaction(LOCTEXT("DeleteNode", "Delete Node"));

			EditorDataPtr->Modify();

			ParentArrayProperty->DeleteItem(Index);

			UE::MetaStoryEditor::RemoveInvalidBindings(EditorDataPtr);
		}
	}
}

void FMetaStoryEditorNodeDetails::OnDeleteAllNodes() const
{
	if (const TSharedPtr<IPropertyHandle> ParentHandle = StructProperty->GetParentHandle())
	{
		if (const TSharedPtr<IPropertyHandleArray> ArrayHandle = ParentHandle->AsArray())
		{
			if (UMetaStoryEditorData* EditorDataPtr = EditorData.Get())
			{
				FScopedTransaction Transaction(LOCTEXT("DeleteAllNodes", "Delete All Nodes"));

				EditorDataPtr->Modify();

				ArrayHandle->EmptyArray();

				UE::MetaStoryEditor::RemoveInvalidBindings(EditorDataPtr);
			}
		}
	}
}

void FMetaStoryEditorNodeDetails::OnDuplicateNode() const
{
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	if (OuterObjects.IsEmpty())
	{
		return;
	}

	if (OuterObjects.Num() != 1)
	{
		// Array Handle Manipulation doesn't support multiple selected objects
		FNotificationInfo NotificationInfo(FText::GetEmpty());
		NotificationInfo.Text = LOCTEXT("NotSupportedByMultipleObjects", "Operation is not supported for multi-selected objects");
		NotificationInfo.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);

		return;
	}

	if (ParentArrayProperty)
	{
		if (UMetaStoryEditorData* EditorDataPtr = EditorData.Get())
		{
			void* NodePtr = nullptr;
			if (StructProperty->GetValueData(NodePtr) == FPropertyAccess::Success)
			{
				UE::MetaStoryEditor::FMetaStoryClipboardEditorData Clipboard;
				Clipboard.Append(EditorDataPtr, TConstArrayView<FMetaStoryEditorNode>(static_cast<FMetaStoryEditorNode*>(NodePtr), 1));
				//UE::MetaStoryEditor::ExportTextAsClipboardEditorData()
				Clipboard.ProcessBuffer(nullptr, EditorDataPtr, OuterObjects[0]);

				if (!Clipboard.IsValid())
				{
					return;
				}

				FScopedTransaction Transaction(LOCTEXT("DuplicateNode", "Duplicate Node"));

				// Might modify the bindings data
				EditorDataPtr->Modify();

				const int32 Index = StructProperty->GetArrayIndex();
				ParentArrayProperty->Insert(Index);

				TSharedPtr<IPropertyHandle> InsertedElementHandle = ParentArrayProperty->GetElement(Index);
				void* InsertedNodePtr = nullptr;
				if (InsertedElementHandle->GetValueData(InsertedNodePtr) == FPropertyAccess::Success)
				{
					*static_cast<FMetaStoryEditorNode*>(InsertedNodePtr) = MoveTemp(Clipboard.GetEditorNodesInBuffer()[0]);

					for (FMetaStoryPropertyPathBinding& Binding : Clipboard.GetBindingsInBuffer())
					{
						EditorDataPtr->GetPropertyEditorBindings()->AddMetaStoryBinding(MoveTemp(Binding));
					}
				}

				// We reinieitalized item nodes on ArrayProperty operations before the data is completely set up. Reinitialize.
				if (PropUtils)
				{
					PropUtils->ForceRefresh();
				}
			}
		}
	}

	
}

void FMetaStoryEditorNodeDetails::OnRenameNode() const
{
	if (NameSwitcher && NameEdit)
	{
		if (NameSwitcher->GetActiveWidgetIndex() == 0)
		{
			// Enter edit mode
			NameSwitcher->SetActiveWidgetIndex(1);

			FSlateApplication::Get().SetKeyboardFocus(NameEdit);
			FSlateApplication::Get().SetUserFocus(0, NameEdit);
			NameEdit->EnterEditingMode();
		}
	}
}

// @todo: refactor it to use FMetaStoryEditorDataFixer
void FMetaStoryEditorNodeDetails::OnNodePicked(const UStruct* InStruct) const
{
	GEditor->BeginTransaction(LOCTEXT("SelectNode", "Select Node"));

	StructProperty->NotifyPreChange();

	UE::MetaStoryEditor::EditorNodeUtils::SetNodeType(StructProperty, InStruct);

	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();

	GEditor->EndTransaction();

	FSlateApplication::Get().DismissAllMenus();
	
	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

void FMetaStoryEditorNodeDetails::HandleAssetChanged()
{
	MakeFlagsWidget();
}

#undef LOCTEXT_NAMESPACE
