using UnrealBuildTool;

public class MetaplotEditor : ModuleRules
{
	public MetaplotEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"AssetTools",
			"PropertyEditor",
			"EditorStyle",
			"ToolMenus",
			"Metaplot"
		});
	}
}
