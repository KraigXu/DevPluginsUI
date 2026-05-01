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
#include "Scenario/MetaplotEditorNodeUtils.h"
#include "Styling/AppStyle.h"
#include "Flow/MetaplotFlow.h"
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
		const TSharedPtr<IPropertyHandle> NodeStatesHandle = DetailBuilder.AddObjectPropertyData(
			FlowObjects,
			GET_MEMBER_NAME_CHECKED(UMetaplotFlow, NodeStates));
		if (!NodeStatesHandle.IsValid() || !NodeStatesHandle->IsValidHandle())
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandleArray> NodeStatesArray = NodeStatesHandle->AsArray();
		if (!NodeStatesArray.IsValid())
		{
			return nullptr;
		}

		const int32 TaskSetIndex = DetailsContext->EditingFlowAsset->NodeStates.IndexOfByPredicate([DetailsContext](const FMetaplotNodeState& Entry)
		{
			return Entry.ID == DetailsContext->SelectedNodeId;
		});
		if (TaskSetIndex == INDEX_NONE)
		{
			return nullptr;
		}

		uint32 NumElements = 0;
		if (NodeStatesArray->GetNumElements(NumElements) != FPropertyAccess::Success || TaskSetIndex < 0 || static_cast<uint32>(TaskSetIndex) >= NumElements)
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandle> TaskSetHandle = NodeStatesArray->GetElement(TaskSetIndex);
		if (!TaskSetHandle.IsValid() || !TaskSetHandle->IsValidHandle())
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandle> StoryTasksHandle = TaskSetHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNodeState, Tasks));
		if (StoryTasksHandle.IsValid() && StoryTasksHandle->IsValidHandle())
		{
			return StoryTasksHandle;
		}

		return nullptr;
	}

	static TSharedPtr<IPropertyHandle> ResolveTasksCompletionHandleFromFlow(
		IDetailLayoutBuilder& DetailBuilder,
		const UMetaplotDetailsContext* DetailsContext)
	{
		if (!DetailsContext || !DetailsContext->EditingFlowAsset || !DetailsContext->SelectedNodeId.IsValid())
		{
			return nullptr;
		}

		TArray<UObject*> FlowObjects;
		FlowObjects.Add(DetailsContext->EditingFlowAsset);
		const TSharedPtr<IPropertyHandle> NodeStatesHandle = DetailBuilder.AddObjectPropertyData(
			FlowObjects,
			GET_MEMBER_NAME_CHECKED(UMetaplotFlow, NodeStates));
		if (!NodeStatesHandle.IsValid() || !NodeStatesHandle->IsValidHandle())
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandleArray> NodeStatesArray = NodeStatesHandle->AsArray();
		if (!NodeStatesArray.IsValid())
		{
			return nullptr;
		}

		const int32 StateIndex = DetailsContext->EditingFlowAsset->NodeStates.IndexOfByPredicate([DetailsContext](const FMetaplotNodeState& Entry)
		{
			return Entry.ID == DetailsContext->SelectedNodeId;
		});
		if (StateIndex == INDEX_NONE)
		{
			return nullptr;
		}

		uint32 NumElements = 0;
		if (NodeStatesArray->GetNumElements(NumElements) != FPropertyAccess::Success || StateIndex < 0 || static_cast<uint32>(StateIndex) >= NumElements)
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandle> StateHandle = NodeStatesArray->GetElement(StateIndex);
		if (!StateHandle.IsValid() || !StateHandle->IsValidHandle())
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandle> TasksCompletionHandle = StateHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaplotNodeState, TasksCompletion));
		if (TasksCompletionHandle.IsValid() && TasksCompletionHandle->IsValidHandle())
		{
			return TasksCompletionHandle;
		}

		return nullptr;
	}

}

TSharedRef<IDetailCustomization> FMetaplotFlowDetailsCustomization::MakeInstance()
{
	return MakeShared<FMetaplotFlowDetailsCustomization>();
}

void FMetaplotFlowDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& CommonCategory = DetailBuilder.EditCategory(TEXT("Common"));
	CommonCategory.SetSortOrder(0);

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
	const TSharedPtr<IPropertyHandle> TasksCompletionProperty = MetaplotDetailsCustomizationPrivate::ResolveTasksCompletionHandleFromFlow(DetailBuilder, DetailsContext);

	auto AddPropertyOnce = [&DetailBuilder, &CommonCategory](const TSharedPtr<IPropertyHandle>& Handle, const bool bReadOnly)
	{
		if (!Handle.IsValid() || !Handle->IsValidHandle())
		{
			return;
		}
		DetailBuilder.HideProperty(Handle);
		IDetailPropertyRow& Row = CommonCategory.AddProperty(Handle);
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
		CommonCategory.AddCustomRow(FText::FromString(TEXT("NodeNotAvailable")))
			.WholeRowContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Node unavailable for current selection context.")))
			];
	}
	
	const bool bAllowTasksCompletion = TasksCompletionProperty.IsValid() && TasksCompletionProperty->IsValidHandle();
	if (bAllowTasksCompletion)
	{
		TasksCompletionProperty->MarkHiddenByCustomization();
	}
	IDetailCategoryBuilder& TaskCategory = UE::MetaplotEditor::EditorNodeUtils::MakeArrayCategoryHeader(
	DetailBuilder,
	StoryTasksHandle,
	TEXT("Tasks"),
	FText::FromString(TEXT("Tasks")),
	TEXT("MetaplotEditor.Tasks"),
	UE::MetaplotEditor::Colors::Cyan,
	bAllowTasksCompletion ? TasksCompletionProperty->CreatePropertyValueWidget(/*bDisplayDefaultPropertyButtons*/false) : TSharedPtr<SWidget>(),
	UE::MetaplotEditor::Colors::Cyan.WithAlpha(192),
	FText::FromString(TEXT("Add new Task")),
	1);
	
	if (StoryTasksHandle.IsValid() && StoryTasksHandle->IsValidHandle())
	{
		UE::MetaplotEditor::EditorNodeUtils::MakeArrayItems(TaskCategory, StoryTasksHandle);
	}
}

TSharedRef<IDetailCustomization> FMetaplotTransitionDetailsProxyCustomization::MakeInstance()
{
	return MakeShared<FMetaplotTransitionDetailsProxyCustomization>();
}

void FMetaplotTransitionDetailsProxyCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& TransitionCategory = DetailBuilder.EditCategory(TEXT("Transition"));

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
