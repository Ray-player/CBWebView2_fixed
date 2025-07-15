// Copyright 2025-Present Xiao Lan fei. All Rights Reserved.

#pragma once
#pragma warning(disable : 4191)

#include "CoreMinimal.h"



#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#include "Stdafx.h"
#include <dcomp.h>

#include <winnt.h>
#include <winrt/Windows.UI.Composition.h>


#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "IWebView2Window.h"
#include "WebView2ComponentBase.h"
#include "WebView2Window.generated.h"

struct FCoreWebView2Settings;
class FWebView2CompositionHost;
namespace winrtComp = winrt::Windows::UI::Composition;


USTRUCT(Blueprintable)
struct WEBVIEW2UTILS_API FWebView2DownloadInfo
{

	GENERATED_BODY()
	
	/**下载URL地址*/
	UPROPERTY(BlueprintReadOnly,EditAnywhere, Category = "WebView2|Info")
	FString URL;

	/**m下载ime类型*/
	UPROPERTY(BlueprintReadOnly,EditAnywhere, Category = "WebView2|Info")
	FString MimeType;

	/**下载内容描述*/
	UPROPERTY(BlueprintReadOnly,EditAnywhere, Category = "WebView2|Info")
	FString  ContentDisposition;

	FWebView2DownloadInfo()
	{
	}
	
};

UENUM(BlueprintType)
enum class EWebView2DownloadState:uint8
{
	//正在下载
	Progress,
	//下载完成
	Completed,
	//下载中断
	Interrupted
};

DECLARE_DELEGATE(FOnWebView2Closed)
DECLARE_DELEGATE_OneParam(FOnWebViewCreated,const bool&)
DECLARE_DELEGATE_OneParam(FOnWebView2MessageReceivedNactive,const FString&)
DECLARE_DELEGATE_OneParam(FOnNewWindowRequestedNactive,const FString&)
DECLARE_DELEGATE_OneParam(FOnNavigationCompletedNactive,const bool&)
DECLARE_DELEGATE_OneParam(FOnNavigationStartingNactive,const FString&)
DECLARE_DELEGATE_OneParam(FOnSourceChangedNactive,const FString&)
DECLARE_DELEGATE_OneParam(FOnCursorChangedNactive,const EMouseCursor::Type&)
DECLARE_DELEGATE_TwoParams(FOnDownloadStateChangedNactive,const EWebView2DownloadState&,const FString&)
DECLARE_DELEGATE_TwoParams(FOnDownloadProgressNactive,const int64&,const int64&)
DECLARE_DELEGATE_OneParam(FOnEstimatedDownloadTimeNactive,const FString&)
DECLARE_DELEGATE_OneParam(FOnDocumentTitleChangedNactive,const FString&)

DECLARE_DELEGATE_OneParam(FOnCanGoBackNactive,const bool&)

DECLARE_DELEGATE_OneParam(FOnCanGoForwardNactive,const bool&)
/**
 * 
 */
class WEBVIEW2UTILS_API FWebView2Window :public IWebView2Window,public TSharedFromThis<FWebView2Window>
{
public:
	FWebView2Window(HWND Hwnd,const FGuid UniqueID,const FString InitialURL,FColor InBackgroundColor);
	virtual ~FWebView2Window();

	void InitializeWebView();
	HRESULT CreateControllerWithOptions();
	
private:
	HRESULT DCompositionCreateDevice2(IUnknown* RenderingDevice, REFIID Riid, void** PPV);
	HRESULT TryCreateDispatcherQueue();
public:
	ICoreWebView2Controller*  GetWebViewController();
	ICoreWebView2* GetWebView();
	ICoreWebView2Environment* GetWebViewEnvironment();
	HWND GetMainWindow();


	bool CloseWebView(bool CleanupUserDataFolder = false);
//	void CleanupUserDataFolder();
	void CloseWindow();
	void DeleteAllComponents();

public:
	/**加载网页*/ 
	virtual void LoadURL(const FString URL) override;


	
	/**前进*/
	virtual void GoForward() const override;
	/**后退*/
	virtual void GoBack() const override;
	
