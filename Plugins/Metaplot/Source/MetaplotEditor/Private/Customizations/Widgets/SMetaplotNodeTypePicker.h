#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FOnMetaplotNodeTypePicked, UClass*);

class SMetaplotNodeTypePicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaplotNodeTypePicker)
		: _Schema(nullptr)
		, _BaseClass(nullptr)
		, _BaseStruct(nullptr)
	{}
		SLATE_ARGUMENT(UObject*, Schema)
		SLATE_ARGUMENT(UClass*, BaseClass)
		SLATE_ARGUMENT(UStruct*, BaseStruct)
		SLATE_EVENT(FOnMetaplotNodeTypePicked, OnNodeTypePicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void CollectCandidateClasses(TArray<UClass*>& OutClasses) const;
	TSharedRef<SWidget> BuildClassListWidget() const;

private:
	TWeakObjectPtr<UObject> SchemaObject;
	TWeakObjectPtr<UClass> BaseClassFilter;
	TWeakObjectPtr<UStruct> BaseStructFilter;
	FOnMetaplotNodeTypePicked OnNodeTypePicked;
};
