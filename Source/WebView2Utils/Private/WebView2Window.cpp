// Copyright 2025-Present Xiao Lan fei. All Rights Reserved.


#include "WebView2Window.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#include <Windows.h>

#include <DispatcherQueue.h>
#include <WinUser.h>
#include <winrt/Windows.UI.Composition.Desktop.h>

#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "WebView2CompositionHost.h"
#include "WebView2FileComponent.h"
#include "WebView2Log.h"
#include "WebView2Manager.h"
#include "WebView2Settings.h"
#include "WebView2Subsystem.h"
#include "Components/SlateWrapperTypes.h"


namespace winSystem = winrt::Windows::System;

FWebView2Window::FWebView2Window(HWND InHwnd, const FGuid UniqueID, const FString InInitialURL,FColor InBackgroundColor)
	:MainWindow(InHwnd)
	,InitialURL(InInitialURL)
	,ID(UniqueID)
	,BackgroundColor(InBackgroundColor)
	,DocumentState(EWebView2DocumentState::NoDocument)
{
	InitializeWebView();
	
}

FWebView2Window::~FWebView2Window()
{
}

void FWebView2Window::InitializeWebView()
{
	bInitialized=false;
	Visible=ESlateVisibility::Visible;
	DcompDevice = nullptr;
	SetBounds(POINT(0.f,0.f),POINT(1.f,1.f));
	UWebView2Settings* Settings=UWebView2Settings::Get();

	if(Settings->WebView2Mode ==EWebView2Mode::VISUAL_DCOMP ||
		Settings->WebView2Mode == EWebView2Mode::VISUAL_DCOMP)
	{
		HRESULT HR = DCompositionCreateDevice2(nullptr, IID_PPV_ARGS(&DcompDevice));
		if(!SUCCEEDED(HR))
		{
			UE_LOG(LogWebView2,Error,TEXT("Create with Windowless DComp Visual Failed:"
								 "DComp device creation failed."
								 "Current OS may not support DComp."))
			return;
		}
	}
	//! [CreateCoreWebView2EnvironmentWithOptions]
	FString Args;
	Args.Append(TEXT("-enable-features=ThirdPartyStoragePartitioning,PartitionedCookies"));
	auto Options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
	Options->put_AdditionalBrowserArguments(*Args);
	Options->put_AdditionalBrowserArguments(L"--autoplay-policy=no-user-gesture-required");
	//Options->put_AdditionalBrowserArguments(L"--disable-frame-rate-limit");
	Options->put_AllowSingleSignOnUsingOSPrimaryAccount(Settings->EnvironmentOptions.bAADSSOEnabled);
	Options->put_ExclusiveUserDataFolderAccess(
		Settings->EnvironmentOptions.bExclusiveUserDataFolderAccess);
	if (!Settings->EnvironmentOptions.Language.IsEmpty())
		Options->put_Language(*(Settings->EnvironmentOptions.Language));
	Options->put_IsCustomCrashReportingEnabled(Settings->EnvironmentOptions.bCustomCrashReportingEnabled);


	//! [CoreWebView2CustomSchemeRegistration]
	Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions4> options4;
	if (Options.As(&options4) == S_OK)
	{
		const WCHAR* AllowedOrigins[1] = {L"https://*.example.com"};
		
		auto customSchemeRegistration =
		  Microsoft::WRL::Make<CoreWebView2CustomSchemeRegistration>(L"custom-scheme");
		customSchemeRegistration->SetAllowedOrigins(
			1,AllowedOrigins);
		auto customSchemeRegistration2 =
			Microsoft::WRL::Make<CoreWebView2CustomSchemeRegistration>(L"wv2rocks");
		customSchemeRegistration2->put_TreatAsSecure(true);
		customSchemeRegistration2->SetAllowedOrigins(1, AllowedOrigins);
		customSchemeRegistration2->put_HasAuthorityComponent(true);
		auto customSchemeRegistration3 =
			Microsoft::WRL::Make<CoreWebView2CustomSchemeRegistration>(
				L"custom-scheme-not-in-allowed-origins");
		ICoreWebView2CustomSchemeRegistration* registrations[3] = {
			customSchemeRegistration.Get(), customSchemeRegistration2.Get(),
			customSchemeRegistration3.Get()};
		options4->SetCustomSchemeRegistrations(
			2, static_cast<ICoreWebView2CustomSchemeRegistration**>(registrations));
	}

	//! [CoreWebView2CustomSchemeRegistration]

	Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions5> Options5;
	if (Options.As(&Options5) == S_OK)
	{
	
		Options5->put_EnableTrackingPrevention(Settings->EnvironmentOptions.bTrackingPreventionEnabled );
	}

	Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions6> Options6;
	if (Options.As(&Options6) == S_OK)
	{
		Options6->put_AreBrowserExtensionsEnabled(true);
	}

	Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions8> Options8;
	if (Options.As(&Options8) == S_OK)
	{
		COREWEBVIEW2_SCROLLBAR_STYLE style = COREWEBVIEW2_SCROLLBAR_STYLE_FLUENT_OVERLAY;
		Options8->put_ScrollBarStyle(style);
	}

	HRESULT Hresult = CreateCoreWebView2EnvironmentWithOptions(
		nullptr, nullptr, Options.Get(),
		Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			this, &FWebView2Window::OnCreateEnvironmentCompleted)
			.Get());

	if (!SUCCEEDED(Hresult))
	{
		switch (Hresult)
		{
		case HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND):
			{
				UE_LOG(LogWebView2,Error,TEXT("Couldn't find Edge WebView2 Runtime.Do you have a version installed?"));
			}
			break;
		case HRESULT_FROM_WIN32(ERROR_FILE_EXISTS):
			{
				UE_LOG(LogWebView2,Error,TEXT("User data folder cannot be created because a file with the same name already"));
			}
			break;
		case E_ACCESSDENIED:
			{
				UE_LOG(LogWebView2,Error,TEXT("Unable to create user data folder, Access Denied."));
			}
			break;
		case E_FAIL:
			{
				UE_LOG(LogWebView2,Error,TEXT("Edge runtime unable to start"));
			}
			break;
		default:
			{
				UE_LOG(LogWebView2,Error,TEXT("Failed to create WebView2 environment"));
			}
		}
	}
}

