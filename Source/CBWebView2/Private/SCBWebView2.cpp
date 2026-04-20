#include "SCBWebView2.h"

#include "WebView2CompositionHost.h"
#include "WebView2Manager.h"
#include "WebView2Subsystem.h"

#include "Brushes/SlateColorBrush.h"
#include "Components/SlateWrapperTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winuser.h>
#include "Windows/HideWindowsPlatformTypes.h"

#define LOCTEXT_NAMESPACE "CBWebView2"

namespace
{
	// 键盘事件在部分路径下会回流到宿主窗口，这个标记用于避免递归重入。
	bool GCBWebView2KeyReentry = false;
}

void SCBWebView2::Construct(const FArguments& InArgs)
{
	// 先缓存 Slate 参数，再尝试创建底层原生 WebView。
	InitialUrl = InArgs._InitialUrl;
	CurrentUrl = InitialUrl;
	BackgroundColor = InArgs._BackgroundColor;
	ParentWindow = InArgs._ParentWindow;
	bShowAddressBar = InArgs._bShowAddressBar;
	bShowControls = InArgs._bShowControls;
	bShowTouchArea = InArgs._bShowTouchArea;
	bEnableTransparencyHitTest = InArgs._bEnableTransparencyHitTest;
	bShowInitialThrobber = InArgs._bShowInitialThrobber;
	bInputOnlyOnWeb = InArgs._bInputOnlyOnWeb;
	OnMessageReceived = InArgs._OnMessageReceived;
	OnNavigationCompleted = InArgs._OnNavigationCompleted;
	OnNavigationStarting = InArgs._OnNavigationStarting;
	OnNewWindowRequested = InArgs._OnNewWindowRequested;
	OnDownloadStarting = InArgs._OnDownloadStarting;
	OnDownloadUpdated = InArgs._OnDownloadUpdated;
	OnPrintToPdfCompleted = InArgs._OnPrintToPdfCompleted;
	OnMonitoredEvent = InArgs._OnMonitoredEvent;
	OnWebViewCreated = InArgs._OnWebViewCreated;
	InstanceId = FGuid::NewGuid();

	ChildSlot
	[
		// 这里统一搭建地址栏、控制按钮和实际 WebView 覆盖层。
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			.Visibility((bShowAddressBar || bShowControls) ? EVisibility::Visible : EVisibility::Collapsed)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 5.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(bShowControls ? EVisibility::Visible : EVisibility::Collapsed)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Back", "Back"))
					.IsEnabled_Lambda([this]() { return bCanGoBack; })
					.OnClicked(this, &SCBWebView2::HandleBackClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Forward", "Forward"))
					.IsEnabled_Lambda([this]() { return bCanGoForward; })
					.OnClicked(this, &SCBWebView2::HandleForwardClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(this, &SCBWebView2::GetReloadButtonText)
					.OnClicked(this, &SCBWebView2::HandleReloadClicked)
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(5.0f)
			[
				SAssignNew(AddressBarTextBox, SEditableTextBox)
				.Visibility(bShowAddressBar ? EVisibility::Visible : EVisibility::Collapsed)
				.Text(this, &SCBWebView2::GetAddressBarUrlText)
				.OnTextCommitted(this, &SCBWebView2::HandleUrlCommitted)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SCircularThrobber)
				.Radius(10.0f)
				.Visibility(this, &SCBWebView2::GetLoadingIndicatorVisibility)
			]
			+ SOverlay::Slot()
			[
				SAssignNew(Overlay, SOverlay)
			]
		]
	];

	if (TSharedPtr<SWindow> PinnedWindow = ParentWindow.Pin())
	{
		// WebView2 必须绑定到真实 HWND，因此在 Slate 构造阶段直接取宿主窗口句柄。
		if (void* NativeHandle = PinnedWindow->GetNativeWindow()->GetOSWindowHandle())
		{
			WebViewWindow = FWebView2Manager::Get().CreateWebView(
				static_cast<HWND>(NativeHandle),
				InstanceId,
				InitialUrl,
				BackgroundColor,
				bEnableTransparencyHitTest);
			BindWebViewEvents();
		}
	}
}

SCBWebView2::~SCBWebView2()
{
	BeginDestroy();
}

void SCBWebView2::BeginDestroy()
{
	// 主动关闭底层窗口，确保销毁顺序可控。
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->CloseWindow();
		WebViewWindow.Reset();
	}
}

