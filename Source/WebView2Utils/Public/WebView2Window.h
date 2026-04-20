#pragma once

#include "CoreMinimal.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

THIRD_PARTY_INCLUDES_START
#include <dcomp.h>
#include <winrt/Windows.UI.Composition.h>
#include <winnt.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "Components/SlateWrapperTypes.h"

#include "Stdafx.h"
#include "IWebView2Window.h"
#include "WebView2EventTypes.h"

class FWebView2CompositionHost;
class FCoreWebView2Settings;

enum class EWebView2DownloadState : uint8
{
	InProgress,
	Interrupted,
	Completed
};

/** 下载状态快照，供原生层、Slate 与蓝图统一使用。 */
struct FWebView2DownloadInfo
{
	/** 原始下载地址。 */
	FString Uri;
	/** 资源 MimeType。 */
	FString MimeType;
	/** 本地目标路径。 */
	FString ResultFilePath;
	/** 已接收字节数。 */
	int64 BytesReceived = 0;
	/** 总字节数，未知时为 -1。 */
	int64 TotalBytesToReceive = -1;
	/** 当前下载状态。 */
	EWebView2DownloadState State = EWebView2DownloadState::InProgress;
};

/** 原生层对外事件委托定义。 */
DECLARE_DELEGATE_OneParam(FOnWebView2MessageReceivedNative, const FString&)
DECLARE_DELEGATE_OneParam(FOnWebView2NavigationCompletedNative, bool)
DECLARE_DELEGATE_OneParam(FOnWebView2NavigationStartingNative, const FString&)
DECLARE_DELEGATE_OneParam(FOnWebView2NewWindowRequestedNative, const FString&)
DECLARE_DELEGATE_OneParam(FOnWebView2CursorChangedNative, EMouseCursor::Type)
DECLARE_DELEGATE_OneParam(FOnWebView2DocumentTitleChangedNative, const FString&)
DECLARE_DELEGATE_OneParam(FOnWebView2SourceChangedNative, const FString&)
DECLARE_DELEGATE_OneParam(FOnWebView2CanGoBackChangedNative, bool)
DECLARE_DELEGATE_OneParam(FOnWebView2CanGoForwardChangedNative, bool)
DECLARE_DELEGATE_OneParam(FOnWebView2DownloadEventNative, const FWebView2DownloadInfo&)
DECLARE_DELEGATE_TwoParams(FOnWebView2PrintToPdfCompletedNative, bool, const FString&)
DECLARE_DELEGATE(FOnWebView2InputActivationNative)
DECLARE_DELEGATE_OneParam(FOnWebViewCreatedNative, bool)

/**
 * WebView2 原生宿主对象。
 *
 * 它封装了 Environment、Controller、CompositionController 与 WebView 本身，
 * 是整个插件的底层核心。
 */
class WEBVIEW2UTILS_API FWebView2Window : public IWebView2Window, public TSharedFromThis<FWebView2Window>
{
public:
	FWebView2Window(HWND InParentWindow, const FGuid& InInstanceId, const FString& InInitialUrl, const FColor& InBackgroundColor, bool bInEnableTransparencyHitTest);
	virtual ~FWebView2Window();

	/** 创建 Environment / Controller / WebView。 */
	void Initialize();
	/** 关闭当前实例，可选删除用户数据目录。 */
	bool Close(bool bCleanupUserDataFolder = false);
	/** 外层统一关闭入口。 */
	void CloseWindow();

	/** 查询是否完成初始化。 */
	bool IsInitialized() const;
	/** 当前 LayerId，用于重叠排序。 */
	int32 GetLayerId() const;
	void SetLayerId(int32 InLayerId);
	FGuid GetInstanceId() const;