HRESULT FWebView2Window::CreateControllerWithOptions()
{
	//! [CreateControllerWithOptions]
	 Microsoft::WRL::ComPtr<ICoreWebView2Environment10> webViewEnvironment10;

	WebViewEnvironment.As(&webViewEnvironment10);
	if (!webViewEnvironment10)
	{
		UE_LOG(LogWebView2,Error,TEXT("Failed to create WebView2 environment"));
		return S_OK;
	}

	Microsoft::WRL::ComPtr<ICoreWebView2ControllerOptions> options;
    // The validation of parameters occurs when setting the properties.
    HRESULT hr = webViewEnvironment10->CreateCoreWebView2ControllerOptions(&options);
    if (hr == E_INVALIDARG)
    {

    	UE_LOG(LogWebView2,Error,TEXT("Unable to create WebView2 due to an invalid profile name."));
        CloseWindow();
        return S_OK;
    }
    //! [CreateControllerWithOptions] 

	UWebView2Settings* Settings=UWebView2Settings::Get();
	
	// 如果使用无效的配置文件名称调用“put_ProfileName”，则会立即返回“E_INVALIDARG”。ProfileName 可以重复使用。
    options->put_ProfileName(*Settings->WebViewCreateOption.Profile);
    options->put_IsInPrivateModeEnabled(Settings->WebViewCreateOption.bInPrivate);

    //! [ScriptLocaleSetting]
   Microsoft::WRL::ComPtr<ICoreWebView2ControllerOptions2> webView2ControllerOptions2;
    if (SUCCEEDED(options->QueryInterface(IID_PPV_ARGS(&webView2ControllerOptions2))))
    {
        if (Settings->WebViewCreateOption.bUseOSRegion)
        {
            wchar_t osLocale[LOCALE_NAME_MAX_LENGTH] = {0};
            GetUserDefaultLocaleName(osLocale, LOCALE_NAME_MAX_LENGTH);
            webView2ControllerOptions2->put_ScriptLocale(osLocale);
        }
        else if (!Settings->WebViewCreateOption.ScriptLocale.IsEmpty())
        {
            webView2ControllerOptions2->put_ScriptLocale(*Settings->WebViewCreateOption.ScriptLocale);
        }
    }
    //! [ScriptLocaleSetting]

    //! [AllowHostInputProcessing]
	Microsoft::WRL::ComPtr<ICoreWebView2ExperimentalControllerOptions2>
	webView2ExperimentalControllerOptions2;
	if (SUCCEEDED(
		options->QueryInterface(IID_PPV_ARGS(&webView2ExperimentalControllerOptions2))))
        {
			//用于启用/禁用在传递到 WebView2 之前通过应用程序传递的输入
			webView2ExperimentalControllerOptions2->put_AllowHostInputProcessing(true);
        }
    
    //! [AllowHostInputProcessing]
    if (DcompDevice || FWebView2Manager::GetInstance()->DispatcherQueueController)
    {
        //! [OnCreateCoreWebView2ControllerCompleted]
        webViewEnvironment10->CreateCoreWebView2CompositionControllerWithOptions(
            MainWindow, options.Get(),
            Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
                [this](
                    HRESULT result,
                    ICoreWebView2CompositionController* compositionController) -> HRESULT
                {
                	CompositionController=compositionController;
                     Microsoft::WRL::ComPtr<ICoreWebView2Controller> controlle;
                     	CompositionController.As(&controlle);
                    return OnCreateCoreWebView2ControllerCompleted(result, controlle.Get());
                })
                .Get());
        //! [OnCreateCoreWebView2ControllerCompleted]
    }
    else
    {
       webViewEnvironment10->CreateCoreWebView2ControllerWithOptions(
            MainWindow, options.Get(),
            Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                this, &FWebView2Window::OnCreateCoreWebView2ControllerCompleted)
                .Get());
    }

    return S_OK;
}


// 我们有自己的 DCompositionCreateDevice2 实现，可动态加载 dcomp.dll 来创建设备。
// 不依赖 dcomp.dll 可使示例应用程序在不支持 dcomp 的 Windows 版本上运行。
HRESULT FWebView2Window::DCompositionCreateDevice2(IUnknown* RenderingDevice, const IID& Riid, void** PPV)
{
	
	HRESULT Result = E_FAIL;
	static decltype(::DCompositionCreateDevice2)* fnCreateDCompDevice2 = nullptr;
	if (fnCreateDCompDevice2 == nullptr)
	{
		HMODULE hmod = ::LoadLibraryEx(L"dcomp.dll", nullptr, 0);
		if (hmod != nullptr)
		{
			fnCreateDCompDevice2 = reinterpret_cast<decltype(::DCompositionCreateDevice2)*>(
				::GetProcAddress(hmod, "DCompositionCreateDevice2"));
		}
	}
	if (fnCreateDCompDevice2 != nullptr)
	{
		Result = fnCreateDCompDevice2(RenderingDevice, Riid, PPV);
	}
	return Result;
}

