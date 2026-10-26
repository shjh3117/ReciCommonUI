// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

using UnrealBuildTool;

public class ReciCommonUI : ModuleRules
{
	public ReciCommonUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"CommonInput",
				"CommonUI",
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"EnhancedInput",
				"InputCore",
				"Slate",
				"SlateCore",
				"UMG"
			}
		);
	}
}
