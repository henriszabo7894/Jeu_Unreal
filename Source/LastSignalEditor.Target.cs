using UnrealBuildTool;

public class LastSignalEditorTarget : TargetRules
{
	public LastSignalEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("LastSignal");
	}
}