HRESULT FWebView2Window::TryCreateDispatcherQueue()
{
	

	HRESULT Hresult = S_OK;
	thread_local winSystem::DispatcherQueueController dispatcherQueueController{nullptr};

	if (dispatcherQueueController == nullptr)
	{
		Hresult = E_FAIL;
		static decltype(::CreateDispatcherQueueController)* fnCreateDispatcherQueueController =
			nullptr;
		if (fnCreateDispatcherQueueController == nullptr)
		{
			HMODULE hmod = ::LoadLibraryEx(L"CoreMessaging.dll", nullptr, 0);
			if (hmod != nullptr)
			{
				fnCreateDispatcherQueueController =
					reinterpret_cast<decltype(::CreateDispatcherQueueController)*>(
						::GetProcAddress(hmod, "CreateDispatcherQueueController"));
			}
		}
		if (fnCreateDispatcherQueueController != nullptr)
		{
			winSystem::DispatcherQueueController controller{nullptr};
			DispatcherQueueOptions options{
				sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT, DQTAT_COM_STA};
			Hresult = fnCreateDispatcherQueueController(
				options, reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(
							 winrt::put_abi(controller)));
			dispatcherQueueController = controller;
		}
	}

	return Hresult;
}

ICoreWebView2Controller* FWebView2Window::GetWebViewController()
{
	return Controller.Get();
}

ICoreWebView2* FWebView2Window::GetWebView()
{
	return WebView.Get();
}

ICoreWebView2Environment* FWebView2Window::GetWebViewEnvironment()
{
	return WebViewEnvironment.Get();
}

HWND FWebView2Window::GetMainWindow()
{
	return MainWindow;
}

bool FWebView2Window::CloseWebView(bool CleanupUserDataFolder)
{
	
	//TODO
	// if (auto file = GetComponent<FWebView2FileComponent>())
  //  {
       // if (file->IsPrintToPdfInProgress())
       // {
        //	UE_LOG(LogWebView2,Log,TEXT("Print to PDF is in progress. Continue closing?"));
        	//return false;
//}
   // }
    // 1. Delete components.
    DeleteAllComponents();

	// 2. 如果需要清理并且 BrowserProcessExited 事件接口可用，则注册在浏览器退出时进行清理。
    Microsoft::WRL::ComPtr<ICoreWebView2Environment5> environment5;
    if (WebViewEnvironment)
    {
    	WebViewEnvironment.As(&environment5);
    }
    if (CleanupUserDataFolder && environment5)
    {
    	// 在关闭 WebView 之前，注册一个处理程序，其中包含在浏览器进程和相关进程终止后运行的代码。
        environment5->add_BrowserProcessExited(
	        Microsoft::WRL::Callback<ICoreWebView2BrowserProcessExitedEventHandler>(
                [environment5, this](
                    ICoreWebView2Environment* sender,
                    ICoreWebView2BrowserProcessExitedEventArgs* args)
                {
                    COREWEBVIEW2_BROWSER_PROCESS_EXIT_KIND kind;
                    UINT32 pid;
                    args->get_BrowserProcessExitKind(&kind);
                    args->get_BrowserProcessId(&pid);

                	// 如果在浏览器退出后但在我们的处理程序运行之前从此 CoreWebView2Environment 创建了新的 WebView，
                	// 则将创建一个新的浏览器进程并再次锁定用户数据文件夹。在这些情况下，请勿尝试清理用户数据文件夹。
                	// 我们根据我们最后一个 CoreWebView2 附加到的浏览器进程的 PID 检查已退出浏览器进程的 PID。
                    if (pid == NewestBrowserPID)
                    {
                    	// 观察浏览器进程是否正常退出。让 ProcessFailed 事件处理程序处理浏览器进程终止失败的情况。
                        if (kind == COREWEBVIEW2_BROWSER_PROCESS_EXIT_KIND_NORMAL)
                        {
                            environment5->remove_BrowserProcessExited(BrowserExitedEventToken);
                            // TODO
                            WebViewEnvironment = nullptr;
                           // RunAsync([this]() { CleanupUserDataFolder(); });
                        }
                    }
                    else
                    {
                    	// TODO
                    }

                    return S_OK;
                })
                .Get(),
            &BrowserExitedEventToken);
    }

    // 3. Close the webview.
    if (Controller)
    {
    	Controller->remove_ZoomFactorChanged(ZoomFactorChangedToken);
    	Controller->remove_AcceleratorKeyPressed(AcceleratorKeyPressedEventHandlerToken);
        Controller->Close();
        Controller = nullptr;

    	WebView->remove_WebMessageReceived(MessageReceiveToken);
    	WebView->remove_NavigationCompleted(NavigationCompletedToken);
    	WebView->remove_HistoryChanged(HistoryChangedToken);
    	WebView->remove_DocumentTitleChanged(DocumentTitleChangedToken);
    	WebView->remove_SourceChanged(SourceChangedToken);
    	WebView->remove_NavigationStarting(NavigationStartingToken);
    	WebView4->remove_DownloadStarting(DownloadStartingToken);
    	WebView4->remove_NewWindowRequested(NewWindowRequestedToken);
        WebView = nullptr;
        WebView4 = nullptr;
    }

	if (CompositionController)
	{
		CompositionController->remove_CursorChanged(CursorChangedEventHandlerToken);
	}
	CompositionController = nullptr;
	
	// 4. 如果 BrowserProcessExited 事件接口不可用，则立即释放环境并进行清理。如果接口可用，则仅在不等待事件的情况下释放环境。
    if (!environment5)
    {
    	
        WebViewEnvironment = nullptr;
        if (CleanupUserDataFolder)
        {
        	//TODO
          //  CleanupUserDataFolder();
        }
    }
    else if (!CleanupUserDataFolder)
    {
    	// 仅当不需要清理时才在此处释放环境对象。
    	// 如果需要清理，则环境对象释放将推迟到浏览器进程退出，否则不会调用 BrowserProcessExited 事件的处理程序。
        WebViewEnvironment = nullptr;
    }

	//5. 清理CompositionHost,
	if(WebView2CompositionHost)
	{
		WebView2CompositionHost->DestroyVisualAsWebview(AsShared());
	}

	
    return true;
}

