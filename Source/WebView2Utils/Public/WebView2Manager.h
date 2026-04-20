#pragma once

#include "CoreMinimal.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

THIRD_PARTY_INCLUDES_START
#include <winrt/Windows.System.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

class FWebView2CompositionHost;
class FWebView2Window;

/**
 * WebView2 全局管理器。
 *
 * 负责：
 * 1. 共享 DispatcherQueue。
 * 2. 为不同宿主 HWND 创建和缓存 CompositionHost。
 * 3. 转发窗口消息给对应的 WebView 宿主。
 */
class WEBVIEW2UTILS_API FWebView2Manager
{
public:
	static FWebView2Manager& Get();

	/** 初始化全局 DispatcherQueue 与内部状态。 */
	void Initialize();
	/** 关闭全部宿主并释放 WinComp 资源。 */
	void Shutdown();

	/** 为指定宿主窗口创建一个新的 WebView 实例。 */
	TSharedPtr<FWebView2Window> CreateWebView(
		HWND ParentWindow,
		const FGuid& InstanceId,
		const FString& InitialUrl,
		const FColor& BackgroundColor,
		bool bEnableTransparencyHitTest);

	/** 获取或创建宿主窗口对应的 WinComp 组合宿主。 */
	TSharedPtr<FWebView2CompositionHost> GetOrCreateCompositionHost(HWND WindowHandle);

	/** 把 Win32 消息转发给对应宿主窗口下的 WebView 组合层。 */
	bool HandleWindowMessage(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);
	/** 当宿主窗口关闭时，清理挂载其上的所有 WebView 资源。 */
	void OnHostWindowClosed(HWND WindowHandle);

	/** 返回共享 DispatcherQueueController，供 WinComp 模式使用。 */
	winrt::Windows::System::DispatcherQueueController GetDispatcherQueueController() const
	{
		return DispatcherQueueController;
	}

private:
	FWebView2Manager() = default;

	/** 惰性创建当前线程的 DispatcherQueue。 */
	bool EnsureDispatcherQueue();

	/** 一个宿主 HWND 对应一个组合宿主。 */
	TMap<HWND, TWeakPtr<FWebView2CompositionHost>> CompositionHosts;
	winrt::Windows::System::DispatcherQueueController DispatcherQueueController{nullptr};
	bool bInitialized = false;
};