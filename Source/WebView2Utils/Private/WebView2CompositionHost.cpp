#include "WebView2CompositionHost.h"

#include "WebView2Log.h"
#include "WebView2Window.h"

namespace
{
	// 鼠标移动消息需要广播给重叠 WebView，用于更新透明穿透状态。
	bool IsMouseMoveMessage(const UINT Message)
	{
		return Message == WM_MOUSEMOVE || Message == WM_NCMOUSEMOVE;
	}

	bool IsMouseButtonDownMessage(const UINT Message)
	{
		// 鼠标按下消息会触发输入激活请求。
		return Message == WM_LBUTTONDOWN || Message == WM_RBUTTONDOWN || Message == WM_MBUTTONDOWN ||
			Message == WM_NCLBUTTONDOWN || Message == WM_NCRBUTTONDOWN || Message == WM_NCMBUTTONDOWN;
	}
}

FWebView2CompositionHost::FWebView2CompositionHost(
	HWND InWindowHandle,
	winrt::Windows::System::DispatcherQueueController InDispatcherQueueController)
	: WindowHandle(InWindowHandle)
	, DispatcherQueueController(InDispatcherQueueController)
{
	if (DispatcherQueueController)
	{
		Compositor = winrt::Windows::UI::Composition::Compositor();
	}
}

FWebView2CompositionHost::~FWebView2CompositionHost()
{
	Destroy();
}

void FWebView2CompositionHost::Initialize()
{
	if (!Compositor)
	{
		return;
	}

	// 初始化当前宿主窗口的 WinComp 根节点。
	CreateDesktopTarget();
	CreateRootVisual();
}

void FWebView2CompositionHost::AttachWebView(const TSharedRef<FWebView2Window>& WebViewWindow)
{
	if (!WebViewLayer || !WebViewWindow->CompositionController)
	{
		return;
	}

	// 每个 WebView 使用独立 ContainerVisual，便于单独定位和排序。
	winrt::Windows::UI::Composition::ContainerVisual Container = Compositor.CreateContainerVisual();
	const RECT Bounds = WebViewWindow->GetBounds();
	Container.Offset({static_cast<float>(Bounds.left), static_cast<float>(Bounds.top), 0.0f});
	Container.Size({
		static_cast<float>(Bounds.right - Bounds.left),
		static_cast<float>(Bounds.bottom - Bounds.top)});

	WebViewLayer.Children().InsertAtTop(Container);
	WebViewWindow->SetContainerVisual(Container);
	WebViewWindow->CompositionController->put_RootVisualTarget(Container.as<IUnknown>().get());

	++NextLayerId;
	WebViewWindow->SetLayerId(NextLayerId);
	WebViews.Add(WebViewWindow->GetInstanceId().ToString(), WebViewWindow);
}

void FWebView2CompositionHost::RefreshVisualOrder()
{
	if (!WebViewLayer)
	{
		return;
	}

	TMap<int32, TSharedRef<FWebView2Window>> Sorted;
	for (const TPair<FString, TWeakPtr<FWebView2Window>>& Pair : WebViews)
	{
		if (const TSharedPtr<FWebView2Window> WebViewWindow = Pair.Value.Pin())
		{
			Sorted.Add(WebViewWindow->GetLayerId(), WebViewWindow.ToSharedRef());
		}
	}

	Sorted.KeySort([](const int32 A, const int32 B)
	{
		return A < B;
	});

	for (const TPair<int32, TSharedRef<FWebView2Window>>& Pair : Sorted)
	{
		WebViewLayer.Children().Remove(Pair.Value->WebViewVisual);
		WebViewLayer.Children().InsertAtTop(Pair.Value->WebViewVisual);
	}
}

void FWebView2CompositionHost::DetachWebView(const FGuid& InstanceId, const winrt::Windows::UI::Composition::ContainerVisual& WebViewContainer)
{
	WebViews.Remove(InstanceId.ToString());
	if (WebViewLayer && WebViewContainer)
	{
		WebViewLayer.Children().Remove(WebViewContainer);
	}
}

void FWebView2CompositionHost::Destroy()
{
	for (const TPair<FString, TWeakPtr<FWebView2Window>>& Pair : WebViews)
	{
		if (const TSharedPtr<FWebView2Window> WebViewWindow = Pair.Value.Pin())
		{
			if (WebViewLayer && WebViewWindow->WebViewVisual)
			{
				WebViewLayer.Children().Remove(WebViewWindow->WebViewVisual);
			}
		}
	}

	WebViews.Empty();

	if (RootVisual)
	{
		RootVisual.Children().RemoveAll();
		RootVisual = nullptr;
	}

	if (WindowTarget)
	{
		WindowTarget.Root(nullptr);
		WindowTarget = nullptr;
	}

	WebViewLayer = nullptr;
}

