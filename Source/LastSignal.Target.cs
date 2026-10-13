using UnrealBuildTool;

public class LastSignalTarget : TargetRules
{
	public LastSignalTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LastSignal");
	}
}
