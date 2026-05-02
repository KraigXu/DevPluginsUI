// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaStoryViewRow.h"
#include "SMetaStoryView.h"
#include "SMetaStoryExpanderArrow.h"
#include "MetaStoryEditor.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorStyle.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "MetaStory.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryDescriptionHelpers.h"
#include "MetaStoryDragDrop.h"
#include "MetaStoryEditorModule.h"
#include "MetaStoryEditorUserSettings.h"
#include "MetaStoryState.h"
#include "MetaStoryTaskBase.h"
#include "MetaStoryTypes.h"
#include "MetaStoryViewModel.h"
#include "Widgets/Views/SListView.h"
#include "TextStyleDecorator.h"
#include "Customizations/MetaStoryEditorNodeUtils.h"
#include "Customizations/Widgets/SMetaStoryContextMenuButton.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

namespace UE::MetaStory::Editor
{
	FLinearColor LerpColorSRGB(const FLinearColor ColorA, FLinearColor ColorB, float T)
	{
		const FColor A = ColorA.ToFColorSRGB();
		const FColor B = ColorB.ToFColorSRGB();
		return FLinearColor(FColor(
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.R) * (1.f - T) + static_cast<float>(B.R) * T)),
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.G) * (1.f - T) + static_cast<float>(B.G) * T)),
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.B) * (1.f - T) + static_cast<float>(B.B) * T)),
			static_cast<uint8>(FMath::RoundToInt(static_cast<float>(A.A) * (1.f - T) + static_cast<float>(B.A) * T))));
	}

	static constexpr FLinearColor IconTint = FLinearColor(1, 1, 1, 0.5f);
} // UE:MetaStory::Editor

void SMetaStoryViewRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TWeakObjectPtr<UMetaStoryState> InState, const TSharedPtr<SScrollBox>& ViewBox, TSharedPtr<FMetaStoryViewModel> InMetaStoryViewModel)
{
	MetaStoryViewModel = InMetaStoryViewModel;
	WeakState = InState;
	const UMetaStoryState* State = InState.Get();
	WeakEditorData = State != nullptr ? State->GetTypedOuter<UMetaStoryEditorData>() : nullptr;

	AssetChangedHandle = MetaStoryViewModel->GetOnAssetChanged().AddSP(this, &SMetaStoryViewRow::HandleAssetChanged);
	StatesChangedHandle = MetaStoryViewModel->GetOnStatesChanged().AddSP(this, &SMetaStoryViewRow::HandleStatesChanged);

	ConstructInternal(STableRow::FArguments()
		.OnDragDetected(this, &SMetaStoryViewRow::HandleDragDetected)
		.OnDragLeave(this, &SMetaStoryViewRow::HandleDragLeave)
		.OnCanAcceptDrop(this, &SMetaStoryViewRow::HandleCanAcceptDrop)
		.OnAcceptDrop(this, &SMetaStoryViewRow::HandleAcceptDrop)
		.Style(&FMetaStoryEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("MetaStory.Selection"))
		, InOwnerTableView);

	TSharedPtr<SVerticalBox> StateAndTasksVerticalBox;
	TSharedPtr<SHorizontalBox> StateHorizontalBox;
	TSharedPtr<SBorder> FlagBorder;

	this->ChildSlot
	.HAlign(HAlign_Fill)
	[
		SNew(SBox)
		.MinDesiredWidth_Lambda([WeakOwnerViewBox = ViewBox.ToWeakPtr()]()
			{
				// Captured as weak ptr so we don't prevent our parent widget from being destroyed (circular pointer reference).
				if (const TSharedPtr<SScrollBox> OwnerViewBox = WeakOwnerViewBox.Pin())
				{
					// Make the row at least as wide as the view.
					// The -1 is needed or we'll see a scrollbar.
					return OwnerViewBox->GetTickSpaceGeometry().GetLocalSize().X - 1;
				}
				return 0.f;
			})
		.Padding(FMargin(0, 0, 0, 0))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SMetaStoryExpanderArrow, SharedThis(this))
				.IndentAmount(24.f)
				.BaseIndentLevel(0)
				.ImageSize(FVector2f(16,16))
				.ImagePadding(FMargin(9,14,0,0))
				.Image(this, &SMetaStoryViewRow::GetSelectorIcon)
				.ColorAndOpacity(FLinearColor(1, 1, 1, 0.2f))
				.WireColorAndOpacity(FLinearColor(1, 1, 1, 0.2f))
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Left)
			.AutoWidth()
			.Padding(FMargin(0, 6, 0, 6))
			[
				// State and tasks
				SAssignNew(StateAndTasksVerticalBox, SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					// State
					SNew(SBox)
					.HeightOverride(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewStateRowHeight())
					.HAlign(HAlign_Left)
					[
						SAssignNew(StateHorizontalBox, SHorizontalBox)

						// State Box
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						//.FillWidth(1.f)
						.AutoWidth()
						[
							SNew(SBox)
							.HeightOverride(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewStateRowHeight())
							.VAlign(VAlign_Fill)
							[
								SNew(SBorder)
								.BorderImage(FMetaStoryEditorStyle::Get().GetBrush("MetaStory.State.Border"))
								.BorderBackgroundColor(this, &SMetaStoryViewRow::GetActiveStateColor)
								[
									SNew(SBorder)
									.BorderImage(FMetaStoryEditorStyle::Get().GetBrush("MetaStory.State"))
									.BorderBackgroundColor(this, &SMetaStoryViewRow::GetTitleColor, 1.0f, 0.0f)
									.Padding(FMargin(0.f, 0.f, 12.f, 0.f))
									.IsEnabled_Lambda([InState]
									{
										const UMetaStoryState* State = InState.Get();
										return State != nullptr && State->bEnabled;
									})
									[
										SNew(SOverlay)
										+ SOverlay::Slot()
										[
											SNew(SHorizontalBox)

											// Sub tree marker
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.AutoWidth()
											.Padding(FMargin(0,0,0,0))
											[
												SNew(SBox)
												.WidthOverride(4.f)
												.HeightOverride(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewStateRowHeight())
												.Visibility(this, &SMetaStoryViewRow::GetSubTreeVisibility)
												.VAlign(VAlign_Fill)
												.HAlign(HAlign_Fill)
												[
													SNew(SBorder)
													.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
													.BorderBackgroundColor(FLinearColor(1,1,1,0.25f))
												]
											]

											// Conditions icon
											+SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.AutoWidth()
											[
												SNew(SBox)
												.Padding(FMargin(4.f, 0.f, -4.f, 0.f))
												.Visibility(this, &SMetaStoryViewRow::GetConditionVisibility)
												[
													SNew(SImage)
													.ColorAndOpacity(UE::MetaStory::Editor::IconTint)
													.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.StateConditions"))
													.ToolTipText(LOCTEXT("StateHasEnterConditions", "State selection is guarded with enter conditions."))
												]
											]

											// Selector icon
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.AutoWidth()
											[
												SNew(SBox)
												.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
												[
													SNew(SImage)
													.Image(this, &SMetaStoryViewRow::GetSelectorIcon)
													.ColorAndOpacity(UE::MetaStory::Editor::IconTint)
													.ToolTipText(this, &SMetaStoryViewRow::GetSelectorTooltip)
												]
											]

											// Warnings
											+SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.AutoWidth()
											[
												SNew(SBox)
												.Padding(FMargin(2.f, 0.f, 2.f, 1.f))
												.Visibility(this, &SMetaStoryViewRow::GetWarningsVisibility)
												[
													SNew(SImage)
													.Image(FAppStyle::Get().GetBrush("Icons.Warning"))
													.ToolTipText(this, &SMetaStoryViewRow::GetWarningsTooltipText)
												]
											]
											
											// State Name
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.AutoWidth()
											[
												SAssignNew(NameTextBlock, SInlineEditableTextBlock)
												.Style(FMetaStoryEditorStyle::Get(), "MetaStory.State.TitleInlineEditableText")
												.OnTextCommitted(this, &SMetaStoryViewRow::HandleNodeLabelTextCommitted)
												.OnVerifyTextChanged(this, &SMetaStoryViewRow::HandleVerifyNodeLabelTextChanged)
												.Text(this, &SMetaStoryViewRow::GetStateDesc)
												.ToolTipText(this, &SMetaStoryViewRow::GetStateTypeTooltip)
												.Clipping(EWidgetClipping::ClipToBounds)
												.IsSelected(this, &SMetaStoryViewRow::IsStateSelected)
											]

											// Description
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.AutoWidth()
											[
												SNew(SBox)
												.Padding(FMargin(2.f, 0.f, 2.f, 1.f))
												.Visibility(this, &SMetaStoryViewRow::GetStateDescriptionVisibility)
												[
													SNew(SImage)
													.Image(FAppStyle::Get().GetBrush(TEXT("Icons.Comment")))
													.ColorAndOpacity(FStyleColors::Foreground)
													.ColorAndOpacity(UE::MetaStory::Editor::IconTint)
													.ToolTipText(this, &SMetaStoryViewRow::GetStateDescription)
												]
											]

											// Flags icons
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.AutoWidth()
											.Padding(FMargin(0.0f))
											[
												SAssignNew(FlagsContainer, SBorder)
												.BorderImage(FStyleDefaults::GetNoBrush())
											]

											// Linked State
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.AutoWidth()
											[
												SNew(SBox)
												.HeightOverride(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewStateRowHeight())
												.VAlign(VAlign_Fill)
												.Visibility(this, &SMetaStoryViewRow::GetLinkedStateVisibility)
												[
													// Link icon
													SNew(SHorizontalBox)
													+ SHorizontalBox::Slot()
													.VAlign(VAlign_Center)
													.AutoWidth()
													.Padding(FMargin(4.f, 0.f, 4.f, 0.f))
													[
														SNew(SImage)
														.ColorAndOpacity(UE::MetaStory::Editor::IconTint)
														.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.StateLinked"))
													]

													// Linked State
													+ SHorizontalBox::Slot()
													.VAlign(VAlign_Center)
													.AutoWidth()
													[
														SNew(STextBlock)
														.Text(this, &SMetaStoryViewRow::GetLinkedStateDesc)
														.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Details")
													]
												]
											]
											
											// State ID
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.AutoWidth()
											[
												SNew(STextBlock)
												.Visibility_Lambda([]()
												{
													return UE::MetaStory::Editor::GbDisplayItemIds ? EVisibility::Visible : EVisibility::Collapsed;
												})
												.Text(this, &SMetaStoryViewRow::GetStateIDDesc)
												.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Details")
											]
										]
										+ SOverlay::Slot()
										[
											SNew(SHorizontalBox)

											// State breakpoint box
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Top)
											.HAlign(HAlign_Left)
											.AutoWidth()
											[
												SNew(SBox)
												.Padding(FMargin(-12.f, -6.f, 0.f, 0.f))
												[
													SNew(SImage)
													.DesiredSizeOverride(FVector2D(12.f, 12.f))
													.Image(FMetaStoryEditorStyle::Get().GetBrush(TEXT("MetaStoryEditor.Debugger.Breakpoint.EnabledAndValid")))
													.Visibility(this, &SMetaStoryViewRow::GetStateBreakpointVisibility)
													.ToolTipText(this, &SMetaStoryViewRow::GetStateBreakpointTooltipText)
												]
											]
										]
									]
								]
							]
						]
					]
				]
			]
		]
	];

	if (EnumHasAllFlags(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewDisplayNodeType(), EMetaStoryEditorUserSettingsNodeType::Transition))
	{
		StateHorizontalBox->AddSlot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Left)
		[
			// Transitions
			SAssignNew(TransitionsContainer, SHorizontalBox)
		];
	}

	if (EnumHasAllFlags(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewDisplayNodeType(), EMetaStoryEditorUserSettingsNodeType::Condition))
	{
		StateAndTasksVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0, 2, 0, 0))
		[
			MakeConditionsWidget(ViewBox)
		];
	}

	if (EnumHasAllFlags(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewDisplayNodeType(), EMetaStoryEditorUserSettingsNodeType::Task))
	{
		StateAndTasksVerticalBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0, 2, 0, 0))
		[
			MakeTasksWidget(ViewBox)
		];
	}

	MakeTransitionsWidget();
	MakeFlagsWidget();
}


