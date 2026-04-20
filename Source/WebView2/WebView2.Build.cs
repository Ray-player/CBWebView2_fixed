using System.IO;
using UnrealBuildTool;

// WebView2 外部模块仅负责把官方 SDK 的 include / lib / loader DLL 接入 UE 构建系统。
public class WebView2 : ModuleRules
{
    protected readonly string ViewVersion = "Microsoft.Web.WebView2.1.0.2895-prerelease";

    // 如后续需要引入额外实现库，可在这里扩展版本定义。

    private string ProjectPath
    {
        get { return Path.Combine(PluginDirectory, "../../"); }
    }
    
    public WebView2(ReadOnlyTargetRules Target) : base(Target)
    {
        // 该模块不生成源码目标，只提供第三方库声明。
        Type = ModuleType.External;
        
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // 暴露官方 SDK 头文件和链接库。
            PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory,ViewVersion,"include"));
            string LibPath = Path.Combine(ModuleDirectory, ViewVersion, "lib");
            
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "WebView2Loader.dll.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "WebView2LoaderStatic.lib"));
        }
        
        // 保证编辑器和打包运行时都能找到 WebView2Loader.dll。
        string SrcWebView2LoaderDll = Path.Combine(ModuleDirectory, "Dependency","WebView2Loader.dll");
        string DstWebView2LoaderDll = Path.Combine(ProjectPath, "Binaries/Win64/WebView2Loader.dll");
        if (!File.Exists(DstWebView2LoaderDll))
        {
            if(!Directory.Exists( Path.Combine(ProjectPath, "Binaries/Win64/")))
                Directory.CreateDirectory(Path.Combine(ProjectPath, "Binaries/Win64/"));
            File.Copy(SrcWebView2LoaderDll,DstWebView2LoaderDll);
        }
        
        RuntimeDependencies.Add(DstWebView2LoaderDll);
    }
}