void SCBWebView2::ExecuteScript(const FString& Script, FOnCBWebView2ScriptCallback Callback) const
{
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->ExecuteScript(Script, [Callback](const FString& Result)
		{
			if (Callback.IsBound())
			{
				Callback.Execute(Result);
			}
		});
	}
}

void SCBWebView2::LoadURL(const FString& InUrl)
{
	CurrentUrl = InUrl;
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->LoadURL(InUrl);
	}
}

void SCBWebView2::GoForward() const
{
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->GoForward();
	}
}

void SCBWebView2::GoBack() const
{
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->GoBack();
	}
}

void SCBWebView2::Reload() const
{
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->Reload();
	}
}

void SCBWebView2::Stop() const
{
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->Stop();
	}
}

void SCBWebView2::OpenDevToolsWindow() const
{
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->OpenDevToolsWindow();
	}
}

void SCBWebView2::PrintToPdf(const FString& OutputPath, bool bLandscape) const
{
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->PrintToPdf(OutputPath, bLandscape);
	}
}

void SCBWebView2::SetBackgroundColor(const FColor& InBackgroundColor)
{
	BackgroundColor = InBackgroundColor;
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->SetBackgroundColor(InBackgroundColor);
	}
}

void SCBWebView2::SetWebViewVisibility(ESlateVisibility InVisibility)
{
	// 原生层和 Slate 层都要同步可见性，否则会出现逻辑可见但原生不可见的错位。
	if (WebViewWindow.IsValid())
	{
		WebViewWindow->SetVisible(InVisibility);
	}

	EVisibility SlateVisibility = EVisibility::Visible;
	switch (InVisibility)
	{
	case ESlateVisibility::Collapsed:
		SlateVisibility = EVisibility::Collapsed;
		break;
	case ESlateVisibility::Hidden:
		SlateVisibility = EVisibility::Hidden;
		break;
	case ESlateVisibility::HitTestInvisible:
		SlateVisibility = EVisibility::HitTestInvisible;
		break;
	case ESlateVisibility::SelfHitTestInvisible:
		SlateVisibility = EVisibility::SelfHitTestInvisible;
		break;
	default:
		SlateVisibility = EVisibility::Visible;
		break;
	}

	SetVisibility(SlateVisibility);
}

ESlateVisibility SCBWebView2::GetWebViewVisibility() const
{
	return WebViewWindow.IsValid() ? WebViewWindow->GetVisible() : ESlateVisibility::Hidden;
}

void SCBWebView2::SetInputOnlyOnWeb(bool bInputOnly)
{
	bInputOnlyOnWeb = bInputOnly;
}

int32 SCBWebView2::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (WebViewWindow.IsValid())
	{
		// 利用绘制阶段拿到最终几何信息，并同步到底层原生 WebView。
		CurrentSlateScale = AllottedGeometry.Scale;

		FVector2D Offset = AllottedGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		FVector2D Size = AllottedGeometry.GetDrawSize();
		Size.X = FMath::Clamp(Size.X, 0.0f, 16000.0f);
		Size.Y = FMath::Clamp(Size.Y, 0.0f, 16000.0f);

		POINT WindowOffset{static_cast<LONG>(Offset.X), static_cast<LONG>(Offset.Y)};
		POINT WindowSize{static_cast<LONG>(Size.X), static_cast<LONG>(Size.Y)};
		if (bShowAddressBar || bShowControls)
		{
			// 顶部工具栏占用了可视区域，需要扣掉其高度。
			WindowOffset.y += static_cast<LONG>(30.0f * AllottedGeometry.Scale);
			WindowSize.y -= static_cast<LONG>(30.0f * AllottedGeometry.Scale);
		}

		WebViewWindow->SetBounds(WindowOffset, WindowSize);

		if (TSharedPtr<FWebView2CompositionHost> CompositionHost = WebViewWindow->GetCompositionHost())
		{
			// 多个 WebView 重叠时，使用 Slate LayerId 驱动原生 Visual 排序。
			if (LayerId != WebViewWindow->GetLayerId())
			{
				WebViewWindow->SetLayerId(LayerId);
				CompositionHost->RefreshVisualOrder();
			}
		}
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

FVector2D SCBWebView2::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	if (GetVisibility() == EVisibility::Collapsed)
	{
		return FVector2D::ZeroVector;
	}

	if (WebViewWindow.IsValid())
	{
		const RECT Bounds = WebViewWindow->GetBounds();
		return FVector2D(Bounds.right - Bounds.left, Bounds.bottom - Bounds.top);
	}

	return FVector2D(640.0f, 360.0f);
}