SMetaStoryViewRow::~SMetaStoryViewRow()
{
	if (MetaStoryViewModel)
	{
		MetaStoryViewModel->GetOnAssetChanged().Remove(AssetChangedHandle);
		MetaStoryViewModel->GetOnStatesChanged().Remove(StatesChangedHandle);
	}
}

TSharedRef<SWidget> SMetaStoryViewRow::MakeTasksWidget(const TSharedPtr<SScrollBox>& ViewBox)
{
	const UMetaStoryEditorData* EditorData = WeakEditorData.Get();
	const UMetaStoryState* State = WeakState.Get();
	if (!EditorData || !State)
	{
		return SNullWidget::NullWidget;
	}

	const TSharedRef<SWrapBox> TasksBox = SNew(SWrapBox)
	.PreferredSize_Lambda([WeakOwnerViewBox = ViewBox.ToWeakPtr()]()
	{
		// Captured as weak ptr so we don't prevent our parent widget from being destroyed (circular pointer reference).
		if (const TSharedPtr<SScrollBox> OwnerViewBox = WeakOwnerViewBox.Pin())
		{
			return FMath::Max(300, OwnerViewBox->GetTickSpaceGeometry().GetLocalSize().X - 200);
		}
		return 0.f;
	});

	if (State->Tasks.IsEmpty())
	{
		return SNullWidget::NullWidget;
	}

	const int32 NumTasks = State->Tasks.Num();

	// The task descriptions can get long. Make some effort to limit how long they can get.
	for (int32 TaskIndex = 0; TaskIndex < NumTasks; TaskIndex++)
	{
		const FMetaStoryEditorNode& TaskNode = State->Tasks[TaskIndex];
		if (const FMetaStoryTaskBase* Task = TaskNode.Node.GetPtr<FMetaStoryTaskBase>())
		{
			const FGuid TaskId = State->Tasks[TaskIndex].ID;
			auto IsTaskEnabledFunc = [WeakState = WeakState, TaskIndex]
				{
					const UMetaStoryState* State = WeakState.Get();
					if (State != nullptr && State->Tasks.IsValidIndex(TaskIndex))
					{
						if (const FMetaStoryTaskBase* Task = State->Tasks[TaskIndex].Node.GetPtr<FMetaStoryTaskBase>())
						{
							return (State->bEnabled && Task->bTaskEnabled);
						}
					}
					return true;
				};

			auto IsTaskBreakpointEnabledFunc = [WeakEditorData = WeakEditorData, TaskId]
				{
#if WITH_METASTORY_TRACE_DEBUGGER
					const UMetaStoryEditorData* EditorData = WeakEditorData.Get();
					if (EditorData != nullptr && EditorData->HasAnyBreakpoint(TaskId))
					{
						return EVisibility::Visible;
					}
#endif // WITH_METASTORY_TRACE_DEBUGGER
					return EVisibility::Hidden;
				};
			
			auto GetTaskBreakpointTooltipFunc = [WeakEditorData = WeakEditorData, TaskId]
				{
#if WITH_METASTORY_TRACE_DEBUGGER
					if (const UMetaStoryEditorData* EditorData = WeakEditorData.Get())
					{
						const bool bHasBreakpointOnEnter = EditorData->HasBreakpoint(TaskId, EMetaStoryBreakpointType::OnEnter);
						const bool bHasBreakpointOnExit = EditorData->HasBreakpoint(TaskId, EMetaStoryBreakpointType::OnExit);
						if (bHasBreakpointOnEnter && bHasBreakpointOnExit)
						{
							return LOCTEXT("MetaStoryTaskBreakpointOnEnterAndOnExitTooltip","Break when entering or exiting task");
						}

						if (bHasBreakpointOnEnter)
						{
							return LOCTEXT("MetaStoryTaskBreakpointOnEnterTooltip","Break when entering task");
						}

						if (bHasBreakpointOnExit)
						{
							return LOCTEXT("MetaStoryTaskBreakpointOnExitTooltip","Break when exiting task");
						}
					}
#endif // WITH_METASTORY_TRACE_DEBUGGER
					return FText::GetEmpty();
				};

			TasksBox->AddSlot()
				.Padding(FMargin(0, 0, 6, 0))
				[
					SNew(SMetaStoryContextMenuButton, MetaStoryViewModel.ToSharedRef(), WeakState, TaskId)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ContentPadding(FMargin(0, 0))
					[
						SNew(SBorder)
						.VAlign(VAlign_Center)
						.BorderImage(FAppStyle::GetNoBrush())
						.Padding(0)
						.IsEnabled_Lambda(IsTaskEnabledFunc)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SBox)
								.HeightOverride(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewNodeRowHeight())
								.Padding(FMargin(0.f, 0.f))
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Left)
									.FillContentWidth(0.f, 0.f)
									[
										SNew(SBox)
										.Padding(FMargin(0.f, 0.f, 2.f, 0.f))
										.Visibility(this, &SMetaStoryViewRow::GetTaskIconVisibility, TaskId)
										[
											SNew(SImage)
											.Image(this, &SMetaStoryViewRow::GetTaskIcon, TaskId)
											.ColorAndOpacity(this, &SMetaStoryViewRow::GetTaskIconColor, TaskId)
										]
									]

									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Left)
									.FillContentWidth(0.f, 1.f)
									[
										SNew(SRichTextBlock)
										.Text(this, &SMetaStoryViewRow::GetTaskDesc, TaskId, EMetaStoryNodeFormatting::RichText)
										.ToolTipText(this, &SMetaStoryViewRow::GetTaskDesc, TaskId, EMetaStoryNodeFormatting::Text)
										.TextStyle(&FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title"))
										.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
										.Clipping(EWidgetClipping::OnDemand)
										+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title")))
										+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title.Bold")))
										+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title.Subdued")))
									]
								]
							]
							+ SOverlay::Slot()
							[
								// Task Breakpoint box
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Top)
								.HAlign(HAlign_Left)
								.AutoWidth()
								[
									SNew(SBox)
									.Padding(FMargin(-2.0f, -2.0f, 0.0f, 0.0f))
									[
										SNew(SImage)
										.DesiredSizeOverride(FVector2D(10.f, 10.f))
										.Image(FMetaStoryEditorStyle::Get().GetBrush(TEXT("MetaStoryEditor.Debugger.Breakpoint.EnabledAndValid")))
										.Visibility_Lambda(IsTaskBreakpointEnabledFunc)
										.ToolTipText_Lambda(GetTaskBreakpointTooltipFunc)
									]
								]
							]
						]
					]
				];
		}
	}

	return TasksBox;
}