bool FWebView2CompositionHost::HandleMouseMessage(UINT Message, WPARAM WParam, LPARAM LParam)
{
	if (!RootVisual || Message == WM_CLOSE)
	{
		return false;
	}

	// 先计算宿主窗口客户区坐标。
	POINT Point;
	POINTSTOPOINT(Point, LParam);
	if (Message == WM_MOUSEWHEEL || Message == WM_MOUSEHWHEEL || Message == WM_NCRBUTTONDOWN || Message == WM_NCRBUTTONUP)
	{
		::ScreenToClient(WindowHandle, &Point);
	}

	const TArray<TSharedRef<FWebView2Window>> VisibleWebViewsUnderCursor = FindWebViewsAtPoint(Point, false);
	if (VisibleWebViewsUnderCursor.IsEmpty())
	{
		return false;
	}

	// 只在当前可交互的 WebView 中挑出真正接收点击/滚轮的顶部目标。
	const TArray<TSharedRef<FWebView2Window>> InteractiveWebViewsUnderCursor = FindWebViewsAtPoint(Point, true);

	TSharedPtr<FWebView2Window> TopMost;
	for (const TSharedRef<FWebView2Window>& WebViewWindow : InteractiveWebViewsUnderCursor)
	{
		if (!TopMost.IsValid() || TopMost->GetLayerId() < WebViewWindow->GetLayerId())
		{
			TopMost = WebViewWindow;
		}
	}

	if (IsMouseMoveMessage(Message))
	{
		// move 会广播给所有重叠且启用透明命中检测的 WebView，
		// 这样上层透明后，下层网页也能及时刷新自己的命中状态。
		for (const TSharedRef<FWebView2Window>& WebViewWindow : VisibleWebViewsUnderCursor)
		{
			if (!WebViewWindow->CompositionController || !WebViewWindow->IsTransparencyHitTestEnabled())
			{
				continue;
			}

			if (TopMost.IsValid() && TopMost->GetInstanceId() == WebViewWindow->GetInstanceId())
			{
				continue;
			}

			POINT LocalPoint = Point;
			const RECT Bounds = WebViewWindow->GetBounds();
			LocalPoint.x -= Bounds.left;
			LocalPoint.y -= Bounds.top;

			WebViewWindow->CompositionController->SendMouseInput(
				COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE,
				static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(GET_KEYSTATE_WPARAM(WParam)),
				0,
				LocalPoint);
		}
	}

	if (!TopMost.IsValid() || !TopMost->CompositionController)
	{
		return false;
	}

	if (IsMouseButtonDownMessage(Message))
	{
		// 鼠标按下前通知目标 WebView 请求上层 Slate 获取键盘焦点。
		TopMost->OnInputActivationRequested.ExecuteIfBound();
	}

	DWORD MouseData = 0;
	if (Message == WM_MOUSEWHEEL || Message == WM_MOUSEHWHEEL)
	{
		MouseData = GET_WHEEL_DELTA_WPARAM(WParam);
	}

	RECT Bounds = TopMost->GetBounds();
	Point.x -= Bounds.left;
	Point.y -= Bounds.top;

	TopMost->CompositionController->SendMouseInput(
		static_cast<COREWEBVIEW2_MOUSE_EVENT_KIND>(Message),
		static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(GET_KEYSTATE_WPARAM(WParam)),
		MouseData,
		Point);

	return true;
}

HWND FWebView2CompositionHost::GetWindowHandle() const
{
	return WindowHandle;
}

void FWebView2CompositionHost::CreateDesktopTarget()
{
	namespace DesktopAbi = ABI::Windows::UI::Composition::Desktop;
	auto Interop = Compositor.as<DesktopAbi::ICompositorDesktopInterop>();
	winrt::check_hresult(Interop->CreateDesktopWindowTarget(
		WindowHandle,
		false,
		reinterpret_cast<DesktopAbi::IDesktopWindowTarget**>(winrt::put_abi(WindowTarget))));
}

void FWebView2CompositionHost::CreateRootVisual()
{
	RootVisual = Compositor.CreateContainerVisual();
	RootVisual.RelativeSizeAdjustment({1.0f, 1.0f});
	WindowTarget.Root(RootVisual);

	WebViewLayer = Compositor.CreateContainerVisual();
	WebViewLayer.RelativeSizeAdjustment({1.0f, 1.0f});
	RootVisual.Children().InsertAtTop(WebViewLayer);
}

TArray<TSharedRef<FWebView2Window>> FWebView2CompositionHost::FindWebViewsAtPoint(const POINT& ClientPoint, bool bRequireHitTestEnabled) const
{
	TArray<TSharedRef<FWebView2Window>> Result;
	for (const TPair<FString, TWeakPtr<FWebView2Window>>& Pair : WebViews)
	{
		const TSharedPtr<FWebView2Window> WebViewWindow = Pair.Value.Pin();
		if (!WebViewWindow.IsValid())
		{
			continue;
		}

		if (!WebViewWindow->WebViewVisual || WebViewWindow->GetVisible() != ESlateVisibility::Visible)
		{
			continue;
		}

		if (bRequireHitTestEnabled && !WebViewWindow->IsHitTestEnabled())
		{
			// 透明区域已声明不可命中时，不参与点击目标竞争。
			continue;
		}

		auto Offset = WebViewWindow->WebViewVisual.Offset();
		auto Size = WebViewWindow->WebViewVisual.Size();
		if (
			ClientPoint.x >= Offset.x &&
			ClientPoint.x < Offset.x + Size.x &&
			ClientPoint.y >= Offset.y &&
			ClientPoint.y < Offset.y + Size.y)
		{
			Result.Add(WebViewWindow.ToSharedRef());
		}
	}

	return Result;
}