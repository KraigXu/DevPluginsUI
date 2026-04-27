#include "Customizations/Widgets/SMetaplotNodeTypePicker.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBox.h"

void SMetaplotNodeTypePicker::Construct(const FArguments& InArgs)
{
	SchemaObject = InArgs._Schema;
	BaseClassFilter = InArgs._BaseClass;
	BaseStructFilter = InArgs._BaseStruct;
	OnNodeTypePicked = InArgs._OnNodeTypePicked;

	ChildSlot
	[
		BuildClassListWidget()
	];
}

void SMetaplotNodeTypePicker::CollectCandidateClasses(TArray<UClass*>& OutClasses) const
{
	UClass* BaseClass = BaseClassFilter.Get();
	if (!BaseClass)
	{
		BaseClass = UObject::StaticClass();
	}

	const UStruct* BaseStruct = BaseStructFilter.Get();
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* CandidateClass = *It;
		if (!CandidateClass
			|| !CandidateClass->IsChildOf(BaseClass)
			|| CandidateClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		if (BaseStruct && !CandidateClass->IsChildOf(BaseStruct))
		{
			continue;
		}

		OutClasses.Add(CandidateClass);
	}

	OutClasses.Sort([](const UClass& A, const UClass& B)
	{
		return A.GetName() < B.GetName();
	});
}

TSharedRef<SWidget> SMetaplotNodeTypePicker::BuildClassListWidget() const
{
	FMenuBuilder MenuBuilder(true, nullptr);

	TArray<UClass*> CandidateClasses;
	CollectCandidateClasses(CandidateClasses);
	if (CandidateClasses.IsEmpty())
	{
		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("No Node Types Found")),
			FText::FromString(TEXT("未找到符合过滤条件的节点类型。")),
			FSlateIcon(),
			FUIAction());
		return MenuBuilder.MakeWidget();
	}

	for (UClass* TaskClass : CandidateClasses)
	{
		const FString ClassName = TaskClass ? TaskClass->GetName() : TEXT("UnknownClass");
		MenuBuilder.AddMenuEntry(
			FText::FromString(ClassName),
			FText::FromString(TEXT("添加该类型节点。")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([TaskClass, OnNodeTypePicked = OnNodeTypePicked]()
			{
				if (TaskClass && OnNodeTypePicked.IsBound())
				{
					OnNodeTypePicked.Execute(TaskClass);
				}
			})));
	}

	return MenuBuilder.MakeWidget();
}
