// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Misc/NotNull.h"

#ifndef UE_WITH_STATETREE_CRASHREPORTER
	#define UE_WITH_STATETREE_CRASHREPORTER WITH_ADDITIONAL_CRASH_CONTEXTS
#endif

class UObject;
class UMetaStory;

#if UE_WITH_STATETREE_CRASHREPORTER

namespace UE::MetaStory
{

/**
 * Additional crash context for MetaStory.
 * Will add extra information on the asset when crashing inside a MetaStory callstack.
 */
struct FCrashReporterHandler
{
	static void Register();
	static void Unregister();
};

/** Helper object allowing easy tracking of MetaStory code in crash reporter. */
class FCrashReporterScope
{
public:
	explicit FCrashReporterScope(TNotNull<const UObject*> Owner, TNotNull<const UMetaStory*> MetaStory, FName Context);
	~FCrashReporterScope();

	FCrashReporterScope() = delete;
	FCrashReporterScope(const FCrashReporterScope&) = delete;
	FCrashReporterScope& operator=(const FCrashReporterScope&) = delete;

private:
	bool bWasEnabled = false;
	uint32 ID = 0;
};

} //namespace

#endif //UE_WITH_STATETREE_CRASHREPORTER

#if UE_WITH_STATETREE_CRASHREPORTER
#define UE_STATETREE_CRASH_REPORTER_SCOPE(InOwner, InStateTree, InContext) ::UE::MetaStory::FCrashReporterScope ANONYMOUS_VARIABLE(AddCrashContext) {(InOwner), (InStateTree), (InContext)}
#else //UE_WITH_STATETREE_CRASHREPORTER
#define UE_STATETREE_CRASH_REPORTER_SCOPE(InOwner, InStateTree, InContext)
#endif //UE_WITH_STATETREE_CRASHREPORTER
