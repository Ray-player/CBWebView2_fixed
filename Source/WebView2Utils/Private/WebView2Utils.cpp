#include "WebView2Utils.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"

#include "WebView2Log.h"

DEFINE_LOG_CATEGORY(LogWebView2Utils);

void FWebView2UtilsModule::StartupModule()
{
	// WebView2Loader.dll 路径固定跟随插件目录，方便编辑器与打包运行时共用。
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CBWebView2"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogWebView2Utils, Error, TEXT("无法定位 CBWebView2 插件目录。"));
		return;
	}

	const FString LoaderPath = FPaths::Combine(
		Plugin->GetBaseDir(),
		TEXT("Source/WebView2/Dependency/WebView2Loader.dll"));

	// 这里显式加载 DLL，避免依赖系统 PATH 或项目外部环境。
	DllHandle = FPlatformProcess::GetDllHandle(*LoaderPath);
	if (!DllHandle)
	{
		const uint32 ErrorCode = FPlatformMisc::GetLastError();
		TCHAR ErrorBuffer[1024] = {};
		FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, UE_ARRAY_COUNT(ErrorBuffer), ErrorCode);
		UE_LOG(
			LogWebView2Utils,
			Error,
			TEXT("加载 WebView2Loader.dll 失败: %s, 错误码: %u, 系统消息: %s"),
			*LoaderPath,
			ErrorCode,
			ErrorBuffer);
	}
	else
	{
		UE_LOG(LogWebView2Utils, Log, TEXT("已加载 WebView2Loader.dll: %s"), *LoaderPath);
	}
}

void FWebView2UtilsModule::ShutdownModule()
{
	// 模块卸载时释放句柄，避免编辑器热重载/关闭过程中遗留引用。
	if (DllHandle)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
		DllHandle = nullptr;
	}
}

IMPLEMENT_MODULE(FWebView2UtilsModule, WebView2Utils)