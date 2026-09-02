using UnrealBuildTool;

public class ZFlip : ModuleRules
{
    public ZFlip(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore"
            }
        );

        // 插件目录 "GravityShift" 内的运行时模块,项目需要直接引用其类型
        PublicDependencyModuleNames.Add("GravityShift");
    }
}
