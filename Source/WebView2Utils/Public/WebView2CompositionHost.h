#pragma once

#include "CoreMinimal.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

THIRD_PARTY_INCLUDES_START
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.h>
#include <windows.ui.composition.interop.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

class FWebView2Window;

/**
 * WinComp 组合宿主。
 *
 * 当 WebView 以无窗口模式运行时，WebView 的 Visual 会挂到这里维护的可视树上。
 */
class WEBVIEW2UTILS_API FWebView2CompositionHost
{
public:
	FWebView2CompositionHost(
		HWND InWindowHandle,
		winrt::Windows::System::DispatcherQueueController InDispatcherQueueController);
	~FWebView2CompositionHost();

	/** 初始化桌面目标与根可视树。 */
	void Initialize();
	/** 把一个 WebView 挂到当前宿主的可视树中。 */
	void AttachWebView(const TSharedRef<FWebView2Window>& WebViewWindow);
	/** 按 LayerId 重新排序所有 WebView Visual。 */
	void RefreshVisualOrder();
	/** 从可视树中移除指定 WebView。 */
	void DetachWebView(const FGuid& InstanceId, const winrt::Windows::UI::Composition::ContainerVisual& WebViewContainer);
	/** 销毁所有可视对象。 */
	void Destroy();

	/** 处理宿主窗口收到的鼠标消息。 */
	bool HandleMouseMessage(UINT Message, WPARAM WParam, LPARAM LParam);
	HWND GetWindowHandle() const;

private:
	/** 创建 DesktopWindowTarget。 */
	void CreateDesktopTarget();
	/** 创建根可视树和 WebView 图层。 */
	void CreateRootVisual();
	/** 查找命中指定点的 WebView，可选要求其当前可交互。 */
	TArray<TSharedRef<FWebView2Window>> FindWebViewsAtPoint(const POINT& ClientPoint, bool bRequireHitTestEnabled) const;

	HWND WindowHandle = nullptr;
	int32 NextLayerId = 0;

	/** 以实例 Id 为键缓存当前宿主下的全部 WebView。 */
	TMap<FString, TWeakPtr<FWebView2Window>> WebViews;

	winrt::Windows::System::DispatcherQueueController DispatcherQueueController{nullptr};
	winrt::Windows::UI::Composition::Compositor Compositor{nullptr};
	winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget WindowTarget{nullptr};
	winrt::Windows::UI::Composition::ContainerVisual RootVisual{nullptr};
	winrt::Windows::UI::Composition::ContainerVisual WebViewLayer{nullptr};
};