TSharedRef<SWidget> SMetaStoryViewRow::MakeConditionsWidget(const TSharedPtr<SScrollBox>& ViewBox)
{
	const UMetaStoryEditorData* EditorData = WeakEditorData.Get();
	const UMetaStoryState* State = WeakState.Get();
	if (!EditorData || !State)
	{
		return SNullWidget::NullWidget;
	}

	if (!State->bHasRequiredEventToEnter && State->EnterConditions.IsEmpty())
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	if (State->bHasRequiredEventToEnter)
	{
		auto IsConditionEnabledFunc = [WeakState = WeakState]
			{
				const UMetaStoryState* State = WeakState.Get();
				return State && State->bEnabled;
			};

		const FName PayloadStructName = State->RequiredEventToEnter.PayloadStruct ? State->RequiredEventToEnter.PayloadStruct->GetFName() : FName();
		const FText Description = FText::Format(LOCTEXT("Condition", "<b>Tag(</>{0}<b>) Payload(</>{1}<b>)</>"), FText::FromName(State->RequiredEventToEnter.Tag.GetTagName()), FText::FromName(PayloadStructName));

		VerticalBox->AddSlot()
		[
			SNew(SBorder)
			.VAlign(VAlign_Center)
			.BorderImage(FAppStyle::GetNoBrush())
			.Padding(FMargin(4.0f, 2.0f, 4.0f, 0.0f))
			.IsEnabled_Lambda(IsConditionEnabledFunc)
			.Padding(0)
			[
				SNew(SBox)
				.HeightOverride(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewNodeRowHeight())
				[
					SNew(SHorizontalBox)
					// Icon
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.Padding(FMargin(4.0f, 0.0f, 4.0f, 0.0f))
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FMetaStoryEditorStyle::Get().GetBrush(FName("MetaStoryEditor.Conditions")))
					]
					// Desc
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						SNew(SRichTextBlock)
						.Text(Description)
						.TextStyle(&FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title"))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						.Clipping(EWidgetClipping::OnDemand)
						+ SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title")))
						+ SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title.Bold")))
					]
				]
			]
		];
	}

	if (!State->EnterConditions.IsEmpty())
	{
		TSharedRef<SWrapBox> ConditionsBox = SNew(SWrapBox)
			.PreferredSize_Lambda([WeakOwnerViewBox = ViewBox.ToWeakPtr()]()
				{
					// Captured as weak ptr so we don't prevent our parent widget from being destroyed (circular pointer reference).
					if (const TSharedPtr<SScrollBox> OwnerViewBox = WeakOwnerViewBox.Pin())
					{
						return FMath::Max(300, OwnerViewBox->GetTickSpaceGeometry().GetLocalSize().X - 200);
					}
					return 0.f;
				});

		const int32 NumConditions = State->EnterConditions.Num();
		for (int32 ConditionIndex = 0; ConditionIndex < NumConditions; ConditionIndex++)
		{
			const FMetaStoryEditorNode& ConditionNode = State->EnterConditions[ConditionIndex];
			if (const FMetaStoryConditionBase* Condition = ConditionNode.Node.GetPtr<FMetaStoryConditionBase>())
			{
				const FGuid ConditionId = ConditionNode.ID;

				auto IsConditionEnabledFunc = [WeakState = WeakState]
				{
					const UMetaStoryState* State = WeakState.Get();
					return State && State->bEnabled;
				};

				auto IsForcedConditionVisibleFunc = [WeakState = WeakState, ConditionIndex]()
					{
						const UMetaStoryState* State = WeakState.Get();
						if (State != nullptr && State->EnterConditions.IsValidIndex(ConditionIndex))
						{
							if (const FMetaStoryConditionBase* Condition = State->EnterConditions[ConditionIndex].Node.GetPtr<FMetaStoryConditionBase>())
							{
								return Condition->EvaluationMode != EMetaStoryConditionEvaluationMode::Evaluated ? EVisibility::Visible : EVisibility::Hidden;
							}
						}
						return EVisibility::Hidden;
					};

				auto GetForcedConditionTooltipFunc = [WeakState = WeakState, ConditionIndex]()
					{
						const UMetaStoryState* State = WeakState.Get();
						if (State != nullptr && State->EnterConditions.IsValidIndex(ConditionIndex))
						{
							if (const FMetaStoryConditionBase* Condition = State->EnterConditions[ConditionIndex].Node.GetPtr<FMetaStoryConditionBase>())
							{
								if (Condition->EvaluationMode == EMetaStoryConditionEvaluationMode::ForcedTrue)
								{
									return LOCTEXT("ForcedTrueConditionTooltip", "This condition is not evaluated and result forced to 'true'.");
								}
								if (Condition->EvaluationMode == EMetaStoryConditionEvaluationMode::ForcedFalse)
								{
									return LOCTEXT("ForcedFalseConditionTooltip", "This condition is not evaluated and result forced to 'false'.");
								}
							}
						}
						return FText::GetEmpty();
					};

				auto GetForcedConditionImageFunc = [WeakState = WeakState, ConditionIndex]() -> const FSlateBrush*
					{
						const UMetaStoryState* State = WeakState.Get();
						if (State != nullptr && State->EnterConditions.IsValidIndex(ConditionIndex))
						{
							if (const FMetaStoryConditionBase* Condition = State->EnterConditions[ConditionIndex].Node.GetPtr<FMetaStoryConditionBase>())
							{
								if (Condition->EvaluationMode == EMetaStoryConditionEvaluationMode::ForcedTrue)
								{
									return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Debugger.Condition.Passed");
								}
								if (Condition->EvaluationMode == EMetaStoryConditionEvaluationMode::ForcedFalse)
								{
									return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Debugger.Condition.Failed");
								}
							}
						}
						return nullptr;
					};

				ConditionsBox->AddSlot()
					[
						SNew(SBorder)
						.VAlign(VAlign_Center)
						.BorderImage(FAppStyle::GetNoBrush())
						.IsEnabled_Lambda(IsConditionEnabledFunc)
						.Padding(0)
						[
							SNew(SBox)
							.HeightOverride(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewNodeRowHeight())
							[
								SNew(SHorizontalBox)

								// Operand
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SBox)
									.Padding(FMargin(4, 2, 4, 0))
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Node.Operand")
										.Text(this, &SMetaStoryViewRow::GetOperandText, ConditionIndex)
									]
								]
								// Open parens
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SBox)
									.Padding(FMargin(FMargin(0.0f, 1.0f, 0.0f, 0.0f)))
									[
										SNew(STextBlock)
										.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Task.Title")
										.Text(this, &SMetaStoryViewRow::GetOpenParens, ConditionIndex)
									]
								]
								// Open parens
								+ SHorizontalBox::Slot() 
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[

										SNew(SMetaStoryContextMenuButton, MetaStoryViewModel.ToSharedRef(), WeakState, ConditionId)
										.ButtonStyle(FAppStyle::Get(), "SimpleButton")
										.ContentPadding(FMargin(2.f, 0.f))
										[
											SNew(SHorizontalBox)
										
											// Icon
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.HAlign(HAlign_Left)
											.AutoWidth()
											[
												SNew(SBox)
												.Padding(FMargin(0.f, 0.f, 2.f, 0.f))
												.Visibility(this, &SMetaStoryViewRow::GetConditionIconVisibility, ConditionId)
												[
													SNew(SImage)
													.Image(this, &SMetaStoryViewRow::GetConditionIcon, ConditionId)
													.ColorAndOpacity(this, &SMetaStoryViewRow::GetConditionIconColor, ConditionId)
												]
											]
											// Desc
											+ SHorizontalBox::Slot()
											.VAlign(VAlign_Center)
											.HAlign(HAlign_Left)
											.AutoWidth()
											[
												SNew(SRichTextBlock)
												.Text(this, &SMetaStoryViewRow::GetConditionDesc, ConditionId, EMetaStoryNodeFormatting::RichText)
												.ToolTipText(this, &SMetaStoryViewRow::GetConditionDesc, ConditionId, EMetaStoryNodeFormatting::Text)
												.TextStyle(&FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title"))
												.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
												.Clipping(EWidgetClipping::OnDemand)
												+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title")))
												+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title.Bold")))
												+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("MetaStory.Task.Title.Subdued")))
											]
										]
									]

									+ SOverlay::Slot()
									[
										// Condition override box
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.VAlign(VAlign_Top)
										.HAlign(HAlign_Left)
										.AutoWidth()
										[
											SNew(SBox)
											.Padding(FMargin(-2.0f, -2.0f, 0.0f, 0.0f))
											[
												SNew(SImage)
												.DesiredSizeOverride(FVector2D(16.f, 16.f))
												.Image_Lambda(GetForcedConditionImageFunc)
												.Visibility_Lambda(IsForcedConditionVisibleFunc)
												.ToolTipText_Lambda(GetForcedConditionTooltipFunc)
											]
										]
									]
								]
							
								// Close parens
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(SBox)
									.Padding(FMargin(0.0f, 1.0f, 0.0f, 0.0f))
									[
										SNew(STextBlock)
										.TextStyle(FMetaStoryEditorStyle::Get(), "MetaStory.Task.Title")
										.Text(this, &SMetaStoryViewRow::GetCloseParens, ConditionIndex)
									]
								]
							]
						]
					];
			}
		}
		VerticalBox->AddSlot()
		[
			ConditionsBox
		];
	}

	return VerticalBox;
}