	/** 常用扩展能力。 */
	void ExecuteScript(const FString& Script, TFunction<void(const FString&)> Callback = nullptr);
	void OpenDevToolsWindow() const;
	void PrintToPdf(const FString& OutputPath, bool bLandscape = false);
	void SetBackgroundColor(const FColor& InBackgroundColor);
	void SetContainerVisual(winrt::Windows::UI::Composition::ContainerVisual InContainerVisual);
	TSharedPtr<FWebView2CompositionHost> GetCompositionHost() const;
	/** 设置当前是否参与命中测试。 */
	void SetHitTestEnabled(bool bInHitTestEnabled);
	bool IsHitTestEnabled() const;
	/** 返回该实例是否启用了透明命中检测脚本。 */
	bool IsTransparencyHitTestEnabled() const;

	virtual void LoadURL(const FString& InUrl) override;
	virtual void GoForward() const override;
	virtual void GoBack() const override;
	virtual void Reload() const override;
	virtual void Stop() const override;
	virtual void SetBounds(const RECT& InRect) override;
	virtual void SetBounds(const POINT& InOffset, const POINT& InSize) override;
	virtual RECT GetBounds() const override;
	virtual void SetVisible(ESlateVisibility InVisibility) override;
	virtual ESlateVisibility GetVisible() const override;
	virtual ECBWebView2DocumentState GetDocumentLoadingState() const override;
	virtual void MoveFocus(bool bFocus) override;
	virtual void SendMouseButton(const FVector2D& Point, bool bIsLeft, bool bIsDown) override;
	virtual void SendMouseMove(const FVector2D& Point) override;
	virtual void SendMouseWheel(const FVector2D& Point, float Delta) override;
	virtual void SendKeyboardMessage(uint32 Msg, uint64 WParam, int64 LParam) override;

	/** 外部可订阅的原生事件。 */
	FOnWebView2MessageReceivedNative OnMessageReceived;
	FOnWebView2NavigationCompletedNative OnNavigationCompleted;
	FOnWebView2NavigationStartingNative OnNavigationStarting;
	FOnWebView2NewWindowRequestedNative OnNewWindowRequested;
	FOnWebView2CursorChangedNative OnCursorChanged;
	FOnWebView2DocumentTitleChangedNative OnDocumentTitleChanged;
	FOnWebView2SourceChangedNative OnSourceChanged;
	FOnWebView2CanGoBackChangedNative OnCanGoBackChanged;
	FOnWebView2CanGoForwardChangedNative OnCanGoForwardChanged;
	FOnWebView2DownloadEventNative OnDownloadStarting;
	FOnWebView2DownloadEventNative OnDownloadUpdated;
	FOnWebView2PrintToPdfCompletedNative OnPrintToPdfCompleted;
	FOnWebView2MonitoredEventNative OnMonitoredEvent;
	/** 当原生路由决定该 WebView 接管输入时触发。 */
	FOnWebView2InputActivationNative OnInputActivationRequested;
	/** 当 WebView 创建完成时触发，参数表示是否创建成功。 */
	FOnWebViewCreatedNative OnWebViewCreated;

	/** 底层 COM 对象，暴露给组合宿主和高级能力使用。 */
	Microsoft::WRL::ComPtr<ICoreWebView2Environment> WebViewEnvironment;
	Microsoft::WRL::ComPtr<ICoreWebView2Controller> Controller;
	Microsoft::WRL::ComPtr<ICoreWebView2CompositionController> CompositionController;
	Microsoft::WRL::ComPtr<ICoreWebView2> WebView;
	winrt::Windows::UI::Composition::ContainerVisual WebViewVisual{nullptr};

private:
	/** Controller 创建辅助。 */
	HRESULT CreateControllerWithOptions();
	HRESULT DCompositionCreateDevice2(IUnknown* RenderingDevice, REFIID Riid, void** PPV);

