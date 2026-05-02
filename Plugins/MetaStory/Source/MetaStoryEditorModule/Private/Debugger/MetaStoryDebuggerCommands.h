// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_METASTORY_TRACE_DEBUGGER

#include "Framework/Commands/Commands.h"

/**
 * MetaStory Debugger command set.
 */
class FMetaStoryDebuggerCommands : public TCommands<FMetaStoryDebuggerCommands>
{
public:
	FMetaStoryDebuggerCommands();

	//~ TCommands<> overrides
	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> StartRecording;
	TSharedPtr<FUICommandInfo> StopRecording;
	TSharedPtr<FUICommandInfo> PreviousFrameWithStateChange;
	TSharedPtr<FUICommandInfo> PreviousFrameWithEvents;
	TSharedPtr<FUICommandInfo> NextFrameWithEvents;
	TSharedPtr<FUICommandInfo> NextFrameWithStateChange;
	TSharedPtr<FUICommandInfo> ResumeDebuggerAnalysis;
	TSharedPtr<FUICommandInfo> ResetTracks;
	TSharedPtr<FUICommandInfo> OpenRewindDebugger;
};

#endif // WITH_METASTORY_TRACE_DEBUGGER