void SMetaStoryViewRow::MakeTransitionsWidget()
{
	SHorizontalBox* TransitionsContainerPtr = TransitionsContainer.Get();
	if (TransitionsContainerPtr == nullptr)
	{
		return;
	}

	TransitionsContainerPtr->ClearChildren();

	TransitionsContainerPtr->AddSlot()
	.VAlign(VAlign_Top)
	.AutoWidth()
	[
		SNew(SBox)
		.HeightOverride(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewStateRowHeight())
		.Visibility(this, &SMetaStoryViewRow::GetTransitionDashVisibility)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Dash"))
			.ColorAndOpacity(UE::MetaStory::Editor::IconTint)
		]
	];

	// On State Completed
	//We don't show any additional signs for On Completed transitions, just the dash
	constexpr FSlateBrush* OnCompletedSlateIcon = nullptr;
	TransitionsContainerPtr->AddSlot()
	.VAlign(VAlign_Top)
	.AutoWidth()
	[
		MakeTransitionWidget(EMetaStoryTransitionTrigger::OnStateCompleted, OnCompletedSlateIcon)
	];

	// On State Succeeded
	TransitionsContainerPtr->AddSlot()
	.VAlign(VAlign_Top)
	.AutoWidth()
	[
		MakeTransitionWidget(EMetaStoryTransitionTrigger::OnStateSucceeded, FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Succeeded"))
	];

	// On State Failed
	TransitionsContainerPtr->AddSlot()
	.VAlign(VAlign_Top)
	.AutoWidth()
	[
		MakeTransitionWidget(EMetaStoryTransitionTrigger::OnStateFailed, FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Failed"))
	];

	// On Tick, Event, Delegate
	TransitionsContainerPtr->AddSlot()
	.VAlign(VAlign_Top)
	.AutoWidth()
	[
		MakeTransitionWidget(EMetaStoryTransitionTrigger::OnTick | EMetaStoryTransitionTrigger::OnEvent | EMetaStoryTransitionTrigger::OnDelegate, FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Condition"))
	];
}

TSharedRef<SWidget> SMetaStoryViewRow::MakeTransitionWidget(const EMetaStoryTransitionTrigger Trigger, const FSlateBrush* Icon)
{
		FTransitionDescFilterOptions FilterOptions;
		FilterOptions.bUseMask = EnumHasAnyFlags(Trigger, EMetaStoryTransitionTrigger::OnTick | EMetaStoryTransitionTrigger::OnEvent | EMetaStoryTransitionTrigger::OnDelegate);

		return SNew(SBox)
		.HeightOverride(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewStateRowHeight())
		.Visibility(this, &SMetaStoryViewRow::GetTransitionsVisibility, Trigger)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(FMargin(0.f, 0.f, 0.f, 0.f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(Icon)
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(this, &SMetaStoryViewRow::GetTransitionsIcon, Trigger)
					.ColorAndOpacity(UE::MetaStory::Editor::IconTint)
				]
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(FMargin(4.f, 0.f, 12.f, 0.f))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					MakeTransitionWidgetInternal(Trigger, FilterOptions)
				]
				+ SOverlay::Slot()
				[
					// Breakpoint box
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						SNew(SBox)
						.Padding(FMargin(-4.f, -4.f, 0.f, 0.f))
						[
							SNew(SImage)
							.DesiredSizeOverride(FVector2D(10.f, 10.f))
							.Image(FMetaStoryEditorStyle::Get().GetBrush(TEXT("MetaStoryEditor.Debugger.Breakpoint.EnabledAndValid")))
							.Visibility(this, &SMetaStoryViewRow::GetTransitionsBreakpointVisibility, Trigger)
							.ToolTipText_Lambda([this, Trigger, InFilterOptions = FilterOptions]
							{
								FTransitionDescFilterOptions FilterOptions = InFilterOptions;
								FilterOptions.WithBreakpoint = ETransitionDescRequirement::RequiredTrue;

								return FText::Format(LOCTEXT("TransitionBreakpointTooltip", "Break when executing transition: {0}"),
													 GetTransitionsDesc(Trigger, FilterOptions));
							})
						]
					]
				]
			]
		];
};

TSharedRef<SWidget> SMetaStoryViewRow::MakeTransitionWidgetInternal(const EMetaStoryTransitionTrigger Trigger, const FTransitionDescFilterOptions FilterOptions)
{
	const UMetaStoryEditorData* TreeEditorData = WeakEditorData.Get();
	const UMetaStoryState* State = WeakState.Get();

	if (!TreeEditorData || !State)
	{
		return SNullWidget::NullWidget;
	}
	
	struct FItem
	{
		FItem() = default;
		FItem(const FMetaStoryStateLink& InLink, const FGuid InNodeID)
			: Link(InLink)
			, NodeID(InNodeID)
		{
		}
		FItem(const FText& InDesc, const FText& InTooltip)
			: Desc(InDesc)
			, Tooltip(InTooltip)
		{
		}
		
		FText Desc;
		FText Tooltip;
		FMetaStoryStateLink Link;
		FGuid NodeID;
	};

	TArray<FItem> DescItems;

	for (const FMetaStoryTransition& Transition : State->Transitions)
	{
		// Apply filter for enabled/disabled transitions
		if ((FilterOptions.Enabled == ETransitionDescRequirement::RequiredTrue && Transition.bTransitionEnabled == false)
			|| (FilterOptions.Enabled == ETransitionDescRequirement::RequiredFalse && Transition.bTransitionEnabled))
		{
			continue;
		}

#if WITH_METASTORY_TRACE_DEBUGGER
		// Apply filter for transitions with/without breakpoint
		const bool bHasBreakpoint = TreeEditorData != nullptr && TreeEditorData->HasBreakpoint(Transition.ID, EMetaStoryBreakpointType::OnTransition);
		if ((FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredTrue && bHasBreakpoint == false)
			|| (FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredFalse && bHasBreakpoint))
		{
			continue;
		}
#endif // WITH_METASTORY_TRACE_DEBUGGER

		const bool bMatch = FilterOptions.bUseMask ? EnumHasAnyFlags(Transition.Trigger, Trigger) : Transition.Trigger == Trigger;
		if (bMatch)
		{
			DescItems.Emplace(Transition.State, Transition.ID);
		}
	}

	// Find states from transition tasks
	if (EnumHasAnyFlags(Trigger, EMetaStoryTransitionTrigger::OnTick | EMetaStoryTransitionTrigger::OnEvent | EMetaStoryTransitionTrigger::OnDelegate))
	{
		auto AddLinksFromStruct = [&DescItems, TreeEditorData](FMetaStoryDataView Struct, const FGuid NodeID)
		{
			if (!Struct.IsValid())
			{
				return;
			}
			for (TPropertyValueIterator<FStructProperty> It(Struct.GetStruct(), Struct.GetMemory()); It; ++It)
			{
				const UScriptStruct* StructType = It.Key()->Struct;
				if (StructType == TBaseStructure<FMetaStoryStateLink>::Get())
				{
					const FMetaStoryStateLink& Link = *static_cast<const FMetaStoryStateLink*>(It.Value());
					if (Link.LinkType != EMetaStoryTransitionType::None)
					{
						DescItems.Emplace(Link, NodeID);
					}
				}
			}
		};
		
		for (const FMetaStoryEditorNode& Task : State->Tasks)
		{
			AddLinksFromStruct(FMetaStoryDataView(Task.Node.GetScriptStruct(), const_cast<uint8*>(Task.Node.GetMemory())), Task.ID);
			AddLinksFromStruct(Task.GetInstance(), Task.ID);
		}

		AddLinksFromStruct(FMetaStoryDataView(State->SingleTask.Node.GetScriptStruct(), const_cast<uint8*>(State->SingleTask.Node.GetMemory())), State->SingleTask.ID);
		AddLinksFromStruct(State->SingleTask.GetInstance(), State->SingleTask.ID);
	}

	if (IsLeafState()
		&& DescItems.Num() == 0
		&& EnumHasAnyFlags(Trigger, EMetaStoryTransitionTrigger::OnStateCompleted))
	{
		if (HasParentTransitionForTrigger(*State, Trigger))
		{
			DescItems.Emplace(
				LOCTEXT("TransitionActionHandleInParentRich", "<i>Parent</>"),
				LOCTEXT("TransitionActionHandleInParent", "Handle transition in parent State")
				);
		}
		else
		{
			DescItems.Emplace(
				LOCTEXT("TransitionActionRootRich", "<i>Root</>"),
				LOCTEXT("TransitionActionRoot", "Transition to Root State.")
				);
		}
	}

	TSharedRef<SHorizontalBox> TransitionContainer = SNew(SHorizontalBox);

	auto IsTransitionEnabledFunc = [WeakState = WeakState]
	{
		const UMetaStoryState* State = WeakState.Get();
		return State && State->bEnabled;
	};

	for (int32 Index = 0; Index < DescItems.Num(); Index++)
	{
		const FItem& Item = DescItems[Index];

		if (Index > 0)
		{
			TransitionContainer->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT(", ")))
				.TextStyle(&FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Transition.Subdued"))
			];
		}

		constexpr bool bIsTransition = true;
		TransitionContainer->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SMetaStoryContextMenuButton, MetaStoryViewModel.ToSharedRef(), WeakState, Item.NodeID, bIsTransition)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ContentPadding(FMargin(0, 0))
			[
				SNew(SBorder)
				.VAlign(VAlign_Center)
				.BorderImage(FAppStyle::GetNoBrush())
				.Padding(0)
				.IsEnabled_Lambda(IsTransitionEnabledFunc)
				[
					SNew(SRichTextBlock)
					.Text_Lambda([WeakEditorData = WeakEditorData, Item]()
					{
						if (!Item.Desc.IsEmpty())
						{
							return Item.Desc;
						}
						return UE::MetaStory::Editor::GetStateLinkDesc(WeakEditorData.Get(), Item.Link, EMetaStoryNodeFormatting::RichText);
					})
					.ToolTipText_Lambda([this, Item]()
					{
						if (!Item.Tooltip.IsEmpty())
						{
							return Item.Tooltip;
						}
						return GetLinkTooltip(Item.Link, Item.NodeID);
					})
					.TextStyle(&FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Transition.Normal"))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Transition.Normal")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Transition.Bold")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("i"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Transition.Italic")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FMetaStoryEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Transition.Subdued")))
				]
			]
		];
	}
	
	return TransitionContainer;
}

