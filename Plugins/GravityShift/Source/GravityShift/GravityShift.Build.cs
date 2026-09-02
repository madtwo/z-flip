using UnrealBuildTool;

public class GravityShift : ModuleRules
{
    public GravityShift(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "PhysicsCore"
            }
        );
    }
}
