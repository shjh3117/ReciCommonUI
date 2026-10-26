// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

using System.IO;
using UnrealBuildTool;

public class ReciCommonUIWidgets : ModuleRules
{
	public ReciCommonUIWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"CommonUI",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"ReciCommonUI",
				"Slate",
				"SlateCore",
				"UMG"
			}
		);

		// Only the tests include the core's private headers: its test harness (Private/Tests), which the core exports
		// for them. The widgets themselves use the core's public API alone (DESIGN.md, principle 15).
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ReciCommonUI", "Private"));
		// The demo-page tests load CommonUI's Input Data, as CommonUI's input subsystem does when a local player starts.
		PrivateDependencyModuleNames.Add("CommonInput");
	}
}
