// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryStateLinkDetails.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "MetaStory.h"
#include "MetaStoryEditorData.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "MetaStoryDescriptionHelpers.h"
#include "MetaStoryEditorStyle.h"
#include "MetaStoryPropertyHelpers.h"
#include "TextStyleDecorator.h"
#include "Widgets/SMetaStoryCompactTreeEditorView.h"
#include "Widgets/Text/SRichTextBlock.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

TSharedRef<IPropertyTypeCustomization> FMetaStoryStateLinkDetails::MakeInstance()
{
	return MakeShareable(new FMetaStoryStateLinkDetails);
}

void FMetaStoryStateLinkDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

	NameProperty = StructProperty->GetChildHandle(TEXT("Name"));
	IDProperty = StructProperty->GetChildHandle(TEXT("ID"));
	LinkTypeProperty = StructProperty->GetChildHandle(TEXT("LinkType"));

	if (const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty())
	{
		static const FName NAME_DirectStatesOnly = "DirectStatesOnly";
		static const FName NAME_SubtreesOnly = "SubtreesOnly";
		
		bDirectStatesOnly = MetaDataProperty->HasMetaData(NAME_DirectStatesOnly);
		bSubtreesOnly = MetaDataProperty->HasMetaData(NAME_SubtreesOnly);
	}

	// Store pointer to editor data.
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (const UObject* Object : OuterObjects)
	{
		if (Object)
		{
			if (const UMetaStory* OuterStateTree = Object->GetTypedOuter<UMetaStory>())
			{
				WeakEditorData = Cast<UMetaStoryEditorData>(OuterStateTree->EditorData);
				if (WeakEditorData.IsValid())
				{
					break;
				}
			}
		}
	}
	
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.VAlign(VAlign_Center)
		[
			SAssignNew(ComboButton, SComboButton)
			.OnGetMenuContent(this, &FMetaStoryStateLinkDetails::GenerateStatePicker)
			.ButtonContent()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0,0,4,0)
				[
					SNew(SImage)
					.ToolTipText(LOCTEXT("MissingState", "The specified state cannot be found."))
					.Visibility_Lambda([this]()
					{
						return IsValidLink() ? EVisibility::Collapsed : EVisibility::Visible;
					})
					.Image(FAppStyle::GetBrush("Icons.ErrorWithColor"))
				]

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(0, 2.0f, 4.0f, 2.0f)
				.AutoWidth()
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
					.Image(this, &FMetaStoryStateLinkDetails::GetCurrentStateIcon)
					.ColorAndOpacity(this, &FMetaStoryStateLinkDetails::GetCurrentStateColor)
				]
				
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SRichTextBlock)
					.Text(this, &FMetaStoryStateLinkDetails::GetCurrentStateDesc)
					.TextStyle(&FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Normal"))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Normal")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Bold")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("i"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Italic")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Subdued")))
				]
			]
		];
}

void FMetaStoryStateLinkDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

