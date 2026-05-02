// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryStyle.h"

#include "MetaStoryTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"

const FLazyName FMetaStoryStyle::StateTitleTextStyleName("MetaStory.State.Title");
const FString FMetaStoryStyle::EngineSlateContentDir(FPaths::EngineContentDir() / TEXT("Slate"));
const FString FMetaStoryStyle::MetaStoryPluginContentDir(FPaths::EnginePluginsDir() / TEXT("Runtime/MetaStory/Resources"));

FMetaStoryStyle::FMetaStoryStyle() : FMetaStoryStyle(TEXT("MetaStoryStyle"))
{
}

FMetaStoryStyle::FMetaStoryStyle(const FName& InStyleSetName)
	: FSlateStyleSet(InStyleSetName)
{
	SetCoreContentRoot(EngineSlateContentDir);
	SetContentRoot(MetaStoryPluginContentDir);

	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	// Debugger
	{
		Set("MetaStoryDebugger.Element.Normal",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10)));

		Set("MetaStoryDebugger.Element.Bold",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 10)));

		Set("MetaStoryDebugger.Element.Subdued",
			FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 10))
			.SetColorAndOpacity(FSlateColor::UseSubduedForeground()));
	}

	// State
	{
		const FTextBlockStyle StateTitle = FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 12))
			.SetColorAndOpacity(FLinearColor(230.0f / 255.0f, 230.0f / 255.0f, 230.0f / 255.0f, 0.9f));
		Set(StateTitleTextStyleName, StateTitle);

		Set("MetaStory.State", new FSlateRoundedBoxBrush(FLinearColor::White, 2.0f));
		Set("MetaStory.State.Border", new FSlateRoundedBoxBrush(FLinearColor::White, 2.0f));
		Set("MetaStory.Debugger.State.Active", FSlateColor(FColor::Yellow));
		Set("MetaStory.CompactView.State", FSlateColor(FLinearColor(FColor(10, 100, 120))));
	}

	// Normal rich text
	{
		Set("Normal.Normal", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FSlateColor::UseForeground())
			.SetFont(DEFAULT_FONT("Regular", 10)));

		Set("Normal.Bold", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FSlateColor::UseForeground())
			.SetFont(DEFAULT_FONT("Bold", 10)));

		Set("Normal.Italic", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FSlateColor::UseForeground())
			.SetFont(DEFAULT_FONT("Italic", 10)));

		Set("Normal.Subdued", FTextBlockStyle(NormalText)
			.SetColorAndOpacity(FSlateColor::UseSubduedForeground())
			.SetFont(DEFAULT_FONT("Regular", 10)));
	}

	{
		// From plugin
		Set("MetaStoryEditor.SelectNone", new IMAGE_BRUSH_SVG("Icons/Select_None", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.TryEnterState", new IMAGE_BRUSH_SVG("Icons/Try_Enter_State", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.TrySelectChildrenInOrder", new IMAGE_BRUSH_SVG("Icons/Try_Select_Children_In_Order", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.TrySelectChildrenAtRandom", new IMAGE_BRUSH_SVG("Icons/Try_Select_Children_At_Random", CoreStyleConstants::Icon16x16));
		Set("MetaStoryEditor.TryFollowTransitions", new IMAGE_BRUSH_SVG("Icons/Try_Follow_Transitions", CoreStyleConstants::Icon16x16));

		Set("MetaStoryEditor.Debugger.Condition.OnTransition", new IMAGE_BRUSH_SVG("Icons/State_Conditions", CoreStyleConstants::Icon16x16, FStyleColors::AccentGray));
	}

	{
		// From generic Engine
		FContentRootScope Scope(this, EngineSlateContentDir);
		Set("MetaStoryEditor.Debugger.State.Enter", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-right", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("MetaStoryEditor.Debugger.State.Exit", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-left", CoreStyleConstants::Icon16x16, FStyleColors::Foreground));
		Set("MetaStoryEditor.Debugger.State.Completed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check", CoreStyleConstants::Icon16x16, FStyleColors::AccentGreen));
		Set("MetaStoryEditor.Debugger.State.Selected", new CORE_IMAGE_BRUSH_SVG("Starship/Common/arrow-right", CoreStyleConstants::Icon16x16, FStyleColors::AccentYellow));

		Set("MetaStoryEditor.Debugger.Log.Warning", new CORE_IMAGE_BRUSH_SVG("Starship/Common/alert-circle", CoreStyleConstants::Icon16x16, FStyleColors::AccentYellow));
		Set("MetaStoryEditor.Debugger.Log.Error", new CORE_IMAGE_BRUSH_SVG("Starship/Common/x-circle", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));

		Set("MetaStoryEditor.Debugger.Task.Failed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/close-small", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));
		Set("MetaStoryEditor.Debugger.Task.Succeeded", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check", CoreStyleConstants::Icon16x16, FStyleColors::AccentGreen));
		Set("MetaStoryEditor.Debugger.Task.Stopped", new CORE_IMAGE_BRUSH_SVG("Starship/Common/close-small", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));

		Set("MetaStoryEditor.Debugger.Condition.Passed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/check", CoreStyleConstants::Icon16x16, FStyleColors::AccentGreen));
		Set("MetaStoryEditor.Debugger.Condition.Failed", new CORE_IMAGE_BRUSH_SVG("Starship/Common/close-small", CoreStyleConstants::Icon16x16, FStyleColors::AccentRed));
		Set("MetaStoryEditor.Debugger.Condition.OnEvaluating", new CORE_IMAGE_BRUSH_SVG("Starship/Common/Update", CoreStyleConstants::Icon16x16, FStyleColors::AccentYellow));
	}

}

void FMetaStoryStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaStoryStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FMetaStoryStyle& FMetaStoryStyle::Get()
{
	static FMetaStoryStyle Instance;
	return Instance;
}

const FSlateBrush* FMetaStoryStyle::GetBrushForSelectionBehaviorType(const EMetaStoryStateSelectionBehavior InSelectionBehavior, const bool bInHasChildren, const EMetaStoryStateType InStateType)
{
	if (InSelectionBehavior == EMetaStoryStateSelectionBehavior::None)
	{
		return Get().GetBrush("MetaStoryEditor.SelectNone");
	}

	if (InSelectionBehavior == EMetaStoryStateSelectionBehavior::TryEnterState)
	{
		return Get().GetBrush("MetaStoryEditor.TryEnterState");
	}

	if (InSelectionBehavior == EMetaStoryStateSelectionBehavior::TrySelectChildrenInOrder
		|| InSelectionBehavior == EMetaStoryStateSelectionBehavior::TrySelectChildrenWithHighestUtility
		|| InSelectionBehavior == EMetaStoryStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility)
	{
		if (!bInHasChildren
			|| InStateType == EMetaStoryStateType::Linked
			|| InStateType == EMetaStoryStateType::LinkedAsset)
		{
			return Get().GetBrush("MetaStoryEditor.TryEnterState");
		}

		return Get().GetBrush("MetaStoryEditor.TrySelectChildrenInOrder");

	}

	if (InSelectionBehavior == EMetaStoryStateSelectionBehavior::TrySelectChildrenAtRandom)
	{
		return Get().GetBrush("MetaStoryEditor.TrySelectChildrenAtRandom");
	}

	if (InSelectionBehavior == EMetaStoryStateSelectionBehavior::TryFollowTransitions)
	{
		return Get().GetBrush("MetaStoryEditor.TryFollowTransitions");
	}

	return nullptr;
}
