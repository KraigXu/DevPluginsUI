// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryCompiler.h"

#define UE_API METASTORYEDITORMODULE_API

namespace UE::MetaStory::Compiler
{

/**
 * 
 */
class FCompilerManager final
{
public:
	static UE_API void Startup();
	static UE_API void Shutdown();

	static UE_API bool CompileSynchronously(TNotNull<UMetaStory*> MetaStory);
	static UE_API bool CompileSynchronously(TNotNull<UMetaStory*> MetaStory, FMetaStoryCompilerLog& Log);
	
private:
	FCompilerManager() = delete;
	FCompilerManager(const FCompilerManager&) = delete;
	FCompilerManager& operator= (const FCompilerManager&) = delete;
};

} // UE::MetaStory::Compiler

#undef UE_API