void SMetaStoryViewRow::MakeFlagsWidget()
{
	FlagsContainer->SetPadding(FMargin(0.0f));
	FlagsContainer->SetContent(SNullWidget::NullWidget);

	const UMetaStory* MetaStory = MetaStoryViewModel ? MetaStoryViewModel->GetMetaStory() : nullptr;
	const UMetaStoryState* State = WeakState.Get();
	const bool bDisplayFlags = EnumHasAllFlags(GetDefault<UMetaStoryEditorUserSettings>()->GetStatesViewDisplayNodeType(), EMetaStoryEditorUserSettingsNodeType::Flag);
	static constexpr FLinearColor IconTint = FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);

	if (bDisplayFlags && State && MetaStory)
	{
		if (const FMetaStoryCompactState* RuntimeState = MetaStory->GetStateFromHandle(MetaStory->GetStateHandleFromId(State->ID)))
		{
			const bool bHasEvents = true;
			const bool bHasBroadcastedDelegates = true;
			if (RuntimeState->DoesRequestTickTasks(bHasEvents) || RuntimeState->bHasCustomTickRate || RuntimeState->ShouldTickTransitions(bHasEvents, bHasBroadcastedDelegates))
			{
				TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
				if (RuntimeState->bHasCustomTickRate)
				{
					Box->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Flags.Tick"))
						.ColorAndOpacity(IconTint)
						.ToolTipText(LOCTEXT("StateCustomTick", "The state has a custom tick rate."))
					];
				}
				else if (RuntimeState->bHasTickTasks)
				{
					Box->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Flags.Tick"))
						.ColorAndOpacity(IconTint)
						.ToolTipText(LOCTEXT("StateNodeTick", "The state contains at least one task that ticks at runtime."))
					];
				}
				else if (RuntimeState->bHasTickTasksOnlyOnEvents)
				{
					Box->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Flags.TickOnEvent"))
						.ColorAndOpacity(IconTint)
						.ToolTipText(LOCTEXT("StateNodeTickEvent", "The state contains at least one task that ticks at runtime when there's an event."))
					];
				}
				
				if (RuntimeState->bHasTransitionTasks)
				{
					Box->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transitions"))
						.ColorAndOpacity(IconTint)
						.ToolTipText(LOCTEXT("StateNodeTickTransition", "The state contains at least one task that ticks at runtime when evaluating transitions."))
					];
				}

				FlagsContainer->SetPadding(FMargin(4.0f));
				FlagsContainer->SetContent(Box);
			}
		}
	}
}

void SMetaStoryViewRow::RequestRename() const
{
	if (NameTextBlock)
	{
		NameTextBlock->EnterEditingMode();
	}
}

FSlateColor SMetaStoryViewRow::GetTitleColor(const float Alpha, const float Lighten) const
{
	const UMetaStoryState* State = WeakState.Get();
	const UMetaStoryEditorData* EditorData = WeakEditorData.Get();

	FLinearColor Color(FColor(31, 151, 167));
	
	if (State != nullptr && EditorData != nullptr)
	{
		if (const FMetaStoryEditorColor* FoundColor = EditorData->FindColor(State->ColorRef))
		{
			if (IsRootState() || State->Type == EMetaStoryStateType::Subtree)
			{
				Color = UE::MetaStory::Editor::LerpColorSRGB(FoundColor->Color, FColor::Black, 0.25f);
			}
			else
			{
				Color = FoundColor->Color;
			}
		}
	}

	if (Lighten > 0.0f)
	{
		Color = UE::MetaStory::Editor::LerpColorSRGB(Color, FColor::White, Lighten);
	}
	
	return Color.CopyWithNewOpacity(Alpha);
}

FSlateColor SMetaStoryViewRow::GetActiveStateColor() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		if (MetaStoryViewModel && MetaStoryViewModel->IsStateActiveInDebugger(*State))
		{
			return FLinearColor::Yellow;
		}
		if (MetaStoryViewModel && MetaStoryViewModel->IsSelected(State))
		{
			// @todo: change to the common selection color.
			return FLinearColor(FColor(236, 134, 39));
		}
	}

	return FLinearColor::Transparent;
}

FSlateColor SMetaStoryViewRow::GetSubTreeMarkerColor() const
{
	// Show color for subtree.
	if (const UMetaStoryState* State = WeakState.Get())
	{
		if (IsRootState() || State->Type == EMetaStoryStateType::Subtree)
		{
			const FSlateColor TitleColor = GetTitleColor();
			return UE::MetaStory::Editor::LerpColorSRGB(TitleColor.GetSpecifiedColor(), FLinearColor::White, 0.2f);
		}
	}

	return GetTitleColor();
}

EVisibility SMetaStoryViewRow::GetSubTreeVisibility() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		if (IsRootState() || State->Type == EMetaStoryStateType::Subtree)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;	
}

FText SMetaStoryViewRow::GetStateDesc() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		return FText::FromName(State->Name);
	}
	return FText::FromName(FName());
}

FText SMetaStoryViewRow::GetStateIDDesc() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		return FText::FromString(*LexToString(State->ID));
	}
	return FText::FromName(FName());
}

EVisibility SMetaStoryViewRow::GetConditionVisibility() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		return State->EnterConditions.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

EVisibility SMetaStoryViewRow::GetStateBreakpointVisibility() const
{
#if WITH_METASTORY_TRACE_DEBUGGER
	const UMetaStoryState* State = WeakState.Get();
	const UMetaStoryEditorData* EditorData = WeakEditorData.Get();
	if (State != nullptr && EditorData != nullptr)
	{
		return (EditorData != nullptr && EditorData->HasAnyBreakpoint(State->ID)) ? EVisibility::Visible : EVisibility::Hidden;
	}
#endif // WITH_METASTORY_TRACE_DEBUGGER
	return EVisibility::Hidden;
}

FText SMetaStoryViewRow::GetStateBreakpointTooltipText() const
{
#if WITH_METASTORY_TRACE_DEBUGGER
	const UMetaStoryState* State = WeakState.Get();
	const UMetaStoryEditorData* EditorData = WeakEditorData.Get();
	if (State != nullptr && EditorData != nullptr)
	{
		const bool bHasBreakpointOnEnter = EditorData->HasBreakpoint(State->ID, EMetaStoryBreakpointType::OnEnter);
		const bool bHasBreakpointOnExit = EditorData->HasBreakpoint(State->ID, EMetaStoryBreakpointType::OnExit);

		if (bHasBreakpointOnEnter && bHasBreakpointOnExit)
		{
			return LOCTEXT("MetaStoryStateBreakpointOnEnterAndOnExitTooltip","Break when entering or exiting state");
		}

		if (bHasBreakpointOnEnter)
		{
			return LOCTEXT("MetaStoryStateBreakpointOnEnterTooltip","Break when entering state");
		}

		if (bHasBreakpointOnExit)
		{
			return LOCTEXT("MetaStoryStateBreakpointOnExitTooltip","Break when exiting state");
		}
	}
#endif // WITH_METASTORY_TRACE_DEBUGGER
	return FText::GetEmpty();
}

const FSlateBrush* SMetaStoryViewRow::GetSelectorIcon() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		return FMetaStoryEditorStyle::GetBrushForSelectionBehaviorType(State->SelectionBehavior, !State->Children.IsEmpty(), State->Type);		
	}

	return nullptr;
}

FText SMetaStoryViewRow::GetSelectorTooltip() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		const UEnum* Enum = StaticEnum<EMetaStoryStateSelectionBehavior>();
		check(Enum);
		const int32 Index = Enum->GetIndexByValue((int64)State->SelectionBehavior);
		
		switch (State->SelectionBehavior)
		{
			case EMetaStoryStateSelectionBehavior::None:
			case EMetaStoryStateSelectionBehavior::TryEnterState:
			case EMetaStoryStateSelectionBehavior::TryFollowTransitions:
				return Enum->GetToolTipTextByIndex(Index);
			case EMetaStoryStateSelectionBehavior::TrySelectChildrenInOrder:
			case EMetaStoryStateSelectionBehavior::TrySelectChildrenAtRandom:
			case EMetaStoryStateSelectionBehavior::TrySelectChildrenWithHighestUtility:
			case EMetaStoryStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility:
				if (State->Children.IsEmpty()
					|| State->Type == EMetaStoryStateType::Linked
					|| State->Type == EMetaStoryStateType::LinkedAsset)
				{
					const int32 EnterStateIndex = Enum->GetIndexByValue((int64)EMetaStoryStateSelectionBehavior::TryEnterState);
					return FText::Format(LOCTEXT("ConvertedToEnterState", "{0}\nAutomatically converted from '{1}' because the State has no child States."),
						Enum->GetToolTipTextByIndex(EnterStateIndex), UEnum::GetDisplayValueAsText(State->SelectionBehavior));
				}
				else
				{
					return Enum->GetToolTipTextByIndex(Index);
				}
			default:
				check(false);
		}
	}

	return FText::GetEmpty();
}

FText SMetaStoryViewRow::GetStateTypeTooltip() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		const UEnum* Enum = StaticEnum<EMetaStoryStateType>();
		check(Enum);
		const int32 Index = Enum->GetIndexByValue((int64)State->Type);
		return Enum->GetToolTipTextByIndex(Index);
	}

	return FText::GetEmpty();
}

const FMetaStoryEditorNode* SMetaStoryViewRow::GetTaskNodeByID(FGuid TaskID) const
{
	const UMetaStoryState* State = WeakState.Get();
	const UMetaStoryEditorData* EditorData = WeakEditorData.Get();
	if (EditorData != nullptr
		&& State != nullptr)
	{
		return State->Tasks.FindByPredicate([&TaskID](const FMetaStoryEditorNode& Node)
		{
			return Node.ID == TaskID;
		});
	}
	return nullptr;
}

EVisibility SMetaStoryViewRow::GetTaskIconVisibility(FGuid TaskID) const
{
	bool bHasIcon = false;
	if (const FMetaStoryEditorNode* TaskNode = GetTaskNodeByID(TaskID))
	{
		if (const FMetaStoryNodeBase* BaseNode = TaskNode->Node.GetPtr<const FMetaStoryNodeBase>())
		{
			bHasIcon = !BaseNode->GetIconName().IsNone();
		}
	}
	return bHasIcon ? EVisibility::Visible : EVisibility::Collapsed;  	
}

