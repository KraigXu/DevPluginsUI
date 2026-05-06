// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MetaStoryModule : ModuleRules
	{
		public MetaStoryModule(ReadOnlyTargetRules Target) : base(Target)
		{
			CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Warning;

			bAllowUETypesInNamespaces = true;

			PublicDebugVisualizerPaths.Add(Path.Combine(ModuleDirectory, "MetaStory.natvis"));

			PublicIncludePaths.AddRange(
				new string[] {
				}
			);

			PublicDependencyModuleNames.AddRange(
				new [] {
					"Core",
					"CoreUObject",
					"DeveloperSettings",
					"Engine",
					"AIModule",
					"GameplayTags",
					"PropertyBindingUtils",
					"StructUtils"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new [] {
					"PropertyPath",
				}
			);

			CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error;

			if (Target.bBuildEditor)
			{
				PublicDependencyModuleNames.AddRange(
					new [] {
						"UnrealEd",
						"BlueprintGraph",
					}
				);
				PrivateDependencyModuleNames.AddRange(
					new [] {
						"StructUtilsEditor",
						"EditorSubsystem",
						"EditorFramework"
					}
				);
			}

			// Allow debugger traces on all non-shipping targets and shipping editors (UEFN)
			if (Target.Configuration != UnrealTargetConfiguration.Shipping || Target.bBuildEditor)
			{
				PublicDefinitions.Add("WITH_METASTORY_TRACE=1");
				PublicDependencyModuleNames.AddRange(
					new []
					{
						"TraceLog"
					}
				);
				
				// Allow debugger trace analysis on desktop platforms
				if (Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop))
				{
					PublicDefinitions.Add("WITH_METASTORY_TRACE_DEBUGGER=1");
					PublicDependencyModuleNames.AddRange(
						new []
						{
							"TraceServices",
							"TraceAnalysis"
						}
					);
				}
				else
				{
					PublicDefinitions.Add("WITH_METASTORY_TRACE_DEBUGGER=0");
				}
			}
			else
			{
				PublicDefinitions.Add("WITH_METASTORY_TRACE=0");
				PublicDefinitions.Add("WITH_METASTORY_TRACE_DEBUGGER=0");
			}
		}
	}
}
