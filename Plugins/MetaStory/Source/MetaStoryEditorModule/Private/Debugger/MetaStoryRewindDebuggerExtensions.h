// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_METASTORY_TRACE_DEBUGGER
#include "IRewindDebuggerExtension.h"

class IRewindDebugger;

namespace UE::MetaStoryDebugger
{

class FRewindDebuggerPlaybackExtension final : public IRewindDebuggerExtension
{
	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) override;
	virtual void RecordingStarted(IRewindDebugger* RewindDebugger) override;
	virtual void RecordingStopped(IRewindDebugger* RewindDebugger) override;
	virtual void Clear(IRewindDebugger* RewindDebugger) override;

	/** Last scrub time we received. Used to avoid redundant updates. */
	double LastScrubTime = 0.0;
};

} // UE::MetaStoryDebugger

#endif // WITH_METASTORY_TRACE_DEBUGGER
