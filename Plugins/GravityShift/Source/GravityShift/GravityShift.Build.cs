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
                "PhysicsCore",
                // 碎裂:几何体集合是引擎自带 runtime 模块,不开插件也能跑。
                "GeometryCollectionEngine",
                // 主菜单 / 设置面板是纯 C++ 建的 UMG,没有蓝图资产。
                "UMG",
                "Slate",
                "SlateCore"
            }
        );
    }
}