void SCBWebView2::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

FReply SCBWebView2::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// bInputOnlyOnWeb 为 true 时，忽略透明检测，直接处理输入
	if (!bInputOnlyOnWeb && !bIsHoveringInteractiveContent)
	{
		// 当前鼠标位于透明区域时，放弃命中，让下层 UMG / WebView 继续处理。
		return FReply::Unhandled();
	}

	if (WebViewWindow.IsValid())
	{
		// 命中可交互网页内容时，转发原生鼠标按下并抢占 Slate 键盘焦点。
		const FVector2D LocalPoint = GetLocalWebViewPoint(MyGeometry, MouseEvent.GetScreenSpacePosition());
		WebViewWindow->SendMouseButton(LocalPoint, MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton, true);
		if (UWebView2Subsystem* Subsystem = UWebView2Subsystem::Get())
		{
			Subsystem->SetWebViewFocused(true);
		}
		return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}

	return FReply::Unhandled();
}

FReply SCBWebView2::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (WebViewWindow.IsValid())
	{
		const FVector2D LocalPoint = GetLocalWebViewPoint(MyGeometry, MouseEvent.GetScreenSpacePosition());
		WebViewWindow->SendMouseButton(LocalPoint, MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton, false);
		return HasMouseCapture() ? FReply::Handled().ReleaseMouseCapture() : FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SCBWebView2::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (WebViewWindow.IsValid())
	{
		// 即使当前区域最终不命中，也要把 move 传下去，让网页实时更新透明命中状态。
		const FVector2D LocalPoint = GetLocalWebViewPoint(MyGeometry, MouseEvent.GetScreenSpacePosition());
		WebViewWindow->SendMouseMove(LocalPoint);
		// bInputOnlyOnWeb 为 true 时，始终返回 Handled，不传递给下层
		return (bInputOnlyOnWeb || bIsHoveringInteractiveContent) ? FReply::Handled() : FReply::Unhandled();
	}

	return FReply::Unhandled();
}

FReply SCBWebView2::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (WebViewWindow.IsValid())
	{
		const FVector2D LocalPoint = GetLocalWebViewPoint(MyGeometry, MouseEvent.GetScreenSpacePosition());
		WebViewWindow->SendMouseWheel(LocalPoint, MouseEvent.GetWheelDelta());
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SCBWebView2::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	if (WebViewWindow.IsValid())
	{
		// Slate 获得焦点后，把真实输入焦点同步给底层 WebView。
		SetVisibility(EVisibility::Visible);
		WebViewWindow->MoveFocus(true);
		if (UWebView2Subsystem* Subsystem = UWebView2Subsystem::Get())
		{
			Subsystem->SetWebViewFocused(true);
		}
	}

	return FReply::Handled();
}

void SCBWebView2::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	if (WebViewWindow.IsValid())
	{
		// 焦点离开时通知 WebView，并把宿主 HWND 重新设为系统焦点持有者。
		WebViewWindow->MoveFocus(false);

		if (TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().GetActiveTopLevelWindow())
		{
			if (CurrentWindow->GetNativeWindow().IsValid())
			{
				if (void* NativeHandle = CurrentWindow->GetNativeWindow()->GetOSWindowHandle())
				{
					::SetFocus(static_cast<HWND>(NativeHandle));
				}
			}
		}

		if (UWebView2Subsystem* Subsystem = UWebView2Subsystem::Get())
		{
			Subsystem->SetWebViewFocused(false);
		}
	}

	// bInputOnlyOnWeb 为 true 时，保持控件始终可命中，不根据透明状态切换
	if (!bInputOnlyOnWeb)
	{
		SetVisibility(bIsHoveringInteractiveContent ? EVisibility::Visible : EVisibility::HitTestInvisible);
	}
	else
	{
		SetVisibility(EVisibility::Visible);
	}

	SCompoundWidget::OnFocusLost(InFocusEvent);
}

bool SCBWebView2::SupportsKeyboardFocus() const
{
	return true;
}