TSharedRef<SWidget> FMetaStoryStateLinkDetails::GenerateStatePicker()
{
	check(ComboButton);
	
	constexpr bool bCloseMenuAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseMenuAfterSelection, nullptr);

	if (!bDirectStatesOnly)
	{
		auto MakeMetaStateWidget = [WeakEditorData = WeakEditorData](EMetaStoryTransitionType Type)
		{
			const FMetaStoryStateLink Link(Type);
			
			return SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(0, 2.0f, 4.0f, 2.0f)
				.AutoWidth()
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
					.Image(UE::MetaStory::Editor::GetStateLinkIcon(WeakEditorData.Get(), Link))
					.ColorAndOpacity(UE::MetaStory::Editor::GetStateLinkColor(WeakEditorData.Get(), Link))
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SRichTextBlock)
					.Text(UE::MetaStory::Editor::GetStateLinkDesc(WeakEditorData.Get(), Link, EMetaStoryNodeFormatting::RichText))
					.TextStyle(&FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal"))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Bold")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("i"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Italic")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Subdued")))
				];
		};

		MenuBuilder.AddMenuEntry(
			FUIAction(
				FExecuteAction::CreateSP(this, &FMetaStoryStateLinkDetails::SetTransitionByType, EMetaStoryTransitionType::None),
				FCanExecuteAction(),
				FIsActionChecked::CreateSPLambda(this, [this]
				{
					return GetTransitionType() == EMetaStoryTransitionType::None; 
				})),
			MakeMetaStateWidget(EMetaStoryTransitionType::None),
			FName(),
			LOCTEXT("TransitionNoneTooltip", "No transition."),
			EUserInterfaceActionType::Check);

		MenuBuilder.AddMenuEntry(
			FUIAction(
				FExecuteAction::CreateSP(this, &FMetaStoryStateLinkDetails::SetTransitionByType, EMetaStoryTransitionType::NextState),
				FCanExecuteAction(),
				FIsActionChecked::CreateSPLambda(this, [this]
				{
					return GetTransitionType() == EMetaStoryTransitionType::NextState; 
				})),
			MakeMetaStateWidget(EMetaStoryTransitionType::NextState),
			FName(),
			LOCTEXT("TransitionNextTooltip", "Goto next sibling State."),
			EUserInterfaceActionType::Check);

		MenuBuilder.AddMenuEntry(
			FUIAction(
				FExecuteAction::CreateSP(this, &FMetaStoryStateLinkDetails::SetTransitionByType, EMetaStoryTransitionType::NextSelectableState),
				FCanExecuteAction(),
				FIsActionChecked::CreateSPLambda(this, [this]
				{
					return GetTransitionType() == EMetaStoryTransitionType::NextSelectableState; 
			})),
			MakeMetaStateWidget(EMetaStoryTransitionType::NextSelectableState),
			FName(),
			LOCTEXT("TransitionNextSelectableTooltip", "Goto next sibling state, whose enter conditions pass."),
			EUserInterfaceActionType::Check);

		MenuBuilder.AddMenuEntry(
			FUIAction(
				FExecuteAction::CreateSP(this, &FMetaStoryStateLinkDetails::SetTransitionByType, EMetaStoryTransitionType::Succeeded),
				FCanExecuteAction(),
				FIsActionChecked::CreateSPLambda(this, [this]
				{
					return GetTransitionType() == EMetaStoryTransitionType::Succeeded; 
				})),
			MakeMetaStateWidget(EMetaStoryTransitionType::Succeeded),
			FName(),
			LOCTEXT("TransitionTreeSuccessTooltip", "Complete tree with success."),
			EUserInterfaceActionType::Check);

		MenuBuilder.AddMenuEntry(
			FUIAction(
				FExecuteAction::CreateSP(this, &FMetaStoryStateLinkDetails::SetTransitionByType, EMetaStoryTransitionType::Failed),
				FCanExecuteAction(),
				FIsActionChecked::CreateSPLambda(this, [this]
				{
					return GetTransitionType() == EMetaStoryTransitionType::Failed; 
				})),
			MakeMetaStateWidget(EMetaStoryTransitionType::Failed),
			FName(),
			LOCTEXT("TransitionTreeFailedTooltip", "Complete tree with failure."),
			EUserInterfaceActionType::Check);
	}

	MenuBuilder.BeginSection("States", LOCTEXT("States", "States"));

	TSharedPtr<UE::MetaStory::SCompactTreeEditorView> StateView;
	
	TSharedRef<SWidget> MenuWidget = 
		SNew(SBox)
		.MinDesiredWidth(300.f)
		.MaxDesiredHeight(400.f)
		.Padding(2)	
		[
			SAssignNew(StateView, UE::MetaStory::SCompactTreeEditorView)
			.MetaStoryEditorData(WeakEditorData.Get())
			.SelectionMode(ESelectionMode::Single)
			.SelectableStatesOnly(true)
			.SubtreesOnly(bSubtreesOnly)
			.OnSelectionChanged(this, &FMetaStoryStateLinkDetails::OnStateSelected)
		];

	check(StateView);
	
	const EMetaStoryTransitionType TransitionType = GetTransitionType().Get(EMetaStoryTransitionType::Failed);
	if (TransitionType == EMetaStoryTransitionType::GotoState)
	{
		if (const UMetaStoryState* State = GetState())
		{
			StateView->SetSelection({ State->ID });
		}
	}

	ComboButton->SetMenuContentWidgetToFocus(StateView->GetWidgetToFocusOnOpen());

	MenuBuilder.AddWidget(MenuWidget, FText::GetEmpty(), /*bInNoIndent*/true);
	
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FMetaStoryStateLinkDetails::OnStateSelected(TConstArrayView<FGuid> SelectedStateIDs)
{
	const UMetaStoryState* State = nullptr;
	if (!SelectedStateIDs.IsEmpty())
	{
		if (const UMetaStoryEditorData* EditorData = WeakEditorData.Get())
		{
			State = EditorData->GetStateByID(SelectedStateIDs[0]);
		}
	}

	if (State
		&& NameProperty
		&& IDProperty)
	{
		FScopedTransaction Transaction(FText::Format(LOCTEXT("SetPropertyValue", "Set {0}"), StructProperty->GetPropertyDisplayName()));

		LinkTypeProperty->SetValue((uint8)EMetaStoryTransitionType::GotoState);

		NameProperty->SetValue(State->Name, EPropertyValueSetFlags::NotTransactable);
		UE::MetaStory::PropertyHelpers::SetStructValue<FGuid>(IDProperty, State->ID, EPropertyValueSetFlags::NotTransactable);
	}

	check(ComboButton);
	ComboButton->SetIsOpen(false);
}

