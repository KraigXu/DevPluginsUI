// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryTransitionDetails.h"
#include "Debugger/MetaStoryDebuggerUIExtensions.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "ScopedTransaction.h"
#include "MetaStoryDescriptionHelpers.h"
#include "MetaStoryEditingSubsystem.h"
#include "MetaStoryEditor.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorNodeUtils.h"
#include "MetaStoryEditorStyle.h"
#include "MetaStoryPropertyHelpers.h"
#include "MetaStoryEditorDataClipboardHelpers.h"
#include "MetaStoryTypes.h"
#include "TextStyleDecorator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

TSharedRef<IPropertyTypeCustomization> FMetaStoryTransitionDetails::MakeInstance()
{
	return MakeShareable(new FMetaStoryTransitionDetails);
}

void FMetaStoryTransitionDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	ParentProperty = StructProperty->GetParentHandle();
	ParentArrayProperty = ParentProperty->AsArray();

	PropUtils = StructCustomizationUtils.GetPropertyUtilities();

	// Find MetaStoryEditorData associated with this panel.
	UMetaStoryEditorData* EditorData = nullptr;
	const TArray<TWeakObjectPtr<>>& Objects = PropUtils->GetSelectedObjects();
	for (const TWeakObjectPtr<>& WeakObject : Objects)
	{
		if (const UObject* Object = WeakObject.Get())
		{
			if (UMetaStoryEditorData* OuterEditorData = Object->GetTypedOuter<UMetaStoryEditorData>())
			{
				EditorData = OuterEditorData;
				break;
			}
		}
	}

	if (UMetaStoryEditingSubsystem* MetaStoryEditingSubsystem = EditorData != nullptr ? GEditor->GetEditorSubsystem<UMetaStoryEditingSubsystem>() : nullptr)
	{
		MetaStoryViewModel = MetaStoryEditingSubsystem->FindOrAddViewModel(EditorData->GetTypedOuter<UMetaStory>());
	}

	TriggerProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, Trigger));
	PriorityProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, Priority));
	RequiredEventProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, RequiredEvent));
	DelegateListener = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, DelegateListener));
	StateProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, State));
	DelayTransitionProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, bDelayTransition));
	DelayDurationProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, DelayDuration));
	DelayRandomVarianceProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, DelayRandomVariance));
	ConditionsProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, Conditions));
	IDProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, ID));

	HeaderRow
		.RowTag(StructProperty->GetProperty()->GetFName())
		.WholeRowContent()
		.VAlign(VAlign_Center)
		[
			// Border to capture mouse clicks on the row (used for right click menu).
			SAssignNew(RowBorder, SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.Padding(0.f)
			.ForegroundColor(this, &FMetaStoryTransitionDetails::GetContentRowColor)
			.OnMouseButtonDown(this, &FMetaStoryTransitionDetails::OnRowMouseDown)
			.OnMouseButtonUp(this, &FMetaStoryTransitionDetails::OnRowMouseUp)
			[

				SNew(SHorizontalBox)

				// Icon
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Goto"))
				]

				// Description
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 1.f, 0.f, 0.f))
				[
					SNew(SRichTextBlock)
					.Text(this, &FMetaStoryTransitionDetails::GetDescription)
					.TextStyle(&FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Normal"))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Normal")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Bold")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("i"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Italic")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Subdued")))
				]
				// Debug and property widgets
				+ SHorizontalBox::Slot()
				.FillContentWidth(1.0f, 0.0f) // grow, no shrinking
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(FMargin(8.f, 0.f, 2.f, 0.f))
				[
					SNew(SHorizontalBox)
					// Debugger labels
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						UE::MetaStoryEditor::DebuggerExtensions::CreateTransitionWidget(StructPropertyHandle, MetaStoryViewModel)
					]

					// Options
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SComboButton)
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.OnGetMenuContent(this, &FMetaStoryTransitionDetails::GenerateOptionsMenu)
						.ToolTipText(LOCTEXT("ItemActions", "Item actions"))
						.HasDownArrow(false)
						.ContentPadding(FMargin(4.f, 2.f))
						.ButtonContent()
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("Icons.ChevronDown"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
					]
				]
			]
		]
		.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FMetaStoryTransitionDetails::OnCopyTransition)))
		.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FMetaStoryTransitionDetails::OnPasteTransitions)));
}

void FMetaStoryTransitionDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	check(TriggerProperty);
	check(RequiredEventProperty);
	check(DelegateListener);
	check(DelayTransitionProperty);
	check(DelayDurationProperty);
	check(DelayRandomVarianceProperty);
	check(StateProperty);
	check(ConditionsProperty);
	check(IDProperty);

	TWeakPtr<FMetaStoryTransitionDetails> WeakSelf = SharedThis(this);
	auto IsNotCompletionTransition = [WeakSelf]()
	{
		if (const TSharedPtr<FMetaStoryTransitionDetails> Self = WeakSelf.Pin())
		{
			return !EnumHasAnyFlags(Self->GetTrigger(), EMetaStoryTransitionTrigger::OnStateCompleted) ? EVisibility::Visible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	};

	if (UE::MetaStory::Editor::GbDisplayItemIds)
	{
		StructBuilder.AddProperty(IDProperty.ToSharedRef());
	}

	// Trigger
	StructBuilder.AddProperty(TriggerProperty.ToSharedRef());

	// Show event only when the trigger is set to Event.
	StructBuilder.AddProperty(RequiredEventProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([WeakSelf]()
		{
			if (const TSharedPtr<FMetaStoryTransitionDetails> Self = WeakSelf.Pin())
			{
				return (Self->GetTrigger() == EMetaStoryTransitionTrigger::OnEvent) ? EVisibility::Visible : EVisibility::Collapsed;
			}
			return EVisibility::Collapsed;
		})));

	IDetailPropertyRow& DelegateDispatcherRow = StructBuilder.AddProperty(DelegateListener.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([WeakSelf]()
		{
			if (const TSharedPtr<FMetaStoryTransitionDetails> Self = WeakSelf.Pin())
			{
				return (Self->GetTrigger() == EMetaStoryTransitionTrigger::OnDelegate) ? EVisibility::Visible : EVisibility::Collapsed;
			}
			return EVisibility::Collapsed;
		})));

	FGuid ID;
	UE::MetaStory::PropertyHelpers::GetStructValue<FGuid>(IDProperty, ID);
	DelegateListener->SetInstanceMetaData(UE::PropertyBinding::MetaDataStructIDName, LexToString(ID));

	// State
	StructBuilder.AddProperty(StateProperty.ToSharedRef());

	// Priority
	StructBuilder.AddProperty(PriorityProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(IsNotCompletionTransition)));

	// Delay
	StructBuilder.AddProperty(DelayTransitionProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(IsNotCompletionTransition)));
	StructBuilder.AddProperty(DelayDurationProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(IsNotCompletionTransition)));
	StructBuilder.AddProperty(DelayRandomVarianceProperty.ToSharedRef())
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(IsNotCompletionTransition)));

	// Show conditions always expanded, with simplified header (remove item count)
	IDetailPropertyRow& ConditionsRow = StructBuilder.AddProperty(ConditionsProperty.ToSharedRef());
	ConditionsRow.ShouldAutoExpand(true);

	constexpr bool bShowChildren = true;
	ConditionsRow.CustomWidget(bShowChildren)
		.RowTag(ConditionsProperty->GetProperty()->GetFName())
		.WholeRowContent()
		[
			SNew(SHorizontalBox)

			// Condition text
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(ConditionsProperty->GetPropertyDisplayName())
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]

			// Conditions button
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.Padding(FMargin(0, 0, 3, 0))
				[
					UE::MetaStoryEditor::EditorNodeUtils::CreateAddNodePickerComboButton(
						LOCTEXT("TransitionConditionAddTooltip", "Add new Transition Condition"),
						UE::MetaStory::Colors::Grey,
						ConditionsProperty,
						PropUtils.ToSharedRef())
				]
			]
		];
}

