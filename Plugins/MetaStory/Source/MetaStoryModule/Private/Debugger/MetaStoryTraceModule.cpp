// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_METASTORY_TRACE_DEBUGGER

#include "Debugger/MetaStoryTraceModule.h"
#include "Debugger/MetaStoryTraceProvider.h"
#include "Debugger/MetaStoryTraceAnalyzer.h"
#include "Debugger/MetaStoryDebugger.h"  // Required to compile TArray<UE::MetaStoryDebugger::FInstanceDescriptor> from MetaStoryTraceProvider
#include "TraceServices/Model/AnalysisSession.h"

FName FMetaStoryTraceModule::ModuleName("TraceModule_MetaStory");

void FMetaStoryTraceModule::GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = ModuleName;
	OutModuleInfo.DisplayName = TEXT("MetaStory");
}

void FMetaStoryTraceModule::OnAnalysisBegin(TraceServices::IAnalysisSession& InSession)
{
	const TSharedPtr<FMetaStoryTraceProvider> Provider = MakeShared<FMetaStoryTraceProvider>(InSession);
	InSession.AddProvider(FMetaStoryTraceProvider::ProviderName, Provider);
	InSession.AddAnalyzer(new FMetaStoryTraceAnalyzer(InSession, *Provider));
}

void FMetaStoryTraceModule::GetLoggers(TArray<const TCHAR *>& OutLoggers)
{
	OutLoggers.Add(TEXT("MetaStory"));
}

#endif // WITH_METASTORY_TRACE_DEBUGGER