#include "WebView2CompositionHost.h"

#include "WebView2Log.h"
#include "WebView2Settings.h"
#include "WebView2Window.h"
#include "Components/SlateWrapperTypes.h"

FWebView2CompositionHost::FWebView2CompositionHost(HWND InHwnd, winrt::Windows::System::DispatcherQueueController QueueController)
	:MDispatcherQueueController(QueueController)
	,MainHwnd(InHwnd)
	,ClickLayerID(0)
	
{
	UWebView2Settings* Settings=UWebView2Settings::Get();

	if(Settings->WebView2Mode ==EWebView2Mode::VISUAL_WINCOMP)
	{
		if (QueueController)
		{
			MCompositor = winrt::Windows::UI::Composition::Compositor();
		}
	}
}

FWebView2CompositionHost::~FWebView2CompositionHost()
{
	DestroyWinCompVisualTree();
}

void FWebView2CompositionHost::Initialize()
{
	if(MCompositor)
	{
		CreateDesktopWindowTarget();
		//跟图元，里面用于放多个webview空间
		CreateCompositionRoot();
	}

}

void FWebView2CompositionHost::CreateWebViewVisual(TSharedRef<FWebView2Window> WebView2Window)
{
	UE_LOG(LogWebView2,Log,TEXT("WebView2Window is Created"))
	
	if(UIVisual )
	{
		if (WebView2Window->Controller)
		{
			winrt::Windows::UI::Composition::ContainerVisual NewChild = MCompositor.CreateContainerVisual();
			UIVisual.Children().InsertAtTop(NewChild);
			RECT bound = WebView2Window->GetBounds();
			NewChild.Offset({static_cast<float>(bound.left), static_cast<float>(bound.top), 0});
			const winrt::Windows::Foundation::Numerics::float2 WebViewSize = {
				(static_cast<float>(bound.right - bound.left)),
				(static_cast<float>(bound.bottom - bound.top))
			};
			NewChild.Size(WebViewSize);
			// webViewVisual.CompositeMode()
			WebView2Window->SetContainerVisual(NewChild);
			WebView2Window->CompositionController->put_RootVisualTarget(NewChild.as<IUnknown>().get());
			
			//每创建一个新的UI，则增加一下层级ID
			ClickLayerID++;
			WebView2Window->SetLayerID(ClickLayerID);
		}
		
	}

	WebViewWindowMap.Add(WebView2Window->GetUniqueID().ToString(),WebView2Window);
	UE_LOG(LogWebView2,Log,TEXT("WebView2Window is Created"))
	
}

void FWebView2CompositionHost::RefreshWebViewVisual()
{
	if(UIVisual)
	{
		TMap<int32,TSharedRef<FWebView2Window>> LayerWebMap;
		for(TPair<FString,TSharedRef<FWebView2Window>> &Element:WebViewWindowMap)
		{
			if(Element.Value.ToSharedPtr().IsValid())
			{
				LayerWebMap.Add(Element.Value->GetLayerID(),Element.Value);
			}
		}

		LayerWebMap.KeySort([](const int& A, const int& B)
		{
			return (A< B);
		});

		for(TPair<int32,TSharedRef<FWebView2Window>> &Element:LayerWebMap)
		{
			UIVisual.Children().Remove(Element.Value->WebViewVisual);
			UIVisual.Children().InsertAtTop(Element.Value->WebViewVisual);
		}
	}
}

void FWebView2CompositionHost::DestroyWinCompVisualTree()
{
	if(!WebViewWindowMap.IsEmpty())
	{
		for(TPair<FString,TSharedRef<FWebView2Window>> &Element:WebViewWindowMap)
		{
			if(Element.Value.ToSharedPtr().IsValid())
			{
				if(UIVisual)
				{
					UIVisual.Children().Remove(Element.Value->WebViewVisual);
				
				}
				
			}
		}
	}
	
	
	if (MRootVisual)
	{
		MRootVisual.Children().RemoveAll();
		MRootVisual = nullptr;

		MTarget.Root(nullptr);
		MTarget = nullptr;
	}
	
}

void FWebView2CompositionHost::DestroyVisualAsWebview(TSharedRef<FWebView2Window> WebView2Window)
{
	if(WebView2Window.ToSharedPtr().IsValid())
	{
		WebViewWindowMap.Find(WebView2Window->GetUniqueID().ToString());
		WebViewWindowMap.Remove(WebView2Window->GetUniqueID().ToString());
	}
}

