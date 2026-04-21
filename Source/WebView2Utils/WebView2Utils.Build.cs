using System.IO;
using UnrealBuildTool;

// WebView2Utils 是插件的原生集成层，负责 Win32 / WinComp / WebView2 Runtime 相关能力。
public class WebView2Utils : ModuleRules
{
	public WebView2Utils(ReadOnlyTargetRules Target) : base(Target)
	{
		// 这里使用 C++20 和显式 PCH，便于处理 WinRT 与 WebView2 头文件。
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		bEnableExceptions = true;
		bUseUnity = false;
#if UE_5_7_OR_LATER
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
#else
		bEnableUndefinedIdentifierWarnings = false;
#endif

		PublicDefinitions.Add("USING_COROUTINES=1");
		PublicDefinitions.Add("WINVER=0x0A00");
		PublicDefinitions.Add("_WIN32_WINNT=0x0A00");

		// 公开依赖给外层模块和头文件使用。
		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"UMG",
				"Core",
				"WebView2",
				"DeveloperSettings"
			});

		// 私有依赖只在当前模块实现文件内使用。
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"ApplicationCore",
				"Projects"
			});

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// DComp 用于部分宿主模式；透明命中脚本需要随包一起分发。
			PublicSystemLibraries.Add("Dcomp.lib");
			RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Extras", "transparency_check.js"), StagedFileType.NonUFS);

			// WinRT 头文件来自系统 SDK。
			PublicIncludePaths.Add(Path.Combine(
				Target.WindowsPlatform.WindowsSdkDir,
				"Include",
				Target.WindowsPlatform.WindowsSdkVersion,
				"cppwinrt"));

			PublicIncludePaths.Add(Path.Combine(
				Target.WindowsPlatform.WindowsSdkDir,
				"Include",
				Target.WindowsPlatform.WindowsSdkVersion,
				"winrt"));
		}
	}
}