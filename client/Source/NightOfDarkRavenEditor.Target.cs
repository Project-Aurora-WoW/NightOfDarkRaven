using UnrealBuildTool;
using System.Collections.Generic;

public class NightOfDarkRavenEditorTarget : TargetRules
{
	public NightOfDarkRavenEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NightOfDarkRaven");
	}
}
