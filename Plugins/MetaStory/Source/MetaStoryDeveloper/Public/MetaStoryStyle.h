// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

#define UE_API METASTORYDEVELOPER_API

enum class EMetaStoryStateSelectionBehavior : uint8;
enum class EMetaStoryStateType : uint8;

class ISlateStyle;

class FMetaStoryStyle : public FSlateStyleSet
{
public:
	static UE_API FMetaStoryStyle& Get();

	static UE_API const FSlateBrush* GetBrushForSelectionBehaviorType(EMetaStoryStateSelectionBehavior InSelectionBehavior, bool bInHasChildren, EMetaStoryStateType InStateType);

protected:
	struct FContentRootScope
	{
		FContentRootScope(FSlateStyleSet* InStyle, const FString& NewContentRoot)
			: Style(InStyle)
			, PreviousContentRoot(InStyle->GetContentRootDir())
		{
			Style->SetContentRoot(NewContentRoot);
		}

		~FContentRootScope()
		{
			Style->SetContentRoot(PreviousContentRoot);
		}
	private:
		FSlateStyleSet* Style;
		FString PreviousContentRoot;
	};

	friend class FMetaStoryDeveloperModule;

	UE_API explicit FMetaStoryStyle(const FName& InStyleSetName);

	UE_API static void Register();
	UE_API static void Unregister();

	UE_API static const FString EngineSlateContentDir;
	UE_API static const FString MetaStoryPluginContentDir;
	UE_API static const FLazyName StateTitleTextStyleName;
private:
	UE_API FMetaStoryStyle();
};

#undef UE_API
