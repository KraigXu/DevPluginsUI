#include "Scenario/MetaplotEditorNodeUtils.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Customizations/Widgets/SMetaplotNodeTypePicker.h"
#include "Flow/MetaplotFlow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Runtime/MetaplotStoryTask.h"
#include "Scenario/MetaplotEditorTaskNode.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MetaplotEditorStyle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace UE::MetaplotEditor::EditorNodeUtils
{
namespace
{
	UObject* ResolveArrayOuterObject(const TSharedPtr<IPropertyHandle>& ArrayPropertyHandle)
	{
		if (!ArrayPropertyHandle.IsValid() || !ArrayPropertyHandle->IsValidHandle())
		{
			return nullptr;
		}

		TArray<UObject*> OuterObjects;
		ArrayPropertyHandle->GetOuterObjects(OuterObjects);
		for (UObject* OuterObject : OuterObjects)
		{
			if (!OuterObject)
			{
				continue;
			}

			if (Cast<UMetaplotFlow>(OuterObject))
			{
				return OuterObject;
			}
			if (UMetaplotFlow* OuterFlow = OuterObject->GetTypedOuter<UMetaplotFlow>())
			{
				return OuterFlow;
			}
		}

		return nullptr;
	}

	void OnArrayTaskNodePicked(
		UClass* PickedClass,
		TSharedPtr<SComboButton> PickerCombo,
		TSharedPtr<IPropertyHandle> ArrayPropertyHandle,
		TSharedRef<IPropertyUtilities> PropUtils)
	{
		if (!PickedClass || !ArrayPropertyHandle.IsValid() || !ArrayPropertyHandle->IsValidHandle())
		{
			if (PickerCombo.IsValid())
			{
				PickerCombo->SetIsOpen(false);
			}
			return;
		}

		UObject* InstanceOuter = ResolveArrayOuterObject(ArrayPropertyHandle);
		if (!InstanceOuter)
		{
			if (PickerCombo.IsValid())
			{
				PickerCombo->SetIsOpen(false);
			}
			return;
		}

		FMetaplotEditorNodeUtils::ModifyNodeInTransaction(
			FText::FromString(TEXT("Add Metaplot Task")),
			ArrayPropertyHandle,
			[&]()
			{
				const TSharedPtr<IPropertyHandleArray> ArrayHandle = ArrayPropertyHandle->AsArray();
				if (!ArrayHandle.IsValid())
				{
					return false;
				}

				uint32 OldNum = 0;
				if (ArrayHandle->GetNumElements(OldNum) != FPropertyAccess::Success)
				{
					return false;
				}
				if (ArrayHandle->AddItem() != FPropertyAccess::Success)
				{
					return false;
				}

				uint32 NewNum = 0;
				if (ArrayHandle->GetNumElements(NewNum) != FPropertyAccess::Success || NewNum == 0)
				{
					return false;
				}

				const uint32 NewIndex = (NewNum > OldNum) ? (NewNum - 1) : OldNum;
				const TSharedPtr<IPropertyHandle> NewNodeHandle = ArrayHandle->GetElement(NewIndex);
				return FMetaplotEditorNodeUtils::SetNodeType(NewNodeHandle, PickedClass, InstanceOuter);
			});

		PropUtils->ForceRefresh();
		if (PickerCombo.IsValid())
		{
			PickerCombo->SetIsOpen(false);
		}
	}

	TSharedRef<SWidget> GenerateArrayNodePicker(
		TSharedPtr<SComboButton> PickerCombo,
		TSharedPtr<IPropertyHandle> ArrayPropertyHandle,
		TSharedRef<IPropertyUtilities> PropUtils)
	{
		return SNew(SBox)
			.MinDesiredWidth(400.f)
			.MinDesiredHeight(300.f)
			.MaxDesiredHeight(300.f)
			.Padding(2.f)
			[
				SNew(SMetaplotNodeTypePicker)
				.BaseClass(UMetaplotStoryTask::StaticClass())
				.BaseStruct(UMetaplotStoryTask::StaticClass())
				.OnNodeTypePicked(FOnMetaplotNodeTypePicked::CreateStatic(
					&OnArrayTaskNodePicked,
					PickerCombo,
					ArrayPropertyHandle,
					PropUtils))
			];
	}

	TSharedRef<SWidget> CreateAddNodePickerComboButton(
		const FText& TooltipText,
		const FLinearColor AddIconColor,
		const TSharedPtr<IPropertyHandle>& ArrayPropertyHandle,
		const TSharedRef<IPropertyUtilities>& PropUtils)
	{
		const TSharedRef<SComboButton> PickerCombo = SNew(SComboButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.HasDownArrow(false)
			.ToolTipText(TooltipText)
			.ContentPadding(FMargin(4.f, 2.f))
			.IsEnabled(PropUtils, &IPropertyUtilities::IsPropertyEditingEnabled)
			.ButtonContent()
			[
				SNew(SImage)
				.Image(FMetaplotEditorStyle::Get().GetBrush(TEXT("MetaplotEditor.Add")))
				.ColorAndOpacity(AddIconColor)
			];

		PickerCombo->SetOnGetMenuContent(FOnGetContent::CreateStatic(
			&GenerateArrayNodePicker,
			PickerCombo.ToSharedPtr(),
			ArrayPropertyHandle,
			PropUtils));

		return PickerCombo;
	}

	TSharedRef<SWidget> CreateAddItemButton(
		const FText& TooltipText,
		const FLinearColor AddIconColor,
		const TSharedPtr<IPropertyHandle>& ArrayPropertyHandle,
		const TSharedRef<IPropertyUtilities>& PropUtils)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(TooltipText)
			.IsEnabled(PropUtils, &IPropertyUtilities::IsPropertyEditingEnabled)
			.OnClicked_Lambda([ArrayPropertyHandle]()
			{
				if (!ArrayPropertyHandle.IsValid() || !ArrayPropertyHandle->IsValidHandle())
				{
					return FReply::Handled();
				}

				if (const TSharedPtr<IPropertyHandleArray> ArrayHandle = ArrayPropertyHandle->AsArray())
				{
					ArrayHandle->AddItem();
				}
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FMetaplotEditorStyle::Get().GetBrush(TEXT("MetaplotEditor.Add")))
				.ColorAndOpacity(AddIconColor)
			];
	}
}

IDetailCategoryBuilder& MakeArrayCategoryHeader(
	IDetailLayoutBuilder& DetailBuilder,
	const TSharedPtr<IPropertyHandle>& ArrayPropertyHandle,
	const FName CategoryName,
	const FText& CategoryDisplayName,
	const FName IconName,
	const FLinearColor IconColor,
	const TSharedPtr<SWidget> Extension,
	const FLinearColor AddIconColor,
	const FText& AddButtonTooltipText,
	const int32 SortOrder)
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, CategoryDisplayName);
	Category.SetSortOrder(SortOrder);

	const TSharedPtr<IPropertyUtilities> PropertyUtils = DetailBuilder.GetPropertyUtilities();
	check(PropertyUtils.IsValid());
	bool bIsTaskNodeArray = false;
	const bool bHasValidArrayHandle = ArrayPropertyHandle.IsValid() && ArrayPropertyHandle->IsValidHandle();
	if (bHasValidArrayHandle)
	{
		if (const FArrayProperty* ArrayProperty = CastField<const FArrayProperty>(ArrayPropertyHandle->GetProperty()))
		{
			if (const FStructProperty* InnerStruct = CastField<const FStructProperty>(ArrayProperty->Inner))
			{
				bIsTaskNodeArray = InnerStruct->Struct == FMetaplotEditorTaskNode::StaticStruct();
			}
		}
	}

	TSharedRef<SWidget> AddWidget = SNullWidget::NullWidget;
	if (bHasValidArrayHandle)
	{
		AddWidget = bIsTaskNodeArray
			? CreateAddNodePickerComboButton(
				AddButtonTooltipText,
				AddIconColor,
				ArrayPropertyHandle,
				PropertyUtils.ToSharedRef())
			: CreateAddItemButton(
				AddButtonTooltipText,
				AddIconColor,
				ArrayPropertyHandle,
				PropertyUtils.ToSharedRef());
	}
	
	const TSharedRef<SHorizontalBox> HeaderContent = SNew(SHorizontalBox);

	if (!IconName.IsNone())
	{
		HeaderContent->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(FMargin(4, 0, 0, 0))
			[
				SNew(SImage)
				.ColorAndOpacity(IconColor)
				.Image(FMetaplotEditorStyle::Get().GetBrush(IconName))
			];
	}
	
	HeaderContent->AddSlot()
		.FillWidth(1.f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(4, 0, 0, 0))
		[
			SNew(STextBlock)
			.TextStyle(&FMetaplotEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("Metaplot.Category")))
			.Text(CategoryDisplayName)
		];

	if (Extension)
	{
		HeaderContent->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				Extension.ToSharedRef()
			];
	}


	HeaderContent->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			AddWidget
		];
	
	Category.HeaderContent(SNew(SBox)
		.MinDesiredHeight(30.f)
		[
			HeaderContent
		],
		/*bWholeRowContent*/true);
	return Category;
}
	
	
void MakeArrayItems(IDetailCategoryBuilder& Category, const TSharedPtr<IPropertyHandle>& ArrayPropertyHandle)
{
	if (!ArrayPropertyHandle.IsValid() || !ArrayPropertyHandle->IsValidHandle())
	{
		return;
	}

	// Add items inline
	const TSharedRef<FDetailArrayBuilder> Builder = MakeShareable(new FDetailArrayBuilder(ArrayPropertyHandle.ToSharedRef(), /*InGenerateHeader*/ false, /*InDisplayResetToDefault*/ true, /*InDisplayElementNum*/ false));
	Builder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda([](TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
	{
		ChildrenBuilder.AddProperty(PropertyHandle);
	}));
	Category.AddCustomBuilder(Builder, /*bForAdvanced*/ false);
}
	
}