	/**重新加载*/
	virtual void ReLoad() const override;
	/**停止加载*/
	virtual void Stop() const override;
	/**设置webview大小*/
	virtual void SetBounds(RECT Rect) override;
	/**设置webview大小*/
	virtual void SetBounds(POINT Offset, POINT Size)override;
	
	/**获取webview大小*/
	virtual RECT GetBounds() const override;

	
	/**设置可见性*/
	virtual void SetVisible(ESlateVisibility InVisibility) override;
	/**获取可见性*/
	virtual ESlateVisibility  GetVisible() const override;

	/** 获取当前文档的加载状态。 */
	virtual EWebView2DocumentState GetDocumentLoadingState() const override;

public:
	void PutCoreWebView2Settings(FCoreWebView2Settings  CoreWebView2Settings);
public:

	void SetBackgroundColor(FColor InBackgroundColor);
	
	bool IsInitialized();
	void SetLayerID(int32 InLayerID);
	int32 GetLayerID() const;

	FGuid GetUniqueID();

	//执行前端脚本
	void ExecuteScript(const FString Script,TFunction<void(const FString ResultString)> Callback=nullptr);

	//按键处理
	void PutHandled(bool bHandled);
	TSharedPtr<FWebView2CompositionHost> GetWebView2CompositionHost();
public:
	void SetContainerVisual(winrt::Windows::UI::Composition::ContainerVisual InWebViewVisual);
	
protected:
	/**创建一个runtime.webview的环境设置的初始化回调函数*/
	HRESULT OnCreateEnvironmentCompleted(HRESULT Result, ICoreWebView2Environment* Environment);

	/**创建一个runtime.webview的WebView2Controller控件的初始化回调函数*/
	HRESULT OnCreateCoreWebView2ControllerCompleted(HRESULT Result, ICoreWebView2Controller* Controller);

	/**信息回调*/
	HRESULT OnMessageReceived(ICoreWebView2* Webview, ICoreWebView2WebMessageReceivedEventArgs* Args);
	

	/**创建一个新的网页链接的回调函数*/
	HRESULT OnCoreWebView2NewWindowRequestedEventHandler(ICoreWebView2* Sender, ICoreWebView2NewWindowRequestedEventArgs* Args);
	
	/**加载完成时的回调函数，可能成功，可能失败*/
	HRESULT OnNavigationCompletedEventHandler(ICoreWebView2* Sender, ICoreWebView2NavigationCompletedEventArgs* Args);
	
	/**开始加载网页时的回调函数*/
	HRESULT OnNavigationStartingEventHandler(ICoreWebView2* Sender, ICoreWebView2NavigationStartingEventArgs* Args);
	
	/**光标改变时的回调函数*/
	HRESULT OnCursorChangedEventHandler( ICoreWebView2CompositionController* Sender,IUnknown* Args);
	
	/**捕获键盘事件回调函数*/
	HRESULT OnAcceleratorKeyPressedEventHandler(ICoreWebView2Controller* Sender, ICoreWebView2AcceleratorKeyPressedEventArgs* Args);
	
	/**开始下载*/
	HRESULT OnDownloadStartingEventHandler(ICoreWebView2* Sender, ICoreWebView2DownloadStartingEventArgs* Args);
	
	/**下载状态发生改变*/
	HRESULT OnDownLoadChangeStateEvent(ICoreWebView2DownloadOperation* Sender,IUnknown* Args);
	
	/**处理下载进度*/
	HRESULT OnBytesReceivedChanged(ICoreWebView2DownloadOperation* Sender, IUnknown* Args);
	
	/**当预计下载时间发生变化时，通知此事件*/
	HRESULT OnEstimatedEndTimeChanged( ICoreWebView2DownloadOperation* Sender, IUnknown* Args);