void FWebView2Window::CloseWindow()
{
	UE_LOG(LogWebView2,Log,TEXT("WebView2Window is destoryed"))
	if (!CloseWebView())
	{
		return;
	}
	
}

void FWebView2Window::DeleteAllComponents()
{
	if (!Components.IsEmpty())
	{
		Components.Empty();
	}
}

auto FWebView2Window::LoadURL(const FString URL) -> void
{
	if(URL.IsEmpty())
	{
		return;;
	}
	if (!WebView)
	{
		return;
	}
	WebView->Navigate(*URL);
}


void FWebView2Window::GoForward() const
{
	if (WebView)
	{
		BOOL bIsSuccess = false;
		WebView->get_CanGoForward(&bIsSuccess);
		if (bIsSuccess)
		{
			WebView->GoForward();
		}
	}
}

void FWebView2Window::GoBack() const
{
	if (WebView)
	{
		BOOL bIsSuccess = false;
		WebView->get_CanGoBack(&bIsSuccess);
		if (bIsSuccess)
		{
			WebView->GoBack();
		}
	}
}

void FWebView2Window::ReLoad() const
{
	if (WebView)
	{
		WebView->Reload();
	}
}

void FWebView2Window::Stop() const
{
	if (WebView)
	{
		WebView->Stop();
	}
}

void FWebView2Window::PutCoreWebView2Settings(FCoreWebView2Settings CoreWebView2Settings)
{
	if(WebView)
	{
		ICoreWebView2Settings* Settings;
		WebView->get_Settings(&Settings);
		Settings->put_AreDefaultContextMenusEnabled(CoreWebView2Settings.bDefaultContextMenusEnabled);
		Settings->put_AreDefaultScriptDialogsEnabled(CoreWebView2Settings.bDefaultScriptDialogsEnabled);
		Settings->put_AreDevToolsEnabled(CoreWebView2Settings.bDevToolsEnabled);
		Settings->put_AreHostObjectsAllowed(CoreWebView2Settings.bHostObjectsAllowed);
		Settings->put_IsBuiltInErrorPageEnabled(CoreWebView2Settings.bBuiltInErrorPageEnabled);
		Settings->put_IsScriptEnabled(CoreWebView2Settings.bDefaultScriptDialogsEnabled);
		Settings->put_IsStatusBarEnabled(CoreWebView2Settings.bStatusBarEnabled);
		Settings->put_IsWebMessageEnabled(CoreWebView2Settings.bWebMessageEnabled);
		Settings->put_IsZoomControlEnabled(CoreWebView2Settings.bZoomControlEnabled);
	}
}

void FWebView2Window::SetBackgroundColor(FColor InBackgroundColor)
{
	BackgroundColor=InBackgroundColor;
	COREWEBVIEW2_COLOR transparentColor = {InBackgroundColor.A, InBackgroundColor.R,InBackgroundColor.G, InBackgroundColor.B};
	Microsoft::WRL::ComPtr<ICoreWebView2Controller2> TmpContrll;
	Controller.As(&TmpContrll);
	if (TmpContrll)
	{
		TmpContrll->get_ParentWindow(nullptr);
		TmpContrll->put_DefaultBackgroundColor(transparentColor);

		TmpContrll->add_ZoomFactorChanged(
			Microsoft::WRL::Callback<ICoreWebView2ZoomFactorChangedEventHandler>(
				[this](ICoreWebView2Controller* sender,
					   IUnknown* args) -> HRESULT
				{
					//double zoomFactor;
					//sender->get_ZoomFactor(&zoomFactor);
					sender->put_ZoomFactor(1.f);
					return S_OK;
				})
			.Get(), &ZoomFactorChangedToken);
	}
}

bool FWebView2Window::IsInitialized()
{
	return bInitialized;
}

void FWebView2Window::SetBounds(RECT Rect)
{
	WebuiBound = Rect;
	if(Controller)
	{
		Controller->put_Bounds(Rect);
	}
	if(CompositionController)
	{
		winrt::com_ptr<IUnknown> RootVisualTarget;
		CompositionController->get_RootVisualTarget(RootVisualTarget.put());
		if(RootVisualTarget)
		{
			winrt::Windows::UI::Composition::ContainerVisual InwebviewVisual = RootVisualTarget.as<winrt::Windows::UI::Composition::ContainerVisual>();
			if (InwebviewVisual)
			{
				InwebviewVisual.Offset({static_cast<float>(WebuiBound.left),static_cast<float>(WebuiBound.top),0});
				InwebviewVisual.Size({
			(static_cast<float>(WebuiBound.right - WebuiBound.left)),
			(static_cast<float>(WebuiBound.bottom - WebuiBound.top))});
			}
		}
	}
}

