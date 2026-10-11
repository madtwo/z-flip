using UnrealBuildTool;
using System.Collections.Generic;

public class ZFlipTarget : TargetRules
{
    public ZFlipTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("ZFlip");

        // ❌ 别在这里加 bUseLoggingInShipping = true —— UBT 会直接拒绝:
        //   "ZFlip modifies the values of properties: [ bUseLoggingInShipping: True != False ].
        //    This is not allowed, as ZFlip has build products in common with UnrealGame."
        // (该开关会改全局编译定义 USE_LOGGING_IN_SHIPPING,而引擎 Shipping 的中间产物是
        // 和 UnrealGame 共用的,改了就得连引擎一起重编。)想真开着 Shipping 日志只有两条路:
        //   ① TargetBuildEnvironment.Unique(整个引擎 Shipping 重编一次,几十上百个 action)
        //   ② 诊断期改打包配置为 Development —— 日志/控制台全在,本项目源码里没有任何
        //      UE_BUILD_SHIPPING 分支,所以行为差异对排查无影响。**排查就用这条。**
    }
}
