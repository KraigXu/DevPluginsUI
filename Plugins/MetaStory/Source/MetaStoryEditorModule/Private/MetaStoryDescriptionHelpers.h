// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class UMetaStoryEditorData;
struct FMetaStoryStateLink;
struct FMetaStoryTransition;
enum class EMetaStoryNodeFormatting : uint8;
struct FSlateBrush;
struct FSlateColor;
class FText;

namespace UE::MetaStory::Editor
{

FText GetStateLinkDesc(const UMetaStoryEditorData* EditorData, const FMetaStoryStateLink& Link, EMetaStoryNodeFormatting Formatting, bool bShowStatePath = false);
const FSlateBrush* GetStateLinkIcon(const UMetaStoryEditorData* EditorData, const FMetaStoryStateLink& Link);
FSlateColor GetStateLinkColor(const UMetaStoryEditorData* EditorData, const FMetaStoryStateLink& Link);

FText GetTransitionDesc(const UMetaStoryEditorData* EditorData, const FMetaStoryTransition& Transition, EMetaStoryNodeFormatting Formatting, bool bShowStatePath = false);

} // UE::MetaStory::Editor