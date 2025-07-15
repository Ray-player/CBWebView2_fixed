// Copyright 2025-Present Xiao Lan fei. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#pragma warning(disable : 4668 4005)


#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#include "Stdafx.h"

#include <dcomp.h>
#include <winrt/Windows.UI.Composition.Desktop.h>


#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "WebView2ComponentBase.h"

class FWebView2Window;
namespace winrtComp = winrt::Windows::UI::Composition;

/**
 * todo 现阶段用不到，webview功能太多，做拆分使用
 */
class  FWebView2ViewComponent:public FWebView2ComponentBase
{
public:
	FWebView2ViewComponent(FWebView2Window* WebView2Window,IDCompositionDevice* DcompDevice,
		winrtComp::Compositor WincompCompositor, bool bDCompTargetMode);
	~FWebView2ViewComponent() override;


private:

	Microsoft::WRL::ComPtr<IDCompositionDevice> DcompDevice;
	Microsoft::WRL::ComPtr<IDCompositionTarget> DcompHwndTarget;
	Microsoft::WRL::ComPtr<IDCompositionVisual> DcompRootVisual;
	Microsoft::WRL::ComPtr<IDCompositionVisual> DcompWebViewVisual;
	
	/**主窗口存放的差插槽*/
	winrtComp::ContainerVisual WincompWebViewVisual{nullptr};

	/**主窗口*/
	HWND MainHwnd;
};
