#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "WebView2Window.h"

class SEditableTextBox;
class SOverlay;

DECLARE_DELEGATE_OneParam(FOnCBWebView2ScriptCallback, const FString&)

/**
 * Slate 层 WebView 控件。
 *
 * 它负责：
 * 1. 在 UE 界面里创建与管理原生 WebView2 宿主。
 * 2. 把 Slate 输入事件转发给底层 WebView。
 * 3. 处理地址栏、控制栏和透明穿透这类 UI 逻辑。
 */
class CBWEBVIEW2_API SCBWebView2 : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCBWebView2)
		: _InitialUrl(TEXT("https://www.bing.com"))
		, _BackgroundColor(FColor(255, 255, 255, 255))
		, _bShowAddressBar(false)
		, _bShowControls(false)
		, _bShowTouchArea(false)
		, _bEnableTransparencyHitTest(true)
		, _bShowInitialThrobber(false)
		, _bInputOnlyOnWeb(false)
	{
	}

		// 以下参数对应 UMG 暴露的常用配置。
		SLATE_ARGUMENT(FString, InitialUrl)
		SLATE_ARGUMENT(FColor, BackgroundColor)
		SLATE_ARGUMENT(bool, bShowAddressBar)
		SLATE_ARGUMENT(bool, bShowControls)
		SLATE_ARGUMENT(bool, bShowTouchArea)
		SLATE_ARGUMENT(bool, bEnableTransparencyHitTest)
		SLATE_ARGUMENT(bool, bShowInitialThrobber)
		SLATE_ARGUMENT(bool, bInputOnlyOnWeb)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)

		SLATE_EVENT(FOnWebView2MessageReceivedNative, OnMessageReceived)
		SLATE_EVENT(FOnWebView2NavigationCompletedNative, OnNavigationCompleted)
		SLATE_EVENT(FOnWebView2NavigationStartingNative, OnNavigationStarting)
		SLATE_EVENT(FOnWebView2NewWindowRequestedNative, OnNewWindowRequested)
		SLATE_EVENT(FOnWebView2DownloadEventNative, OnDownloadStarting)
		SLATE_EVENT(FOnWebView2DownloadEventNative, OnDownloadUpdated)
		SLATE_EVENT(FOnWebView2PrintToPdfCompletedNative, OnPrintToPdfCompleted)
		SLATE_EVENT(FOnWebView2MonitoredEventNative, OnMonitoredEvent)
		SLATE_EVENT(FOnWebViewCreatedNative, OnWebViewCreated)
	SLATE_END_ARGS()

	/** 构建 Slate 层浏览器控件并创建底层原生宿主。 */
	void Construct(const FArguments& InArgs);
	virtual ~SCBWebView2() override;

	/** 主动关闭底层 WebView 并释放原生资源。 */
	void BeginDestroy();
	/** 执行 JS 脚本。 */
	void ExecuteScript(const FString& Script, FOnCBWebView2ScriptCallback Callback = FOnCBWebView2ScriptCallback()) const;
	/** 导航到新的地址。 */
	void LoadURL(const FString& InUrl);
	/** 浏览器前进。 */
	void GoForward() const;
	/** 浏览器后退。 */
	void GoBack() const;
	/** 刷新当前页。 */
	void Reload() const;
	/** 停止加载。 */
	void Stop() const;
	/** 打开 DevTools。 */
	void OpenDevToolsWindow() const;
	/** 将页面打印为 PDF。 */
	void PrintToPdf(const FString& OutputPath, bool bLandscape = false) const;
	/** 修改默认背景色。 */
	void SetBackgroundColor(const FColor& InBackgroundColor);
	/** 同步设置 Slate 与原生 WebView 的可见性。 */
	void SetWebViewVisibility(ESlateVisibility InVisibility);
	/** 获取底层原生可见性状态。 */
	ESlateVisibility GetWebViewVisibility() const;
	/** 设置是否将所有输入都传递给网页。 */
	void SetInputOnlyOnWeb(bool bInputOnly);

	/** 在绘制阶段同步原生 WebView 的位置、尺寸和层级。 */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	/** 给 UMG/Slate 布局系统一个默认尺寸。 */
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	/** 预留 Tick 入口，便于后续扩展。 */
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

	/** 以下输入函数把 Slate 事件转成 WebView2 原生输入。 */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;

private:
	/** 工具栏事件和展示辅助函数。 */
	FReply HandleBackClicked();
	FReply HandleForwardClicked();
	FReply HandleReloadClicked();
	void HandleUrlCommitted(const FText& NewText, ETextCommit::Type CommitType);
	FText GetReloadButtonText() const;
	FText GetTitleText() const;
	FText GetAddressBarUrlText() const;
	EVisibility GetLoadingIndicatorVisibility() const;
	FVector2D GetLocalWebViewPoint(const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition) const;

	/** 绑定原生 WebView2 委托到 Slate 与外部事件。 */
	void BindWebViewEvents();
	/** 处理来自网页的消息，包括透明区域命中状态消息。 */
	void HandleMessageFromWeb(const FString& Message);

	/** 底层原生 WebView 宿主。 */
	TSharedPtr<FWebView2Window> WebViewWindow;
	/** 可选地址栏。 */
	TSharedPtr<SEditableTextBox> AddressBarTextBox;
	/** WebView 的覆盖层容器。 */
	TSharedPtr<SOverlay> Overlay;

	FString InitialUrl;
	FString CurrentUrl;
	FString CurrentTitle;
	FColor BackgroundColor;
	FGuid InstanceId;
	TWeakPtr<SWindow> ParentWindow;
	mutable float CurrentSlateScale = 1.0f;

	/** 外观与输入行为开关。 */
	bool bShowAddressBar = false;
	bool bShowControls = false;
	bool bShowTouchArea = false;
	bool bEnableTransparencyHitTest = true;
	bool bShowInitialThrobber = false;
	bool bCanGoBack = false;
	bool bCanGoForward = false;
	bool bIsHoveringInteractiveContent = true;
	bool bInputOnlyOnWeb = false;

	/** 转发给外部的原生委托集合。 */
	FOnWebView2MessageReceivedNative OnMessageReceived;
	FOnWebView2NavigationCompletedNative OnNavigationCompleted;
	FOnWebView2NavigationStartingNative OnNavigationStarting;
	FOnWebView2NewWindowRequestedNative OnNewWindowRequested;
	FOnWebView2DownloadEventNative OnDownloadStarting;
	FOnWebView2DownloadEventNative OnDownloadUpdated;
	FOnWebView2PrintToPdfCompletedNative OnPrintToPdfCompleted;
	FOnWebView2MonitoredEventNative OnMonitoredEvent;
	FOnWebViewCreatedNative OnWebViewCreated;
};