FReply SCBWebView2::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (GCBWebView2KeyReentry || !WebViewWindow.IsValid())
	{
		return WebViewWindow.IsValid() ? FReply::Handled() : FReply::Unhandled();
	}

	GCBWebView2KeyReentry = true;
	// Slate 键盘事件转换成 Win32 风格消息参数，再交给宿主窗口转发给 WebView2。
	const uint32 KeyCode = InKeyEvent.GetKeyCode();
	const uint32 ScanCode = MapVirtualKey(KeyCode, 0);
	uint64 LParam = 1 | (static_cast<uint64>(ScanCode) << 16);
	if (InKeyEvent.IsRepeat())
	{
		LParam |= 0x40000000;
	}

	WebViewWindow->SendKeyboardMessage(WM_KEYDOWN, KeyCode, static_cast<int64>(LParam));
	GCBWebView2KeyReentry = false;
	return FReply::Handled();
}

FReply SCBWebView2::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (GCBWebView2KeyReentry || !WebViewWindow.IsValid())
	{
		return WebViewWindow.IsValid() ? FReply::Handled() : FReply::Unhandled();
	}

	GCBWebView2KeyReentry = true;
	const uint32 KeyCode = InKeyEvent.GetKeyCode();
	const uint32 ScanCode = MapVirtualKey(KeyCode, 0);
	const uint64 LParam = 1 | (static_cast<uint64>(ScanCode) << 16) | 0xC0000000;
	WebViewWindow->SendKeyboardMessage(WM_KEYUP, KeyCode, static_cast<int64>(LParam));
	GCBWebView2KeyReentry = false;
	return FReply::Handled();
}

FReply SCBWebView2::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	if (GCBWebView2KeyReentry || !WebViewWindow.IsValid())
	{
		return WebViewWindow.IsValid() ? FReply::Handled() : FReply::Unhandled();
	}

	GCBWebView2KeyReentry = true;
	WebViewWindow->SendKeyboardMessage(WM_CHAR, InCharacterEvent.GetCharacter(), 1);
	GCBWebView2KeyReentry = false;
	return FReply::Handled();
}

FReply SCBWebView2::HandleBackClicked()
{
	GoBack();
	return FReply::Handled();
}

FReply SCBWebView2::HandleForwardClicked()
{
	GoForward();
	return FReply::Handled();
}

FReply SCBWebView2::HandleReloadClicked()
{
	if (WebViewWindow.IsValid() && WebViewWindow->GetDocumentLoadingState() == ECBWebView2DocumentState::Loading)
	{
		Stop();
	}
	else
	{
		Reload();
	}

	return FReply::Handled();
}

void SCBWebView2::HandleUrlCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		LoadURL(NewText.ToString());
	}
}

FText SCBWebView2::GetReloadButtonText() const
{
	return (WebViewWindow.IsValid() && WebViewWindow->GetDocumentLoadingState() == ECBWebView2DocumentState::Loading)
		? LOCTEXT("Stop", "Stop")
		: LOCTEXT("Reload", "Reload");
}

FText SCBWebView2::GetTitleText() const
{
	return FText::FromString(CurrentTitle);
}

FText SCBWebView2::GetAddressBarUrlText() const
{
	return FText::FromString(CurrentUrl);
}

EVisibility SCBWebView2::GetLoadingIndicatorVisibility() const
{
	return (bShowInitialThrobber && WebViewWindow.IsValid() && !WebViewWindow->IsInitialized())
		? EVisibility::Visible
		: EVisibility::Hidden;
}

FVector2D SCBWebView2::GetLocalWebViewPoint(const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition) const
{
	FVector2D LocalPoint = MyGeometry.AbsoluteToLocal(ScreenSpacePosition);
	if (bShowAddressBar || bShowControls)
	{
		LocalPoint.Y -= 30.0f;
	}

	return LocalPoint * MyGeometry.Scale;
}