void FWebView2Window::SetBounds(POINT Offset, POINT Size)
{
	WebuiBound.top = LONG(Offset.y);
	WebuiBound.left = LONG(Offset.x);
	WebuiBound.bottom = LONG(Size.y + WebuiBound.top);
	WebuiBound.right = LONG(Size.x + WebuiBound.left);
	SetBounds(WebuiBound);
}

RECT FWebView2Window::GetBounds() const
{
	return WebuiBound;
}

void FWebView2Window::SetVisible(ESlateVisibility InVisibility)
{
	Visible=InVisibility;
	if(InVisibility==ESlateVisibility::Collapsed ||InVisibility==ESlateVisibility::Hidden)
	{
		if(Controller)
		{
			Controller->put_IsVisible(false);
		}
	}
	else
	{
		Controller->put_IsVisible(true);
	}
}

ESlateVisibility FWebView2Window::GetVisible() const
{
	return Visible;
}

EWebView2DocumentState FWebView2Window::GetDocumentLoadingState() const
{
	return DocumentState;
}


void FWebView2Window::SetLayerID(int32 InLayerID)
{
	LayerID = InLayerID;
}

int32 FWebView2Window::GetLayerID() const
{
	return LayerID;
}

FGuid FWebView2Window::GetUniqueID()
{
	return ID;
}

void FWebView2Window::ExecuteScript(const FString Script, TFunction<void(const FString ResultString)> InCallback)
{
	if (!WebView)
	{
		return;
	}
	WebView->ExecuteScript(*Script,
	Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
			[InCallback](HRESULT ErrorCode, LPCWSTR ResultString) -> HRESULT
			{
				if (!ErrorCode && ResultString)
				{
					if (InCallback)
					{
						UE_LOG(LogTemp,Warning,TEXT("%s"),ResultString);
						InCallback(ResultString);
					}
				}

				return S_OK;
			}).Get());
}

void FWebView2Window::PutHandled(bool bHandled)
{
	if(!Controller)
	{
		return;
	}
	Controller->add_AcceleratorKeyPressed(
		Microsoft::WRL::Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
		[bHandled](ICoreWebView2Controller* sender, ICoreWebView2AcceleratorKeyPressedEventArgs* args) -> HRESULT {
			COREWEBVIEW2_KEY_EVENT_KIND kind;
			args->get_KeyEventKind(&kind);
				   // We only care about key down events.
			if (kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN ||kind == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN)
			{
				args->put_Handled(bHandled);
			}
			return S_OK;
		}).Get(), &MoveFocusRequestedToken);
}

TSharedPtr<FWebView2CompositionHost> FWebView2Window::GetWebView2CompositionHost()
{
	return  WebView2CompositionHost;
}


void FWebView2Window::SetContainerVisual(winrt::Windows::UI::Composition::ContainerVisual InWebViewVisual)
{
	WebViewVisual=InWebViewVisual;
}

HRESULT FWebView2Window::OnCreateEnvironmentCompleted(HRESULT Result, ICoreWebView2Environment* Environment)
{
	if (Result != S_OK)
	{
		UE_LOG(LogWebView2,Error,TEXT("Failed to create environment object."));
		return S_OK;
	}
	WebViewEnvironment = Environment;
	bInitialized = true;
	return CreateControllerWithOptions();

}

