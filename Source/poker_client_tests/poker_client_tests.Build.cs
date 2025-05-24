using UnrealBuildTool;

public class poker_client_tests : ModuleRules
{
    public poker_client_tests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {}
        );

        PrivateIncludePaths.AddRange(
            new string[] {}
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "poker_client" 
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "CoreUObject", 
                "Engine",      
                "UnrealEd",    
                "AutomationController", 
                "AutomationWindow"    
            }
        );

        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {}
            );
        }
    }
}