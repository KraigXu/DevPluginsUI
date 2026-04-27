#pragma once

#include "CoreMinimal.h"

class IDetailCategoryBuilder;
class IPropertyHandle;
class IPropertyUtilities;
class SWidget;
class UClass;
class UObject;

struct FMetaplotEditorNodeUtils
{
	static bool ModifyNodeInTransaction(
		const FText& TransactionText,
		const TSharedPtr<IPropertyHandle>& ChangedHandle,
		TFunctionRef<bool()> ModifyOperation);

	static void MakeArrayCategoryHeader(
		IDetailCategoryBuilder& CategoryBuilder,
		const FName& IconBrushName,
		const FText& CategoryTitle,
		TFunction<TSharedRef<SWidget>()> BuildAddMenuWidget);

	static void MakeArrayItems(
		IDetailCategoryBuilder& CategoryBuilder,
		const TSharedPtr<IPropertyHandle>& ArrayHandle);

	static bool SetNodeType(
		const TSharedPtr<IPropertyHandle>& NodeHandle,
		UClass* SelectedTaskClass,
		UObject* InstanceOuter);

	static bool ConditionalUpdateNodeInstanceData(
		const TSharedPtr<IPropertyHandle>& NodeHandle,
		UObject* InstanceOuter);

	static bool InstantiateStructSubobjects(
		const TSharedPtr<IPropertyHandle>& NodeHandle,
		UObject* InstanceOuter);
};