FSlateColor FMetaStoryTransitionDetails::GetContentRowColor() const
{
	return UE::MetaStoryEditor::DebuggerExtensions::IsTransitionEnabled(StructProperty)
		? FSlateColor::UseForeground()
		: FSlateColor::UseSubduedForeground();
}

FReply FMetaStoryTransitionDetails::OnRowMouseDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply FMetaStoryTransitionDetails::OnRowMouseUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(
			RowBorder.ToSharedRef(),
			WidgetPath,
			GenerateOptionsMenu(),
			MouseEvent.GetScreenSpacePosition(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> FMetaStoryTransitionDetails::GenerateOptionsMenu()
{
	FMenuBuilder MenuBuilder(/*ShouldCloseWindowAfterMenuSelection*/true, /*CommandList*/nullptr);

	MenuBuilder.BeginSection(FName("Edit"), LOCTEXT("Edit", "Edit"));

	// Copy
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CopyItem", "Copy"),
		LOCTEXT("CopyItemTooltip", "Copy this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryTransitionDetails::OnCopyTransition),
			FCanExecuteAction()
		));

	// Copy all
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CopyAllItems", "Copy all"),
		LOCTEXT("CopyAllItemsTooltip", "Copy all items"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryTransitionDetails::OnCopyAllTransitions),
			FCanExecuteAction()
		));

	// Paste
	MenuBuilder.AddMenuEntry(
		LOCTEXT("PasteItem", "Paste"),
		LOCTEXT("PasteItemTooltip", "Paste into this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryTransitionDetails::OnPasteTransitions),
			FCanExecuteAction()
		));

	// Duplicate
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DuplicateItem", "Duplicate"),
		LOCTEXT("DuplicateItemTooltip", "Duplicate this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Duplicate"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryTransitionDetails::OnDuplicateTransition),
			FCanExecuteAction()
		));

	// Delete
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeleteItem", "Delete"),
		LOCTEXT("DeleteItemTooltip", "Delete this item"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryTransitionDetails::OnDeleteTransition),
			FCanExecuteAction()
		));

	// Delete all
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DeleteAllItems", "Delete all"),
		LOCTEXT("DeleteAllItemsTooltip", "Delete all items"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMetaStoryTransitionDetails::OnDeleteAllTransitions),
			FCanExecuteAction()
		));

	MenuBuilder.EndSection();

	// Append debugger items.
	UE::MetaStoryEditor::DebuggerExtensions::AppendTransitionMenuItems(MenuBuilder, StructProperty, MetaStoryViewModel);

	return MenuBuilder.MakeWidget();
}

void FMetaStoryTransitionDetails::OnDeleteTransition() const
{
	const int32 Index = StructProperty->GetArrayIndex();
	if (const TSharedPtr<IPropertyHandle> ParentHandle = StructProperty->GetParentHandle())
	{
		if (const TSharedPtr<IPropertyHandleArray> ArrayHandle = ParentHandle->AsArray())
		{
			if (UMetaStoryEditorData* EditorData = GetEditorData())
			{
				FScopedTransaction Transaction(LOCTEXT("DeleteTransition", "Delete Transition"));
				EditorData->Modify();

				ArrayHandle->DeleteItem(Index);

				UE::MetaStoryEditor::RemoveInvalidBindings(EditorData);
			}
		}
	}
}

void FMetaStoryTransitionDetails::OnDeleteAllTransitions() const
{
	if (const TSharedPtr<IPropertyHandle> ParentHandle = StructProperty->GetParentHandle())
	{
		if (const TSharedPtr<IPropertyHandleArray> ArrayHandle = ParentHandle->AsArray())
		{
			if (UMetaStoryEditorData* EditorData = GetEditorData())
			{
				FScopedTransaction Transaction(LOCTEXT("DeleteAllTransitions", "Delete All Transitions"));
				EditorData->Modify();

				ArrayHandle->EmptyArray();

				UE::MetaStoryEditor::RemoveInvalidBindings(EditorData);
			}
		}
	}
}

