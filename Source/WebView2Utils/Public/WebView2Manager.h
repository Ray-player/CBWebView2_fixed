#pragma once
#include "WebView2Window.h"

class WEBVIEW2UTILS_API FWebView2Manager
{
public:
	FWebView2Manager();
	
	static FWebView2Manager* GetInstance();
	void Init();

	TSharedPtr<FWebView2Window> CreateWebview(HWND Hwnd,const FGuid UniqueID,const FString URL,FColor InBackgroundColor);


	TSharedPtr<FWebView2CompositionHost> GetMessageProcess(HWND Hwnd);

	/*
     * 消息处理函数（window平台）
     */
    bool HandleWindowMessage(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam);
	
	/*
    * 释放资源
    */
    void Shutdown();

	void Closed(HWND Hwnd);
public:

	TMap<HWND,TWeakPtr<FWebView2CompositionHost>>  WebView2CompositionHostMap;
	
	winrt::Windows::System::DispatcherQueueController DispatcherQueueController{nullptr};
private:
	static FWebView2Manager* WebView2ManagerInstance;

};
