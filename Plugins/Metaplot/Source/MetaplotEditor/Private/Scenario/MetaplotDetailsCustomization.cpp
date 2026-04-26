#include "Scenario/MetaplotDetailsCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MetaplotEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Runtime/MetaplotStoryTask.h"
#include "Scenario/MetaplotDetailsProxy.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace MetaplotDetailsCustomizationPrivate
{
	static EVisibility GetVisibilityByConditionType(const TSharedPtr<IPropertyHandle>& TypeHandle, EMetaplotConditionType ExpectedType)
	{
		if (!TypeHandle.IsValid() || !TypeHandle->IsValidHandle())
		{
			return EVisibility::Collapsed;
		}

		uint8 TypeValue = static_cast<uint8>(EMetaplotConditionType::RequiredNodeCompleted);
		if (TypeHandle->GetValue(TypeValue) != FPropertyAccess::Success)
		{
			return EVisibility::Collapsed;
		}

		return static_cast<EMetaplotConditionType>(TypeValue) == ExpectedType
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}

	static bool IsBlackboardCompare(const TSharedPtr<IPropertyHandle>& TypeHandle)
	{
		return GetVisibilityByConditionType(TypeHandle, EMetaplotConditionType::BlackboardCompare) == EVisibility::Visible;
	}

	static bool TryResolveBlackboardType(
		const TSharedPtr<IPropertyHandle>& BlackboardKeyHandle,
		const TSharedPtr<IPropertyUtilities>& PropertyUtils,
		EMetaplotBlackboardType& OutType)
	{
		if (!BlackboardKeyHandle.IsValid() || !BlackboardKeyHandle->IsValidHandle())
		{
			return false;
		}

		FName KeyName = NAME_None;
		if (BlackboardKeyHandle->GetValue(KeyName) != FPropertyAccess::Success || KeyName.IsNone())
		{
			return false;
		}

		if (!PropertyUtils.IsValid())
		{
			return false;
		}

		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyUtils->GetSelectedObjects();
		for (const TWeakObjectPtr<UObject>& WeakObj : SelectedObjects)
		{
			const UMetaplotTransitionDetailsProxy* Proxy = Cast<UMetaplotTransitionDetailsProxy>(WeakObj.Get());
			if (Proxy && Proxy->ResolveBlackboardType(KeyName, OutType))
			{
				return true;
			}
		}

		return false;
	}

	static EVisibility GetBlackboardValueVisibility(
		const TSharedPtr<IPropertyHandle>& TypeHandle,
		const TSharedPtr<IPropertyHandle>& BlackboardKeyHandle,
		const TSharedPtr<IPropertyUtilities>& PropertyUtils,
		EMetaplotBlackboardType ExpectedBlackboardType)
	{
		if (!IsBlackboardCompare(TypeHandle))
		{
			return EVisibility::Collapsed;
		}

		EMetaplotBlackboardType ResolvedType = EMetaplotBlackboardType::Bool;
		if (!TryResolveBlackboardType(BlackboardKeyHandle, PropertyUtils, ResolvedType))
		{
			// 无法解析类型时先保持可见，避免字段完全消失导致不可编辑。
			return EVisibility::Visible;
		}

		return ResolvedType == ExpectedBlackboardType
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}

	static void AddTaskWithClass(
		const TSharedPtr<IPropertyHandle>& StoryTasksHandle,
		const TSharedPtr<IPropertyUtilities>& PropertyUtils,
		UClass* SelectedTaskClass)
	{
		if (!StoryTasksHandle.IsValid() || !StoryTasksHandle->IsValidHandle() || !SelectedTaskClass)
		{
			return;
		}

		const TSharedPtr<IPropertyHandleArray> StoryTasksArray = StoryTasksHandle->AsArray();
		if (!StoryTasksArray.IsValid())
		{
			return;
		}

		uint32 OldNum = 0;
		if (StoryTasksArray->GetNumElements(OldNum) != FPropertyAccess::Success)
		{
			return;
		}

		const FScopedTransaction ScopedTransaction(FText::FromString(TEXT("Add Metaplot Task")));
		StoryTasksHandle->NotifyPreChange();

		if (StoryTasksArray->AddItem() != FPropertyAccess::Success)
		{
			StoryTasksHandle->NotifyPostChange(EPropertyChangeType::Unspecified);
			StoryTasksHandle->NotifyFinishedChangingProperties();
			return;
		}

		uint32 NewNum = 0;
		if (StoryTasksArray->GetNumElements(NewNum) != FPropertyAccess::Success || NewNum == 0)
		{
			return;
		}

		const uint32 NewIndex = (NewNum > OldNum) ? (NewNum - 1) : OldNum;
		const TSharedPtr<IPropertyHandle> NewItemHandle = StoryTasksArray->GetElement(NewIndex);
		if (!NewItemHandle.IsValid() || !NewItemHandle->IsValidHandle())
		{
			return;
		}

		const TSharedPtr<IPropertyHandle> TaskHandle = NewItemHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotStoryTaskSpec, Task));
		const TSharedPtr<IPropertyHandle> TaskClassHandle = NewItemHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotStoryTaskSpec, TaskClass));
		if ((!TaskHandle.IsValid() || !TaskHandle->IsValidHandle())
			&& (!TaskClassHandle.IsValid() || !TaskClassHandle->IsValidHandle()))
		{
			StoryTasksHandle->NotifyPostChange(EPropertyChangeType::Unspecified);
			StoryTasksHandle->NotifyFinishedChangingProperties();
			return;
		}

		if (TaskHandle.IsValid() && TaskHandle->IsValidHandle())
		{
			TaskHandle->SetValue(static_cast<UObject*>(nullptr));
		}
		if (TaskClassHandle.IsValid() && TaskClassHandle->IsValidHandle())
		{
			// Task instance will be created in proxy PushToFlow with FlowAsset as Outer.
			const FSoftObjectPath ClassPath(SelectedTaskClass);
			TaskClassHandle->SetValueFromFormattedString(ClassPath.ToString());
		}
		NewItemHandle->SetExpanded(true);

		StoryTasksHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		StoryTasksHandle->NotifyFinishedChangingProperties();

		if (PropertyUtils.IsValid())
		{
			PropertyUtils->ForceRefresh();
		}
	}

	static TSharedRef<SWidget> BuildAddTaskMenu(
		const TSharedPtr<IPropertyHandle>& StoryTasksHandle,
		const TSharedPtr<IPropertyUtilities>& PropertyUtils)
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		TArray<UClass*> TaskClasses;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* CandidateClass = *It;
			if (!CandidateClass
				|| !CandidateClass->IsChildOf(UMetaplotStoryTask::StaticClass())
				|| CandidateClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}
			TaskClasses.Add(CandidateClass);
		}

		TaskClasses.Sort([](const UClass& A, const UClass& B)
		{
			return A.GetName() < B.GetName();
		});

		if (TaskClasses.IsEmpty())
		{
			MenuBuilder.AddMenuEntry(
				FText::FromString(TEXT("No Task Classes Found")),
				FText::FromString(TEXT("未找到可用的 UMetaplotStoryTask 子类。")),
				FSlateIcon(),
				FUIAction());
			return MenuBuilder.MakeWidget();
		}

		for (UClass* TaskClass : TaskClasses)
		{
			const FString ClassName = TaskClass ? TaskClass->GetName() : TEXT("UnknownTask");
			MenuBuilder.AddMenuEntry(
				FText::FromString(ClassName),
				FText::FromString(TEXT("添加该任务到当前节点。")),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([StoryTasksHandle, PropertyUtils, TaskClass]()
				{
					AddTaskWithClass(StoryTasksHandle, PropertyUtils, TaskClass);
				})));
		}

		return MenuBuilder.MakeWidget();
	}

	static TSharedRef<SComboButton> CreateAddTaskPickerComboButton(
		const TSharedPtr<IPropertyHandle>& StoryTasksHandle,
		const TSharedPtr<IPropertyUtilities>& PropertyUtils)
	{
		const FSlateColor AccentColor(FMetaplotEditorStyle::Get().GetColor("Metaplot.Category.IconColor"));
		return SNew(SComboButton)
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>(TEXT("SimpleComboButton")))
			.HasDownArrow(false)
			.ToolTipText(FText::FromString(TEXT("Add new Task")))
			.ButtonContent()
			[
				SNew(SImage)
				.Image(FMetaplotEditorStyle::Get().GetBrush(TEXT("MetaplotEditor.Add")))
				.ColorAndOpacity(AccentColor)
			]
			.MenuContent()
			[
				BuildAddTaskMenu(StoryTasksHandle, PropertyUtils)
			];
	}

	static void ConfigureTaskCategoryHeader(
		IDetailCategoryBuilder& TaskCategory,
		const TSharedPtr<IPropertyHandle>& StoryTasksHandle,
		const TSharedPtr<IPropertyUtilities>& PropertyUtils)
	{
		const FSlateColor AccentColor(FMetaplotEditorStyle::Get().GetColor("Metaplot.Category.IconColor"));
		TaskCategory.HeaderContent(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FMetaplotEditorStyle::Get().GetBrush(TEXT("MetaplotEditor.Tasks")))
				.ColorAndOpacity(AccentColor)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Tasks")))
				.TextStyle(&FMetaplotEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("Metaplot.Category")))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				CreateAddTaskPickerComboButton(StoryTasksHandle, PropertyUtils)
			]);
	}
}