// 这是传递给 CreateCoreWebView2Controller 的回调。在这里我们初始化所有与 WebView 相关的状态，并向 WebView 注册大多数事件处理程序。
HRESULT FWebView2Window::OnCreateCoreWebView2ControllerCompleted(HRESULT Result, ICoreWebView2Controller* InController)
{
	if (Result == S_OK)
	{
		Controller = InController;
		Microsoft::WRL::ComPtr<ICoreWebView2> CoreWebView2;
		InController->get_CoreWebView2(&CoreWebView2);

		// 我们应该在这里检查是否失败，因为如果此应用使用的 SDK 版本比 Edge 浏览器的安装版本更新，
		// 则 Edge 浏览器可能不支持最新版本的ICoreWebView2_N 接口。

		//CoreWebView2.query_to(&WebView);
		CoreWebView2.As(&WebView);
		// 保存浏览器进程的 PID，该进程为从我们的 CoreWebView2Environment 创建的最后一个 WebView 提供服务。
		// 我们知道控制器是用 S_OK 创建的，并且它尚未关闭（我们尚未调用 Close，并且尚未引发 ProcessFailed 事件），因此 PID 可用。
		(WebView->get_BrowserProcessId(&NewestBrowserPID));

		Microsoft::WRL::ComPtr<ICoreWebView2_13>webView2_13;
		CoreWebView2.As(&webView2_13);
		if (webView2_13)
		{
			Microsoft::WRL::ComPtr<ICoreWebView2Profile> profile;
			webView2_13->get_Profile(&profile);
			BOOL inPrivate = false;
			profile->get_IsInPrivateModeEnabled(&inPrivate);

			UWebView2Settings* Settings=UWebView2Settings::Get();
			if (!Settings->WebViewCreateOption.DownloadPath.IsEmpty())
			{
				profile->put_DefaultDownloadFolderPath(
					*Settings->WebViewCreateOption.DownloadPath);
			}
			
		}

		auto MessageReceive = Microsoft::WRL::Callback<
			ICoreWebView2WebMessageReceivedEventHandler>(this, &FWebView2Window::OnMessageReceived);
		WebView->add_WebMessageReceived(MessageReceive.Get(), &MessageReceiveToken);

		Controller->put_Bounds(WebuiBound);
		if(Visible==ESlateVisibility::Collapsed ||Visible==ESlateVisibility::Hidden)
		{
			Controller->put_IsVisible(false);
		}
		else
		{
			Controller->put_IsVisible(true);
		}
		
		WebView->Navigate(*InitialURL);

		SetBackgroundColor(BackgroundColor);
				
		WebView2CompositionHost = FWebView2Manager::GetInstance()->GetMessageProcess(MainWindow);
		if(WebView2CompositionHost)
		{
			WebView2CompositionHost->CreateWebViewVisual(AsShared());
			//触发WebView创建完成事件
			OnWebViewCreated.ExecuteIfBound((bool)WebView2CompositionHost);
		}

		
		
		PutCoreWebView2Settings(UWebView2Settings::Get()->CoreWebView2Settings);


		//是否静音
		Microsoft::WRL::ComPtr<ICoreWebView2_8> Webview2_8;
		WebView.As(&Webview2_8);
		Webview2_8->put_IsMuted(UWebView2Settings::Get()->bMuted);
		
		auto NewWindowRequestedHandle = Microsoft::WRL::Callback<ICoreWebView2NewWindowRequestedEventHandler>(
	  this, &FWebView2Window::OnCoreWebView2NewWindowRequestedEventHandler);
		WebView->add_NewWindowRequested(NewWindowRequestedHandle.Get(), &NewWindowRequestedToken);
		
		//绑定开始下载事件
		WebView.As(&WebView4);
		auto DownloadStartingHandler = Microsoft::WRL::Callback<ICoreWebView2DownloadStartingEventHandler>(
			this, &FWebView2Window::OnDownloadStartingEventHandler);
		WebView4->add_DownloadStarting(DownloadStartingHandler.Get(), &DownloadStartingToken);

		//绑定光标发生变化时的回调
		auto CursorChangedEventHandler = Microsoft::WRL::Callback<ICoreWebView2CursorChangedEventHandler>(
			this, &FWebView2Window::OnCursorChangedEventHandler);
		CompositionController->add_CursorChanged(CursorChangedEventHandler.Get(), &CursorChangedEventHandlerToken);


		//捕获webView键盘事件
		auto AcceleratorKeyPressedEventHandler = Microsoft::WRL::Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
			this, &FWebView2Window::OnAcceleratorKeyPressedEventHandler);
		Controller->add_AcceleratorKeyPressed(AcceleratorKeyPressedEventHandler.Get(),
													 &AcceleratorKeyPressedEventHandlerToken);
		
		//绑定网页开始加载时的回调
		auto NavigationStartingEventHandler = Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
			this, &FWebView2Window::OnNavigationStartingEventHandler);
		WebView->add_NavigationStarting(NavigationStartingEventHandler.Get(), &NavigationStartingToken);
		
		//绑定网页加载完成时的回调
		auto NavigationCompletedEventHandler = Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
			this, &FWebView2Window::OnNavigationCompletedEventHandler);
		WebView->add_NavigationCompleted(NavigationCompletedEventHandler.Get(), &NavigationCompletedToken);

		auto HistoryChangedEventHandler=Microsoft::WRL::Callback<ICoreWebView2HistoryChangedEventHandler>(
			this, &FWebView2Window::OnHistoryChanged);
		WebView->add_HistoryChanged(HistoryChangedEventHandler.Get(), &HistoryChangedToken);

		auto DocumentTitleChangedEventHandler =Microsoft::WRL::Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
			this, &FWebView2Window::OnDocumentTitleChanged);
		WebView->add_DocumentTitleChanged(DocumentTitleChangedEventHandler.Get(), &DocumentTitleChangedToken);
 
		auto SourceChangedEventHandler =Microsoft::WRL::Callback<ICoreWebView2SourceChangedEventHandler>(
			this, &FWebView2Window::OnSourceChanged);
		WebView->add_SourceChanged(SourceChangedEventHandler.Get(), &SourceChangedToken);

	}

	return  S_OK;
}

HRESULT FWebView2Window::OnMessageReceived(ICoreWebView2* Webview, ICoreWebView2WebMessageReceivedEventArgs* Args)
{
	LPWSTR MessageRaw;
	Args->get_WebMessageAsJson(&MessageRaw);
	std::wstring Message = MessageRaw;
	OnWebView2MessageReceived.ExecuteIfBound(FString(Message.c_str()));

	UWebView2Subsystem::GetWebView2Subsystem()->OnMessageReceivedNactive.ExecuteIfBound(FString(Message.c_str()));
	UWebView2Subsystem::GetWebView2Subsystem()->OnWebMessageReceived.Broadcast(FString(Message.c_str()));
	return S_OK;
}

HRESULT FWebView2Window::OnCoreWebView2NewWindowRequestedEventHandler(ICoreWebView2* Sender,
	ICoreWebView2NewWindowRequestedEventArgs* Args)
{
	LPWSTR URI;
	Args->get_Uri(&URI);
	OnNewWindowRequested.ExecuteIfBound(FString(URI));
	Args->put_Handled(true);
	return S_OK;
}

HRESULT FWebView2Window::OnNavigationCompletedEventHandler(ICoreWebView2* Sender,
	ICoreWebView2NavigationCompletedEventArgs* Args)
{
	BOOL bSuccess;
	Args->get_IsSuccess(&bSuccess);
	OnNavigationCompleted.ExecuteIfBound((bool)bSuccess);
	if (!bSuccess)
	{
		COREWEBVIEW2_WEB_ERROR_STATUS WebErrorStatus;
		Args->get_WebErrorStatus(&WebErrorStatus);

		if (WebErrorStatus != COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED)
		{
			UE_LOG(LogWebView2,Error,TEXT("Iframe navigation failed:%d, Navigation Failure"),WebErrorStatus);
			DocumentState=EWebView2DocumentState::Error;
		}
	}
	else
	{
		DocumentState=EWebView2DocumentState::Completed;
	}
	return S_OK;
}

