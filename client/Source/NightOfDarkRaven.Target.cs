using UnrealBuildTool;
using System.Collections.Generic;

public class NightOfDarkRavenTarget : TargetRules
{
	public NightOfDarkRavenTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NightOfDarkRaven");
	}
}
