#include "WebView2Manager.h"



#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#include <DispatcherQueue.h>

#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"
#include "WebView2Settings.h"
#include "WebView2CompositionHost.h"
#include "WebView2Log.h"
FWebView2Manager* FWebView2Manager::WebView2ManagerInstance = nullptr;

namespace winSystem = winrt::Windows::System;

FWebView2Manager::FWebView2Manager()
	
{
	
}

FWebView2Manager* FWebView2Manager::GetInstance()
{
	if(!WebView2ManagerInstance)
	{
		WebView2ManagerInstance = new FWebView2Manager;
		WebView2ManagerInstance->Init();
	}
	return WebView2ManagerInstance;
}

void FWebView2Manager::Init()
{
	UWebView2Settings* Settings=UWebView2Settings::Get();

	if(Settings->WebView2Mode ==EWebView2Mode::VISUAL_WINCOMP)
	{
		
		HRESULT Hresult = S_OK;

		if (DispatcherQueueController == nullptr)
		{
			Hresult = E_FAIL;
			static decltype(::CreateDispatcherQueueController)* fnCreateDispatcherQueueController =
				nullptr;
			if (fnCreateDispatcherQueueController == nullptr)
			{
				HMODULE hmod = ::LoadLibraryEx(L"CoreMessaging.dll", nullptr, 0);
				if (hmod != nullptr)
				{
					fnCreateDispatcherQueueController =
						reinterpret_cast<decltype(::CreateDispatcherQueueController)*>(
							::GetProcAddress(hmod, "CreateDispatcherQueueController"));
				}
			}
			if (fnCreateDispatcherQueueController != nullptr)
			{
				winSystem::DispatcherQueueController controller{nullptr};
				DispatcherQueueOptions options{
					sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT, DQTAT_COM_STA};
				Hresult = fnCreateDispatcherQueueController(
					options, reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(
								 winrt::put_abi(controller)));
				DispatcherQueueController = controller;
			}
		}

		if (!SUCCEEDED(Hresult))
		{
			UE_LOG(LogWebView2,Error,TEXT("Create with Windowless WinComp Visual Failed:"
								 "WinComp compositor creation failed."
								 "Current OS may not support WinComp."))
			return;
		}
	}
	
}

TSharedPtr<FWebView2Window> FWebView2Manager::CreateWebview(HWND InHwnd,const FGuid UniqueID, const FString URL,FColor InBackgroundColor)
{
	TSharedPtr<FWebView2Window> WebView2Window = MakeShared<FWebView2Window>(InHwnd,UniqueID,URL,InBackgroundColor);
	return WebView2Window;
}

TSharedPtr<FWebView2CompositionHost> FWebView2Manager::GetMessageProcess(HWND Hwnd)
{
	if(WebView2CompositionHostMap.Contains(Hwnd))
	{
		if( WebView2CompositionHostMap.Find(Hwnd)->Pin())
		{
			return WebView2CompositionHostMap.Find(Hwnd)->Pin();
		}
	}

	TSharedPtr<FWebView2CompositionHost> WebView2CompositionHost=MakeShared<FWebView2CompositionHost>(Hwnd,DispatcherQueueController);
	WebView2CompositionHost->Initialize();
	WebView2CompositionHostMap.Add(Hwnd,WebView2CompositionHost);
	return WebView2CompositionHost;
}

bool FWebView2Manager::HandleWindowMessage(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
	if (WebView2CompositionHostMap.Num() == 0)
	{
		return false;
	}
		
	for(TPair<HWND,TWeakPtr<FWebView2CompositionHost>>& WebView2CompositionHost: WebView2CompositionHostMap)
	{
		if(WebView2CompositionHost.Value.IsValid() &&Hwnd==WebView2CompositionHost.Key)
		{
			WebView2CompositionHost.Value.Pin()->MouseMessage(Message,WParam,LParam);
		}
	}

	return false;
}


void FWebView2Manager::Shutdown()
{
	for(TPair<HWND,TWeakPtr<FWebView2CompositionHost>> &It:WebView2CompositionHostMap)
	{
		if (It.Value.Pin())
		{
			It.Value.Pin()->DestroyWinCompVisualTree();
		}
		
	}
	
	WebView2CompositionHostMap.Empty();
	
	if(WebView2ManagerInstance)
	{
		delete WebView2ManagerInstance;
		WebView2ManagerInstance = nullptr;
	}
}

void FWebView2Manager::Closed(HWND Hwnd)
{
	if(!Hwnd)
	{
		return;
	}
	
	if(WebView2CompositionHostMap.Contains(Hwnd))
	{
		if(WebView2CompositionHostMap.Find(Hwnd)->Pin())
		{
			WebView2CompositionHostMap.Find(Hwnd)->Pin()->DestroyWinCompVisualTree();
		}
		
	}
	WebView2CompositionHostMap.Remove(Hwnd);
}
