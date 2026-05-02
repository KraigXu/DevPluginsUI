// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryStyle.h"

#define UE_API METASTORYEDITORMODULE_API

enum class EMetaStoryStateSelectionBehavior : uint8;
enum class EMetaStoryStateType : uint8;

class ISlateStyle;

class FMetaStoryEditorStyle : public FMetaStoryStyle
{
public:
	static UE_API FMetaStoryEditorStyle& Get();

	static UE_API const FSlateBrush* GetBrushForSelectionBehaviorType(EMetaStoryStateSelectionBehavior InSelectionBehavior, bool bInHasChildren, EMetaStoryStateType InStateType);

protected:
	friend class FMetaStoryEditorModule;

	static UE_API void Register();
	static UE_API void Unregister();

private:
	UE_API FMetaStoryEditorStyle();
};

#undef UE_API
