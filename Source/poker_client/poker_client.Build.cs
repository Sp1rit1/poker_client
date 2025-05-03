// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class poker_client : ModuleRules
{
	public poker_client(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput", // Если используешь Enhanced Input
            "UMG",           // Для User Widgets
            "HTTP",          // Для HTTP запросов
            "Json",          // Для работы с JSON
            "JsonUtilities", // Для JSON утилит
            "Slate",         // Для доступа к SWindow и FSlateApplication
            "SlateCore"      // Базовые элементы Slate
        });

        PrivateDependencyModuleNames.AddRange(new string[] { 
            "Slate", 
            "SlateCore",
            "MoviePlayer",
        });

		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