TSharedRef<IDetailCustomization> FMetaplotNodeDetailsProxyCustomization::MakeInstance()
{
	return MakeShared<FMetaplotNodeDetailsProxyCustomization>();
}

void FMetaplotNodeDetailsProxyCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& NodeCategory = DetailBuilder.EditCategory(TEXT("Metaplot|Node"));
	IDetailCategoryBuilder& TaskCategory = DetailBuilder.EditCategory(TEXT("Metaplot|Tasks"));
	const TSharedPtr<IPropertyUtilities> PropertyUtils = DetailBuilder.GetPropertyUtilities();

	const TSharedPtr<IPropertyHandle> NodeIdHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, NodeId));
	const TSharedPtr<IPropertyHandle> NodeNameHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, NodeName));
	const TSharedPtr<IPropertyHandle> DescriptionHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, Description));
	const TSharedPtr<IPropertyHandle> NodeTypeHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, NodeType));
	const TSharedPtr<IPropertyHandle> StageIndexHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, StageIndex));
	const TSharedPtr<IPropertyHandle> LayerIndexHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, LayerIndex));
	const TSharedPtr<IPropertyHandle> CompletionPolicyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, CompletionPolicy));
	const TSharedPtr<IPropertyHandle> ResultPolicyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, ResultPolicy));
	const TSharedPtr<IPropertyHandle> RuntimeResultHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, RuntimeResult));
	const TSharedPtr<IPropertyHandle> StoryTasksHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, StoryTasks));

	auto AddPropertyOnce = [&DetailBuilder, &NodeCategory](const TSharedPtr<IPropertyHandle>& Handle, const bool bReadOnly)
	{
		if (!Handle.IsValid() || !Handle->IsValidHandle())
		{
			return;
		}
		DetailBuilder.HideProperty(Handle);
		IDetailPropertyRow& Row = NodeCategory.AddProperty(Handle);
		if (bReadOnly)
		{
			Row.IsEnabled(false);
		}
	};

	AddPropertyOnce(NodeIdHandle, true);
	AddPropertyOnce(NodeNameHandle, false);
	AddPropertyOnce(DescriptionHandle, false);
	AddPropertyOnce(NodeTypeHandle, false);
	AddPropertyOnce(StageIndexHandle, false);
	AddPropertyOnce(LayerIndexHandle, false);
	AddPropertyOnce(CompletionPolicyHandle, false);
	AddPropertyOnce(ResultPolicyHandle, false);
	AddPropertyOnce(RuntimeResultHandle, true);

	if (StoryTasksHandle.IsValid() && StoryTasksHandle->IsValidHandle())
	{
		DetailBuilder.HideProperty(StoryTasksHandle);
		MetaplotDetailsCustomizationPrivate::ConfigureTaskCategoryHeader(TaskCategory, StoryTasksHandle, PropertyUtils);
		const TSharedRef<FDetailArrayBuilder> TaskArrayBuilder = MakeShareable(new FDetailArrayBuilder(
			StoryTasksHandle.ToSharedRef(),
			/*InGenerateHeader*/ false,
			/*InDisplayResetToDefault*/ true,
			/*InDisplayElementNum*/ false));
		TaskArrayBuilder->OnGenerateArrayElementWidget(
			FOnGenerateArrayElementWidget::CreateLambda([](TSharedRef<IPropertyHandle> PropertyHandle, int32 /*ArrayIndex*/, IDetailChildrenBuilder& ChildrenBuilder)
			{
				ChildrenBuilder.AddProperty(PropertyHandle);
			}));
		TaskCategory.AddCustomBuilder(TaskArrayBuilder, /*bForAdvanced*/ false);
	}
}