void FMetaStoryTransitionDetails::OnDuplicateTransition() const
{
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	if (OuterObjects.IsEmpty())
	{
		return;
	}

	if (OuterObjects.Num() != 1)
	{
		// Array Handle Manipulation doesn't support multiple selected objects
		FNotificationInfo NotificationInfo(FText::GetEmpty());
		NotificationInfo.Text = LOCTEXT("NotSupportedByMultipleObjects", "Operation is not supported for multi-selected objects");
		NotificationInfo.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);

		return;
	}

	if (ParentArrayProperty)
	{
		if (UMetaStoryEditorData* EditorDataPtr = GetEditorData())
		{
			void* TransitionPtr = nullptr;
			if (StructProperty->GetValueData(TransitionPtr) == FPropertyAccess::Success)
			{
				UE::MetaStoryEditor::FMetaStoryClipboardEditorData Clipboard;
				Clipboard.Append(EditorDataPtr, TConstArrayView<FMetaStoryTransition>(static_cast<FMetaStoryTransition*>(TransitionPtr), 1));
				Clipboard.ProcessBuffer(nullptr, EditorDataPtr, OuterObjects[0]);

				if (!Clipboard.IsValid())
				{
					return;
				}

				FScopedTransaction Transaction(LOCTEXT("DuplicateTransition", "Duplicate Transition"));

				// Might modify the bindings data
				EditorDataPtr->Modify();

				const int32 Index = StructProperty->GetArrayIndex();
				ParentArrayProperty->Insert(Index);

				TSharedPtr<IPropertyHandle> InsertedElementHandle = ParentArrayProperty->GetElement(Index);
				void* InsertedTransitionPtr = nullptr;
				if (InsertedElementHandle->GetValueData(InsertedTransitionPtr) == FPropertyAccess::Success)
				{
					*static_cast<FMetaStoryTransition*>(InsertedTransitionPtr) = MoveTemp(Clipboard.GetTransitionsInBuffer()[0]);

					for (FMetaStoryPropertyPathBinding& Binding : Clipboard.GetBindingsInBuffer())
					{
						EditorDataPtr->GetPropertyEditorBindings()->AddStateTreeBinding(MoveTemp(Binding));
					}
				}

				// We reinieitalized item nodes on ArrayProperty operations before the data is completely set up. Reinitialize.
				if (PropUtils)
				{
					PropUtils->ForceRefresh();
				}
			}
		}
	}
}

EMetaStoryTransitionTrigger FMetaStoryTransitionDetails::GetTrigger() const
{
	check(TriggerProperty);
	EMetaStoryTransitionTrigger TriggerValue = EMetaStoryTransitionTrigger::None;
	if (TriggerProperty.IsValid())
	{
		TriggerProperty->GetValue((uint8&)TriggerValue);
	}
	return TriggerValue;
}

bool FMetaStoryTransitionDetails::GetDelayTransition() const
{
	check(DelayTransitionProperty);
	bool bDelayTransition = false;
	if (DelayTransitionProperty.IsValid())
	{
		DelayTransitionProperty->GetValue(bDelayTransition);
	}
	return bDelayTransition;
}

FText FMetaStoryTransitionDetails::GetDescription() const
{
	check(StateProperty);

	const FMetaStoryTransition* Transition = UE::MetaStory::PropertyHelpers::GetStructPtr<FMetaStoryTransition>(StructProperty);
	if (!Transition)
	{
		return LOCTEXT("MultipleSelected", "Multiple Selected");
	}

	return UE::MetaStory::Editor::GetTransitionDesc(GetEditorData(), *Transition, EMetaStoryNodeFormatting::RichText);
}

