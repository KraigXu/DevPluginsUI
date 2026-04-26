#include "Scenario/MetaplotDetailsCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Scenario/MetaplotDetailsProxy.h"
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
}

TSharedRef<IDetailCustomization> FMetaplotNodeDetailsProxyCustomization::MakeInstance()
{
	return MakeShared<FMetaplotNodeDetailsProxyCustomization>();
}

void FMetaplotNodeDetailsProxyCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& NodeCategory = DetailBuilder.EditCategory(TEXT("Metaplot|Node"));

	const TSharedPtr<IPropertyHandle> NodeIdHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, NodeId));
	const TSharedPtr<IPropertyHandle> NodeNameHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, NodeName));
	const TSharedPtr<IPropertyHandle> DescriptionHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, Description));
	const TSharedPtr<IPropertyHandle> NodeTypeHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, NodeType));
	const TSharedPtr<IPropertyHandle> StageIndexHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, StageIndex));
	const TSharedPtr<IPropertyHandle> LayerIndexHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, LayerIndex));
	const TSharedPtr<IPropertyHandle> CompletionPolicyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, CompletionPolicy));
	const TSharedPtr<IPropertyHandle> ResultPolicyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, ResultPolicy));
	const TSharedPtr<IPropertyHandle> RuntimeResultHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMetaplotNodeDetailsProxy, RuntimeResult));

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