bool FMetaplotEditorNodeUtils::ModifyNodeInTransaction(
	const FText& TransactionText,
	const TSharedPtr<IPropertyHandle>& ChangedHandle,
	TFunctionRef<bool()> ModifyOperation)
{
	if (!ChangedHandle.IsValid() || !ChangedHandle->IsValidHandle())
	{
		return false;
	}

	const FScopedTransaction ScopedTransaction(TransactionText);
	ChangedHandle->NotifyPreChange();

	const bool bSuccess = ModifyOperation();
	ChangedHandle->NotifyPostChange(bSuccess ? EPropertyChangeType::ValueSet : EPropertyChangeType::Unspecified);
	ChangedHandle->NotifyFinishedChangingProperties();
	return bSuccess;
}

void FMetaplotEditorNodeUtils::MakeArrayCategoryHeader(
	IDetailCategoryBuilder& CategoryBuilder,
	TFunction<TSharedRef<SWidget>()> BuildAddMenuWidget)
{
	CategoryBuilder.AddCustomRow(FText::GetEmpty())
		.WholeRowContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>(TEXT("SimpleComboButton")))
				.HasDownArrow(false)
				.ToolTipText(FText::FromString(TEXT("Add new Task")))
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FMetaplotEditorStyle::Get().GetBrush(TEXT("MetaplotEditor.Add")))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				.MenuContent()
				[
					BuildAddMenuWidget()
				]
			]
		];
}