void FMetaStoryTransitionDetails::OnCopyTransition() const
{
	UMetaStoryEditorData* EditorDataPtr = GetEditorData();
	if (!EditorDataPtr)
	{
		return;
	}

	void* TransitionAddress = nullptr;
	if (StructProperty->GetValueData(TransitionAddress) == FPropertyAccess::Success)
	{
		UE::MetaStoryEditor::FMetaStoryClipboardEditorData Clipboard;
		Clipboard.Append(EditorDataPtr, TConstArrayView<FMetaStoryTransition>(static_cast<FMetaStoryTransition*>(TransitionAddress), 1));

		UE::MetaStoryEditor::ExportTextAsClipboardEditorData(Clipboard);
	}
}

void FMetaStoryTransitionDetails::OnCopyAllTransitions() const
{
	UMetaStoryEditorData* EditorDataPtr = GetEditorData();
	if (!EditorDataPtr)
	{
		return;
	}

	if (ParentArrayProperty)
	{
		void* TransitionArrayAddress = nullptr;
		if (ParentProperty->GetValueData(TransitionArrayAddress) == FPropertyAccess::Success)
		{
			UE::MetaStoryEditor::FMetaStoryClipboardEditorData Clipboard;
			Clipboard.Append(EditorDataPtr, *static_cast<TArray<FMetaStoryTransition>*>(TransitionArrayAddress));

			UE::MetaStoryEditor::ExportTextAsClipboardEditorData(Clipboard);
		}
	}
	else
	{
		OnCopyTransition();
	}
}

UMetaStoryEditorData* FMetaStoryTransitionDetails::GetEditorData() const
{
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (UObject* Outer : OuterObjects)
	{
		UMetaStoryEditorData* OuterEditorData = Cast<UMetaStoryEditorData>(Outer);
		if (OuterEditorData == nullptr)
		{
			OuterEditorData = Outer->GetTypedOuter<UMetaStoryEditorData>();
		}
		if (OuterEditorData)
		{
			return OuterEditorData;
		}
	}
	return nullptr;
}