HRESULT FWebView2Window::OnNavigationStartingEventHandler(ICoreWebView2* Sender,
	ICoreWebView2NavigationStartingEventArgs* Args)
{
	LPWSTR URI;
	Args->get_Uri(&URI);
	OnNavigationStarting.ExecuteIfBound(FString(URI));
	DocumentState=EWebView2DocumentState::Loading;
	return S_OK;
}

HRESULT FWebView2Window::OnCursorChangedEventHandler(ICoreWebView2CompositionController* Sender, IUnknown* Args)
{
	HCURSOR WebviewCursor;
	HRESULT Result = Sender->get_Cursor(&WebviewCursor);

	if (FAILED(Result))
	{
		return Result;
	}

	EMouseCursor::Type MouseCursor = EMouseCursor::Default;
	HCURSOR HANDCursor = LoadCursor(NULL, IDC_HAND);
	if (WebviewCursor == HANDCursor)
	{
		MouseCursor = EMouseCursor::Hand;
	}
	HCURSOR hIBeamCursor = LoadCursor(NULL, IDC_IBEAM);
	if (WebviewCursor == hIBeamCursor)
	{
		MouseCursor = EMouseCursor::TextEditBeam;
	}
	 OnCursorChangedNactive.ExecuteIfBound(MouseCursor);

	/**
	if (MouseCursor)
	{
		TSharedPtr<ICursor> PlatformCursor = FSlateApplication::Get().GetPlatformCursor();

		if (PlatformCursor.IsValid())
		{
			PlatformCursor->SetTypeShape(MouseCursor, (void*)MouseCursor);
			::SetCursor(WebviewCursor);
		}
		return true;
	}*/
	return S_OK;
	

}

HRESULT FWebView2Window::OnAcceleratorKeyPressedEventHandler(ICoreWebView2Controller* Sender,
	ICoreWebView2AcceleratorKeyPressedEventArgs* Args)
{
	COREWEBVIEW2_KEY_EVENT_KIND keyEventKind;
	Args->get_KeyEventKind(&keyEventKind);
	UINT key;
	Args->get_VirtualKey(&key);

	if (keyEventKind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN)
		PostMessageW(MainWindow, WM_KEYDOWN, key, 0);
	else if (keyEventKind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_UP)
		PostMessageW(MainWindow, WM_KEYUP, key, 0);

	return S_OK;
}

HRESULT FWebView2Window::OnDownloadStartingEventHandler(ICoreWebView2* Sender,
	ICoreWebView2DownloadStartingEventArgs* Args)
{
	 // 获取下载操作的实例
    Args->get_DownloadOperation(&DownloadOperation);
    // // 获取下载是否被取消的状态
    // BOOL cancel = FALSE;
    // args->get_Cancel(&cancel);
    // 获取下载的 URL 地址
	LPWSTR URI;
    DownloadOperation->get_Uri(&URI);
	DownloadInfo.URL=URI;

	LPWSTR MiniType;
	DownloadOperation->get_MimeType(&MiniType);
	DownloadInfo.MimeType=MiniType;

	LPWSTR ContentDisposition;
	DownloadOperation->get_ContentDisposition(&ContentDisposition);
	DownloadInfo.ContentDisposition=ContentDisposition;

	DownloadOperation->get_TotalBytesToReceive(&TotalBytesToReceive);

	// 获取是否处理下载的状态
	// BOOL handled = FALSE;
	// args->get_Handled(&handled);
	//隐藏下载的UI界面
	Args->put_Handled(true);

	auto ActiveDownloadFunc = [this]()
	{
		// 当下载状态发生改变时，定义处理函数
		DownloadOperation->add_StateChanged(Microsoft::WRL::Callback<ICoreWebView2StateChangedEventHandler>(
			                                    this, &FWebView2Window::OnDownLoadChangeStateEvent).Get(), &StateChangedToken);

		// 当下载接收字节改变时，定义处理函数
		DownloadOperation->add_BytesReceivedChanged(
			Microsoft::WRL::Callback<ICoreWebView2BytesReceivedChangedEventHandler>(
				this, &FWebView2Window::OnBytesReceivedChanged).Get(), &BytesReceivedChangedToken);

		// 当预估下载结束时间改变时，定义处理函数
		DownloadOperation->add_EstimatedEndTimeChanged(
			Microsoft::WRL::Callback<ICoreWebView2EstimatedEndTimeChangedEventHandler>(
				this, &FWebView2Window::OnEstimatedEndTimeChanged).Get(), &EstimatedEndTimeChanged);
	};

	ActiveDownloadFunc();

	return S_OK;
}

