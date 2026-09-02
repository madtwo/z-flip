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
    }
}
