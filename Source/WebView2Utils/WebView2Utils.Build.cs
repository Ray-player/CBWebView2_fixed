using System.IO;
using UnrealBuildTool;

public class WebView2Utils : ModuleRules
{
    public WebView2Utils(ReadOnlyTargetRules Target) : base(Target)
    {
        
        PublicDefinitions.Add("USING_COROUTINES=1");
        bEnableExceptions = true;
        bUseUnity = false;
        CppStandard = CppStandardVersion.Cpp20;
        
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicSystemLibraries.AddRange(new string[] { "Dcomp.lib",});
        
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "WebView2",
                "DeveloperSettings",
                "UMG"
            }
        );
        

        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "UnrealEd",
                }
            );
        }
        
        PublicDefinitions.Add("WINVER=0x0A00");
        PublicDefinitions.Add("_WIN32_WINNT=0x0A00");
        
        PublicIncludePaths.Add(Path.Combine(Target.WindowsPlatform.WindowsSdkDir,        
            "Include", 
            Target.WindowsPlatform.WindowsSdkVersion, 
            "cppwinrt"));
            
            
        PublicIncludePaths.Add(Path.Combine(Target.WindowsPlatform.WindowsSdkDir,        
            "Include", 
            Target.WindowsPlatform.WindowsSdkVersion, 
            "winrt"));
        
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore"
            }
        );
        
    }
}