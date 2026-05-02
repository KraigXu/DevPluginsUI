// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

struct FMetaStoryEditorNode;
enum class EMetaStoryConditionEvaluationMode : uint8;

class SWidget;
class IPropertyHandle;
class IDetailLayoutBuilder;
class FDetailWidgetRow;
class FMenuBuilder;
class FMetaStoryViewModel;
class UMetaStoryEditorData;

namespace UE::MetaStoryEditor::DebuggerExtensions
{

TSharedRef<SWidget> CreateStateWidget(TSharedPtr<IPropertyHandle> StateEnabledProperty, const TSharedPtr<FMetaStoryViewModel>& InStateTreeViewModel);
void AppendStateMenuItems(FMenuBuilder& InMenuBuilder, TSharedPtr<IPropertyHandle> StateEnabledProperty, const TSharedPtr<FMetaStoryViewModel>& InStateTreeViewModel);

TSharedRef<SWidget> CreateEditorNodeWidget(const TSharedPtr<IPropertyHandle>& StructPropertyHandle, const TSharedPtr<FMetaStoryViewModel>& InStateTreeViewModel);
void AppendEditorNodeMenuItems(FMenuBuilder& InMenuBuilder, const TSharedPtr<IPropertyHandle>& StructPropertyHandle, const TSharedPtr<FMetaStoryViewModel>& InStateTreeViewModel);
bool IsEditorNodeEnabled(const TSharedPtr<IPropertyHandle>& StructPropertyHandle);

TSharedRef<SWidget> CreateTransitionWidget(const TSharedPtr<IPropertyHandle>& StructPropertyHandle, const TSharedPtr<FMetaStoryViewModel>& InStateTreeViewModel);
void AppendTransitionMenuItems(FMenuBuilder& InMenuBuilder, const TSharedPtr<IPropertyHandle>& StructPropertyHandle, const TSharedPtr<FMetaStoryViewModel>& InStateTreeViewModel);
bool IsTransitionEnabled(const TSharedPtr<IPropertyHandle>& StructPropertyHandle);

}; // UE::MetaStoryEditor::DebuggerExtensions