HWND FWebView2CompositionHost::GetMainWindowHandle() const
{
	return MainHwnd;
}

bool FWebView2CompositionHost::MouseMessage(UINT Message, WPARAM WParam, LPARAM LParam)
{
	 if (!MRootVisual)
    {
        return false;
    }
    if(Message==WM_CLOSE)
        return false;
    
    POINT point;
    POINTSTOPOINT(point, LParam);
    if (Message == WM_MOUSEWHEEL ||
        Message == WM_MOUSEHWHEEL
        || Message == WM_NCRBUTTONDOWN || Message == WM_NCRBUTTONUP
    )
    {
        //鼠标滚轮消息以屏幕坐标传递。
        //SendMouseInput需要WebView的客户端坐标，因此转换
        //从屏幕到客户端的点。
        ::ScreenToClient(MainHwnd, &point);
    }

   TArray<TSharedRef<FWebView2Window>> Focus_UI = FindWebviewFromPoint(point);
    if (Focus_UI.Num()==0)
    {
    	
    	return false;
    }
        

   TSharedPtr<FWebView2Window> TopUI=nullptr;
    for(auto It = Focus_UI.begin();It!=Focus_UI.end();++It)
    {

        if(!TopUI)
        {
            TopUI = (*It);
            continue;
        }
        if(TopUI->GetLayerID()<(*It)->GetLayerID())
        {
            TopUI = (*It);
        }
    }

    
    DWORD MouseData = 0;
	
    switch (Message)
    {
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        MouseData = GET_WHEEL_DELTA_WPARAM(WParam);
        break;
    }
	
    if (Message != WM_MOUSELEAVE)
    {
        if(TopUI)
        {
            RECT AppBounds = TopUI->GetBounds();
            point.x -= AppBounds.left;
            point.y -= AppBounds.top;
        	
           TopUI->CompositionController->SendMouseInput(
			static_cast<COREWEBVIEW2_MOUSE_EVENT_KIND>(Message),
			static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(GET_KEYSTATE_WPARAM(WParam)), MouseData,
			point);
        }
        return true;
    }

    return false;
}

void FWebView2CompositionHost::CreateDesktopWindowTarget()
{
	namespace abi = ABI::Windows::UI::Composition::Desktop;

	auto interop = MCompositor.as<abi::ICompositorDesktopInterop>();
	winrt::check_hresult(interop->CreateDesktopWindowTarget(
		MainHwnd, false, reinterpret_cast<abi::IDesktopWindowTarget**>(winrt::put_abi(MTarget))));
}

void FWebView2CompositionHost::CreateCompositionRoot()
{
	//创建根容器
	MRootVisual = MCompositor.CreateContainerVisual();
	MRootVisual.RelativeSizeAdjustment({1.0f, 1.0f});
	MRootVisual.Offset({0, 0, 0});
	MTarget.Root(MRootVisual);

	//创建拥于存放2DUI的容器，2DUI的网页容器层级要高于3DUI容器
	UIVisual = MCompositor.CreateContainerVisual();
	MRootVisual.Children().InsertAtTop(UIVisual);
	UIVisual.RelativeSizeAdjustment({1.0f, 1.0f});
	UIVisual.Offset({0, 0, 0});
}

TArray<TSharedRef<FWebView2Window>> FWebView2CompositionHost::FindWebviewFromPoint(POINT Point)
{
	TArray<TSharedRef<FWebView2Window>> Windows;
	for(TPair<FString,TSharedRef<FWebView2Window>> &Element:WebViewWindowMap)
	{
		if(!Element.Value.ToSharedPtr())
		{
			continue;
		}
		if(Element.Value->WebViewVisual)
		{
			auto Offset = Element.Value->WebViewVisual.Offset();
			auto Size = Element.Value->WebViewVisual.Size();
			if ((Point.x >= Offset.x) && (Point.x < Offset.x + Size.x) && (Point.y >= Offset.y) &&
				(Point.y < Offset.y + Size.y))
			{
				if(Element.Value->GetVisible()==ESlateVisibility::Visible &&Element.Value->bIsMouseOverPositionArea)
				{
					Windows.Add(Element.Value);
				}
				
			}
			
		}
	}

	return Windows;
}
