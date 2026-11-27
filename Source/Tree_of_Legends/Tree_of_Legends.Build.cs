// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class Tree_of_Legends : ModuleRules
{
	public Tree_of_Legends(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// The cutting .cpp files share helper names (WeldKey, GetTriangle, FMeshEdge) in their anonymous
		// namespaces; unity builds merge them into one translation unit and fail to compile.
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings" });

		PrivateDependencyModuleNames.AddRange(new string[] { "ProceduralMeshComponent", "EnhancedInput", "AssetRegistry", "PhysicsCore", "UMG", "Slate", "SlateCore", "MoviePlayer", "RenderCore", "RHI" });
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
