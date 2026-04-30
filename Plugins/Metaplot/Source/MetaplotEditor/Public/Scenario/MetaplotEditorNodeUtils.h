#pragma once

#include "CoreMinimal.h"

class IDetailCategoryBuilder;
class IDetailLayoutBuilder;
class IPropertyHandle;
class IPropertyUtilities;
class SWidget;
class UClass;
class UObject;

namespace UE::MetaplotEditor::EditorNodeUtils
{
	/**
	 * Creates a category and sets the contents of the row to: [Icon] [DisplayName]  [+].
	 * @param DetailBuilder Detail builder where the category is added.
	 * @param ArrayPropertyHandle Property handle pointing to a TArray.
	 * @param CategoryName Name of the category to create.
	 * @param CategoryDisplayName Display name of the category.
	 * @param IconName Name to the icon resource to show in front of the category display name.
	 * @param IconColor Color of icon resource.
	 * @param AddIconColor Color of the add icon.
	 * @param AddButtonTooltipText Tooltip text to show on the add buttons.
	 * @param SortOrder Category sort order (categories with smaller sort order appear earlier in details panel).
	 * @return Reference to the category builder of the category just created.
	 */
	IDetailCategoryBuilder& MakeArrayCategoryHeader(
		IDetailLayoutBuilder& DetailBuilder,
		const TSharedPtr<IPropertyHandle>& ArrayPropertyHandle,
		FName CategoryName,
		const FText& CategoryDisplayName,
		FName IconName,
		FLinearColor IconColor,
		const TSharedPtr<SWidget> Extension,
		FLinearColor AddIconColor,
		const FText& AddButtonTooltipText,
		int32 SortOrder);
	
}


struct FMetaplotEditorNodeUtils
{
	static bool ModifyNodeInTransaction(
		const FText& TransactionText,
		const TSharedPtr<IPropertyHandle>& ChangedHandle,
		TFunctionRef<bool()> ModifyOperation);

	static void MakeArrayCategoryHeader(
		IDetailCategoryBuilder& CategoryBuilder,
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

	static bool EnsureNodeInstanceMatchesClass(
		const TSharedPtr<IPropertyHandle>& NodeHandle,
		UObject* InstanceOuter);

	static bool InstantiateStructSubobjects(
		const TSharedPtr<IPropertyHandle>& NodeHandle,
		UObject* InstanceOuter);
};



namespace UE::MetaplotEditor::Colors
{
	inline FColor Darken(const FColor Col, const float Level)
	{
		const int32 Mul = (int32)FMath::Clamp(Level * 255.f, 0.f, 255.f);
		const int32 R = (int32)Col.R * Mul / 255;
		const int32 G = (int32)Col.G * Mul / 255;
		const int32 B = (int32)Col.B * Mul / 255;
		return FColor((uint8)R, (uint8)G, (uint8)B, Col.A);
	}

	const FColor Grey = FColor::FromHex(TEXT("#949494"));
	const FColor Red = FColor::FromHex(TEXT("#DE6659"));
	const FColor Orange = FColor::FromHex(TEXT("#E3983F"));
	const FColor Yellow = FColor::FromHex(TEXT("#EFD964"));
	const FColor Green = FColor::FromHex(TEXT("#8AB75E"));
	const FColor Cyan = FColor::FromHex(TEXT("#56C3BD"));
	const FColor Blue = FColor::FromHex(TEXT("#649ED3"));
	const FColor Purple = FColor::FromHex(TEXT("#B397D6"));
	const FColor Magenta = FColor::FromHex(TEXT("#CE85C7"));
	const FColor Bronze = FColorList::Bronze;

	constexpr float DarkenLevel = 0.6f;
	const FColor DarkGrey = Darken(Grey, DarkenLevel);
	const FColor DarkRed = Darken(Red, DarkenLevel);
	const FColor DarkOrange = Darken(Orange, DarkenLevel);
	const FColor DarkYellow = Darken(Yellow, DarkenLevel);
	const FColor DarkGreen = Darken(Green, DarkenLevel);
	const FColor DarkCyan = Darken(Cyan, DarkenLevel);
	const FColor DarkBlue = Darken(Blue, DarkenLevel);
	const FColor DarkPurple = Darken(Purple, DarkenLevel);
	const FColor DarkMagenta = Darken(Magenta, DarkenLevel);
	const FColor DarkBronze = Darken(Bronze, DarkenLevel);
} // UE::StateTree::Colors
