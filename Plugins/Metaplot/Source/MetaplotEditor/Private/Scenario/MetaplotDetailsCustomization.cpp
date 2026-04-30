#include "Scenario/MetaplotDetailsCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MetaplotEditorStyle.h"
#include "Customizations/Widgets/SMetaplotNodeTypePicker.h"
#include "Scenario/MetaplotEditorNodeUtils.h"
#include "Styling/AppStyle.h"
#include "Flow/MetaplotFlow.h"
#include "Runtime/MetaplotStoryTask.h"
#include "Scenario/MetaplotDetailsContext.h"
#include "Scenario/MetaplotEditorTaskNode.h"
#include "Scenario/MetaplotTransitionDetailsProxy.h"
#include "Widgets/Input/SComboButton.h"
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

	static UMetaplotDetailsContext* ResolveDetailsContext(const TSharedPtr<IPropertyUtilities>& PropertyUtils)
	{
		if (!PropertyUtils.IsValid())
		{
			return UMetaplotDetailsContext::GetActiveContext();
		}

		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyUtils->GetSelectedObjects();
		for (const TWeakObjectPtr<UObject>& WeakObj : SelectedObjects)
		{
			if (UMetaplotDetailsContext* DetailsContext = Cast<UMetaplotDetailsContext>(WeakObj.Get()))
			{
				return DetailsContext;
			}
			if (const UMetaplotTransitionDetailsProxy* TransitionProxy = Cast<UMetaplotTransitionDetailsProxy>(WeakObj.Get()))
			{
				return TransitionProxy->GetDetailsContext();
			}
		}
		return UMetaplotDetailsContext::GetActiveContext();
	}

	static UMetaplotFlow* ResolveEditingFlowAsset(const TSharedPtr<IPropertyUtilities>& PropertyUtils)
	{
		if (const UMetaplotDetailsContext* DetailsContext = ResolveDetailsContext(PropertyUtils))
		{
			if (DetailsContext->EditingFlowAsset)
			{
				return DetailsContext->EditingFlowAsset;
			}
		}

		if (!PropertyUtils.IsValid())
		{
			return nullptr;
		}

		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyUtils->GetSelectedObjects();
		for (const TWeakObjectPtr<UObject>& WeakObj : SelectedObjects)
		{
			if (UMetaplotFlow* Flow = Cast<UMetaplotFlow>(WeakObj.Get()))
			{
				return Flow;
			}
		}

		return nullptr;
	}

	static TSharedPtr<IPropertyHandle> ResolveSelectedNodeHandleFromFlow(
		IDetailLayoutBuilder& DetailBuilder,
		const UMetaplotDetailsContext* DetailsContext)
	{
		if (!DetailsContext || !DetailsContext->EditingFlowAsset || !DetailsContext->SelectedNodeId.IsValid())
		{
			return nullptr;
		}

		TArray<UObject*> FlowObjects;
		FlowObjects.Add(DetailsContext->EditingFlowAsset);
		const TSharedPtr<IPropertyHandle> NodesHandle = DetailBuilder.AddObjectPropertyData(
			FlowObjects,
			GET_MEMBER_NAME_CHECKED(UMetaplotFlow, Nodes));
		if (!NodesHandle.IsValid() || !NodesHandle->IsValidHandle())
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandleArray> NodesArray = NodesHandle->AsArray();
		if (!NodesArray.IsValid())
		{
			return nullptr;
		}

		const int32 NodeIndex = DetailsContext->EditingFlowAsset->Nodes.IndexOfByPredicate([DetailsContext](const FMetaplotNode& Node)
		{
			return Node.NodeId == DetailsContext->SelectedNodeId;
		});
		if (NodeIndex == INDEX_NONE)
		{
			return nullptr;
		}

		uint32 NumElements = 0;
		if (NodesArray->GetNumElements(NumElements) != FPropertyAccess::Success || NodeIndex < 0 || static_cast<uint32>(NodeIndex) >= NumElements)
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandle> NodeHandle = NodesArray->GetElement(NodeIndex);
		return (NodeHandle.IsValid() && NodeHandle->IsValidHandle()) ? NodeHandle : nullptr;
	}

	static TSharedPtr<IPropertyHandle> ResolveStoryTasksHandleFromFlow(
		IDetailLayoutBuilder& DetailBuilder,
		const UMetaplotDetailsContext* DetailsContext)
	{
		if (!DetailsContext || !DetailsContext->EditingFlowAsset || !DetailsContext->SelectedNodeId.IsValid())
		{
			return nullptr;
		}

		TArray<UObject*> FlowObjects;
		FlowObjects.Add(DetailsContext->EditingFlowAsset);
		const TSharedPtr<IPropertyHandle> NodeTaskSetsHandle = DetailBuilder.AddObjectPropertyData(
			FlowObjects,
			GET_MEMBER_NAME_CHECKED(UMetaplotFlow, NodeEditorTaskSets));
		if (!NodeTaskSetsHandle.IsValid() || !NodeTaskSetsHandle->IsValidHandle())
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandleArray> NodeTaskSetsArray = NodeTaskSetsHandle->AsArray();
		if (!NodeTaskSetsArray.IsValid())
		{
			return nullptr;
		}

		const int32 TaskSetIndex = DetailsContext->EditingFlowAsset->NodeEditorTaskSets.IndexOfByPredicate([DetailsContext](const FMetaplotNodeEditorTasks& Entry)
		{
			return Entry.NodeId == DetailsContext->SelectedNodeId;
		});
		if (TaskSetIndex == INDEX_NONE)
		{
			return nullptr;
		}

		uint32 NumElements = 0;
		if (NodeTaskSetsArray->GetNumElements(NumElements) != FPropertyAccess::Success || TaskSetIndex < 0 || static_cast<uint32>(TaskSetIndex) >= NumElements)
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandle> TaskSetHandle = NodeTaskSetsArray->GetElement(TaskSetIndex);
		if (!TaskSetHandle.IsValid() || !TaskSetHandle->IsValidHandle())
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandle> StoryTasksHandle = TaskSetHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNodeEditorTasks, Tasks));
		if (StoryTasksHandle.IsValid() && StoryTasksHandle->IsValidHandle())
		{
			return StoryTasksHandle;
		}

		return nullptr;
	}

	static TSharedRef<SWidget> BuildAddTaskMenu(
		const TSharedPtr<IPropertyHandle>& StoryTasksHandle,
		const TSharedPtr<IPropertyUtilities>& PropertyUtils)
	{
		return SNew(SMetaplotNodeTypePicker)
			.BaseClass(UMetaplotStoryTask::StaticClass())
			.BaseStruct(UMetaplotStoryTask::StaticClass())
			.OnNodeTypePicked(FOnMetaplotNodeTypePicked::CreateLambda([StoryTasksHandle, PropertyUtils](UClass* PickedClass)
			{
				const UMetaplotDetailsContext* DetailsContext = ResolveDetailsContext(PropertyUtils);
				if (!DetailsContext || !DetailsContext->EditingFlowAsset || !PickedClass || !StoryTasksHandle.IsValid() || !StoryTasksHandle->IsValidHandle())
				{
					return;
				}

				const TSharedPtr<IPropertyHandleArray> StoryTasksArray = StoryTasksHandle->AsArray();
				if (!StoryTasksArray.IsValid())
				{
					return;
				}

				FMetaplotEditorNodeUtils::ModifyNodeInTransaction(
					FText::FromString(TEXT("Add Metaplot Task")),
					StoryTasksHandle,
					[&]()
					{
						uint32 OldNum = 0;
						if (StoryTasksArray->GetNumElements(OldNum) != FPropertyAccess::Success)
						{
							return false;
						}
						if (StoryTasksArray->AddItem() != FPropertyAccess::Success)
						{
							return false;
						}

						uint32 NewNum = 0;
						if (StoryTasksArray->GetNumElements(NewNum) != FPropertyAccess::Success || NewNum == 0)
						{
							return false;
						}
						const uint32 NewIndex = (NewNum > OldNum) ? (NewNum - 1) : OldNum;
						const TSharedPtr<IPropertyHandle> NewItemHandle = StoryTasksArray->GetElement(NewIndex);
						return FMetaplotEditorNodeUtils::SetNodeType(NewItemHandle, PickedClass, DetailsContext->EditingFlowAsset);
					});

				if (PropertyUtils.IsValid())
				{
					PropertyUtils->ForceRefresh();
				}
			}));
	}

}

