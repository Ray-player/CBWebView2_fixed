#include "WebView2Subsystem.h"

#include "WebView2Log.h"
#include "WebView2Manager.h"

#include "Framework/Application/SlateApplication.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsApplication.h"
#include "Windows/WindowsHWrapper.h"
#endif

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
	class FCBWebView2WindowsMessageHandler : public IWindowsMessageHandler
	{
	public:
		virtual bool ProcessMessage(HWND Hwnd, uint32 Msg, WPARAM WParam, LPARAM LParam, int32& OutResult) override
		{
			// 当 WebView 未持有真实键盘焦点时，不拦截宿主窗口键盘消息。
			UWebView2Subsystem* Subsystem = UWebView2Subsystem::Get();
			if ((Msg == WM_KEYDOWN || Msg == WM_KEYUP || Msg == WM_SYSKEYDOWN || Msg == WM_SYSKEYUP) &&
				(!Subsystem || !Subsystem->IsWebViewFocused()))
			{
				return false;
			}

			return FWebView2Manager::Get().HandleWindowMessage(Hwnd, Msg, WParam, LParam);
		}
	};

	FCBWebView2WindowsMessageHandler GWebView2MessageHandler;
}

void UWebView2Subsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 先初始化全局管理器，再挂接 Windows 消息处理器。
	FWebView2Manager::Get().Initialize();

#if PLATFORM_WINDOWS
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<FWindowsApplication> WindowsApplication = StaticCastSharedPtr<FWindowsApplication>(FSlateApplication::Get().GetPlatformApplication());
		if (WindowsApplication.IsValid())
		{
			WindowsApplication->AddMessageHandler(GWebView2MessageHandler);
		}
	}
#endif

#if WITH_EDITOR
	FEditorDelegates::EndPIE.AddUObject(this, &UWebView2Subsystem::OnEndPIE);
#endif
}

void UWebView2Subsystem::Deinitialize()
{
	// 先移除编辑器和窗口回调，再整体关闭管理器。
#if WITH_EDITOR
	FEditorDelegates::EndPIE.RemoveAll(this);
#endif

#if PLATFORM_WINDOWS
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<FWindowsApplication> WindowsApplication = StaticCastSharedPtr<FWindowsApplication>(FSlateApplication::Get().GetPlatformApplication());
		if (WindowsApplication.IsValid())
		{
			WindowsApplication->RemoveMessageHandler(GWebView2MessageHandler);
		}
	}
#endif

	FWebView2Manager::Get().Shutdown();
	Super::Deinitialize();
}

void UWebView2Subsystem::Tick(float DeltaTime)
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

#if PLATFORM_WINDOWS
	if (!bIsWebViewFocused)
	{
		// WebView 不持有焦点时，把焦点归还给宿主 HWND，避免输入悬空在隐藏的子路径上。
		if (TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().GetActiveTopLevelWindow())
		{
			if (void* NativeHandle = CurrentWindow->GetNativeWindow()->GetOSWindowHandle())
			{
				HWND NativeWindow = static_cast<HWND>(NativeHandle);
				if (::GetForegroundWindow() == NativeWindow && ::GetFocus() != NativeWindow)
				{
					::SetFocus(NativeWindow);
				}
			}
		}
	}
#endif
}

bool UWebView2Subsystem::IsTickable() const
{
	return true;
}

TStatId UWebView2Subsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWebView2Subsystem, STATGROUP_Tickables);
}

UWebView2Subsystem* UWebView2Subsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UWebView2Subsystem>() : nullptr;
}

void UWebView2Subsystem::SetWebViewFocused(bool bInFocused)
{
	// 这里只维护真实输入焦点，不参与透明穿透状态判断。
	bIsWebViewFocused = bInFocused;
}

bool UWebView2Subsystem::IsWebViewFocused() const
{
	return bIsWebViewFocused;
}

void UWebView2Subsystem::OnEndPIE(bool bIsSimulating)
{
#if PLATFORM_WINDOWS
	if (GEngine && GEngine->GameViewport)
	{
		if (TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow())
		{
			if (void* NativeHandle = Window->GetNativeWindow()->GetOSWindowHandle())
			{
				FWebView2Manager::Get().OnHostWindowClosed(static_cast<HWND>(NativeHandle));
			}
		}
	}
#endif
}