void FMetaStoryStateLinkDetails::SetTransitionByType(const EMetaStoryTransitionType TransitionType)
{
	if (NameProperty
		&& IDProperty)
	{
		FScopedTransaction Transaction(FText::Format(LOCTEXT("SetPropertyValue", "Set {0}"), StructProperty->GetPropertyDisplayName()));

		LinkTypeProperty->SetValue((uint8)TransitionType);

		// Clear name and id.
		NameProperty->SetValue(FName(), EPropertyValueSetFlags::NotTransactable);
		UE::MetaStory::PropertyHelpers::SetStructValue<FGuid>(IDProperty, FGuid(), EPropertyValueSetFlags::NotTransactable);
	}

	check(ComboButton);
	ComboButton->SetIsOpen(false);
}

const UMetaStoryState* FMetaStoryStateLinkDetails::GetState() const
{
	if (const UMetaStoryEditorData* EditorData = WeakEditorData.Get())
	{
		FGuid StateID;
		if (UE::MetaStory::PropertyHelpers::GetStructValue<FGuid>(IDProperty, StateID) == FPropertyAccess::Success)
		{
			return EditorData->GetStateByID(StateID);
		}
	}
	return nullptr;
}

FText FMetaStoryStateLinkDetails::GetCurrentStateDesc() const
{
	if (const FMetaStoryStateLink* Link = UE::MetaStory::PropertyHelpers::GetStructPtr<FMetaStoryStateLink>(StructProperty))
	{
		return UE::MetaStory::Editor::GetStateLinkDesc(WeakEditorData.Get(), *Link, EMetaStoryNodeFormatting::RichText);
	}
	return LOCTEXT("MultipleSelected", "Multiple Selected");
}

const FSlateBrush* FMetaStoryStateLinkDetails::GetCurrentStateIcon() const
{
	if (const FMetaStoryStateLink* Link = UE::MetaStory::PropertyHelpers::GetStructPtr<FMetaStoryStateLink>(StructProperty))
	{
		return UE::MetaStory::Editor::GetStateLinkIcon(WeakEditorData.Get(), *Link);
	}
	return nullptr;
}

FSlateColor FMetaStoryStateLinkDetails::GetCurrentStateColor() const
{
	if (const FMetaStoryStateLink* Link = UE::MetaStory::PropertyHelpers::GetStructPtr<FMetaStoryStateLink>(StructProperty))
	{
		return UE::MetaStory::Editor::GetStateLinkColor(WeakEditorData.Get(), *Link);
	}
	return FSlateColor::UseForeground();
}

bool FMetaStoryStateLinkDetails::IsValidLink() const
{
	const EMetaStoryTransitionType TransitionType = GetTransitionType().Get(EMetaStoryTransitionType::Failed);

	if (TransitionType == EMetaStoryTransitionType::GotoState)
	{
		return GetState() != nullptr;
	}

	return true;
}

TOptional<EMetaStoryTransitionType> FMetaStoryStateLinkDetails::GetTransitionType() const
{
	if (LinkTypeProperty)
	{
		uint8 Value;
		if (LinkTypeProperty->GetValue(Value) == FPropertyAccess::Success)
		{
			return EMetaStoryTransitionType(Value);
		}
	}
	return TOptional<EMetaStoryTransitionType>();
}

#undef LOCTEXT_NAMESPACE
