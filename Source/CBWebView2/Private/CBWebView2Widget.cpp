#include "CBWebView2Widget.h"

#include "SCBWebView2.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// 原生下载状态转成蓝图枚举，避免 Blueprint 直接依赖底层类型。
	ECBWebView2DownloadState ToBlueprintDownloadState(const EWebView2DownloadState InState)
	{
		switch (InState)
		{
		case EWebView2DownloadState::Interrupted:
			return ECBWebView2DownloadState::Interrupted;
		case EWebView2DownloadState::Completed:
			return ECBWebView2DownloadState::Completed;
		default:
			return ECBWebView2DownloadState::InProgress;
		}
	}

	// 原生下载结构转成蓝图可见结构。
	FCBWebView2DownloadInfo ToBlueprintDownloadInfo(const FWebView2DownloadInfo& InInfo)
	{
		FCBWebView2DownloadInfo OutInfo;
		OutInfo.Uri = InInfo.Uri;
		OutInfo.MimeType = InInfo.MimeType;
		OutInfo.ResultFilePath = InInfo.ResultFilePath;
		OutInfo.BytesReceived = InInfo.BytesReceived;
		OutInfo.TotalBytesToReceive = InInfo.TotalBytesToReceive;
		OutInfo.State = ToBlueprintDownloadState(InInfo.State);
		return OutInfo;
	}
}

UCBWebView2Widget::UCBWebView2Widget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InitialUrl(TEXT("https://www.bing.com"))
	, BackgroundColor(FColor(255, 255, 255, 255))
	, bShowAddressBar(false)
	, bShowControls(false)
	, bShowTouchArea(false)
	, bEnableTransparencyHitTest(false)
	, bInputOnlyOnWeb(false)
{
}

void UCBWebView2Widget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	// UMG 重建或销毁时，显式释放内部 Slate WebView，避免原生资源滞留。
	if (SlateWebView.IsValid())
	{
		SlateWebView->BeginDestroy();
		SlateWebView.Reset();
	}
}

TSharedRef<SWidget> UCBWebView2Widget::RebuildWidget()
{
	// WebView2 依赖真实宿主窗口，因此优先从 GameViewport 取得顶层窗口句柄。
	if (GEngine && GEngine->GameViewport)
	{
		TSharedPtr<SWindow> ParentWindow = GEngine->GameViewport->GetWindow();
		if (ParentWindow.IsValid())
		{
			return SAssignNew(SlateWebView, SCBWebView2)
				.InitialUrl(InitialUrl)
				.BackgroundColor(BackgroundColor)
				.bShowAddressBar(bShowAddressBar)
				.bShowControls(bShowControls)
				.bShowTouchArea(bShowTouchArea)
				.bEnableTransparencyHitTest(bEnableTransparencyHitTest)
				.bInputOnlyOnWeb(bInputOnlyOnWeb)
				.ParentWindow(ParentWindow)
				.OnMessageReceived(FOnWebView2MessageReceivedNative::CreateUObject(this, &UCBWebView2Widget::HandleMessageReceived))
				.OnNavigationCompleted(FOnWebView2NavigationCompletedNative::CreateUObject(this, &UCBWebView2Widget::HandleNavigationCompleted))
				.OnNavigationStarting(FOnWebView2NavigationStartingNative::CreateUObject(this, &UCBWebView2Widget::HandleNavigationStarted))
				.OnNewWindowRequested(FOnWebView2NewWindowRequestedNative::CreateUObject(this, &UCBWebView2Widget::HandleNewWindowRequested))
				.OnDownloadStarting(FOnWebView2DownloadEventNative::CreateUObject(this, &UCBWebView2Widget::HandleDownloadStartingNative))
				.OnDownloadUpdated(FOnWebView2DownloadEventNative::CreateUObject(this, &UCBWebView2Widget::HandleDownloadUpdatedNative))
				.OnPrintToPdfCompleted(FOnWebView2PrintToPdfCompletedNative::CreateUObject(this, &UCBWebView2Widget::HandlePrintToPdfCompletedNative))
				.OnMonitoredEvent(FOnWebView2MonitoredEventNative::CreateUObject(this, &UCBWebView2Widget::HandleMonitoredEventNative))
				.OnWebViewCreated(FOnWebViewCreatedNative::CreateUObject(this, &UCBWebView2Widget::HandleWebViewCreatedNative));
		}
	}

	// 没有有效窗口时返回占位控件，避免 UMG 构建阶段崩溃。
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("CBWebView2 需要有效的 GameViewport 窗口")))
		];
}