void SCBWebView2::BindWebViewEvents()
{
	if (!WebViewWindow.IsValid())
	{
		return;
	}

	// 这里把原生层事件统一桥接到 Slate 内部状态与外部委托。
	WebViewWindow->OnMessageReceived.BindSP(this, &SCBWebView2::HandleMessageFromWeb);
	WebViewWindow->OnNavigationCompleted.BindLambda([this](bool bSuccess)
	{
		OnNavigationCompleted.ExecuteIfBound(bSuccess);
	});

	WebViewWindow->OnNavigationStarting.BindLambda([this](const FString& Url)
	{
		CurrentUrl = Url;
		OnNavigationStarting.ExecuteIfBound(Url);
	});

	WebViewWindow->OnNewWindowRequested.BindLambda([this](const FString& Url)
	{
		OnNewWindowRequested.ExecuteIfBound(Url);
	});

	WebViewWindow->OnDownloadStarting.BindLambda([this](const FWebView2DownloadInfo& DownloadInfo)
	{
		OnDownloadStarting.ExecuteIfBound(DownloadInfo);
	});

	WebViewWindow->OnDownloadUpdated.BindLambda([this](const FWebView2DownloadInfo& DownloadInfo)
	{
		OnDownloadUpdated.ExecuteIfBound(DownloadInfo);
	});

	WebViewWindow->OnPrintToPdfCompleted.BindLambda([this](bool bSuccess, const FString& OutputPath)
	{
		OnPrintToPdfCompleted.ExecuteIfBound(bSuccess, OutputPath);
	});

	WebViewWindow->OnMonitoredEvent.BindLambda([this](const FCBWebView2MonitoredEvent& EventInfo)
	{
		OnMonitoredEvent.ExecuteIfBound(EventInfo);
	});

	WebViewWindow->OnInputActivationRequested.BindLambda([this]()
	{
		// 当原生 WinComp 路由确认当前 WebView 应接管输入时，主动拉起 Slate 键盘焦点。
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::Mouse);
		if (UWebView2Subsystem* Subsystem = UWebView2Subsystem::Get())
		{
			Subsystem->SetWebViewFocused(true);
		}
	});

	WebViewWindow->OnCanGoBackChanged.BindLambda([this](bool bValue)
	{
		bCanGoBack = bValue;
	});

	WebViewWindow->OnCanGoForwardChanged.BindLambda([this](bool bValue)
	{
		bCanGoForward = bValue;
	});

	WebViewWindow->OnDocumentTitleChanged.BindLambda([this](const FString& Title)
	{
		CurrentTitle = Title;
	});

	WebViewWindow->OnSourceChanged.BindLambda([this](const FString& Url)
	{
		CurrentUrl = Url;
	});

	WebViewWindow->OnCursorChanged.BindLambda([this](EMouseCursor::Type CursorType)
	{
		SetCursor(CursorType);
	});

	WebViewWindow->OnWebViewCreated.BindLambda([this](bool bSuccess)
	{
		OnWebViewCreated.ExecuteIfBound(bSuccess);
	});
}

void SCBWebView2::HandleMessageFromWeb(const FString& Message)
{
	if (Message.Contains(TEXT("IsClickable:")))
	{
		// bInputOnlyOnWeb 为 true 时，忽略透明检测消息，保持原有输入处理
		if (bInputOnlyOnWeb)
		{
			return;
		}

		// transparency_check.js 会发送 IsClickable:0/1，表示鼠标当前位置是否命中可交互 DOM。
			const bool bNewHoverState = Message.Contains(TEXT("1"));

			if (bNewHoverState != bIsHoveringInteractiveContent)
			{
				UE_LOG(LogTemp, Log, TEXT("SCBWebView2 Transparency Change: Old=%d, New=%d"), bIsHoveringInteractiveContent, bNewHoverState);
			}

			bIsHoveringInteractiveContent = bNewHoverState;
			// 网页消息只负责决定当前区域是否允许鼠标命中，
			// 不能再拿它去覆盖真实的键盘焦点状态，否则输入焦点会在 0/1 间来回抖动。
			if (WebViewWindow.IsValid())
			{
				// 将网页穿透状态同步到底层独立命中标记，
				// 让多个独立 UMG WebView 在 WinComp 路由时按可交互性而不是纯可见性竞争命中。
				WebViewWindow->SetHitTestEnabled(bNewHoverState);
			}

			if (!bNewHoverState)
			{
				if (HasMouseCapture() && FSlateApplication::IsInitialized())
				{
					FSlateApplication::Get().ReleaseAllPointerCapture();
				}
			}

			SetVisibility(bNewHoverState ? EVisibility::Visible : EVisibility::HitTestInvisible);
	}

	// 无论是否为透明检测消息，都继续向上层广播，方便业务自行解析网页消息。
	OnMessageReceived.ExecuteIfBound(Message);
}

#undef LOCTEXT_NAMESPACE