bool FMetaplotEditorNodeUtils::SetNodeType(
	const TSharedPtr<IPropertyHandle>& NodeHandle,
	UClass* SelectedTaskClass,
	UObject* InstanceOuter)
{
	if (!NodeHandle.IsValid() || !NodeHandle->IsValidHandle() || !SelectedTaskClass || !InstanceOuter)
	{
		return false;
	}

	const TSharedPtr<IPropertyHandle> IdHandle = NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, ID));
	const TSharedPtr<IPropertyHandle> TaskClassHandle = NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, TaskClass));
	const TSharedPtr<IPropertyHandle> InstanceHandle = NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, InstanceObject));
	const TSharedPtr<IPropertyHandle> EnabledHandle = NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, bEnabled));
	const TSharedPtr<IPropertyHandle> CompletionHandle = NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, bConsideredForCompletion));
	if (!TaskClassHandle.IsValid() || !TaskClassHandle->IsValidHandle() || !InstanceHandle.IsValid() || !InstanceHandle->IsValidHandle())
	{
		return false;
	}

	UMetaplotStoryTask* CreatedTask = nullptr;
	if (SelectedTaskClass->IsChildOf(UMetaplotStoryTask::StaticClass()))
	{
		CreatedTask = NewObject<UMetaplotStoryTask>(InstanceOuter, SelectedTaskClass, NAME_None, RF_Transactional);
	}

	if (IdHandle.IsValid() && IdHandle->IsValidHandle())
	{
		const FString NewIdString = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensInBraces);
		IdHandle->SetValueFromFormattedString(NewIdString);
	}
	const FSoftObjectPath ClassPath(SelectedTaskClass);
	TaskClassHandle->SetValueFromFormattedString(ClassPath.ToString());
	InstanceHandle->SetValue(CreatedTask);
	if (EnabledHandle.IsValid() && EnabledHandle->IsValidHandle())
	{
		EnabledHandle->SetValue(true);
	}
	if (CompletionHandle.IsValid() && CompletionHandle->IsValidHandle())
	{
		CompletionHandle->SetValue(true);
	}
	if (UMetaplotFlow* Flow = Cast<UMetaplotFlow>(InstanceOuter))
	{
		Flow->NormalizeEditorTaskNodes();
	}
	NodeHandle->SetExpanded(true);
	return true;
}