const FSlateBrush* SMetaStoryViewRow::GetTaskIcon(FGuid TaskID) const
{
	if (const FMetaStoryEditorNode* TaskNode = GetTaskNodeByID(TaskID))
	{
		if (const FMetaStoryNodeBase* BaseNode = TaskNode->Node.GetPtr<const FMetaStoryNodeBase>())
		{
			return UE::MetaStoryEditor::EditorNodeUtils::ParseIcon(BaseNode->GetIconName()).GetIcon();
		}
	}
	return nullptr;	
}

FSlateColor SMetaStoryViewRow::GetTaskIconColor(FGuid TaskID) const
{
	if (const FMetaStoryEditorNode* TaskNode = GetTaskNodeByID(TaskID))
	{
		if (const FMetaStoryNodeBase* BaseNode = TaskNode->Node.GetPtr<const FMetaStoryNodeBase>())
		{
			return FLinearColor(BaseNode->GetIconColor());
		}
	}
	return FSlateColor::UseForeground();
}

FText SMetaStoryViewRow::GetTaskDesc(FGuid TaskID, EMetaStoryNodeFormatting Formatting) const
{
	FText TaskName;
	if (const UMetaStoryEditorData* EditorData = WeakEditorData.Get())
	{
		if (const FMetaStoryEditorNode* TaskNode = GetTaskNodeByID(TaskID))
		{
			if (UE::MetaStory::Editor::GbDisplayItemIds)
			{
				TaskName = FText::Format(LOCTEXT("NodeNameWithID", "{0} ({1})"), EditorData->GetNodeDescription(*TaskNode, Formatting), FText::AsCultureInvariant(*LexToString(TaskID)));
			}
			else
			{
				TaskName = EditorData->GetNodeDescription(*TaskNode, Formatting);
			}
		}
	}
	return TaskName;
}

const FMetaStoryEditorNode* SMetaStoryViewRow::GetConditionNodeByID(FGuid ConditionID) const
{
	const UMetaStoryState* State = WeakState.Get();
	const UMetaStoryEditorData* EditorData = WeakEditorData.Get();
	if (EditorData != nullptr
		&& State != nullptr)
	{
		return State->EnterConditions.FindByPredicate([&ConditionID](const FMetaStoryEditorNode& Node)
		{
			return Node.ID == ConditionID;
		});
	}
	return nullptr;
}

EVisibility SMetaStoryViewRow::GetConditionIconVisibility(FGuid ConditionID) const
{
	bool bHasIcon = false;
	if (const FMetaStoryEditorNode* Node = GetConditionNodeByID(ConditionID))
	{
		if (const FMetaStoryNodeBase* BaseNode = Node->Node.GetPtr<const FMetaStoryNodeBase>())
		{
			bHasIcon = !BaseNode->GetIconName().IsNone();
		}
	}
	return bHasIcon ? EVisibility::Visible : EVisibility::Collapsed;  	
}

const FSlateBrush* SMetaStoryViewRow::GetConditionIcon(FGuid ConditionID) const
{
	if (const FMetaStoryEditorNode* Node = GetConditionNodeByID(ConditionID))
	{
		if (const FMetaStoryNodeBase* BaseNode = Node->Node.GetPtr<const FMetaStoryNodeBase>())
		{
			return UE::MetaStoryEditor::EditorNodeUtils::ParseIcon(BaseNode->GetIconName()).GetIcon();
		}
	}
	return nullptr;	
}

FSlateColor SMetaStoryViewRow::GetConditionIconColor(FGuid ConditionID) const
{
	if (const FMetaStoryEditorNode* Node = GetConditionNodeByID(ConditionID))
	{
		if (const FMetaStoryNodeBase* BaseNode = Node->Node.GetPtr<const FMetaStoryNodeBase>())
		{
			return FLinearColor(BaseNode->GetIconColor());
		}
	}
	return FSlateColor::UseForeground();
}

FText SMetaStoryViewRow::GetConditionDesc(FGuid ConditionID, EMetaStoryNodeFormatting Formatting) const
{
	FText Description;
	if (const UMetaStoryEditorData* EditorData = WeakEditorData.Get())
	{
		if (const FMetaStoryEditorNode* Node = GetConditionNodeByID(ConditionID))
		{
			if (UE::MetaStory::Editor::GbDisplayItemIds)
			{
				Description = FText::Format(LOCTEXT("NodeNameWithID", "{0} ({1})"), EditorData->GetNodeDescription(*Node, Formatting), FText::AsCultureInvariant(*LexToString(ConditionID)));
			}
			else
			{
				Description = EditorData->GetNodeDescription(*Node, Formatting);
			}
		}
	}
	return Description;
}

FText SMetaStoryViewRow::GetOperandText(const int32 ConditionIndex) const
{
	const UMetaStoryState* State = WeakState.Get();
	if (!State
		|| !State->EnterConditions.IsValidIndex(ConditionIndex))
	{
		return FText::GetEmpty();
	}

	// First item does not relate to anything existing, it could be empty. 
	// return IF to indicate that we're building condition and IS for consideration.
	if (ConditionIndex == 0)
	{
		return LOCTEXT("IfOperand", "IF");
	}

	const EMetaStoryExpressionOperand Operand = State->EnterConditions[ConditionIndex].ExpressionOperand;

	if (Operand == EMetaStoryExpressionOperand::And)
	{
		return LOCTEXT("AndOperand", "AND");
	}
	else if (Operand == EMetaStoryExpressionOperand::Or)
	{
		return LOCTEXT("OrOperand", "OR");
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled operand %s"), *UEnum::GetValueAsString(Operand));
	}

	return FText::GetEmpty();
}

FText SMetaStoryViewRow::GetOpenParens(const int32 ConditionIndex) const
{
	const UMetaStoryState* State = WeakState.Get();
	if (!State
		|| !State->EnterConditions.IsValidIndex(ConditionIndex))
	{
		return FText::GetEmpty();
	}

	const int32 NumConditions = State->EnterConditions.Num();
	const int32 CurrIndent = ConditionIndex == 0 ? 0 : (State->EnterConditions[ConditionIndex].ExpressionIndent + 1);
	const int32 NextIndent = (ConditionIndex + 1) >= NumConditions ? 0 : (State->EnterConditions[ConditionIndex + 1].ExpressionIndent + 1);
	const int32 DeltaIndent = NextIndent - CurrIndent;
	const int32 OpenParens = FMath::Max(0, DeltaIndent);

	static_assert(UE::MetaStory::MaxExpressionIndent == 4);
	switch (OpenParens)
	{
	case 1: return FText::FromString(TEXT("("));
	case 2: return FText::FromString(TEXT("(("));
	case 3: return FText::FromString(TEXT("((("));
	case 4: return FText::FromString(TEXT("(((("));
	case 5: return FText::FromString(TEXT("((((("));
	}
	return FText::GetEmpty();
}

FText SMetaStoryViewRow::GetCloseParens(const int32 ConditionIndex) const
{
	const UMetaStoryState* State = WeakState.Get();
	if (!State
		|| !State->EnterConditions.IsValidIndex(ConditionIndex))
	{
		return FText::GetEmpty();
	}

	const int32 NumConditions = State->EnterConditions.Num();
	const int32 CurrIndent = ConditionIndex == 0 ? 0 : (State->EnterConditions[ConditionIndex].ExpressionIndent + 1);
	const int32 NextIndent = (ConditionIndex + 1) >= NumConditions ? 0 : (State->EnterConditions[ConditionIndex + 1].ExpressionIndent + 1);
	const int32 DeltaIndent = NextIndent - CurrIndent;
	const int32 CloseParens = FMath::Max(0, -DeltaIndent);

	static_assert(UE::MetaStory::MaxExpressionIndent == 4);
	switch (CloseParens)
	{
	case 1: return FText::FromString(TEXT(")"));
	case 2: return FText::FromString(TEXT("))"));
	case 3: return FText::FromString(TEXT(")))"));
	case 4: return FText::FromString(TEXT("))))"));
	case 5: return FText::FromString(TEXT(")))))"));
	}
	return FText::GetEmpty();
}


EVisibility SMetaStoryViewRow::GetLinkedStateVisibility() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		return (State->Type == EMetaStoryStateType::Linked || State->Type == EMetaStoryStateType::LinkedAsset) ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

bool SMetaStoryViewRow::GetStateWarnings(FText* OutText) const
{
	bool bHasWarnings = false;
	
	const UMetaStoryState* State = WeakState.Get();
	if (!State)
	{
		return bHasWarnings;
	}

	// Linked States cannot have children.
	if ((State->Type == EMetaStoryStateType::Linked
		|| State->Type == EMetaStoryStateType::LinkedAsset)
		&& State->Children.Num() > 0)
	{
		if (OutText)
		{
			*OutText = LOCTEXT("LinkedStateChildWarning", "Linked State cannot have child states, because the state selection will enter to the linked state on activation.");
		}
		bHasWarnings = true;
	}

	// Child states should not have any considerations if their parent doesn't use utility
	if (State->Considerations.Num() != 0)
	{
		if (!State->Parent 
			|| (State->Parent->SelectionBehavior != EMetaStoryStateSelectionBehavior::TrySelectChildrenWithHighestUtility
				&& State->Parent->SelectionBehavior != EMetaStoryStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility))
		{
			if (OutText)
			{
				*OutText = LOCTEXT("ChildStateUtilityConsiderationWarning", 
					"State has Utility Considerations but they don't have effect."
					"The Utility Considerations are used only when parent State's Selection Behavior is:"
					"\"Try Select Children with Highest Utility\" or \"Try Select Children At Random Weighted By Utility.");
			}
			bHasWarnings = true;
		}
	}

	return bHasWarnings;
}

