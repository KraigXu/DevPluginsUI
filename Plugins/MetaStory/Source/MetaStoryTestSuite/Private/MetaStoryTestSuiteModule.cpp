// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryTestSuiteModule.h"

#define LOCTEXT_NAMESPACE "MetaStoryTestSuite"

class FMetaStoryTestSuiteModule : public IMetaStoryTestSuiteModule
{
};

IMPLEMENT_MODULE(FMetaStoryTestSuiteModule, MetaStoryTestSuite)

#undef LOCTEXT_NAMESPACE
