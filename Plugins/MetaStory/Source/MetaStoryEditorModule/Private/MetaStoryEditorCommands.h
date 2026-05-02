// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"


/**
 * MetaStory editor command set.
 */
class FMetaStoryEditorCommands : public TCommands<FMetaStoryEditorCommands>
{
public:
	FMetaStoryEditorCommands();

	// TCommands<> overrides
	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> Compile;
	TSharedPtr<FUICommandInfo> SaveOnCompile_Never;
	TSharedPtr<FUICommandInfo> SaveOnCompile_SuccessOnly;
	TSharedPtr<FUICommandInfo> SaveOnCompile_Always;
	TSharedPtr<FUICommandInfo> LogCompilationResult;
	TSharedPtr<FUICommandInfo> LogDependencies;

	TSharedPtr<FUICommandInfo> AddSiblingState;
	TSharedPtr<FUICommandInfo> AddChildState;
	TSharedPtr<FUICommandInfo> RenameState;
	TSharedPtr<FUICommandInfo> CutStates;
	TSharedPtr<FUICommandInfo> CopyStates;
	TSharedPtr<FUICommandInfo> PasteStatesAsSiblings;
	TSharedPtr<FUICommandInfo> PasteStatesAsChildren;
	TSharedPtr<FUICommandInfo> PasteNodesToSelectedStates;
	TSharedPtr<FUICommandInfo> DuplicateStates;
	TSharedPtr<FUICommandInfo> DeleteStates;
	TSharedPtr<FUICommandInfo> EnableStates;

#if WITH_METASTORY_TRACE_DEBUGGER
	TSharedPtr<FUICommandInfo> EnableOnEnterStateBreakpoint;
	TSharedPtr<FUICommandInfo> EnableOnExitStateBreakpoint;
#endif // WITH_METASTORY_TRACE_DEBUGGER
};