FText SMetaStoryViewRow::GetLinkedStateDesc() const
{
	const UMetaStoryState* State = WeakState.Get();
	if (!State)
	{
		return FText::GetEmpty();
	}

	if (State->Type == EMetaStoryStateType::Linked)
	{
		return FText::FromName(State->LinkedSubtree.Name);
	}
	else if (State->Type == EMetaStoryStateType::LinkedAsset)
	{
		return FText::FromString(GetNameSafe(State->LinkedAsset.Get()));
	}
	
	return FText::GetEmpty();
}

EVisibility SMetaStoryViewRow::GetWarningsVisibility() const
{
	return GetStateWarnings(nullptr) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SMetaStoryViewRow::GetWarningsTooltipText() const
{
	FText Warnings = FText::GetEmpty();
	GetStateWarnings(&Warnings);
	return Warnings;
}

bool SMetaStoryViewRow::HasParentTransitionForTrigger(const UMetaStoryState& State, const EMetaStoryTransitionTrigger Trigger) const
{
	EMetaStoryTransitionTrigger CombinedTrigger = EMetaStoryTransitionTrigger::None;
	for (const UMetaStoryState* ParentState = State.Parent; ParentState != nullptr; ParentState = ParentState->Parent)
	{
		for (const FMetaStoryTransition& Transition : ParentState->Transitions)
		{
			CombinedTrigger |= Transition.Trigger;
		}
	}
	return EnumHasAllFlags(CombinedTrigger, Trigger);
}

FText SMetaStoryViewRow::GetLinkTooltip(const FMetaStoryStateLink& Link, const FGuid NodeID) const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		const int32 TaskIndex = State->Tasks.IndexOfByPredicate([&NodeID](const FMetaStoryEditorNode& Node)
		{
			return Node.ID == NodeID;
		});
		if (TaskIndex != INDEX_NONE)
		{
			return FText::Format(LOCTEXT("TaskTransitionDesc", "Task {0} transitions to {1}"),
				FText::FromName(State->Tasks[TaskIndex].GetName()),
				UE::MetaStory::Editor::GetStateLinkDesc(WeakEditorData.Get(), Link, EMetaStoryNodeFormatting::Text, /*bShowStatePath*/true));
		}

		if (State->SingleTask.ID == NodeID)
		{
			return FText::Format(LOCTEXT("TaskTransitionDesc", "Task {0} transitions to {1}"),
				FText::FromName(State->SingleTask.GetName()),
				UE::MetaStory::Editor::GetStateLinkDesc(WeakEditorData.Get(), Link, EMetaStoryNodeFormatting::Text, /*bShowStatePath*/true));
		}

		const int32 TransitionIndex = State->Transitions.IndexOfByPredicate([&NodeID](const FMetaStoryTransition& Transition)
		{
			return Transition.ID == NodeID;
		});
		if (TransitionIndex != INDEX_NONE)
		{
			return UE::MetaStory::Editor::GetTransitionDesc(WeakEditorData.Get(), State->Transitions[TransitionIndex], EMetaStoryNodeFormatting::Text, /*bShowStatePath*/true);
		}
	}
	
	return FText::GetEmpty();
};

bool SMetaStoryViewRow::IsLeafState() const
{
	const UMetaStoryState* State = WeakState.Get();
	return State
		&& State->Children.Num() == 0
		&& !IsRootState()
		&& (State->Type == EMetaStoryStateType::State
			|| State->Type == EMetaStoryStateType::Linked
			|| State->Type == EMetaStoryStateType::LinkedAsset);
}

FText SMetaStoryViewRow::GetTransitionsDesc(const EMetaStoryTransitionTrigger Trigger, const FTransitionDescFilterOptions FilterOptions) const
{
	const UMetaStoryState* State = WeakState.Get();
	const UMetaStoryEditorData* EditorData = WeakEditorData.Get();
	if (!State || !EditorData)
	{
		return FText::GetEmpty();
	}

	TArray<FText> DescItems;

	for (const FMetaStoryTransition& Transition : State->Transitions)
	{
		// Apply filter for enabled/disabled transitions
		if ((FilterOptions.Enabled == ETransitionDescRequirement::RequiredTrue && Transition.bTransitionEnabled == false)
			|| (FilterOptions.Enabled == ETransitionDescRequirement::RequiredFalse && Transition.bTransitionEnabled))
		{
			continue;
		}

#if WITH_METASTORY_TRACE_DEBUGGER
		// Apply filter for transitions with/without breakpoint
		const bool bHasBreakpoint = EditorData->HasBreakpoint(Transition.ID, EMetaStoryBreakpointType::OnTransition);
		if ((FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredTrue && bHasBreakpoint == false)
			|| (FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredFalse && bHasBreakpoint))
		{
			continue;
		}
#endif // WITH_METASTORY_TRACE_DEBUGGER

		const bool bMatch = FilterOptions.bUseMask ? EnumHasAnyFlags(Transition.Trigger, Trigger) : Transition.Trigger == Trigger;
		if (bMatch)
		{
			DescItems.Add(UE::MetaStory::Editor::GetStateLinkDesc(EditorData, Transition.State, EMetaStoryNodeFormatting::RichText));
		}
	}

	// Find states from transition tasks
	if (EnumHasAnyFlags(Trigger, EMetaStoryTransitionTrigger::OnTick | EMetaStoryTransitionTrigger::OnEvent| EMetaStoryTransitionTrigger::OnDelegate))
	{
		auto AddLinksFromStruct = [EditorData, &DescItems](FMetaStoryDataView Struct)
		{
			if (!Struct.IsValid())
			{
				return;
			}
			for (TPropertyValueIterator<FStructProperty> It(Struct.GetStruct(), Struct.GetMemory()); It; ++It)
			{
				const UScriptStruct* StructType = It.Key()->Struct;
				if (StructType == TBaseStructure<FMetaStoryStateLink>::Get())
				{
					const FMetaStoryStateLink& Link = *static_cast<const FMetaStoryStateLink*>(It.Value());
					if (Link.LinkType != EMetaStoryTransitionType::None)
					{
						DescItems.Add(UE::MetaStory::Editor::GetStateLinkDesc(EditorData, Link, EMetaStoryNodeFormatting::RichText));
					}
				}
			}
		};
		
		for (const FMetaStoryEditorNode& Task : State->Tasks)
		{
			AddLinksFromStruct(FMetaStoryDataView(Task.Node.GetScriptStruct(), const_cast<uint8*>(Task.Node.GetMemory())));
			AddLinksFromStruct(Task.GetInstance());
		}

		AddLinksFromStruct(FMetaStoryDataView(State->SingleTask.Node.GetScriptStruct(), const_cast<uint8*>(State->SingleTask.Node.GetMemory())));
		AddLinksFromStruct(State->SingleTask.GetInstance());
	}

	if (IsLeafState()
		&& DescItems.Num() == 0
		&& EnumHasAnyFlags(Trigger, EMetaStoryTransitionTrigger::OnStateCompleted))
	{
		if (HasParentTransitionForTrigger(*State, Trigger))
		{
			DescItems.Add(LOCTEXT("TransitionActionHandleInParentRich", "<i>Parent</>"));
		}
		else
		{
			DescItems.Add(LOCTEXT("TransitionActionRootRich", "<i>Root</>"));
		}
	}
	
	return FText::Join(FText::FromString(TEXT(", ")), DescItems);
}

const FSlateBrush* SMetaStoryViewRow::GetTransitionsIcon(const EMetaStoryTransitionTrigger Trigger) const
{
	const UMetaStoryState* State = WeakState.Get();
	if (!State)
	{
		return nullptr;
	}

	if (EnumHasAnyFlags(Trigger, EMetaStoryTransitionTrigger::OnTick | EMetaStoryTransitionTrigger::OnEvent| EMetaStoryTransitionTrigger::OnDelegate))
	{
		return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Goto");
	}

	enum EIconType
	{
		IconNone = 0,
		IconGoto = 1 << 0,
		IconNext = 1 << 1,
		IconParent = 1 << 2,
	};
	uint8 IconType = IconNone;
	
	for (const FMetaStoryTransition& Transition : State->Transitions)
	{
		// Apply filter for enabled/disabled transitions
/*		if ((FilterOptions.Enabled == ETransitionDescRequirement::RequiredTrue && Transition.bTransitionEnabled == false)
			|| (FilterOptions.Enabled == ETransitionDescRequirement::RequiredFalse && Transition.bTransitionEnabled))
		{
			continue;
		}*/

/*		
#if WITH_METASTORY_TRACE_DEBUGGER
		// Apply filter for transitions with/without breakpoint
		const bool bHasBreakpoint = EditorData != nullptr && EditorData->HasBreakpoint(Transition.ID, EMetaStoryBreakpointType::OnTransition);
		if ((FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredTrue && bHasBreakpoint == false)
			|| (FilterOptions.WithBreakpoint == ETransitionDescRequirement::RequiredFalse && bHasBreakpoint))
		{
			continue;
		}
#endif // WITH_METASTORY_TRACE_DEBUGGER
*/
		
		// The icons here depict "transition direction", not the type specifically.
		const bool bMatch = /*FilterOptions.bUseMask ? EnumHasAnyFlags(Transition.Trigger, Trigger) :*/ Transition.Trigger == Trigger;
		if (bMatch)
		{
			switch (Transition.State.LinkType)
			{
			case EMetaStoryTransitionType::None:
				IconType |= IconGoto;
				break;
			case EMetaStoryTransitionType::Succeeded:
				IconType |= IconGoto;
				break;
			case EMetaStoryTransitionType::Failed:
				IconType |= IconGoto;
				break;
			case EMetaStoryTransitionType::NextState:
			case EMetaStoryTransitionType::NextSelectableState:
				IconType |= IconNext;
				break;
			case EMetaStoryTransitionType::GotoState:
				IconType |= IconGoto;
				break;
			default:
				ensureMsgf(false, TEXT("Unhandled transition type."));
				break;
			}
		}
	}

	if (FMath::CountBits(static_cast<uint64>(IconType)) > 1)
	{
		// Prune down to just one icon.
		IconType = IconGoto;
	}

	if (IsLeafState()
		&& IconType == IconNone
		&& EnumHasAnyFlags(Trigger, EMetaStoryTransitionTrigger::OnStateCompleted))
	{
		// Transition is handled on parent state, or implicit Root.
		IconType = IconParent;
	}

	switch (IconType)
	{
		case IconGoto:
			return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Goto");
		case IconNext:
			return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Next");
		case IconParent:
			return FMetaStoryEditorStyle::Get().GetBrush("MetaStoryEditor.Transition.Parent");
		default:
			break;
	}
	
	return nullptr;
}