	/** WebView2 生命周期回调。 */
	HRESULT OnCreateEnvironmentCompleted(HRESULT Result, ICoreWebView2Environment* Environment);
	HRESULT OnCreateControllerCompleted(HRESULT Result, ICoreWebView2Controller* InController);
	HRESULT OnMessageReceivedInternal(ICoreWebView2* Sender, ICoreWebView2WebMessageReceivedEventArgs* Args);
	HRESULT OnNewWindowRequestedInternal(ICoreWebView2* Sender, ICoreWebView2NewWindowRequestedEventArgs* Args);
	HRESULT OnNavigationCompletedInternal(ICoreWebView2* Sender, ICoreWebView2NavigationCompletedEventArgs* Args);
	HRESULT OnNavigationStartingInternal(ICoreWebView2* Sender, ICoreWebView2NavigationStartingEventArgs* Args);
	HRESULT OnCursorChangedInternal(ICoreWebView2CompositionController* Sender, IUnknown* Args);
	HRESULT OnAcceleratorKeyPressedInternal(ICoreWebView2Controller* Sender, ICoreWebView2AcceleratorKeyPressedEventArgs* Args);
	HRESULT OnHistoryChangedInternal(ICoreWebView2* Sender, IUnknown* Args);
	HRESULT OnDocumentTitleChangedInternal(ICoreWebView2* Sender, IUnknown* Args);
	HRESULT OnSourceChangedInternal(ICoreWebView2* Sender, ICoreWebView2SourceChangedEventArgs* Args);
	HRESULT OnDownloadStartingInternal(ICoreWebView2* Sender, ICoreWebView2DownloadStartingEventArgs* Args);
	HRESULT OnPermissionRequestedInternal(ICoreWebView2* Sender, ICoreWebView2PermissionRequestedEventArgs* Args);
	FWebView2DownloadInfo BuildDownloadInfo(ICoreWebView2DownloadOperation* DownloadOperation) const;
	FCBWebView2MonitoredEvent BuildDownloadMonitoredEvent(ECBWebView2MonitoredEventType EventType, ICoreWebView2DownloadOperation* DownloadOperation) const;
	void EmitMonitoredEvent(const FCBWebView2MonitoredEvent& EventInfo) const;
	void RegisterDownloadTracking(ICoreWebView2DownloadOperation* DownloadOperation);
	void UnregisterDownloadTracking(uint64 DownloadKey);

	void ApplyRuntimeSettings() const;
	void RegisterCoreEvents();

	/** 下载事件注册信息，用于后续移除回调。 */
	struct FTrackedDownloadRegistration
	{
		Microsoft::WRL::ComPtr<ICoreWebView2DownloadOperation> Operation;
		EventRegistrationToken BytesReceivedChangedToken = {};
		EventRegistrationToken StateChangedToken = {};
	};

	HWND ParentWindow = nullptr;
	RECT Bounds = {};
	ESlateVisibility Visible = ESlateVisibility::Visible;
	ECBWebView2DocumentState DocumentState = ECBWebView2DocumentState::NoDocument;
	bool bInitialized = false;
	bool bCloseRequested = false;
	FGuid InstanceId;
	FString InitialUrl;
	FColor BackgroundColor;
	bool bEnableTransparencyHitTest = false;
	bool bHitTestEnabled = true;
	int32 LayerId = 0;
	UINT32 BrowserProcessId = 0;
	Microsoft::WRL::ComPtr<IDCompositionDevice> DCompDevice;
	TSharedPtr<FWebView2CompositionHost> CompositionHost;

	/** 核心事件 Token，用于 Close 时安全解绑。 */
	EventRegistrationToken MessageReceivedToken = {};
	EventRegistrationToken NavigationCompletedToken = {};
	EventRegistrationToken NavigationStartingToken = {};
	EventRegistrationToken NewWindowRequestedToken = {};
	EventRegistrationToken CursorChangedToken = {};
	EventRegistrationToken AcceleratorKeyPressedToken = {};
	EventRegistrationToken HistoryChangedToken = {};
	EventRegistrationToken DocumentTitleChangedToken = {};
	EventRegistrationToken SourceChangedToken = {};
	EventRegistrationToken DownloadStartingToken = {};
	EventRegistrationToken PermissionRequestedToken = {};
	EventRegistrationToken ZoomFactorChangedToken = {};
	TMap<uint64, FTrackedDownloadRegistration> ActiveDownloads;
};