HRESULT FWebView2Window::OnDownLoadChangeStateEvent(ICoreWebView2DownloadOperation* Sender, IUnknown* Args)
{
	if (!DownloadOperation)
	{
		return S_FALSE;
	}
		
	if (!OnCursorChangedNactive.IsBound())
	{
		return S_OK;
	}
		
	COREWEBVIEW2_DOWNLOAD_STATE State;
	DownloadOperation->get_State(&State);
	switch (State)
	{
	case COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS:
		OnDownloadStateChangedNactive.ExecuteIfBound(EWebView2DownloadState::Progress,TEXT("正在下载中。。。"));
		break;
	case COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED:
		{
			COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON interrupt_reason;
			DownloadOperation->get_InterruptReason(&interrupt_reason);
			FString Reasen = InterruptReasonToString(interrupt_reason);
			OnDownloadStateChangedNactive.ExecuteIfBound(EWebView2DownloadState::Interrupted, Reasen);
			DownloadOperation->remove_StateChanged(StateChangedToken);
			DownloadOperation->remove_BytesReceivedChanged(BytesReceivedChangedToken);
			DownloadOperation->remove_EstimatedEndTimeChanged(EstimatedEndTimeChanged);
			DownloadOperation = nullptr;
		}
		break;
	case COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED:
		OnDownloadStateChangedNactive.ExecuteIfBound(EWebView2DownloadState::Completed, TEXT("下载已完成"));
		DownloadOperation->remove_StateChanged(StateChangedToken);
		DownloadOperation->remove_BytesReceivedChanged(BytesReceivedChangedToken);
		DownloadOperation->remove_EstimatedEndTimeChanged(EstimatedEndTimeChanged);
		break;
	}
	return S_OK;
}

HRESULT FWebView2Window::OnBytesReceivedChanged(ICoreWebView2DownloadOperation* Sender, IUnknown* Args)
{
	if (!DownloadOperation)
		return S_FALSE;
	//获取当前下载的字节数
	int64 BytesReceived = 0;
	DownloadOperation->get_BytesReceived(&BytesReceived);
	//更新下载进度
	OnDownloadProgressNactive.ExecuteIfBound(BytesReceived, TotalBytesToReceive);
	
	return S_OK;
}

HRESULT FWebView2Window::OnEstimatedEndTimeChanged(ICoreWebView2DownloadOperation* Sender, IUnknown* Args)
{
	if (!DownloadOperation)
	{
		return S_FALSE;
	}
		
	LPWSTR EstimatedEndTime;
	DownloadOperation->get_EstimatedEndTime(&EstimatedEndTime);

	OnEstimatedDownloadTimeNactive.ExecuteIfBound(EstimatedEndTime);
	
	return S_OK;
}

HRESULT FWebView2Window::OnHistoryChanged(ICoreWebView2* Sender, IUnknown* Args)
{
	BOOL canGoBack;
	BOOL canGoForward;
	Sender->get_CanGoBack(&canGoBack);
	Sender->get_CanGoForward(&canGoForward);
	OnCanGoBackNactive.ExecuteIfBound(canGoBack);
	OnCanGoForwardNactive.ExecuteIfBound(canGoForward);
	
	return S_OK;
}

HRESULT FWebView2Window::OnDocumentTitleChanged(ICoreWebView2* Sender, IUnknown* Args)
{
	LPWSTR TitleRaw;
	Sender->get_DocumentTitle(&TitleRaw);
	std::wstring Title = TitleRaw;
	OnDocumentTitleChangedNactive.ExecuteIfBound(FString(Title.c_str()));

	return S_OK;
}

HRESULT FWebView2Window::OnSourceChanged(ICoreWebView2* Sender, ICoreWebView2SourceChangedEventArgs* Args)
{
	LPWSTR URIRaw;
	Sender->get_Source(&URIRaw);
	std::wstring URI = URIRaw;

	FString RUL=URI.c_str();

	if (RUL.IsEmpty())
	{
		RUL=TEXT("about:blank");
	}

	OnSourceChangedNactive.ExecuteIfBound(RUL);
	return S_OK;
}

FString FWebView2Window::InterruptReasonToString(const COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON InterruptReason)
{
	FString ReasonString ;
    switch (InterruptReason)
    {
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NONE:
        ReasonString = TEXT("None");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_FAILED:
        ReasonString = TEXT("下载失败");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
        ReasonString = TEXT("文件访问被拒绝");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
        ReasonString =TEXT("文件没有空间");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG:
        ReasonString = TEXT("文件名太长");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE:
        ReasonString = TEXT("文件太大");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_MALICIOUS:
        ReasonString = TEXT("文件恶意");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR:
        ReasonString =TEXT("文件瞬态错误");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED_BY_POLICY:
        ReasonString = TEXT("文件被策略阻止");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED:
        ReasonString = TEXT("文件安全检查失败");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT:
        ReasonString = TEXT("文件太短");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH:
        ReasonString = TEXT("文件哈希不匹配");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED:
        ReasonString =TEXT("网络故障");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
        ReasonString = TEXT("网络超时");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED:
        ReasonString = TEXT("网络已断开连接");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN:
        ReasonString = TEXT("网络服务器故障");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST:
        ReasonString = TEXT("网络无效请求");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED:
        ReasonString = TEXT("服务器失败");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE:
        ReasonString = TEXT("服务器无范围");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT:
        ReasonString = TEXT("服务器内容错误");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED:
        ReasonString = TEXT("服务器未授权");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_CERTIFICATE_PROBLEM:
        ReasonString = TEXT("服务器证书问题");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN:
        ReasonString = TEXT("服务器被禁止");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_UNEXPECTED_RESPONSE:
        ReasonString = TEXT("服务器意外响应");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH:
        ReasonString = TEXT("服务器内容长度不匹配");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT:
        ReasonString = TEXT("服务器跨源重定向");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
        ReasonString = TEXT("用户已取消");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN:
        ReasonString = TEXT("用户关机");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_USER_PAUSED:
        ReasonString = TEXT("用户已暂停");
        break;
    case COREWEBVIEW2_DOWNLOAD_INTERRUPT_REASON_DOWNLOAD_PROCESS_CRASHED:
        ReasonString = TEXT("下载进程崩溃");
        break;
    }
    return ReasonString;
}