TSharedRef<IDetailCustomization> FMetaplotFlowDetailsCustomization::MakeInstance()
{
	return MakeShared<FMetaplotFlowDetailsCustomization>();
}

void FMetaplotFlowDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& NodeCategory = DetailBuilder.EditCategory(TEXT("Metaplot|Node"));
	IDetailCategoryBuilder& TaskCategory = DetailBuilder.EditCategory(TEXT("Metaplot|Tasks"));
	const TSharedPtr<IPropertyUtilities> PropertyUtils = DetailBuilder.GetPropertyUtilities();
	const UMetaplotDetailsContext* DetailsContext = MetaplotDetailsCustomizationPrivate::ResolveDetailsContext(PropertyUtils);
	const TSharedPtr<IPropertyHandle> NodeHandle = MetaplotDetailsCustomizationPrivate::ResolveSelectedNodeHandleFromFlow(DetailBuilder, DetailsContext);
	const TSharedPtr<IPropertyHandle> NodeIdHandle = NodeHandle.IsValid()
		? NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNode, NodeId))
		: nullptr;
	const TSharedPtr<IPropertyHandle> NodeNameHandle = NodeHandle.IsValid()
		? NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNode, NodeName))
		: nullptr;
	const TSharedPtr<IPropertyHandle> DescriptionHandle = NodeHandle.IsValid()
		? NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNode, Description))
		: nullptr;
	const TSharedPtr<IPropertyHandle> NodeTypeHandle = NodeHandle.IsValid()
		? NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNode, NodeType))
		: nullptr;
	const TSharedPtr<IPropertyHandle> StageIndexHandle = NodeHandle.IsValid()
		? NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNode, StageIndex))
		: nullptr;
	const TSharedPtr<IPropertyHandle> LayerIndexHandle = NodeHandle.IsValid()
		? NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNode, LayerIndex))
		: nullptr;
	const TSharedPtr<IPropertyHandle> CompletionPolicyHandle = NodeHandle.IsValid()
		? NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNode, CompletionPolicy))
		: nullptr;
	const TSharedPtr<IPropertyHandle> ResultPolicyHandle = NodeHandle.IsValid()
		? NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNode, ResultPolicy))
		: nullptr;
	const TSharedPtr<IPropertyHandle> RuntimeResultHandle = NodeHandle.IsValid()
		? NodeHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNode, RuntimeResult))
		: nullptr;
	const TSharedPtr<IPropertyHandle> StoryTasksHandle = MetaplotDetailsCustomizationPrivate::ResolveStoryTasksHandleFromFlow(DetailBuilder, DetailsContext);

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

	if (NodeHandle.IsValid() && NodeHandle->IsValidHandle())
	{
		AddPropertyOnce(NodeIdHandle, true);
		AddPropertyOnce(NodeNameHandle, false);
		AddPropertyOnce(DescriptionHandle, false);
		AddPropertyOnce(NodeTypeHandle, false);
		AddPropertyOnce(StageIndexHandle, false);
		AddPropertyOnce(LayerIndexHandle, false);
		AddPropertyOnce(CompletionPolicyHandle, false);
		AddPropertyOnce(ResultPolicyHandle, false);
		AddPropertyOnce(RuntimeResultHandle, true);
	}
	else
	{
		NodeCategory.AddCustomRow(FText::FromString(TEXT("NodeNotAvailable")))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Node unavailable for current selection context.")))
			];
	}

	if (StoryTasksHandle.IsValid() && StoryTasksHandle->IsValidHandle())
	{
		FMetaplotEditorNodeUtils::MakeArrayCategoryHeader(
			TaskCategory,
			TEXT("MetaplotEditor.Tasks"),
			FText::FromString(TEXT("Tasks")),
			[StoryTasksHandle, PropertyUtils]()
			{
				return MetaplotDetailsCustomizationPrivate::BuildAddTaskMenu(StoryTasksHandle, PropertyUtils);
			});
		FMetaplotEditorNodeUtils::MakeArrayItems(TaskCategory, StoryTasksHandle);
	}
	else
	{
		TaskCategory.AddCustomRow(FText::FromString(TEXT("TasksNotAvailable")))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Tasks unavailable for current node context.")))
			];
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
	(void)CustomizationUtils;
	const TSharedPtr<IPropertyHandle> TypeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, Type));
	const TSharedPtr<IPropertyHandle> RequiredNodeIdHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, RequiredNodeId));
	const TSharedPtr<IPropertyHandle> ProbabilityHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotCondition, Probability));

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

	if (ProbabilityHandle.IsValid() && ProbabilityHandle->IsValidHandle())
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(ProbabilityHandle.ToSharedRef());
		Row.Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle]()
		{
			return MetaplotDetailsCustomizationPrivate::GetVisibilityByConditionType(TypeHandle, EMetaplotConditionType::RandomProbability);
		}));
	}
}