EVisibility SMetaStoryViewRow::GetTransitionsVisibility(const EMetaStoryTransitionTrigger Trigger) const
{
	const UMetaStoryState* State = WeakState.Get();
	if (!State)
	{
		return EVisibility::Collapsed;
	}
	
	// Handle completed, succeeded and failed transitions.
	if (EnumHasAnyFlags(Trigger, EMetaStoryTransitionTrigger::OnStateCompleted))
	{
		EMetaStoryTransitionTrigger HandledTriggers = EMetaStoryTransitionTrigger::None;
		bool bExactMatch = false;

		for (const FMetaStoryTransition& Transition : State->Transitions)
		{
			// Skip disabled transitions
			if (Transition.bTransitionEnabled == false)
			{
				continue;
			}

			HandledTriggers |= Transition.Trigger;
			bExactMatch |= (Transition.Trigger == Trigger);

			if (bExactMatch)
			{
				break;
			}
		}

		// Assume that leaf states should have completion transitions.
		if (!bExactMatch && IsLeafState())
		{
			// Find the missing transition type, note: Completed = Succeeded|Failed.
			const EMetaStoryTransitionTrigger MissingTriggers = HandledTriggers ^ EMetaStoryTransitionTrigger::OnStateCompleted;
			return MissingTriggers == Trigger ? EVisibility::Visible : EVisibility::Collapsed;
		}
		
		return bExactMatch ? EVisibility::Visible : EVisibility::Collapsed;
	}

	// Find states from transition tasks
	if (EnumHasAnyFlags(Trigger, EMetaStoryTransitionTrigger::OnTick | EMetaStoryTransitionTrigger::OnEvent| EMetaStoryTransitionTrigger::OnDelegate))
	{
		auto HasAnyLinksInStruct = [](FMetaStoryDataView Struct) -> bool
		{
			if (!Struct.IsValid())
			{
				return false;
			}
			for (TPropertyValueIterator<FStructProperty> It(Struct.GetStruct(), Struct.GetMemory()); It; ++It)
			{
				const UScriptStruct* StructType = It.Key()->Struct;
				if (StructType == TBaseStructure<FMetaStoryStateLink>::Get())
				{
					const FMetaStoryStateLink& Link = *static_cast<const FMetaStoryStateLink*>(It.Value());
					if (Link.LinkType != EMetaStoryTransitionType::None)
					{
						return true;
					}
				}
			}
			return false;
		};
		
		for (const FMetaStoryEditorNode& Task : State->Tasks)
		{
			if (HasAnyLinksInStruct(FMetaStoryDataView(Task.Node.GetScriptStruct(), const_cast<uint8*>(Task.Node.GetMemory())))
				|| HasAnyLinksInStruct(Task.GetInstance()))
			{
				return EVisibility::Visible;
			}
		}

		if (HasAnyLinksInStruct(FMetaStoryDataView(State->SingleTask.Node.GetScriptStruct(), const_cast<uint8*>(State->SingleTask.Node.GetMemory())))
			|| HasAnyLinksInStruct(State->SingleTask.GetInstance()))
		{
			return EVisibility::Visible;
		}
	}
	
	// Handle the test
	for (const FMetaStoryTransition& Transition : State->Transitions)
	{
		// Skip disabled transitions
		if (Transition.bTransitionEnabled == false)
		{
			continue;
		}

		if (EnumHasAnyFlags(Trigger, Transition.Trigger))
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

EVisibility SMetaStoryViewRow::GetTransitionsBreakpointVisibility(const EMetaStoryTransitionTrigger Trigger) const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
#if WITH_METASTORY_TRACE_DEBUGGER
		if (const UMetaStoryEditorData* EditorData = WeakEditorData.Get())
		{
			for (const FMetaStoryTransition& Transition : State->Transitions)
			{
				if (Transition.bTransitionEnabled && EnumHasAnyFlags(Trigger, Transition.Trigger))
				{
					if (EditorData->HasBreakpoint(Transition.ID, EMetaStoryBreakpointType::OnTransition))
					{
						return GetTransitionsVisibility(Trigger);
					}
				}
			}
		}
#endif // WITH_METASTORY_TRACE_DEBUGGER
	}
	
	return EVisibility::Collapsed;
}

EVisibility SMetaStoryViewRow::GetStateDescriptionVisibility() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		return State->Description.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FText SMetaStoryViewRow::GetStateDescription() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		return FText::FromString(State->Description);
	}
	return FText::GetEmpty();
}

EVisibility SMetaStoryViewRow::GetTransitionDashVisibility() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		return State->Transitions.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
	}
	return EVisibility::Collapsed;	
}

bool SMetaStoryViewRow::IsRootState() const
{
	// Routines can be identified by not having parent state.
	const UMetaStoryState* State = WeakState.Get();
	return State ? State->Parent == nullptr : false;
}

bool SMetaStoryViewRow::IsStateSelected() const
{
	if (const UMetaStoryState* State = WeakState.Get())
	{
		if (MetaStoryViewModel)
		{
			return MetaStoryViewModel->IsSelected(State);
		}
	}
	return false;
}

bool SMetaStoryViewRow::HandleVerifyNodeLabelTextChanged(const FText& InText, FText& OutErrorMessage) const
{
	if (MetaStoryViewModel)
	{
		if (const UMetaStoryState* State = WeakState.Get())
		{
			const FString NewName = FText::TrimPrecedingAndTrailing(InText).ToString();
			if (NewName.Len() >= NAME_SIZE)
			{
				OutErrorMessage = LOCTEXT("VerifyNodeLabelFailed_MaxLength", "Max length exceeded");
				return false;
			}
			return NewName.Len() > 0;
		}
	}
	OutErrorMessage = LOCTEXT("VerifyNodeLabelFailed", "Invalid node");
	return false;
}

void SMetaStoryViewRow::HandleNodeLabelTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType) const
{
	if (MetaStoryViewModel)
	{
		if (UMetaStoryState* State = WeakState.Get())
		{
			const FString NewName = FText::TrimPrecedingAndTrailing(NewLabel).ToString();
			if (NewName.Len() > 0 && NewName.Len() < NAME_SIZE)
			{
				MetaStoryViewModel->RenameState(State, FName(NewName));
			}
		}
	}
}

FReply SMetaStoryViewRow::HandleDragDetected(const FGeometry&, const FPointerEvent&) const
{
	return FReply::Handled().BeginDragDrop(FMetaStorySelectedDragDrop::New(MetaStoryViewModel));
}

void SMetaStoryViewRow::HandleDragLeave(const FDragDropEvent& DragDropEvent) const
{
	const TSharedPtr<FMetaStorySelectedDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FMetaStorySelectedDragDrop>();
	if (DragDropOperation.IsValid())
	{
		DragDropOperation->SetCanDrop(false);
	}	
}

TOptional<EItemDropZone> SMetaStoryViewRow::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TWeakObjectPtr<UMetaStoryState> TargetState) const
{
	const TSharedPtr<FMetaStorySelectedDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FMetaStorySelectedDragDrop>();
	if (DragDropOperation.IsValid())
	{
		DragDropOperation->SetCanDrop(true);

		// Cannot drop on selection or child of selection.
		if (MetaStoryViewModel && MetaStoryViewModel->IsChildOfSelection(TargetState.Get()))
		{
			DragDropOperation->SetCanDrop(false);
			return TOptional<EItemDropZone>();
		}

		return DropZone;
	}

	return TOptional<EItemDropZone>();
}

FReply SMetaStoryViewRow::HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TWeakObjectPtr<UMetaStoryState> TargetState) const
{
	const TSharedPtr<FMetaStorySelectedDragDrop> DragDropOperation = DragDropEvent.GetOperationAs<FMetaStorySelectedDragDrop>();
	if (DragDropOperation.IsValid())
	{
		if (MetaStoryViewModel)
		{
			if (DropZone == EItemDropZone::AboveItem)
			{
				MetaStoryViewModel->MoveSelectedStatesBefore(TargetState.Get());
			}
			else if (DropZone == EItemDropZone::BelowItem)
			{
				MetaStoryViewModel->MoveSelectedStatesAfter(TargetState.Get());
			}
			else
			{
				MetaStoryViewModel->MoveSelectedStatesInto(TargetState.Get());
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SMetaStoryViewRow::HandleAssetChanged()
{
	MakeFlagsWidget();
	MakeTransitionsWidget();
}

void SMetaStoryViewRow::HandleStatesChanged(const TSet<UMetaStoryState*>& ChangedStates, const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (const UMetaStoryState* OwnerState = WeakState.Get())
	{
		if (ChangedStates.Contains(OwnerState))
		{
			if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMetaStoryState, Transitions)
				|| PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FMetaStoryStateLink, LinkType))
			{
				MakeTransitionsWidget();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
