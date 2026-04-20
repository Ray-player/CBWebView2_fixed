#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "WebView2EventTypes.h"

#include "CBWebView2Widget.generated.h"

class SCBWebView2;
struct FWebView2DownloadInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCBWebView2MessageReceived, const FString&, Message);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnCBWebView2ScriptExecuted, const FString&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCBWebView2LoadCompleted, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCBWebView2LoadStarted, const FString&, Url);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCBWebView2NewWindowRequested, const FString&, Url);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCBWebView2PrintToPdfCompleted, bool, bSuccess, const FString&, OutputPath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCBWebView2Created, bool, bSuccess);

UENUM(BlueprintType)
enum class ECBWebView2DownloadState : uint8
{
	InProgress UMETA(DisplayName = "下载中"),
	Interrupted UMETA(DisplayName = "已中断"),
	Completed UMETA(DisplayName = "已完成")
};

USTRUCT(BlueprintType)
struct FCBWebView2DownloadInfo
{
	GENERATED_BODY()

	/** 原始下载地址。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|下载")
	FString Uri;

	/** 服务器返回的 MimeType。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|下载")
	FString MimeType;

	/** 本地落盘路径。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|下载")
	FString ResultFilePath;

	/** 已接收字节数。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|下载")
	int64 BytesReceived = 0;

	/** 目标总字节数，未知时通常为 -1。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|下载")
	int64 TotalBytesToReceive = -1;

	/** 当前下载状态。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|下载")
	ECBWebView2DownloadState State = ECBWebView2DownloadState::InProgress;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCBWebView2DownloadEvent, const FCBWebView2DownloadInfo&, DownloadInfo);

/**
 * UMG 层 WebView2 控件。
 *
 * 这是项目侧最常用的入口，蓝图通常直接操作这个类。
 */
UCLASS()
class CBWEBVIEW2_API UCBWebView2Widget : public UWidget
{
	GENERATED_BODY()

public:
	UCBWebView2Widget(const FObjectInitializer& ObjectInitializer);

	/** 释放内部 Slate 资源。 */
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	/** 构建真正承载 WebView 的 Slate 控件。 */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** 初始加载地址。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CBWebView2")
	FString InitialUrl;

	/** WebView 默认背景色，A 为 0 时表现为透明背景。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CBWebView2|外观")
	FColor BackgroundColor;

	/** 是否显示地址栏。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CBWebView2|外观")
	bool bShowAddressBar;

	/** 是否显示控制按钮栏。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CBWebView2|外观")
	bool bShowControls;

	/** 是否显示触摸区域调试层。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CBWebView2|调试")
	bool bShowTouchArea;

	/** 是否启用网页穿透检测脚本注入。关闭后该页面将不再自动注入 transparency_check.js。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CBWebView2|交互")
	bool bEnableTransparencyHitTest;

	/** 是否将所有输入(鼠标、键盘)都传递给网页。为true时输入不会传递给下层UI。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CBWebView2|交互")
	bool bInputOnlyOnWeb;

	UPROPERTY(BlueprintAssignable, Category = "CBWebView2|事件")
	FOnCBWebView2MessageReceived OnMessageReceived;

	UPROPERTY(BlueprintAssignable, Category = "CBWebView2|事件")
	FOnCBWebView2LoadCompleted OnLoadCompleted;

	UPROPERTY(BlueprintAssignable, Category = "CBWebView2|事件")
	FOnCBWebView2LoadStarted OnLoadStarted;

	UPROPERTY(BlueprintAssignable, Category = "CBWebView2|事件")
	FOnCBWebView2NewWindowRequested OnNewWindowRequested;

	/** 下载创建时触发一次。 */
	UPROPERTY(BlueprintAssignable, Category = "CBWebView2|事件")
	FOnCBWebView2DownloadEvent OnDownloadStarting;

	/** 下载进度与状态变化时持续触发。 */
	UPROPERTY(BlueprintAssignable, Category = "CBWebView2|事件")
	FOnCBWebView2DownloadEvent OnDownloadUpdated;

	/** PrintToPdf 完成后返回结果。 */
	UPROPERTY(BlueprintAssignable, Category = "CBWebView2|事件")
	FOnCBWebView2PrintToPdfCompleted OnPrintToPdfCompleted;

	/** 统一监控事件出口，便于做日志、埋点和调试。 */
	UPROPERTY(BlueprintAssignable, Category = "CBWebView2|监控")
	FOnCBWebView2MonitoredEvent OnMonitoredEvent;

	/** WebView 创建完成时触发，参数表示是否创建成功。建议在此事件后执行 LoadURL。 */
	UPROPERTY(BlueprintAssignable, Category = "CBWebView2|事件")
	FOnCBWebView2Created OnCreatedWebViewWidget;

	/** 跳转到指定 URL。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2")
	void LoadURL(const FString& InUrl);

	/** 执行一段 JS，并通过回调返回结果字符串。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2")
	void ExecuteScript(const FString& Script, FOnCBWebView2ScriptExecuted Callback);

	/** 浏览器前进。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2")
	void GoForward();

	/** 浏览器后退。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2")
	void GoBack();

	/** 刷新当前页面。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2")
	void Reload();

	/** 停止当前加载。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2")
	void StopLoading();

	/** 打开浏览器 DevTools 窗口。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2|调试")
	void OpenDevToolsWindow();

	/** 将当前页面输出为 PDF。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2|打印")
	void PrintToPdf(const FString& OutputPath, bool bLandscape = false);

	/** 动态修改默认背景色。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2|外观")
	void SetBackgroundColorEx(FColor InBackgroundColor);

	/** 同步设置 UMG 与原生 WebView 的可见性。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2|外观")
	void SetWebViewVisibility(ESlateVisibility InVisibility);

	/** 设置是否将所有输入都传递给网页。运行时修改会立即生效。 */
	UFUNCTION(BlueprintCallable, Category = "CBWebView2|交互")
	void SetInputOnlyOnWeb(bool bInputOnly);

private:
	/** 以下函数用于把原生/Slate 事件桥接到蓝图动态委托。 */
	void HandleMessageReceived(const FString& Message);
	void HandleNavigationCompleted(bool bSuccess);
	void HandleNavigationStarted(const FString& Url);
	void HandleNewWindowRequested(const FString& Url);
	void HandleDownloadStartingNative(const FWebView2DownloadInfo& DownloadInfo);
	void HandleDownloadUpdatedNative(const FWebView2DownloadInfo& DownloadInfo);
	void HandlePrintToPdfCompletedNative(bool bSuccess, const FString& OutputPath);
	void HandleMonitoredEventNative(const FCBWebView2MonitoredEvent& EventInfo);
	void HandleWebViewCreatedNative(bool bSuccess);

	/** UMG 包装的真正 Slate 控件实例。 */
	TSharedPtr<SCBWebView2> SlateWebView;
};