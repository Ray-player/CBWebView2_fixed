using UnrealBuildTool;

// CBWebView2 是插件对外暴露的 UMG / Slate 模块。
// 这里仅声明依赖关系，不承载原生 WebView2 集成细节。
public class CBWebView2 : ModuleRules
{
	public CBWebView2(ReadOnlyTargetRules Target) : base(Target)
	{
		// 使用显式或共享 PCH，保持与 UE 常规模块一致。
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 对外暴露给本模块头文件使用的依赖。
		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"UMG",
				"WebView2Utils"
			});

		// 仅在模块内部实现中使用的依赖。
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"ApplicationCore"
			});
	}
}