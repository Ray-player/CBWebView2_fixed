#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * WebView2Utils 是插件的原生集成层模块。
 *
 * 它负责：
 * 1. 加载 WebView2Loader.dll。
 * 2. 持有日志分类。
 * 3. 提供 WebView2 原生宿主能力。
 */
class WEBVIEW2UTILS_API FWebView2UtilsModule : public IModuleInterface
{
public:
	/** 启动时尝试加载 WebView2Loader.dll。 */
	virtual void StartupModule() override;
	/** 关闭时释放加载的 DLL 句柄。 */
	virtual void ShutdownModule() override;

private:
	/** 手动持有 Loader DLL，避免运行期符号缺失。 */
	void* DllHandle = nullptr;
};