TSharedRef<IDetailCustomization> FMetaplotTransitionDetailsProxyCustomization::MakeInstance()
{
	return MakeShared<FMetaplotTransitionDetailsProxyCustomization>();
}

void FMetaplotTransitionDetailsProxyCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& TransitionCategory = DetailBuilder.EditCategory(TEXT("Metaplot|Transition"));

	const TSharedPtr<IPropertyHandle> SourceHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotTransitionDetailsProxy, SourceNodeId));
	const TSharedPtr<IPropertyHandle> TargetHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotTransitionDetailsProxy, TargetNodeId));
	const TSharedPtr<IPropertyHandle> ConditionsHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotTransitionDetailsProxy, Conditions));

	auto AddPropertyOnce = [&DetailBuilder, &TransitionCategory](const TSharedPtr<IPropertyHandle>& Handle, const bool bReadOnly)
	{
		if (!Handle.IsValid() || !Handle->IsValidHandle())
		{
			return;
		}
		DetailBuilder.HideProperty(Handle);
		IDetailPropertyRow& Row = TransitionCategory.AddProperty(Handle);
		if (bReadOnly)
		{
			Row.IsEnabled(false);
		}
	};

	AddPropertyOnce(SourceHandle, true);
	AddPropertyOnce(TargetHandle, true);
	AddPropertyOnce(ConditionsHandle, false);
}