	/**当历史记录发生变化时候*/
	HRESULT OnHistoryChanged( ICoreWebView2* Sender, IUnknown* Args);

	/**标题修改后*/
	HRESULT OnDocumentTitleChanged(ICoreWebView2* Sender, IUnknown* Args);

	/**网页链接修改后*/
	HRESULT OnSourceChanged(ICoreWebView2* Sender, ICoreWebView2SourceChangedEventArgs* Args);
	
private:
	FString InterruptReasonToString(const COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON InterruptReason);
public:
	TArray<TSharedPtr<FWebView2ComponentBase>> Components;

	Microsoft::WRL::ComPtr<ICoreWebView2Environment> WebViewEnvironment;
	Microsoft::WRL::ComPtr<ICoreWebView2CompositionController> CompositionController;
	Microsoft::WRL::ComPtr<ICoreWebView2Controller> Controller;
	Microsoft::WRL::ComPtr<ICoreWebView2> WebView;
	Microsoft::WRL::ComPtr<ICoreWebView2_4> WebView4;
	
	winrt::Windows::UI::Composition::ContainerVisual WebViewVisual={nullptr};


	HWND MainWindow = nullptr;

	Microsoft::WRL::ComPtr<IDCompositionDevice> DcompDevice;
	FOnWebViewCreated OnWebViewCreated;
	FOnWebView2MessageReceivedNactive OnWebView2MessageReceived;
	FOnNewWindowRequestedNactive OnNewWindowRequested;
	FOnNavigationCompletedNactive OnNavigationCompleted;
	FOnNavigationStartingNactive OnNavigationStarting;
	FOnCursorChangedNactive OnCursorChangedNactive;
	FOnCanGoBackNactive OnCanGoBackNactive;
	FOnCanGoForwardNactive OnCanGoForwardNactive;
	FOnDocumentTitleChangedNactive OnDocumentTitleChangedNactive;
	FOnSourceChangedNactive  OnSourceChangedNactive;
	EventRegistrationToken NavigationCompletedToken = {};
	EventRegistrationToken NavigationStartingToken = {};
	EventRegistrationToken CursorChangedEventHandlerToken = {};
	EventRegistrationToken AcceleratorKeyPressedEventHandlerToken = {};
	EventRegistrationToken NewWindowRequestedToken = {};
	
	EventRegistrationToken DocumentTitleChangedToken = {};
	EventRegistrationToken HistoryChangedToken = {};
	EventRegistrationToken SourceChangedToken = {};
	//下载模块
	FWebView2DownloadInfo DownloadInfo;
	int64 TotalBytesToReceive = 0;
	Microsoft::WRL::ComPtr<ICoreWebView2DownloadOperation> DownloadOperation;
	EventRegistrationToken DownloadStartingToken = {};
	EventRegistrationToken StateChangedToken = {};
	EventRegistrationToken BytesReceivedChangedToken = {};
	EventRegistrationToken EstimatedEndTimeChanged = {};

	
	FOnDownloadStateChangedNactive OnDownloadStateChangedNactive ;
	FOnDownloadProgressNactive OnDownloadProgressNactive;
	FOnEstimatedDownloadTimeNactive OnEstimatedDownloadTimeNactive;


	//判断鼠标是否在位置区域内的标志  暂时放这里
	bool bIsMouseOverPositionArea = true;
private:

	RECT WebuiBound;
	
	ESlateVisibility Visible;
	 bool bInitialized;
	FString InitialURL;
	FGuid ID;
	UINT32 NewestBrowserPID;
	
	FColor  BackgroundColor;
	int32 LayerID;
	TSharedPtr<FWebView2CompositionHost> WebView2CompositionHost;
	
	EventRegistrationToken BrowserExitedEventToken = {};
	EventRegistrationToken MessageReceiveToken= {};
	EventRegistrationToken ZoomFactorChangedToken = {};
	EventRegistrationToken MoveFocusRequestedToken = {};


	EWebView2DocumentState DocumentState;
	
};




