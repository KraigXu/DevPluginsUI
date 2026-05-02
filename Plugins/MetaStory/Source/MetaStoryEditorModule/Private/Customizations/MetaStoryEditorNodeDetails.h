// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "IMetaStoryEditorHost.h"
#include "Widgets/Views/STreeView.h"
#include "Textures/SlateIcon.h"

struct FObjectKey;
struct FOptionalSize;

class FDetailWidgetRow;
class FMetaStoryViewModel;
class IDetailChildrenBuilder;
class IPropertyHandle;
class IPropertyHandleArray;
class IPropertyUtilities;
class SComboButton;
class SWidget;
class SSearchBox;
class SWidgetSwitcher;
class SInlineEditableTextBlock;
class SBorder;
class SMenuAnchor;
class UMetaStory;
class UMetaStoryState;
class UMetaStoryEditorData;
struct EVisibility;
struct FMetaStoryEditorNode;
struct FPropertyBindingPath;
enum class EMetaStoryExpressionOperand : uint8;

/**
 * Type customization for nodes (Conditions, Evaluators and Tasks) in MetaStoryState.
 */
class FMetaStoryEditorNodeDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual ~FMetaStoryEditorNodeDetails();
	
	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	void MakeFlagsWidget();

	bool ShouldResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle) const;
	void ResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle);
	void OnCopyNode();
	void OnCopyAllNodes();
	void OnPasteNodes();
	TSharedPtr<IPropertyHandle> GetInstancedObjectValueHandle(TSharedPtr<IPropertyHandle> PropertyHandle);

	FOptionalSize GetIndentSize() const;
	FReply HandleIndentPlus();
	FReply HandleIndentMinus();
	
	int32 GetIndent() const;
	void SetIndent(const int32 Indent) const;
	bool IsIndent(const int32 Indent) const;
	
	FSlateColor GetContentRowColor() const;

	FText GetOperandText() const;
	FText GetConditionOperandText() const;
	FText GetConsiderationOperandText() const;
	FSlateColor GetOperandColor() const;
	TSharedRef<SWidget> OnGetOperandContent() const;
	TSharedRef<SWidget> GetConditionOperandContent() const;
	TSharedRef<SWidget> GetConsiderationOperandContent() const;
	bool IsOperandEnabled() const;

	void SetOperand(const EMetaStoryExpressionOperand Operand) const;
	bool IsOperand(const EMetaStoryExpressionOperand Operand) const;

	bool IsFirstItem() const;
	int32 GetCurrIndent() const;
	int32 GetNextIndent() const;
	
	FText GetOpenParens() const;
	FText GetCloseParens() const;

	EVisibility IsConditionVisible() const;
	EVisibility IsConsiderationVisible() const;
	EVisibility IsOperandVisible() const;
	EVisibility AreIndentButtonsVisible() const;
	EVisibility AreParensVisible() const;
	EVisibility AreFlagsVisible() const;
	
	EVisibility IsIconVisible() const;
	const FSlateBrush* GetIcon() const;
	FSlateColor GetIconColor() const;

	FReply OnDescriptionClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;
	FText GetNodeDescription() const;
	EVisibility IsNodeDescriptionVisible() const;
	
	FText GetNodeTooltip() const;

	FReply OnRowMouseDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	FReply OnRowMouseUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	FText GetName() const;
	bool HandleVerifyNameChanged(const FText& InText, FText& OutErrorMessage) const;
	void HandleNameCommitted(const FText& NewLabel, ETextCommit::Type CommitType) const;

	FReply HandleToggleCompletionTaskClicked();
	FText GetToggleCompletionTaskTooltip() const;
	FSlateColor GetToggleCompletionTaskColor() const;
	const FSlateBrush* GetToggleCompletionTaskIcon() const;
	EVisibility GetToggleCompletionTaskVisibility() const;

	FText GetNodePickerTooltip() const;
	void OnNodePicked(const UStruct* InStruct) const;

	void OnIdentifierChanged(const UMetaStory& MetaStory);
	void OnBindingChanged(const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath);
	void FindOuterObjects();

	FReply OnBrowseToSource() const;
	FReply OnBrowseToNodeBlueprint() const;
	FReply OnEditNodeBlueprint() const;
	EVisibility IsBrowseToSourceVisible() const;
	EVisibility IsBrowseToNodeBlueprintVisible() const;
	EVisibility IsEditNodeBlueprintVisible() const;

	TSharedRef<SWidget> GenerateOptionsMenu();
	void GeneratePickerMenu(class FMenuBuilder& InMenuBuilder);
	void OnDeleteNode() const;
	void OnDeleteAllNodes() const;
	void OnDuplicateNode() const;
	void OnRenameNode() const;
	void HandleAssetChanged();

	TWeakObjectPtr<UScriptStruct> BaseScriptStruct;
	TWeakObjectPtr<UClass> BaseClass;
	TSharedPtr<SWidgetSwitcher> NameSwitcher;
	TSharedPtr<SInlineEditableTextBlock> NameEdit;
	TSharedPtr<SBorder> RowBorder; 
	TSharedPtr<SBorder> FlagsContainer; 

	TWeakObjectPtr<UMetaStoryEditorData> EditorData;
	TWeakObjectPtr<UMetaStory> MetaStory;
	TSharedPtr<FMetaStoryViewModel> MetaStoryViewModel;
	
	TSharedPtr<IPropertyUtilities> PropUtils;
	TSharedPtr<IPropertyHandle> StructProperty;
	TSharedPtr<IPropertyHandle> NodeProperty;
	TSharedPtr<IPropertyHandle> InstanceProperty;
	TSharedPtr<IPropertyHandle> InstanceObjectProperty;
	TSharedPtr<IPropertyHandle> ExecutionRuntimeDataProperty;
	TSharedPtr<IPropertyHandle> ExecutionRuntimeDataObjectProperty;
	TSharedPtr<IPropertyHandle> IDProperty;

	TSharedPtr<IPropertyHandle> IndentProperty;
	TSharedPtr<IPropertyHandle> OperandProperty;

	TSharedPtr<IPropertyHandle> ParentProperty;
	TSharedPtr<IPropertyHandleArray> ParentArrayProperty;

	FDelegateHandle OnBindingChangedHandle;
	FDelegateHandle OnChangedAssetHandle;
};