TSharedRef<IPropertyTypeCustomization> FMetaplotEditorTaskNodeCustomization::MakeInstance()
{
	return MakeShared<FMetaplotEditorTaskNodeCustomization>();
}

void FMetaplotEditorTaskNodeCustomization::CustomizeHeader(
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
		.Text(FText::FromString(TEXT("Task Node")))
	];
}

void FMetaplotEditorTaskNodeCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const TSharedPtr<IPropertyUtilities> PropertyUtils = CustomizationUtils.GetPropertyUtilities();
	const TSharedPtr<IPropertyHandle> IdHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, ID));
	const TSharedPtr<IPropertyHandle> TaskClassHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, TaskClass));
	const TSharedPtr<IPropertyHandle> InstanceHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, InstanceObject));
	const TSharedPtr<IPropertyHandle> EnabledHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, bEnabled));
	const TSharedPtr<IPropertyHandle> CompletionHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotEditorTaskNode, bConsideredForCompletion));

	if (IdHandle.IsValid() && IdHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(IdHandle.ToSharedRef());
	}
	if (TaskClassHandle.IsValid() && TaskClassHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(TaskClassHandle.ToSharedRef());
		TaskClassHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([PropertyHandle, PropertyUtils]()
		{
			if (UMetaplotFlow* Flow = MetaplotDetailsCustomizationPrivate::ResolveEditingFlowAsset(PropertyUtils))
			{
				FMetaplotEditorNodeUtils::EnsureNodeInstanceMatchesClass(PropertyHandle, Flow);
				Flow->NormalizeEditorTaskNodes();
			}
			if (PropertyUtils.IsValid())
			{
				PropertyUtils->ForceRefresh();
			}
		}));
	}
	if (InstanceHandle.IsValid() && InstanceHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(InstanceHandle.ToSharedRef());
	}
	if (EnabledHandle.IsValid() && EnabledHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(EnabledHandle.ToSharedRef());
	}
	if (CompletionHandle.IsValid() && CompletionHandle->IsValidHandle())
	{
		ChildBuilder.AddProperty(CompletionHandle.ToSharedRef());
	}
}
