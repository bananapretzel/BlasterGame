// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BlasterEditorTarget : TargetRules
{
    public BlasterEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V4;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
        ExtraModuleNames.Add("Blaster");
    }
}