void FMetaStoryTransitionDetails::OnPasteTransitions() const
{
	using namespace UE::MetaStoryEditor;

	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	if (OuterObjects.IsEmpty())
	{
		return;
	}

	UMetaStoryEditorData* EditorDataPtr = GetEditorData();
	if (!EditorDataPtr)
	{
		return;
	}

	// In case its multi selected, we need to have a unique copy for each Object
	TArray<FMetaStoryClipboardEditorData, TInlineAllocator<2>> ClipboardTransitions;
	ClipboardTransitions.AddDefaulted(OuterObjects.Num());

	for (int32 Idx = 0; Idx < OuterObjects.Num(); ++Idx)
	{
		const bool bSuccess = ImportTextAsClipboardEditorData
		(TBaseStructure<FMetaStoryTransition>::Get(),
			EditorDataPtr,
			OuterObjects[Idx],
			ClipboardTransitions[Idx]);

		if (!bSuccess)
		{
			return;
		}
	}

	// make sure each Clipboard has the same number of nodes
	for (int32 Idx = 0; Idx < ClipboardTransitions.Num() - 1; ++Idx)
	{
		check(ClipboardTransitions[Idx].GetTransitionsInBuffer().Num() == ClipboardTransitions[Idx + 1].GetTransitionsInBuffer().Num());
	}

	int32 NumTransitionsInBuffer = ClipboardTransitions[0].GetTransitionsInBuffer().Num();

	if (NumTransitionsInBuffer == 0)
	{
		return;
	}

	if (!ParentArrayProperty.IsValid() && NumTransitionsInBuffer != 1)
	{
		// Transition is not in an array. we can't do multi-to-one paste
		return;
	}

	if (OuterObjects.Num() != 1 && NumTransitionsInBuffer != 1)
	{
		// if multiple selected objects, and we have more than one nodes to paste into
		// Array Handle doesn't support manipulation on multiple objects.
		FNotificationInfo NotificationInfo(FText::GetEmpty());
		NotificationInfo.Text = LOCTEXT("NotSupportedByMultipleObjects", "Operation is not supported for multi-selected objects");
		NotificationInfo.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);

		return;
	}

	{
		FScopedTransaction Transaction(LOCTEXT("PasteTransition", "Paste Transition"));

		EditorDataPtr->Modify();	// we might modify the bindings on Editor Data
		StructProperty->NotifyPreChange();

		if (ParentArrayProperty)
		{
			// Paste multi nodes into one node
			const int32 StructIndex = StructProperty->GetIndexInArray();
			check(StructIndex != INDEX_NONE);

			uint32 NumArrayElements = 0;
			ParentArrayProperty->GetNumElements(NumArrayElements);
			check(NumArrayElements > 0);	// since we already have at least one element to paste into

			// Insert or append uninitialized elements after the current node to match the number of nodes in the paste buffer and retain the order of elements 
			// The first node in the buffer goes into the current node
			const int32 IndexToInsert = StructIndex + 1;
			const int32 NumElementsToAddOrInsert = ClipboardTransitions[0].GetTransitionsInBuffer().Num() - 1;

			int32 Cnt = 0;
			if (IndexToInsert == NumArrayElements)
			{
				while (Cnt++ < NumElementsToAddOrInsert)
				{
					FPropertyHandleItemAddResult Result = ParentArrayProperty->AddItem();
					if (Result.GetAccessResult() != FPropertyAccess::Success)
					{
						return;
					}
				}
			}
			else
			{
				while (Cnt++ < NumElementsToAddOrInsert)
				{
					FPropertyAccess::Result Result = ParentArrayProperty->Insert(IndexToInsert);
					if (Result != FPropertyAccess::Success)
					{
						return;
					}
				}
			}

			TArray<void*> RawDatasArray;
			ParentProperty->AccessRawData(RawDatasArray);
			check(RawDatasArray.Num() == OuterObjects.Num());

			for (int32 ObjIdx = 0; ObjIdx < OuterObjects.Num(); ++ObjIdx)
			{
				if (TArray<FMetaStoryTransition>* TransitionsPtr = static_cast<TArray<FMetaStoryTransition>*>(RawDatasArray[ObjIdx]))
				{
					TArrayView<FMetaStoryTransition> TransitionsClipboardBuffer = ClipboardTransitions[ObjIdx].GetTransitionsInBuffer();
					TArray<FMetaStoryTransition>& TransitionsToPasteInto = *TransitionsPtr;

					for (int32 Idx = 0; Idx < TransitionsClipboardBuffer.Num(); ++Idx)
					{
						TransitionsToPasteInto[Idx + StructIndex] = MoveTemp(TransitionsClipboardBuffer[Idx]);
					}

					for (FMetaStoryPropertyPathBinding& Binding : ClipboardTransitions[ObjIdx].GetBindingsInBuffer())
					{
						EditorDataPtr->GetPropertyEditorBindings()->AddStateTreeBinding(MoveTemp(Binding));
					}
				}
			}
		}
		else
		{
			// Paste single node to a single Node
			TArray<void*> RawDatas;
			StructProperty->AccessRawData(RawDatas);
			check(RawDatas.Num() == OuterObjects.Num());

			for (int32 Idx = 0; Idx < RawDatas.Num(); ++Idx)
			{
				if (FMetaStoryTransition* CurrentTransition = static_cast<FMetaStoryTransition*>(RawDatas[Idx]))
				{
					*CurrentTransition = MoveTemp(ClipboardTransitions[Idx].GetTransitionsInBuffer()[0]);

					for (FMetaStoryPropertyPathBinding& Binding : ClipboardTransitions[Idx].GetBindingsInBuffer())
					{
						EditorDataPtr->GetPropertyEditorBindings()->AddStateTreeBinding(MoveTemp(Binding));
					}
				}
			}
		}

		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();
	}

	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