TSharedRef<IPropertyTypeCustomization> FMetaplotConditionCustomization::MakeInstance()
{
	return MakeShared<FMetaplotConditionCustomization>();
}

void FMetaplotConditionCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	(void)CustomizationUtils;
	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(260.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Condition")))
	];
}

void FMetaplotConditionCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const TSharedPtr<IPropertyHandle> TypeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, Type));
	const TSharedPtr<IPropertyHandle> RequiredNodeIdHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, RequiredNodeId));
	const TSharedPtr<IPropertyHandle> BlackboardKeyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, BlackboardKey));
	const TSharedPtr<IPropertyHandle> ComparisonOpHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, ComparisonOp));
	const TSharedPtr<IPropertyHandle> BoolValueHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, BoolValue));
	const TSharedPtr<IPropertyHandle> IntValueHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, IntValue));
	const TSharedPtr<IPropertyHandle> FloatValueHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, FloatValue));
	const TSharedPtr<IPropertyHandle> StringValueHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, StringValue));
	const TSharedPtr<IPropertyHandle> ObjectValueHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, ObjectValue));
	const TSharedPtr<IPropertyHandle> ProbabilityHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, Probability));
	const TSharedPtr<IPropertyUtilities> PropertyUtils = CustomizationUtils.GetPropertyUtilities();

	if (TypeHandle.IsValid() && TypeHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(TypeHandle.ToSharedRef());
	}

	if (RequiredNodeIdHandle.IsValid() && RequiredNodeIdHandle->IsValidHandle())
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(RequiredNodeIdHandle.ToSharedRef());
		Row.Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle]()
		{
			return MetaplotDetailsCustomizationPrivate::GetVisibilityByConditionType(TypeHandle, EMetaplotConditionType::RequiredNodeCompleted);
		}));
	}

	if (BlackboardKeyHandle.IsValid() && BlackboardKeyHandle->IsValidHandle())
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(BlackboardKeyHandle.ToSharedRef());
		Row.Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle]()
		{
			return MetaplotDetailsCustomizationPrivate::GetVisibilityByConditionType(TypeHandle, EMetaplotConditionType::BlackboardCompare);
		}));
	}

	if (ComparisonOpHandle.IsValid() && ComparisonOpHandle->IsValidHandle())
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(ComparisonOpHandle.ToSharedRef());
		Row.Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle]()
		{
			return MetaplotDetailsCustomizationPrivate::GetVisibilityByConditionType(TypeHandle, EMetaplotConditionType::BlackboardCompare);
		}));
	}

	if (BoolValueHandle.IsValid() && BoolValueHandle->IsValidHandle())
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(BoolValueHandle.ToSharedRef());
		Row.Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle, BlackboardKeyHandle, PropertyUtils]()
		{
			return MetaplotDetailsCustomizationPrivate::GetBlackboardValueVisibility(
				TypeHandle,
				BlackboardKeyHandle,
				PropertyUtils,
				EMetaplotBlackboardType::Bool);
		}));
	}

	if (IntValueHandle.IsValid() && IntValueHandle->IsValidHandle())
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(IntValueHandle.ToSharedRef());
		Row.Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle, BlackboardKeyHandle, PropertyUtils]()
		{
			return MetaplotDetailsCustomizationPrivate::GetBlackboardValueVisibility(
				TypeHandle,
				BlackboardKeyHandle,
				PropertyUtils,
				EMetaplotBlackboardType::Int);
		}));
	}

	if (FloatValueHandle.IsValid() && FloatValueHandle->IsValidHandle())
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(FloatValueHandle.ToSharedRef());
		Row.Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle, BlackboardKeyHandle, PropertyUtils]()
		{
			return MetaplotDetailsCustomizationPrivate::GetBlackboardValueVisibility(
				TypeHandle,
				BlackboardKeyHandle,
				PropertyUtils,
				EMetaplotBlackboardType::Float);
		}));
	}

	if (StringValueHandle.IsValid() && StringValueHandle->IsValidHandle())
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(StringValueHandle.ToSharedRef());
		Row.Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle, BlackboardKeyHandle, PropertyUtils]()
		{
			return MetaplotDetailsCustomizationPrivate::GetBlackboardValueVisibility(
				TypeHandle,
				BlackboardKeyHandle,
				PropertyUtils,
				EMetaplotBlackboardType::String);
		}));
	}

	if (ObjectValueHandle.IsValid() && ObjectValueHandle->IsValidHandle())
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(ObjectValueHandle.ToSharedRef());
		Row.Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle, BlackboardKeyHandle, PropertyUtils]()
		{
			return MetaplotDetailsCustomizationPrivate::GetBlackboardValueVisibility(
				TypeHandle,
				BlackboardKeyHandle,
				PropertyUtils,
				EMetaplotBlackboardType::Object);
		}));
	}

	if (ProbabilityHandle.IsValid() && ProbabilityHandle->IsValidHandle())
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(ProbabilityHandle.ToSharedRef());
		Row.Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle]()
		{
			return MetaplotDetailsCustomizationPrivate::GetVisibilityByConditionType(TypeHandle, EMetaplotConditionType::RandomProbability);
		}));
	}
}

