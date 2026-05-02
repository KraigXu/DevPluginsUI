// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_METASTORY_TRACE
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"

namespace UE::MetaStoryDebugger
{

class FRewindDebuggerRecordingExtension final : public IRewindDebuggerRuntimeExtension
{
	virtual void RecordingStarted() override;
	virtual void RecordingStopped() override;
	virtual void Clear() override;
};

} // UE::MetaStoryDebugger

#endif // WITH_METASTORY_TRACE