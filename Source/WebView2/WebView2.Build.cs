using System.IO;
using UnrealBuildTool;

public class WebView2 : ModuleRules
{
    protected readonly string ViewVersion = "Microsoft.Web.WebView2.1.0.2895-prerelease";

  // protected readonly string ImpVersion = "Microsoft.Windows.ImplementationLibrary.1.0.220201.1";

    private string ProjectPath
    {
        get { return Path.Combine(PluginDirectory, "../../"); }
    }
    
    public WebView2(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory,ViewVersion,"include"));
            string LibPath = Path.Combine(ModuleDirectory, ViewVersion, "lib");
            
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "WebView2Loader.dll.lib"));
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "WebView2LoaderStatic.lib"));
        }
        
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