void UCBWebView2Widget::LoadURL(const FString& InUrl)
{
	if (SlateWebView.IsValid())
	{
		SlateWebView->LoadURL(InUrl);
	}
}

void UCBWebView2Widget::ExecuteScript(const FString& Script, FOnCBWebView2ScriptExecuted Callback)
{
	if (SlateWebView.IsValid())
	{
		SlateWebView->ExecuteScript(Script, FOnCBWebView2ScriptCallback::CreateLambda([Callback](const FString& Result)
		{
			if (Callback.IsBound())
			{
				Callback.Execute(Result);
			}
		}));
	}
}

void UCBWebView2Widget::GoForward()
{
	if (SlateWebView.IsValid())
	{
		SlateWebView->GoForward();
	}
}

void UCBWebView2Widget::GoBack()
{
	if (SlateWebView.IsValid())
	{
		SlateWebView->GoBack();
	}
}

void UCBWebView2Widget::Reload()
{
	if (SlateWebView.IsValid())
	{
		SlateWebView->Reload();
	}
}

void UCBWebView2Widget::StopLoading()
{
	if (SlateWebView.IsValid())
	{
		SlateWebView->Stop();
	}
}

void UCBWebView2Widget::OpenDevToolsWindow()
{
	if (SlateWebView.IsValid())
	{
		SlateWebView->OpenDevToolsWindow();
	}
}

void UCBWebView2Widget::PrintToPdf(const FString& OutputPath, bool bLandscape)
{
	if (SlateWebView.IsValid())
	{
		SlateWebView->PrintToPdf(OutputPath, bLandscape);
	}
}

void UCBWebView2Widget::SetBackgroundColorEx(FColor InBackgroundColor)
{
	BackgroundColor = InBackgroundColor;
	if (SlateWebView.IsValid())
	{
		SlateWebView->SetBackgroundColor(InBackgroundColor);
	}
}

void UCBWebView2Widget::SetWebViewVisibility(ESlateVisibility InVisibility)
{
	SetVisibility(InVisibility);
	if (SlateWebView.IsValid())
	{
		SlateWebView->SetWebViewVisibility(InVisibility);
	}
}

void UCBWebView2Widget::SetInputOnlyOnWeb(bool bInputOnly)
{
	bInputOnlyOnWeb = bInputOnly;
	if (SlateWebView.IsValid())
	{
		SlateWebView->SetInputOnlyOnWeb(bInputOnly);
	}
}

void UCBWebView2Widget::HandleMessageReceived(const FString& Message)
{
	// 蓝图统一从这里接收网页 postMessage / 透明检测消息。
	OnMessageReceived.Broadcast(Message);
}

void UCBWebView2Widget::HandleNavigationCompleted(bool bSuccess)
{
	OnLoadCompleted.Broadcast(bSuccess);
}

void UCBWebView2Widget::HandleNavigationStarted(const FString& Url)
{
	OnLoadStarted.Broadcast(Url);
}

void UCBWebView2Widget::HandleNewWindowRequested(const FString& Url)
{
	OnNewWindowRequested.Broadcast(Url);
}

void UCBWebView2Widget::HandleDownloadStartingNative(const FWebView2DownloadInfo& DownloadInfo)
{
	// 统一转换为蓝图结构，保持 UMG 层 API 稳定。
	OnDownloadStarting.Broadcast(ToBlueprintDownloadInfo(DownloadInfo));
}

void UCBWebView2Widget::HandleDownloadUpdatedNative(const FWebView2DownloadInfo& DownloadInfo)
{
	OnDownloadUpdated.Broadcast(ToBlueprintDownloadInfo(DownloadInfo));
}

void UCBWebView2Widget::HandlePrintToPdfCompletedNative(bool bSuccess, const FString& OutputPath)
{
	OnPrintToPdfCompleted.Broadcast(bSuccess, OutputPath);
}

void UCBWebView2Widget::HandleMonitoredEventNative(const FCBWebView2MonitoredEvent& EventInfo)
{
	OnMonitoredEvent.Broadcast(EventInfo);
}

void UCBWebView2Widget::HandleWebViewCreatedNative(bool bSuccess)
{
	OnCreatedWebViewWidget.Broadcast(bSuccess);
}