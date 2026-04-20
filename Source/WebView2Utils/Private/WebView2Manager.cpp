#include "WebView2Manager.h"

#include "WebView2CompositionHost.h"
#include "WebView2Log.h"
#include "WebView2Settings.h"
#include "WebView2Window.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <DispatcherQueue.h>
#include "Windows/HideWindowsPlatformTypes.h"

FWebView2Manager& FWebView2Manager::Get()
{
	static FWebView2Manager Instance;
	return Instance;
}

void FWebView2Manager::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	// 只初始化一次，避免重复创建 DispatcherQueue。
	bInitialized = true;
	const UWebView2Settings* Settings = UWebView2Settings::Get();
	if (Settings && Settings->Mode == ECBWebView2Mode::VisualWinComp)
	{
		// WinComp 模式必须依赖 DispatcherQueue。
		EnsureDispatcherQueue();
	}
}

void FWebView2Manager::Shutdown()
{
	// 全量销毁宿主，确保编辑器退出或 PIE 切换时不残留原生资源。
	for (TPair<HWND, TWeakPtr<FWebView2CompositionHost>>& Pair : CompositionHosts)
	{
		if (const TSharedPtr<FWebView2CompositionHost> Host = Pair.Value.Pin())
		{
			Host->Destroy();
		}
	}

	CompositionHosts.Empty();
	DispatcherQueueController = nullptr;
	bInitialized = false;
}

TSharedPtr<FWebView2Window> FWebView2Manager::CreateWebView(
	HWND ParentWindow,
	const FGuid& InstanceId,
	const FString& InitialUrl,
	const FColor& BackgroundColor,
	bool bEnableTransparencyHitTest)
{
	Initialize();
	return MakeShared<FWebView2Window>(ParentWindow, InstanceId, InitialUrl, BackgroundColor, bEnableTransparencyHitTest);
}

TSharedPtr<FWebView2CompositionHost> FWebView2Manager::GetOrCreateCompositionHost(HWND WindowHandle)
{
	if (const TWeakPtr<FWebView2CompositionHost>* Existing = CompositionHosts.Find(WindowHandle))
	{
		if (const TSharedPtr<FWebView2CompositionHost> Host = Existing->Pin())
		{
			return Host;
		}
	}

	TSharedPtr<FWebView2CompositionHost> Host = MakeShared<FWebView2CompositionHost>(
		WindowHandle,
		DispatcherQueueController);
	Host->Initialize();
	CompositionHosts.Add(WindowHandle, Host);
	return Host;
}

bool FWebView2Manager::HandleWindowMessage(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam)
{
	// 只把消息路由给当前宿主 HWND 对应的 CompositionHost。
	bool bHandled = false;
	for (TPair<HWND, TWeakPtr<FWebView2CompositionHost>>& Pair : CompositionHosts)
	{
		if (Pair.Key != WindowHandle)
		{
			continue;
		}

		if (const TSharedPtr<FWebView2CompositionHost> Host = Pair.Value.Pin())
		{
			bHandled |= Host->HandleMouseMessage(Message, WParam, LParam);
		}
	}

	return bHandled;
}

void FWebView2Manager::OnHostWindowClosed(HWND WindowHandle)
{
	if (const TWeakPtr<FWebView2CompositionHost>* Existing = CompositionHosts.Find(WindowHandle))
	{
		if (const TSharedPtr<FWebView2CompositionHost> Host = Existing->Pin())
		{
			Host->Destroy();
		}
	}

	CompositionHosts.Remove(WindowHandle);
}

bool FWebView2Manager::EnsureDispatcherQueue()
{
	if (DispatcherQueueController != nullptr)
	{
		return true;
	}

	// CoreMessaging.dll 中的 CreateDispatcherQueueController 是运行时按需加载的。
	static decltype(::CreateDispatcherQueueController)* CreateDispatcherQueueControllerFunc = nullptr;
	if (!CreateDispatcherQueueControllerFunc)
	{
		if (HMODULE ModuleHandle = ::LoadLibraryEx(L"CoreMessaging.dll", nullptr, 0))
		{
			CreateDispatcherQueueControllerFunc =
				reinterpret_cast<decltype(::CreateDispatcherQueueController)*>(
					::GetProcAddress(ModuleHandle, "CreateDispatcherQueueController"));
		}
	}

	if (!CreateDispatcherQueueControllerFunc)
	{
		UE_LOG(LogWebView2Utils, Error, TEXT("无法创建 DispatcherQueueController，系统可能不支持 WinComp。"));
		return false;
	}

	winrt::Windows::System::DispatcherQueueController NewController{nullptr};
	DispatcherQueueOptions Options{
		sizeof(DispatcherQueueOptions),
		DQTYPE_THREAD_CURRENT,
		DQTAT_COM_STA};

	const HRESULT Hr = CreateDispatcherQueueControllerFunc(
		Options,
		reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(winrt::put_abi(NewController)));

	if (FAILED(Hr))
	{
		UE_LOG(LogWebView2Utils, Error, TEXT("创建 DispatcherQueueController 失败，HRESULT=0x%08x"), Hr);
		return false;
	}

	DispatcherQueueController = NewController;
	return true;
}