bool FMetaplotEditorNodeUtils::ConditionalUpdateNodeInstanceData(
	const TSharedPtr<IPropertyHandle>& NodeHandle,
	UObject* InstanceOuter)
{
	return EnsureNodeInstanceMatchesClass(NodeHandle, InstanceOuter);
}

bool FMetaplotEditorNodeUtils::EnsureNodeInstanceMatchesClass(
	const TSharedPtr<IPropertyHandle>& NodeHandle,
	UObject* InstanceOuter)
{
	if (!NodeHandle.IsValid() || !NodeHandle->IsValidHandle() || !InstanceOuter)
	{
		return false;
	}

	const TSharedPtr<IPropertyHandle> TaskClassHandle = NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, TaskClass));
	const TSharedPtr<IPropertyHandle> InstanceHandle = NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, InstanceObject));
	if (!TaskClassHandle.IsValid() || !TaskClassHandle->IsValidHandle() || !InstanceHandle.IsValid() || !InstanceHandle->IsValidHandle())
	{
		return false;
	}

	FString ClassPathString;
	if (TaskClassHandle->GetValueAsFormattedString(ClassPathString) != FPropertyAccess::Success)
	{
		return false;
	}

	const FSoftClassPath TaskClassPath(ClassPathString);
	UClass* TaskClass = TaskClassPath.TryLoadClass<UMetaplotStoryTask>();

	UObject* ExistingObject = nullptr;
	InstanceHandle->GetValue(ExistingObject);
	UMetaplotStoryTask* ExistingTask = Cast<UMetaplotStoryTask>(ExistingObject);

	if (!TaskClass || !TaskClass->IsChildOf(UMetaplotStoryTask::StaticClass()))
	{
		return InstanceHandle->SetValue(static_cast<UObject*>(nullptr)) == FPropertyAccess::Success;
	}

	if (ExistingTask && ExistingTask->GetClass() == TaskClass)
	{
		if (ExistingTask->GetOuter() != InstanceOuter)
		{
			ExistingTask->Rename(nullptr, InstanceOuter, REN_DontCreateRedirectors | REN_NonTransactional);
		}
		return true;
	}

	UMetaplotStoryTask* CreatedTask = NewObject<UMetaplotStoryTask>(InstanceOuter, TaskClass, NAME_None, RF_Transactional);
	const bool bSetSuccess = InstanceHandle->SetValue(CreatedTask) == FPropertyAccess::Success;
	if (bSetSuccess)
	{
		if (UMetaplotFlow* Flow = Cast<UMetaplotFlow>(InstanceOuter))
		{
			Flow->NormalizeEditorTaskNodes();
		}
	}
	return bSetSuccess;
}

