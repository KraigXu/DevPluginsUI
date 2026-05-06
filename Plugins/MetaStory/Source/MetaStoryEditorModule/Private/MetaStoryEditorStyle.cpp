// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorStyle.h"
#include "Brushes/SlateBoxBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "Misc/Paths.h"
#include "Styling/StyleColors.h"
#include "MetaStoryTypes.h"
#include "Styling/SlateStyleMacros.h"

FMetaStoryEditorStyle::FMetaStoryEditorStyle()
	: FMetaStoryStyle(TEXT("MetaStoryEditorStyle"))
{
	const FString EngineEditorSlateContentDir = FPaths::EngineContentDir() / TEXT("Editor/Slate");

	const FScrollBarStyle ScrollBar = FAppStyle::GetWidgetStyle<FScrollBarStyle>("ScrollBar");
	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	// State
	{
		const FEditableTextBoxStyle StateTitleEditableText = FEditableTextBoxStyle()
			.SetTextStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 12))
			.SetBackgroundImageNormal(CORE_BOX_BRUSH("Common/TextBox", FMargin(4.0f / 16.0f)))
			.SetBackgroundImageHovered(CORE_BOX_BRUSH("Common/TextBox_Hovered", FMargin(4.0f / 16.0f)))
			.SetBackgroundImageFocused(CORE_BOX_BRUSH("Common/TextBox_Hovered", FMargin(4.0f / 16.0f)))
			.SetBackgroundImageReadOnly(CORE_BOX_BRUSH("Common/TextBox_ReadOnly", FMargin(4.0f / 16.0f)))
			.SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.1f))
			.SetPadding(FMargin(0))
			.SetScrollBarStyle(ScrollBar);
		Set("MetaStory.State.TitleEditableText", StateTitleEditableText);

		Set("MetaStory.State.TitleInlineEditableText", FInlineEditableTextBlockStyle()
			.SetTextStyle(GetWidgetStyle<FTextBlockStyle>(StateTitleTextStyleName))
			.SetEditableTextBoxStyle(StateTitleEditableText));
	}

	// Details
	{
		const FTextBlockStyle Details = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10))
			.SetColorAndOpacity(FLinearColor(230.0f / 255.0f, 230.0f / 255.0f, 230.0f / 255.0f, 0.75f));
		Set("MetaStory.Details", Details);

		Set("MetaStory.Node.Label", new FSlateRoundedBoxBrush(FStyleColors::AccentGray, 6.f));

		// For multi selection with mixed values for a given property
		const FLinearColor Color = FStyleColors::Hover.GetSpecifiedColor();
		const FLinearColor HollowColor = Color.CopyWithNewOpacity(0.0);
		Set("MetaStory.Node.Label.Mixed", new FSlateRoundedBoxBrush(HollowColor, 6.0f, Color, 1.0f));

		const FTextBlockStyle DetailsCategory = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 8));
		Set("MetaStory.Category", DetailsCategory);
	}

	// Task
	{
		const FLinearColor ForegroundCol = FStyleColors::Foreground.GetSpecifiedColor();

		Set("MetaStory.Task.Title", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10))
			.SetColorAndOpacity(ForegroundCol.CopyWithNewOpacity(0.8f)));

		Set("MetaStory.Task.Title.Bold", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 10))
			.SetColorAndOpacity(ForegroundCol.CopyWithNewOpacity(0.8f)));

		Set("MetaStory.Task.Title.Subdued", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10))
			.SetColorAndOpacity(ForegroundCol.CopyWithNewOpacity(0.4f)));

		// Tasks to be show up a bit darker than the state
		Set("MetaStory.Task.Rect", new FSlateColorBrush(FLinearColor(FVector3f(0.67f))));
	}

	// Details rich text
	{
		Set("Details.Normal", FTextBlockStyle(NormalText)
			.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"))));

		Set("Details.Bold", FTextBlockStyle(NormalText)
			.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont"))));

		Set("Details.Italic", FTextBlockStyle(NormalText)
			.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.ItalicFont"))));

		Set("Details.Subdued", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FSlateColor::UseSubduedForeground())
			.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"))));
	}

	// Transition rich text
	{
		const FLinearColor ForegroundCol = FStyleColors::White.GetSpecifiedColor();
		Set("Transition.Normal", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(ForegroundCol.CopyWithNewOpacity(0.9f))
			.SetFont(DEFAULT_FONT("Regular", 11)));

		Set("Transition.Bold", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(ForegroundCol.CopyWithNewOpacity(0.9f))
			.SetFont(DEFAULT_FONT("Bold", 11)));

		Set("Transition.Italic", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(ForegroundCol.CopyWithNewOpacity(0.9f))
			.SetFont(DEFAULT_FONT("Italic", 11)));

		Set("Transition.Subdued", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(ForegroundCol.CopyWithNewOpacity(0.5f))
			.SetFont(DEFAULT_FONT("Regular", 11)));
	}

	// Diff tool
	{
		Set("DiffTools.Added", FLinearColor(0.3f, 1.f, 0.3f)); // green
		Set("DiffTools.Removed", FLinearColor(1.0f, 0.2f, 0.3f)); // red
		Set("DiffTools.Changed", FLinearColor(0.85f, 0.71f, 0.25f)); // yellow
		Set("DiffTools.Moved", FLinearColor(0.5f, 0.8f, 1.f)); // light blue
		Set("DiffTools.Enabled", FLinearColor(0.7f, 1.f, 0.7f)); // light green
		Set("DiffTools.Disabled", FLinearColor(1.0f, 0.6f, 0.5f)); // light red
		Set("DiffTools.Properties", FLinearColor(0.2f, 0.4f, 1.f)); // blue
	}

	const FLinearColor SelectionColor = FColor(0, 0, 0, 32);
	const FTableRowStyle& NormalTableRowStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");
	Set("MetaStory.Selection",
		FTableRowStyle(NormalTableRowStyle)
		.SetActiveBrush(CORE_IMAGE_BRUSH("Common/Selection", CoreStyleConstants::Icon8x8, SelectionColor))
		.SetActiveHoveredBrush(CORE_IMAGE_BRUSH("Common/Selection", CoreStyleConstants::Icon8x8, SelectionColor))
		.SetInactiveBrush(CORE_IMAGE_BRUSH("Common/Selection", CoreStyleConstants::Icon8x8, SelectionColor))
		.SetInactiveHoveredBrush(CORE_IMAGE_BRUSH("Common/Selection", CoreStyleConstants::Icon8x8, SelectionColor))
		.SetSelectorFocusedBrush(CORE_IMAGE_BRUSH("Common/Selection", CoreStyleConstants::Icon8x8, SelectionColor))
	);

	const FComboButtonStyle& ComboButtonStyle = FCoreStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton");

	// Expression Operand combo button
	const FButtonStyle OperandButton = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FStyleColors::AccentGreen.GetSpecifiedColor().Desaturate(0.3f), 4.0f))
		.SetHovered(FSlateRoundedBoxBrush(FStyleColors::AccentGreen.GetSpecifiedColor().Desaturate(0.2f), 4.0f))
		.SetPressed(FSlateRoundedBoxBrush(FStyleColors::AccentGreen.GetSpecifiedColor().Desaturate(0.1f), 4.0f))
		.SetNormalForeground(FStyleColors::Foreground)
		.SetHoveredForeground(FStyleColors::ForegroundHover)
		.SetPressedForeground(FStyleColors::ForegroundHover)
		.SetDisabledForeground(FStyleColors::ForegroundHover)
		.SetNormalPadding(FMargin(2, 2, 2, 2))
		.SetPressedPadding(FMargin(2, 3, 2, 1));

	Set("MetaStory.Node.Operand.ComboBox", FComboButtonStyle(ComboButtonStyle).SetButtonStyle(OperandButton));

	Set("MetaStory.Node.Operand", FTextBlockStyle(NormalText)
		.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
		.SetFontSize(8));

	Set("MetaStory.Node.Parens", FTextBlockStyle(NormalText)
		.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.SetFontSize(12));

	// Parameter labels
	Set("MetaStory.Param.Label", FTextBlockStyle(NormalText)
		.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
		.SetFontSize(7));

	Set("MetaStory.Param.Background", new FSlateRoundedBoxBrush(FStyleColors::Hover, 6.f));

	// Expression Indent combo button
	const FButtonStyle IndentButton = FButtonStyle()
		.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, 2.0f))
		.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Background, 2.0f, FStyleColors::InputOutline, 1.0f))
		.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Background, 2.0f, FStyleColors::Hover, 1.0f))
		.SetNormalForeground(FStyleColors::Transparent)
		.SetHoveredForeground(FStyleColors::Hover)
		.SetPressedForeground(FStyleColors::Foreground)
		.SetNormalPadding(FMargin(2, 2, 2, 2))
		.SetPressedPadding(FMargin(2, 3, 2, 1));

	Set("MetaStory.Node.Indent.ComboBox", FComboButtonStyle(ComboButtonStyle).SetButtonStyle(IndentButton));

	// Node text styles
	{
		FEditableTextStyle EditableTextStyle = FEditableTextStyle(FAppStyle::GetWidgetStyle<FEditableTextStyle>("NormalEditableText"));
		EditableTextStyle.Font = FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"));
		EditableTextStyle.Font.Size = 10.0f;
		Set("MetaStory.Node.Editable", EditableTextStyle);

		FEditableTextBoxStyle EditableTextBlockStyle = FEditableTextBoxStyle(FAppStyle::GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"));
		EditableTextStyle.Font = FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"));
		EditableTextStyle.Font.Size = 10.0f;
		Set("MetaStory.Node.EditableTextBlock", EditableTextBlockStyle);

		const FTextBlockStyle StateNodeNormalText = FTextBlockStyle(NormalText)
			.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.SetFontSize(10);
		Set("MetaStory.Node.Normal", StateNodeNormalText);

		Set("MetaStory.Node.Bold", FTextBlockStyle(NormalText)
			.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
			.SetFontSize(10));

		Set("MetaStory.Node.Subdued", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FSlateColor::UseSubduedForeground())
			.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			.SetFontSize(10));

		Set("MetaStory.Node.TitleInlineEditableText", FInlineEditableTextBlockStyle()
			.SetTextStyle(StateNodeNormalText)
			.SetEditableTextBoxStyle(EditableTextBlockStyle));
	}

	// Command icons
	{
		// From generic Engine
		FContentRootScope Scope(this, EngineSlateContentDir);
		Set("MetaStoryEditor.CutStates", new IMAGE_BRUSH_SVG("Starship/Common/Cut", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.CopyStates", new IMAGE_BRUSH_SVG("Starship/Common/Copy", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.DuplicateStates", new IMAGE_BRUSH_SVG("Starship/Common/Duplicate", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.DeleteStates", new IMAGE_BRUSH_SVG("Starship/Common/Delete", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.RenameState", new IMAGE_BRUSH_SVG("Starship/Common/Rename", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.AutoScroll", new IMAGE_BRUSH_SVG("Starship/Insights/AutoScrollRight_20", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.Debugger.ResetTracks", new IMAGE_BRUSH_SVG("Starship/Common/Delete", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.Debugger.Task.Enter", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-right", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("MetaStoryEditor.Debugger.Task.Exit", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-left", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));

		Set("MetaStoryEditor.Debugger.Unset", new CORE_IMAGE_BRUSH_SVG("Starship/Common/help", CoreStyleConstants::Icon16x16, FStyleColors::AccentBlack));

		// Common Node Icons
		Set("Node.EnableDisable", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check-circle", CoreStyleConstants::Icon16x16));
		Set("Node.Time", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Recent", CoreStyleConstants::Icon16x16));
		Set("Node.Sync", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Update", CoreStyleConstants::Icon16x16));
	}

	{
		// From RewindDebugger
		FContentRootScope Scope(this, FPaths::EngineDir() / TEXT("Plugins/Animation/GameplayInsights/Content"));
		Set("MetaStoryEditor.Debugger.OpenRewindDebugger", new IMAGE_BRUSH("Rewind_24x", CoreStyleConstants::Icon16x16));
	}

	{
		// From generic Engine Editor
		FContentRootScope Scope(this, EngineEditorSlateContentDir);
		Set("MetaStoryEditor.Debugger.StartRewindDebuggerRecording", new IMAGE_BRUSH("Sequencer/Transport_Bar/Record_24x", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.Debugger.StartRecording", new IMAGE_BRUSH("Sequencer/Transport_Bar/Record_24x", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Debugger.StopRecording", new IMAGE_BRUSH("Sequencer/Transport_Bar/Recording_24x", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.Debugger.PreviousFrameWithStateChange", new IMAGE_BRUSH("Sequencer/Transport_Bar/Go_To_Front_24x", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Debugger.PreviousFrameWithEvents", new IMAGE_BRUSH("Sequencer/Transport_Bar/Step_Backwards_24x", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Debugger.NextFrameWithEvents", new IMAGE_BRUSH("Sequencer/Transport_Bar/Step_Forward_24x", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Debugger.NextFrameWithStateChange", new IMAGE_BRUSH("Sequencer/Transport_Bar/Go_To_End_24x", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.Debugger.ToggleOnEnterStateBreakpoint", new IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Debugger.EnableOnEnterStateBreakpoint", new IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Debugger.EnableOnExitStateBreakpoint", new IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.DebugOptions", new IMAGE_BRUSH_SVG("Starship/Common/Bug", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.Debugger.OwnerTrack", new IMAGE_BRUSH_SVG("Starship/AssetIcons/AIController_64", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Debugger.InstanceTrack", new IMAGE_BRUSH_SVG("Starship/AssetIcons/AnimInstance_64", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.EnableStates", new IMAGE_BRUSH("Icons/Empty_16x", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Debugger.Breakpoint.EnabledAndValid", new IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));
		Set("MetaStoryEditor.Debugger.ResumeDebuggerAnalysis", new IMAGE_BRUSH_SVG("Starship/Common/Timeline", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.Transition.None", new CORE_IMAGE_BRUSH_SVG("Starship/Common/x-circle", CoreStyleConstants::Icon16x16, FSlateColor::UseSubduedForeground()));
		Set("MetaStoryEditor.Transition.Succeeded", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check", CoreStyleConstants::Icon16x16, FStyleColors::AccentGreen));
		Set("MetaStoryEditor.Transition.Failed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/close-small", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));

		Set("MetaStoryEditor.Transition.Succeeded", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check", CoreStyleConstants::Icon16x16, FStyleColors::AccentGreen));
		Set("MetaStoryEditor.Transition.Failed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/close-small", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));

		// Common Node Icons
		Set("Node.Navigation", new IMAGE_BRUSH_SVG("Starship/Common/Navigation", CoreStyleConstants::Icon16x16));
		Set("Node.Event", new IMAGE_BRUSH_SVG("Starship/Common/Event", CoreStyleConstants::Icon16x16));
		Set("Node.Animation", new IMAGE_BRUSH_SVG("Starship/Common/Animation", CoreStyleConstants::Icon16x16));
		Set("Node.Debug", new IMAGE_BRUSH_SVG("Starship/Common/Debug", CoreStyleConstants::Icon16x16));
		Set("Node.Find", new IMAGE_BRUSH_SVG("Starship/Common/Find", CoreStyleConstants::Icon16x16));
	}

	{
		// From plugin (Resources/Icons/MetaStory_*.svg; logo files MetaStory.svg / MetaStory_64.svg)
		Set("ClassThumbnail.MetaStory", new IMAGE_BRUSH_SVG("Icons/MetaStory_64", CoreStyleConstants::Icon64x64));
		Set("ClassIcon.MetaStory", new IMAGE_BRUSH_SVG("Icons/MetaStory", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.AddSiblingState", new IMAGE_BRUSH_SVG("Icons/MetaStory_Sibling_State", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.AddChildState", new IMAGE_BRUSH_SVG("Icons/MetaStory_Child_State", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.PasteStatesAsSiblings", new IMAGE_BRUSH_SVG("Icons/MetaStory_Sibling_State", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.PasteStatesAsChildren", new IMAGE_BRUSH_SVG("Icons/MetaStory_Child_State", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.StateConditions", new IMAGE_BRUSH_SVG("Icons/MetaStory_State_Conditions", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.Conditions", new IMAGE_BRUSH_SVG("Icons/MetaStory_Conditions", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Conditions.Large", new IMAGE_BRUSH_SVG("Icons/MetaStory_Conditions", CoreStyleConstants::Icon24x24));
		Set("MetaStoryEditor.Evaluators", new IMAGE_BRUSH_SVG("Icons/MetaStory_Evaluators", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Parameters", new IMAGE_BRUSH_SVG("Icons/MetaStory_Parameters", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Utility", new IMAGE_BRUSH_SVG("Icons/MetaStory_Utility", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Utility.Large", new IMAGE_BRUSH_SVG("Icons/MetaStory_Utility", CoreStyleConstants::Icon24x24));
		Set("MetaStoryEditor.Tasks", new IMAGE_BRUSH_SVG("Icons/MetaStory_Tasks", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Tasks.Large", new IMAGE_BRUSH_SVG("Icons/MetaStory_Tasks", CoreStyleConstants::Icon24x24));
		Set("MetaStoryEditor.Transitions", new IMAGE_BRUSH_SVG("Icons/MetaStory_Transitions", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.TasksCompletion.Enabled", new IMAGE_BRUSH_SVG("Icons/MetaStory_ConsiderTask", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.TasksCompletion.Disabled", new IMAGE_BRUSH_SVG("Icons/MetaStory_NotConsiderTask", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.StateSubtree", new IMAGE_BRUSH_SVG("Icons/MetaStory_State_Subtree", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.StateLinked", new IMAGE_BRUSH_SVG("Icons/MetaStory_State_Linked", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.Transition.Dash", new IMAGE_BRUSH_SVG("Icons/MetaStory_Transition_Dash", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("MetaStoryEditor.Transition.Goto", new IMAGE_BRUSH_SVG("Icons/MetaStory_Transition_Goto", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("MetaStoryEditor.Transition.Next", new IMAGE_BRUSH_SVG("Icons/MetaStory_Transition_Next", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("MetaStoryEditor.Transition.Parent", new IMAGE_BRUSH_SVG("Icons/MetaStory_Transition_Parent", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));

		Set("MetaStoryEditor.Transition.Condition", new IMAGE_BRUSH_SVG("Icons/MetaStory_State_Conditions", CoreStyleConstants::Icon16x16, FStyleColors::AccentGray));

		// Common Node Icons
		Set("Node.Movement", new IMAGE_BRUSH_SVG("Icons/MetaStory_Movement", CoreStyleConstants::Icon16x16));
		Set("Node.Tag", new IMAGE_BRUSH_SVG("Icons/MetaStory_Tag", CoreStyleConstants::Icon16x16));
		Set("Node.RunParallel", new IMAGE_BRUSH_SVG("Icons/MetaStory_RunParallel", CoreStyleConstants::Icon16x16));
		Set("Node.Task", new IMAGE_BRUSH_SVG("Icons/MetaStory_Task", CoreStyleConstants::Icon16x16));
		Set("Node.Text", new IMAGE_BRUSH_SVG("Icons/MetaStory_Text", CoreStyleConstants::Icon16x16));
		Set("Node.Function", new IMAGE_BRUSH_SVG("Icons/MetaStory_Function", CoreStyleConstants::Icon16x16));

		// Runtime flag
		Set("MetaStoryEditor.Flags.Tick", new IMAGE_BRUSH_SVG("Icons/MetaStory_Tick", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.Flags.TickOnEvent", new IMAGE_BRUSH_SVG("Icons/MetaStory_TickEvent", CoreStyleConstants::Icon16x16));
	}
	{
		Set("Colors.StateLinkingIn", FLinearColor::Yellow);
		Set("Colors.StateLinkedOut", FLinearColor::Green);

		// Embedded main Flow graph (SMetaStoryFlowGraph) — read via GetColor("FlowGraph.*", Fallback)
		Set("FlowGraph.Canvas.Background", FLinearColor(0.07f, 0.07f, 0.075f, 1.0f));
		Set("FlowGraph.Grid.Line", FLinearColor(1.0f, 1.0f, 1.0f, 0.06f));
		Set("FlowGraph.StageBand.A", FLinearColor(0.17f, 0.23f, 0.34f, 0.05f));
		Set("FlowGraph.StageBand.B", FLinearColor(0.11f, 0.15f, 0.22f, 0.035f));
		Set("FlowGraph.StageHeader.Background", FLinearColor(0.08f, 0.11f, 0.17f, 0.78f));
		Set("FlowGraph.StageHeader.Text", FLinearColor(0.82f, 0.90f, 1.0f, 0.95f));
		Set("FlowGraph.LayerLabel.Text", FLinearColor(0.70f, 0.76f, 0.84f, 0.9f));
		Set("FlowGraph.Timeline.Accent", FLinearColor(0.40f, 0.72f, 0.98f, 0.95f));
		Set("FlowGraph.Link.Emphasized", FLinearColor(0.38f, 0.78f, 1.0f, 0.96f));
		Set("FlowGraph.Link.Default", FLinearColor(0.68f, 0.70f, 0.74f, 0.52f));
		Set("FlowGraph.DragCell.Valid", FLinearColor(0.15f, 0.85f, 0.35f, 0.14f));
		Set("FlowGraph.DragCell.Invalid", FLinearColor(0.95f, 0.28f, 0.22f, 0.11f));
		Set("FlowGraph.Node.Border", FLinearColor(0.12f, 0.12f, 0.14f, 1.0f));
		Set("FlowGraph.Node.BorderSelected", FStyleColors::Primary.GetSpecifiedColor());
		Set("FlowGraph.Node.RuntimeActive", FLinearColor(0.28f, 0.92f, 0.52f, 1.0f));
		Set("FlowGraph.Preview.Valid", FStyleColors::Primary.GetSpecifiedColor().CopyWithNewOpacity(0.9f));
		Set("FlowGraph.Preview.Invalid", FLinearColor(1.0f, 0.45f, 0.35f, 0.92f));
		Set("FlowGraph.Preview.InvalidBackward", FLinearColor(0.65f, 0.65f, 0.68f, 0.95f));
		Set("FlowGraph.Preview.InvalidSkipStage", FLinearColor(1.0f, 0.62f, 0.30f, 0.95f));
		Set("FlowGraph.Preview.InvalidDuplicate", FLinearColor(0.95f, 0.80f, 0.30f, 0.95f));
		Set("FlowGraph.Pin.ActiveValid", FStyleColors::Primary.GetSpecifiedColor());
		Set("FlowGraph.Pin.ActiveInvalid", FLinearColor(1.0f, 0.35f, 0.35f, 1.0f));
		Set("FlowGraph.Pin.Idle", FLinearColor(0.70f, 0.74f, 0.80f, 0.95f));
		Set("FlowGraph.Node.Title", FLinearColor::White);
		Set("FlowGraph.Node.MetaText", FLinearColor(0.65f, 0.68f, 0.72f, 1.0f));
		Set("FlowGraph.Node.DescText", FLinearColor(0.75f, 0.78f, 0.82f, 1.0f));
		Set("FlowGraph.Badge.Background", FLinearColor(0.06f, 0.07f, 0.09f, 0.94f));
		Set("FlowGraph.Badge.CountZero", FLinearColor(0.52f, 0.55f, 0.60f, 1.0f));
		Set("FlowGraph.Badge.CountNonZero", FLinearColor(0.92f, 0.94f, 0.98f, 1.0f));
		Set("FlowGraph.Hint.Warning", FLinearColor(1.0f, 0.70f, 0.65f, 0.98f));
	}
}

void FMetaStoryEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaStoryEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FMetaStoryEditorStyle& FMetaStoryEditorStyle::Get()
{
	static FMetaStoryEditorStyle Instance;
	return Instance;
}

const FSlateBrush* FMetaStoryEditorStyle::GetBrushForSelectionBehaviorType(const EMetaStoryStateSelectionBehavior InSelectionBehavior, const bool bInHasChildren, const EMetaStoryStateType InStateType)
{
	// Simply redirecting to the base class.
	// Keeping the method in the derived class otherwise other modules that were already calling it will have a missing module dependency to MetaStoryDeveloper otherwise.
	return FMetaStoryStyle::GetBrushForSelectionBehaviorType(InSelectionBehavior, bInHasChildren, InStateType);
}
