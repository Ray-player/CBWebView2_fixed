#pragma once

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#include <Stdafx.h>

#include <winrt/Windows.UI.Composition.Desktop.h>
#include <windows.ui.composition.interop.h>
#include <winrt/Windows.UI.Composition.h>
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

class FWebView2Window;

class  WEBVIEW2UTILS_API FWebView2CompositionHost
{
public:

	FWebView2CompositionHost(HWND Hwnd, winrt::Windows::System::DispatcherQueueController QueueController);
	~FWebView2CompositionHost();
	
	void Initialize();
	void CreateWebViewVisual(TSharedRef<FWebView2Window> WebView2Window);
	void RefreshWebViewVisual();

	//删除所有网页
	void DestroyWinCompVisualTree();
	void DestroyVisualAsWebview(TSharedRef<FWebView2Window> WebView2Window);
	HWND GetMainWindowHandle() const;


	bool MouseMessage(UINT Message, WPARAM WParam, LPARAM LParam);

	/*
	* 寻找当前鼠标位置的网页控件
	*/
	TArray<TSharedRef<FWebView2Window>> FindWebviewFromPoint(POINT Point);

public:
	TMap<FString,TSharedRef<FWebView2Window>> WebViewWindowMap;
protected:

	void CreateDesktopWindowTarget();
	//创建组合根
	void CreateCompositionRoot();
	
	
private:
	winrt::Windows::UI::Composition::Compositor MCompositor{nullptr};
	winrt::Windows::System::DispatcherQueueController MDispatcherQueueController{nullptr};
	winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget MTarget{nullptr};
	winrt::Windows::UI::Composition::ContainerVisual MRootVisual{nullptr};
	//UI存放的插槽
	winrt::Windows::UI::Composition::ContainerVisual UIVisual{nullptr};
	

	HWND MainHwnd;

	int32 ClickLayerID;
};
