#include "WebView2Window.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#include "WebView2CompositionHost.h"
#include "WebView2Log.h"
#include "WebView2Manager.h"
#include "WebView2Settings.h"
#include "WebView2Subsystem.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <DispatcherQueue.h>
#include <WinUser.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	// 统一接管 COM 分配的字符串，转换成 FString 后立即释放原始内存。
	FString TakeCoTaskMemString(LPWSTR RawString)
	{
		const FString Result = RawString ? FString(RawString) : FString();
		if (RawString)
		{
			::CoTaskMemFree(RawString);
		}
		return Result;
	}

	EWebView2DownloadState ToDownloadState(const COREWEBVIEW2_DOWNLOAD_STATE InState)
	{
		// SDK 下载状态转为插件内部枚举，便于跨层复用。
		switch (InState)
		{
		case COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED:
			return EWebView2DownloadState::Interrupted;
		case COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED:
			return EWebView2DownloadState::Completed;
		default:
			return EWebView2DownloadState::InProgress;
		}
	}

	FString DownloadStateToString(const EWebView2DownloadState InState)
	{
		switch (InState)
		{
		case EWebView2DownloadState::Interrupted:
			return TEXT("Interrupted");
		case EWebView2DownloadState::Completed:
			return TEXT("Completed");
		default:
			return TEXT("InProgress");
		}
	}

	FString PermissionKindToString(const COREWEBVIEW2_PERMISSION_KIND InPermissionKind)
	{
		switch (InPermissionKind)
		{
		case COREWEBVIEW2_PERMISSION_KIND_MICROPHONE:
			return TEXT("Microphone");
		case COREWEBVIEW2_PERMISSION_KIND_CAMERA:
			return TEXT("Camera");
		case COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION:
			return TEXT("Geolocation");
		case COREWEBVIEW2_PERMISSION_KIND_NOTIFICATIONS:
			return TEXT("Notifications");
		case COREWEBVIEW2_PERMISSION_KIND_OTHER_SENSORS:
			return TEXT("OtherSensors");
		case COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ:
			return TEXT("ClipboardRead");
		case COREWEBVIEW2_PERMISSION_KIND_MULTIPLE_AUTOMATIC_DOWNLOADS:
			return TEXT("MultipleAutomaticDownloads");
		case COREWEBVIEW2_PERMISSION_KIND_FILE_READ_WRITE:
			return TEXT("FileReadWrite");
		case COREWEBVIEW2_PERMISSION_KIND_AUTOPLAY:
			return TEXT("Autoplay");
		case COREWEBVIEW2_PERMISSION_KIND_LOCAL_FONTS:
			return TEXT("LocalFonts");
		case COREWEBVIEW2_PERMISSION_KIND_MIDI_SYSTEM_EXCLUSIVE_MESSAGES:
			return TEXT("MidiSystemExclusiveMessages");
		case COREWEBVIEW2_PERMISSION_KIND_WINDOW_MANAGEMENT:
			return TEXT("WindowManagement");
		default:
			return TEXT("Unknown");
		}
	}

	winrt::hstring BuildBrowserArguments(const UWebView2Settings* Settings)
	{
		FString Arguments = TEXT("--enable-features=ThirdPartyStoragePartitioning,PartitionedCookies");
		if (Settings)
		{
			for (const FString& Entry : Settings->Environment.AdditionalBrowserArguments)
			{
				const FString TrimmedEntry = Entry.TrimStartAndEnd();
				if (!TrimmedEntry.IsEmpty())
				{
					Arguments += TEXT(" ");
					Arguments += TrimmedEntry;
				}
			}
		}

		Arguments.TrimStartAndEndInline();
		return winrt::hstring(*Arguments);
	}

	COREWEBVIEW2_COLOR ToCoreColor(const FColor& InColor)
	{
		return COREWEBVIEW2_COLOR{InColor.A, InColor.R, InColor.G, InColor.B};
	}

	FString LoadTransparencyCheckScript()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CBWebView2"));
		if (!Plugin.IsValid())
		{
			return FString();
		}

		FString ScriptContent;
		const FString PrimaryScriptPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Extras/transparency_check.js"));
		const FString LegacyScriptPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/Web/transparency_check.js"));

		// 优先从 Extras 加载，避免示例网页目录整理时误删；旧路径仅作为兼容回退。
		if (!FFileHelper::LoadFileToString(ScriptContent, *PrimaryScriptPath) &&
			!FFileHelper::LoadFileToString(ScriptContent, *LegacyScriptPath))
		{
			UE_LOG(LogWebView2Utils, Warning, TEXT("无法加载透明穿透脚本，已尝试路径: %s ; %s"), *PrimaryScriptPath, *LegacyScriptPath);
			return FString();
		}

		// 使用全局哨兵避免页面自己已引入脚本时重复注册监听。
		return FString::Printf(
			TEXT("if (!window.__cbwebview2TransparencyCheckInstalled) { window.__cbwebview2TransparencyCheckInstalled = true; %s }")
			,
			*ScriptContent);
	}
}

FWebView2Window::FWebView2Window(
	HWND InParentWindow,
	const FGuid& InInstanceId,
	const FString& InInitialUrl,
	const FColor& InBackgroundColor,
	bool bInEnableTransparencyHitTest)
	: ParentWindow(InParentWindow)
	, InstanceId(InInstanceId)
	, InitialUrl(InInitialUrl)
	, BackgroundColor(InBackgroundColor)
	, bEnableTransparencyHitTest(bInEnableTransparencyHitTest)
{
	Initialize();
}

FWebView2Window::~FWebView2Window()
{
	Close();
}

void FWebView2Window::Initialize()
{
	// 每次初始化都先重置运行时状态，确保重建实例时没有旧状态残留。
	bCloseRequested = false;
	bInitialized = false;
	Visible = ESlateVisibility::Visible;
	bHitTestEnabled = true;
	DocumentState = ECBWebView2DocumentState::NoDocument;
	Bounds = {};
	DCompDevice = nullptr;

	const UWebView2Settings* Settings = UWebView2Settings::Get();
	const ECBWebView2Mode Mode = Settings ? Settings->Mode : ECBWebView2Mode::VisualWinComp;

	if (Mode == ECBWebView2Mode::VisualDComp || Mode == ECBWebView2Mode::TargetDComp)
	{
		// DComp 模式需要显式创建合成设备。
		const HRESULT Hr = DCompositionCreateDevice2(nullptr, IID_PPV_ARGS(&DCompDevice));
		if (FAILED(Hr))
		{
			UE_LOG(LogWebView2Utils, Error, TEXT("创建 DComp 设备失败，HRESULT=0x%08x"), Hr);
			return;
		}
	}

	auto Options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
	// 启动参数在 Environment 创建时一次性生效。
	const winrt::hstring BrowserArguments = BuildBrowserArguments(Settings);
	Options->put_AdditionalBrowserArguments(BrowserArguments.c_str());

	if (Settings)
	{
		Options->put_AllowSingleSignOnUsingOSPrimaryAccount(Settings->Environment.bEnableSingleSignOn);
		Options->put_ExclusiveUserDataFolderAccess(Settings->Environment.bExclusiveUserDataFolderAccess);
		Options->put_IsCustomCrashReportingEnabled(Settings->Environment.bCustomCrashReporting);

		if (!Settings->Environment.Language.IsEmpty())
		{
			Options->put_Language(*Settings->Environment.Language);
		}

		Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions5> Options5;
		if (SUCCEEDED(Options.As(&Options5)))
		{
			Options5->put_EnableTrackingPrevention(Settings->Environment.bTrackingPrevention);
		}

		Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions6> Options6;
		if (SUCCEEDED(Options.As(&Options6)))
		{
			Options6->put_AreBrowserExtensionsEnabled(Settings->Environment.bEnableBrowserExtensions);
		}

		Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions8> Options8;
		if (SUCCEEDED(Options.As(&Options8)))
		{
			Options8->put_ScrollBarStyle(COREWEBVIEW2_SCROLLBAR_STYLE_FLUENT_OVERLAY);
		}
	}

	FString UserDataFolder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CBWebView2/UserData"));
	// 用户数据目录统一落到 Saved 下，避免污染工程根目录。
	FPaths::NormalizeDirectoryName(UserDataFolder);
	IFileManager::Get().MakeDirectory(*UserDataFolder, true);

	const HRESULT Hr = CreateCoreWebView2EnvironmentWithOptions(
		nullptr,
		*UserDataFolder,
		Options.Get(),
		Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			this,
			&FWebView2Window::OnCreateEnvironmentCompleted)
			.Get());

	if (FAILED(Hr))
	{
		UE_LOG(LogWebView2Utils, Error, TEXT("创建 WebView2 Environment 失败，HRESULT=0x%08x"), Hr);
	}
}

bool FWebView2Window::Close(bool bCleanupUserDataFolder)
{
	if (bCloseRequested)
	{
		return true;
	}

	// 先标记关闭，再逆序解绑事件、分离可视树和释放 COM 对象。
	bCloseRequested = true;

	if (CompositionHost.IsValid())
	{
		CompositionHost->DetachWebView(InstanceId, WebViewVisual);
		CompositionHost.Reset();
	}

	if (Controller)
	{
		if (WebView)
		{
			WebView->remove_WebMessageReceived(MessageReceivedToken);
			WebView->remove_NavigationCompleted(NavigationCompletedToken);
			WebView->remove_NavigationStarting(NavigationStartingToken);
			WebView->remove_HistoryChanged(HistoryChangedToken);
			WebView->remove_DocumentTitleChanged(DocumentTitleChangedToken);
			WebView->remove_SourceChanged(SourceChangedToken);
			if (Microsoft::WRL::ComPtr<ICoreWebView2_4> WebView4; SUCCEEDED(WebView.As(&WebView4)) && WebView4)
			{
				WebView4->remove_DownloadStarting(DownloadStartingToken);
			}
			WebView->remove_PermissionRequested(PermissionRequestedToken);
		}

		if (CompositionController)
		{
			CompositionController->put_RootVisualTarget(nullptr);
			CompositionController->remove_CursorChanged(CursorChangedToken);
		}

		Controller->remove_AcceleratorKeyPressed(AcceleratorKeyPressedToken);
		Controller->Close();
	}

	WebView = nullptr;
	CompositionController = nullptr;
	Controller = nullptr;
	WebViewEnvironment = nullptr;
	WebViewVisual = nullptr;
	for (const TPair<uint64, FTrackedDownloadRegistration>& Pair : ActiveDownloads)
	{
		if (Pair.Value.Operation)
		{
			Pair.Value.Operation->remove_BytesReceivedChanged(Pair.Value.BytesReceivedChangedToken);
			Pair.Value.Operation->remove_StateChanged(Pair.Value.StateChangedToken);
		}
	}
	ActiveDownloads.Empty();
	bInitialized = false;
	return true;
}

void FWebView2Window::CloseWindow()
{
	Close();
}

bool FWebView2Window::IsInitialized() const
{
	return bInitialized;
}

int32 FWebView2Window::GetLayerId() const
{
	return LayerId;
}

void FWebView2Window::SetLayerId(int32 InLayerId)
{
	LayerId = InLayerId;
}

FGuid FWebView2Window::GetInstanceId() const
{
	return InstanceId;
}

void FWebView2Window::ExecuteScript(const FString& Script, TFunction<void(const FString&)> Callback)
{
	if (!WebView)
	{
		return;
	}

	WebView->ExecuteScript(
		*Script,
		Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
			[Callback](HRESULT ErrorCode, LPCWSTR ResultString) -> HRESULT
			{
				if (SUCCEEDED(ErrorCode) && ResultString && Callback)
				{
					Callback(FString(ResultString));
				}
				return S_OK;
			})
			.Get());
}

void FWebView2Window::OpenDevToolsWindow() const
{
	if (WebView)
	{
		WebView->OpenDevToolsWindow();
	}
}

void FWebView2Window::PrintToPdf(const FString& OutputPath, bool bLandscape)
{
	if (!WebView || OutputPath.IsEmpty())
	{
		OnPrintToPdfCompleted.ExecuteIfBound(false, OutputPath);
		return;
	}

	Microsoft::WRL::ComPtr<ICoreWebView2_7> WebView7;
	if (FAILED(WebView.As(&WebView7)) || !WebView7)
	{
		UE_LOG(LogWebView2Utils, Warning, TEXT("当前 WebView2 Runtime 不支持 PrintToPdf 接口。"));
		OnPrintToPdfCompleted.ExecuteIfBound(false, OutputPath);
		return;
	}

	const FString AbsoluteOutputPath = FPaths::ConvertRelativePathToFull(OutputPath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsoluteOutputPath), true);

	Microsoft::WRL::ComPtr<ICoreWebView2PrintSettings> PrintSettings;
	if (bLandscape)
	{
		Microsoft::WRL::ComPtr<ICoreWebView2Environment6> Environment6;
		if (SUCCEEDED(WebViewEnvironment.As(&Environment6)) && Environment6)
		{
			if (SUCCEEDED(Environment6->CreatePrintSettings(&PrintSettings)) && PrintSettings)
			{
				PrintSettings->put_Orientation(COREWEBVIEW2_PRINT_ORIENTATION_LANDSCAPE);
			}
		}
	}

	WebView7->PrintToPdf(
		*AbsoluteOutputPath,
		PrintSettings.Get(),
		Microsoft::WRL::Callback<ICoreWebView2PrintToPdfCompletedHandler>(
			[this, AbsoluteOutputPath](HRESULT ErrorCode, BOOL bSuccess) -> HRESULT
			{
				if (FAILED(ErrorCode))
				{
					UE_LOG(LogWebView2Utils, Error, TEXT("PrintToPdf 失败，HRESULT=0x%08x，路径=%s"), ErrorCode, *AbsoluteOutputPath);
					OnPrintToPdfCompleted.ExecuteIfBound(false, AbsoluteOutputPath);
					return S_OK;
				}

				OnPrintToPdfCompleted.ExecuteIfBound(!!bSuccess, AbsoluteOutputPath);
				return S_OK;
			})
			.Get());
}

void FWebView2Window::SetBackgroundColor(const FColor& InBackgroundColor)
{
	BackgroundColor = InBackgroundColor;
	Microsoft::WRL::ComPtr<ICoreWebView2Controller2> Controller2;
	if (SUCCEEDED(Controller.As(&Controller2)) && Controller2)
	{
		Controller2->put_DefaultBackgroundColor(ToCoreColor(InBackgroundColor));
	}
}

void FWebView2Window::SetContainerVisual(winrt::Windows::UI::Composition::ContainerVisual InContainerVisual)
{
	WebViewVisual = InContainerVisual;
}

TSharedPtr<FWebView2CompositionHost> FWebView2Window::GetCompositionHost() const
{
	return CompositionHost;
}

void FWebView2Window::SetHitTestEnabled(bool bInHitTestEnabled)
{
	bHitTestEnabled = bInHitTestEnabled;
}

bool FWebView2Window::IsHitTestEnabled() const
{
	return bHitTestEnabled;
}

bool FWebView2Window::IsTransparencyHitTestEnabled() const
{
	return bEnableTransparencyHitTest;
}

void FWebView2Window::LoadURL(const FString& InUrl)
{
	if (WebView && !InUrl.IsEmpty())
	{
		WebView->Navigate(*InUrl);
	}
}

void FWebView2Window::GoForward() const
{
	if (WebView)
	{
		BOOL bCanGoForward = 0;
		WebView->get_CanGoForward(&bCanGoForward);
		if (bCanGoForward)
		{
			WebView->GoForward();
		}
	}
}

void FWebView2Window::GoBack() const
{
	if (WebView)
	{
		BOOL bCanGoBack = 0;
		WebView->get_CanGoBack(&bCanGoBack);
		if (bCanGoBack)
		{
			WebView->GoBack();
		}
	}
}

void FWebView2Window::Reload() const
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

void FWebView2Window::SetBounds(const RECT& InRect)
{
	Bounds = InRect;
	if (Controller)
	{
		// Windowed 模式直接更新 Controller Bounds。
		Controller->put_Bounds(Bounds);
	}

	if (CompositionController)
	{
		// WinComp 模式同步更新挂在 RootVisualTarget 上的容器位置与尺寸。
		winrt::com_ptr<IUnknown> RootVisualTarget;
		CompositionController->get_RootVisualTarget(RootVisualTarget.put());
		if (RootVisualTarget)
		{
			auto Container = RootVisualTarget.as<winrt::Windows::UI::Composition::ContainerVisual>();
			Container.Offset({static_cast<float>(Bounds.left), static_cast<float>(Bounds.top), 0.0f});
			Container.Size({static_cast<float>(Bounds.right - Bounds.left), static_cast<float>(Bounds.bottom - Bounds.top)});
		}
	}
}

void FWebView2Window::SetBounds(const POINT& InOffset, const POINT& InSize)
{
	RECT NewRect;
	NewRect.left = InOffset.x;
	NewRect.top = InOffset.y;
	NewRect.right = InOffset.x + InSize.x;
	NewRect.bottom = InOffset.y + InSize.y;
	SetBounds(NewRect);
}

RECT FWebView2Window::GetBounds() const
{
	return Bounds;
}

void FWebView2Window::SetVisible(ESlateVisibility InVisibility)
{
	Visible = InVisibility;
	if (Controller)
	{
		Controller->put_IsVisible(InVisibility != ESlateVisibility::Collapsed && InVisibility != ESlateVisibility::Hidden);
	}
}

ESlateVisibility FWebView2Window::GetVisible() const
{
	return Visible;
}

ECBWebView2DocumentState FWebView2Window::GetDocumentLoadingState() const
{
	return DocumentState;
}

void FWebView2Window::MoveFocus(bool bFocus)
{
	if (bFocus && ParentWindow)
	{
		// 先确保宿主 HWND 拿到系统焦点，再请求 WebView 内部焦点移动。
		if (::GetFocus() != ParentWindow)
		{
			::SetFocus(ParentWindow);
		}
	}

	if (Controller && bFocus)
	{
		Controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
	}
}

void FWebView2Window::SendMouseButton(const FVector2D& Point, bool bIsLeft, bool bIsDown)
{
	if (CompositionController)
	{
		POINT MousePoint{static_cast<LONG>(Point.X), static_cast<LONG>(Point.Y)};
		const COREWEBVIEW2_MOUSE_EVENT_KIND Kind = bIsLeft
			? (bIsDown ? COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN : COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP)
			: (bIsDown ? COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN : COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP);
		CompositionController->SendMouseInput(Kind, COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE, 0, MousePoint);
	}
}

void FWebView2Window::SendMouseMove(const FVector2D& Point)
{
	if (CompositionController)
	{
		POINT MousePoint{static_cast<LONG>(Point.X), static_cast<LONG>(Point.Y)};
		CompositionController->SendMouseInput(COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE, COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE, 0, MousePoint);
	}
}

void FWebView2Window::SendMouseWheel(const FVector2D& Point, float Delta)
{
	if (CompositionController)
	{
		POINT MousePoint{static_cast<LONG>(Point.X), static_cast<LONG>(Point.Y)};
		CompositionController->SendMouseInput(
			COREWEBVIEW2_MOUSE_EVENT_KIND_WHEEL,
			COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE,
			static_cast<UINT32>(Delta * 120.0f),
			MousePoint);
	}
}

void FWebView2Window::SendKeyboardMessage(uint32 Msg, uint64 WParam, int64 LParam)
{
	if (ParentWindow)
	{
		// 统一走宿主窗口消息链，便于和现有输入处理保持一致。
		::SendMessageW(ParentWindow, Msg, static_cast<WPARAM>(WParam), static_cast<LPARAM>(LParam));
	}
}

HRESULT FWebView2Window::CreateControllerWithOptions()
{
	// 优先尝试高版本 Environment10，以获得 ControllerOptions 能力。
	Microsoft::WRL::ComPtr<ICoreWebView2Environment10> Environment10;
	WebViewEnvironment.As(&Environment10);

	const UWebView2Settings* Settings = UWebView2Settings::Get();
	const ECBWebView2Mode Mode = Settings ? Settings->Mode : ECBWebView2Mode::VisualWinComp;

	if (!Environment10)
	{
		// 旧版 Runtime 回退到基础 Controller / CompositionController 创建路径。
		Microsoft::WRL::ComPtr<ICoreWebView2Environment3> Environment3;
		WebViewEnvironment.As(&Environment3);

		if (Mode == ECBWebView2Mode::Windowed)
		{
			return WebViewEnvironment->CreateCoreWebView2Controller(
				ParentWindow,
				Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
					this,
					&FWebView2Window::OnCreateControllerCompleted)
					.Get());
		}

		if (!Environment3)
		{
			return E_NOINTERFACE;
		}

		return Environment3->CreateCoreWebView2CompositionController(
			ParentWindow,
			Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
				[this](HRESULT Result, ICoreWebView2CompositionController* InCompositionController) -> HRESULT
				{
					CompositionController = InCompositionController;
					Microsoft::WRL::ComPtr<ICoreWebView2Controller> BaseController;
					CompositionController.As(&BaseController);
					return OnCreateControllerCompleted(Result, BaseController.Get());
				})
				.Get());
	}

	Microsoft::WRL::ComPtr<ICoreWebView2ControllerOptions> ControllerOptions;
	HRESULT Hr = Environment10->CreateCoreWebView2ControllerOptions(&ControllerOptions);
	if (FAILED(Hr))
	{
		UE_LOG(LogWebView2Utils, Error, TEXT("创建 ControllerOptions 失败，HRESULT=0x%08x"), Hr);
		return Hr;
	}

	if (Settings)
	{
		// 这一部分属于 Controller 级配置，需要在创建前一次性写入。
		ControllerOptions->put_ProfileName(*Settings->Controller.ProfileName);
		ControllerOptions->put_IsInPrivateModeEnabled(Settings->Controller.bInPrivate);

		Microsoft::WRL::ComPtr<ICoreWebView2ControllerOptions2> ControllerOptions2;
		if (SUCCEEDED(ControllerOptions.As(&ControllerOptions2)))
		{
			if (Settings->Controller.bUseOSRegion)
			{
				wchar_t LocaleName[LOCALE_NAME_MAX_LENGTH] = {};
				GetUserDefaultLocaleName(LocaleName, LOCALE_NAME_MAX_LENGTH);
				ControllerOptions2->put_ScriptLocale(LocaleName);
			}
			else if (!Settings->Controller.ScriptLocale.IsEmpty())
			{
				ControllerOptions2->put_ScriptLocale(*Settings->Controller.ScriptLocale);
			}
		}

		Microsoft::WRL::ComPtr<ICoreWebView2ExperimentalControllerOptions2> ExperimentalControllerOptions2;
		if (SUCCEEDED(ControllerOptions.As(&ExperimentalControllerOptions2)))
		{
			ExperimentalControllerOptions2->put_AllowHostInputProcessing(Settings->Controller.bAllowHostInputProcessing);
		}
	}

	if (Mode == ECBWebView2Mode::Windowed)
	{
		return Environment10->CreateCoreWebView2ControllerWithOptions(
			ParentWindow,
			ControllerOptions.Get(),
			Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				this,
				&FWebView2Window::OnCreateControllerCompleted)
				.Get());
	}

	return Environment10->CreateCoreWebView2CompositionControllerWithOptions(
		ParentWindow,
		ControllerOptions.Get(),
		Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
			[this](HRESULT Result, ICoreWebView2CompositionController* InCompositionController) -> HRESULT
			{
				CompositionController = InCompositionController;
				Microsoft::WRL::ComPtr<ICoreWebView2Controller> BaseController;
				CompositionController.As(&BaseController);
				return OnCreateControllerCompleted(Result, BaseController.Get());
			})
			.Get());
}

HRESULT FWebView2Window::DCompositionCreateDevice2(IUnknown* RenderingDevice, REFIID Riid, void** PPV)
{
	static decltype(::DCompositionCreateDevice2)* CreateDeviceFunc = nullptr;
	if (!CreateDeviceFunc)
	{
		if (HMODULE ModuleHandle = ::LoadLibraryEx(L"dcomp.dll", nullptr, 0))
		{
			CreateDeviceFunc = reinterpret_cast<decltype(::DCompositionCreateDevice2)*>(
				::GetProcAddress(ModuleHandle, "DCompositionCreateDevice2"));
		}
	}

	return CreateDeviceFunc ? CreateDeviceFunc(RenderingDevice, Riid, PPV) : E_FAIL;
}

HRESULT FWebView2Window::OnCreateEnvironmentCompleted(HRESULT Result, ICoreWebView2Environment* Environment)
{
	if (FAILED(Result) || !Environment)
	{
		UE_LOG(LogWebView2Utils, Error, TEXT("WebView2 Environment 创建失败，HRESULT=0x%08x"), Result);
		return S_OK;
	}

	WebViewEnvironment = Environment;
	bInitialized = true;
	return CreateControllerWithOptions();
}

HRESULT FWebView2Window::OnCreateControllerCompleted(HRESULT Result, ICoreWebView2Controller* InController)
{
	if (FAILED(Result) || !InController)
	{
		UE_LOG(LogWebView2Utils, Error, TEXT("WebView2 Controller 创建失败，HRESULT=0x%08x"), Result);
		OnWebViewCreated.ExecuteIfBound(false);
		return S_OK;
	}

	Controller = InController;
	Microsoft::WRL::ComPtr<ICoreWebView2> CoreWebView;
	Controller->get_CoreWebView2(&CoreWebView);
	CoreWebView.As(&WebView);

	if (WebView)
	{
		WebView->get_BrowserProcessId(&BrowserProcessId);
	}

	const UWebView2Settings* Settings = UWebView2Settings::Get();
	if (Settings)
	{
		Microsoft::WRL::ComPtr<ICoreWebView2_13> WebView13;
		if (SUCCEEDED(WebView.As(&WebView13)) && WebView13 && !Settings->Controller.DownloadPath.IsEmpty())
		{
			Microsoft::WRL::ComPtr<ICoreWebView2Profile> Profile;
			if (SUCCEEDED(WebView13->get_Profile(&Profile)) && Profile)
			{
				Profile->put_DefaultDownloadFolderPath(*Settings->Controller.DownloadPath);
			}
		}
	}

	Controller->put_Bounds(Bounds);
	Controller->put_IsVisible(Visible != ESlateVisibility::Collapsed && Visible != ESlateVisibility::Hidden);

	if (CompositionController)
	{
		// 无窗口模式下把当前 WebView 挂接到宿主 HWND 对应的组合宿主中。
		CompositionHost = FWebView2Manager::Get().GetOrCreateCompositionHost(ParentWindow);
		if (CompositionHost.IsValid())
		{
			CompositionHost->AttachWebView(AsShared());
		}
	}

	ApplyRuntimeSettings();
	RegisterCoreEvents();
	SetBackgroundColor(BackgroundColor);

	if (WebView && bEnableTransparencyHitTest)
	{
		// 为启用透明命中检测的页面注入辅助脚本。
		const FString InjectedScript = LoadTransparencyCheckScript();
		if (!InjectedScript.IsEmpty())
		{
			WebView->AddScriptToExecuteOnDocumentCreated(
				*InjectedScript,
				Microsoft::WRL::Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
					[](HRESULT ErrorCode, PCWSTR ScriptId) -> HRESULT
					{
						if (FAILED(ErrorCode))
						{
							UE_LOG(LogWebView2Utils, Warning, TEXT("注入透明穿透脚本失败，HRESULT=0x%08x"), ErrorCode);
						}
						return S_OK;
					})
					.Get());
		}
	}

	if (!InitialUrl.IsEmpty())
	{
		WebView->Navigate(*InitialUrl);
	}

	OnWebViewCreated.ExecuteIfBound(true);

	return S_OK;
}

HRESULT FWebView2Window::OnMessageReceivedInternal(ICoreWebView2* Sender, ICoreWebView2WebMessageReceivedEventArgs* Args)
{
	// 优先按纯字符串读取，失败时再回退到 JSON，兼容不同 postMessage 发送方式。
	LPWSTR RawMessage = nullptr;
	if (FAILED(Args->TryGetWebMessageAsString(&RawMessage)) || RawMessage == nullptr)
	{
		Args->get_WebMessageAsJson(&RawMessage);
	}

	const FString Message = TakeCoTaskMemString(RawMessage);
	UE_LOG(LogWebView2Utils, Verbose, TEXT("收到网页消息: %s"), *Message);

	FCBWebView2MonitoredEvent EventInfo;
	EventInfo.EventType = ECBWebView2MonitoredEventType::WebMessageReceived;
	EventInfo.Message = Message;
	EmitMonitoredEvent(EventInfo);

	OnMessageReceived.ExecuteIfBound(Message);
	if (UWebView2Subsystem* Subsystem = UWebView2Subsystem::Get())
	{
		// 统一向子系统广播，方便全局监听。
		Subsystem->OnWebMessageReceivedNative.ExecuteIfBound(Message);
		Subsystem->OnWebMessageReceived.Broadcast(Message);
	}

	return S_OK;
}

HRESULT FWebView2Window::OnNewWindowRequestedInternal(ICoreWebView2* Sender, ICoreWebView2NewWindowRequestedEventArgs* Args)
{
	LPWSTR Uri = nullptr;
	Args->get_Uri(&Uri);
	const FString TargetUrl = TakeCoTaskMemString(Uri);

	FCBWebView2MonitoredEvent EventInfo;
	EventInfo.EventType = ECBWebView2MonitoredEventType::NewWindowRequested;
	EventInfo.Url = TargetUrl;
	EmitMonitoredEvent(EventInfo);

	OnNewWindowRequested.ExecuteIfBound(TargetUrl);
	Args->put_Handled(true);
	return S_OK;
}

HRESULT FWebView2Window::OnNavigationCompletedInternal(ICoreWebView2* Sender, ICoreWebView2NavigationCompletedEventArgs* Args)
{
	BOOL bSuccess = 0;
	Args->get_IsSuccess(&bSuccess);
	DocumentState = bSuccess ? ECBWebView2DocumentState::Completed : ECBWebView2DocumentState::Error;

	FCBWebView2MonitoredEvent EventInfo;
	EventInfo.EventType = ECBWebView2MonitoredEventType::NavigationCompleted;
	EventInfo.bSuccess = !!bSuccess;
	EmitMonitoredEvent(EventInfo);

	OnNavigationCompleted.ExecuteIfBound(!!bSuccess);
	return S_OK;
}

HRESULT FWebView2Window::OnNavigationStartingInternal(ICoreWebView2* Sender, ICoreWebView2NavigationStartingEventArgs* Args)
{
	LPWSTR Uri = nullptr;
	Args->get_Uri(&Uri);
	DocumentState = ECBWebView2DocumentState::Loading;
	const FString TargetUrl = TakeCoTaskMemString(Uri);

	FCBWebView2MonitoredEvent EventInfo;
	EventInfo.EventType = ECBWebView2MonitoredEventType::NavigationStarting;
	EventInfo.Url = TargetUrl;
	EmitMonitoredEvent(EventInfo);

	OnNavigationStarting.ExecuteIfBound(TargetUrl);
	return S_OK;
}

HRESULT FWebView2Window::OnCursorChangedInternal(ICoreWebView2CompositionController* Sender, IUnknown* Args)
{
	HCURSOR CursorHandle = nullptr;
	const HRESULT Hr = Sender->get_Cursor(&CursorHandle);
	if (FAILED(Hr))
	{
		return Hr;
	}

	EMouseCursor::Type CursorType = EMouseCursor::Default;
	if (CursorHandle == LoadCursor(nullptr, IDC_HAND))
	{
		CursorType = EMouseCursor::Hand;
	}
	else if (CursorHandle == LoadCursor(nullptr, IDC_IBEAM))
	{
		CursorType = EMouseCursor::TextEditBeam;
	}

	OnCursorChanged.ExecuteIfBound(CursorType);
	return S_OK;
}

HRESULT FWebView2Window::OnAcceleratorKeyPressedInternal(ICoreWebView2Controller* Sender, ICoreWebView2AcceleratorKeyPressedEventArgs* Args)
{
	COREWEBVIEW2_KEY_EVENT_KIND EventKind = COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN;
	Args->get_KeyEventKind(&EventKind);

	UINT VirtualKey = 0;
	Args->get_VirtualKey(&VirtualKey);

	if (EventKind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN)
	{
		::PostMessageW(ParentWindow, WM_KEYDOWN, VirtualKey, 0);
	}
	else if (EventKind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_UP)
	{
		::PostMessageW(ParentWindow, WM_KEYUP, VirtualKey, 0);
	}

	return S_OK;
}

HRESULT FWebView2Window::OnHistoryChangedInternal(ICoreWebView2* Sender, IUnknown* Args)
{
	BOOL bCanGoBack = 0;
	BOOL bCanGoForward = 0;
	Sender->get_CanGoBack(&bCanGoBack);
	Sender->get_CanGoForward(&bCanGoForward);

	FCBWebView2MonitoredEvent EventInfo;
	EventInfo.EventType = ECBWebView2MonitoredEventType::HistoryChanged;
	EventInfo.bCanGoBack = !!bCanGoBack;
	EventInfo.bCanGoForward = !!bCanGoForward;
	EmitMonitoredEvent(EventInfo);

	OnCanGoBackChanged.ExecuteIfBound(!!bCanGoBack);
	OnCanGoForwardChanged.ExecuteIfBound(!!bCanGoForward);
	return S_OK;
}

HRESULT FWebView2Window::OnDocumentTitleChangedInternal(ICoreWebView2* Sender, IUnknown* Args)
{
	LPWSTR RawTitle = nullptr;
	Sender->get_DocumentTitle(&RawTitle);
	const FString Title = TakeCoTaskMemString(RawTitle);

	FCBWebView2MonitoredEvent EventInfo;
	EventInfo.EventType = ECBWebView2MonitoredEventType::DocumentTitleChanged;
	EventInfo.Title = Title;
	EmitMonitoredEvent(EventInfo);

	OnDocumentTitleChanged.ExecuteIfBound(Title);
	return S_OK;
}

HRESULT FWebView2Window::OnSourceChangedInternal(ICoreWebView2* Sender, ICoreWebView2SourceChangedEventArgs* Args)
{
	LPWSTR RawSource = nullptr;
	Sender->get_Source(&RawSource);
	const FString Source = RawSource ? TakeCoTaskMemString(RawSource) : TEXT("about:blank");

	FCBWebView2MonitoredEvent EventInfo;
	EventInfo.EventType = ECBWebView2MonitoredEventType::SourceChanged;
	EventInfo.Url = Source;
	EmitMonitoredEvent(EventInfo);

	OnSourceChanged.ExecuteIfBound(Source);
	return S_OK;
}

HRESULT FWebView2Window::OnDownloadStartingInternal(ICoreWebView2* Sender, ICoreWebView2DownloadStartingEventArgs* Args)
{
	Microsoft::WRL::ComPtr<ICoreWebView2DownloadOperation> DownloadOperation;
	if (FAILED(Args->get_DownloadOperation(&DownloadOperation)) || !DownloadOperation)
	{
		return S_OK;
	}

	EmitMonitoredEvent(BuildDownloadMonitoredEvent(ECBWebView2MonitoredEventType::DownloadStarting, DownloadOperation.Get()));
	OnDownloadStarting.ExecuteIfBound(BuildDownloadInfo(DownloadOperation.Get()));
	// 下载开始后注册两个持续事件：字节变化和状态变化。
	RegisterDownloadTracking(DownloadOperation.Get());
	return S_OK;
}

HRESULT FWebView2Window::OnPermissionRequestedInternal(ICoreWebView2* Sender, ICoreWebView2PermissionRequestedEventArgs* Args)
{
	COREWEBVIEW2_PERMISSION_KIND PermissionKind = COREWEBVIEW2_PERMISSION_KIND_UNKNOWN_PERMISSION;
	Args->get_PermissionKind(&PermissionKind);

	LPWSTR RawUri = nullptr;
	Args->get_Uri(&RawUri);

	BOOL bUserInitiated = 0;
	Args->get_IsUserInitiated(&bUserInitiated);

	FCBWebView2MonitoredEvent EventInfo;
	EventInfo.EventType = ECBWebView2MonitoredEventType::PermissionRequested;
	EventInfo.Url = TakeCoTaskMemString(RawUri);
	EventInfo.PermissionKind = PermissionKindToString(PermissionKind);
	EventInfo.bUserInitiated = !!bUserInitiated;
	EmitMonitoredEvent(EventInfo);
	return S_OK;
}

FWebView2DownloadInfo FWebView2Window::BuildDownloadInfo(ICoreWebView2DownloadOperation* DownloadOperation) const
{
	FWebView2DownloadInfo Info;
	if (!DownloadOperation)
	{
		return Info;
	}

	LPWSTR RawString = nullptr;
	if (SUCCEEDED(DownloadOperation->get_Uri(&RawString)))
	{
		Info.Uri = TakeCoTaskMemString(RawString);
	}

	RawString = nullptr;
	if (SUCCEEDED(DownloadOperation->get_MimeType(&RawString)))
	{
		Info.MimeType = TakeCoTaskMemString(RawString);
	}

	RawString = nullptr;
	if (SUCCEEDED(DownloadOperation->get_ResultFilePath(&RawString)))
	{
		Info.ResultFilePath = TakeCoTaskMemString(RawString);
	}

	DownloadOperation->get_BytesReceived(&Info.BytesReceived);
	DownloadOperation->get_TotalBytesToReceive(&Info.TotalBytesToReceive);

	COREWEBVIEW2_DOWNLOAD_STATE DownloadState = COREWEBVIEW2_DOWNLOAD_STATE_IN_PROGRESS;
	if (SUCCEEDED(DownloadOperation->get_State(&DownloadState)))
	{
		Info.State = ToDownloadState(DownloadState);
	}

	return Info;
}

FCBWebView2MonitoredEvent FWebView2Window::BuildDownloadMonitoredEvent(ECBWebView2MonitoredEventType EventType, ICoreWebView2DownloadOperation* DownloadOperation) const
{
	const FWebView2DownloadInfo DownloadInfo = BuildDownloadInfo(DownloadOperation);

	FCBWebView2MonitoredEvent EventInfo;
	EventInfo.EventType = EventType;
	EventInfo.Url = DownloadInfo.Uri;
	EventInfo.ResultFilePath = DownloadInfo.ResultFilePath;
	EventInfo.BytesReceived = DownloadInfo.BytesReceived;
	EventInfo.TotalBytesToReceive = DownloadInfo.TotalBytesToReceive;
	EventInfo.State = DownloadStateToString(DownloadInfo.State);
	EventInfo.bSuccess = DownloadInfo.State == EWebView2DownloadState::Completed;
	return EventInfo;
}

void FWebView2Window::EmitMonitoredEvent(const FCBWebView2MonitoredEvent& EventInfo) const
{
	// 先通知当前实例监听者，再向子系统做全局广播。
	OnMonitoredEvent.ExecuteIfBound(EventInfo);
	if (UWebView2Subsystem* Subsystem = UWebView2Subsystem::Get())
	{
		Subsystem->OnMonitoredEventNative.ExecuteIfBound(EventInfo);
		Subsystem->OnMonitoredEvent.Broadcast(EventInfo);
	}
}

void FWebView2Window::RegisterDownloadTracking(ICoreWebView2DownloadOperation* DownloadOperation)
{
	if (!DownloadOperation)
	{
		return;
	}

	// 以 COM 指针地址作为下载实例键，便于去重与解绑。
	const uint64 DownloadKey = reinterpret_cast<uint64>(DownloadOperation);
	if (ActiveDownloads.Contains(DownloadKey))
	{
		return;
	}

	FTrackedDownloadRegistration Registration;
	Registration.Operation = DownloadOperation;

	DownloadOperation->add_BytesReceivedChanged(
		Microsoft::WRL::Callback<ICoreWebView2BytesReceivedChangedEventHandler>(
			[this](ICoreWebView2DownloadOperation* InDownloadOperation, IUnknown* Args) -> HRESULT
			{
				EmitMonitoredEvent(BuildDownloadMonitoredEvent(ECBWebView2MonitoredEventType::DownloadUpdated, InDownloadOperation));
				OnDownloadUpdated.ExecuteIfBound(BuildDownloadInfo(InDownloadOperation));
				return S_OK;
			})
			.Get(),
		&Registration.BytesReceivedChangedToken);

	DownloadOperation->add_StateChanged(
		Microsoft::WRL::Callback<ICoreWebView2StateChangedEventHandler>(
			[this](ICoreWebView2DownloadOperation* InDownloadOperation, IUnknown* Args) -> HRESULT
			{
				const FWebView2DownloadInfo DownloadInfo = BuildDownloadInfo(InDownloadOperation);
				EmitMonitoredEvent(BuildDownloadMonitoredEvent(ECBWebView2MonitoredEventType::DownloadUpdated, InDownloadOperation));
				OnDownloadUpdated.ExecuteIfBound(DownloadInfo);

				if (DownloadInfo.State != EWebView2DownloadState::InProgress)
				{
					UnregisterDownloadTracking(reinterpret_cast<uint64>(InDownloadOperation));
				}
				return S_OK;
			})
			.Get(),
		&Registration.StateChangedToken);

	ActiveDownloads.Add(DownloadKey, MoveTemp(Registration));
}

void FWebView2Window::UnregisterDownloadTracking(uint64 DownloadKey)
{
	if (FTrackedDownloadRegistration* Registration = ActiveDownloads.Find(DownloadKey))
	{
		if (Registration->Operation)
		{
			Registration->Operation->remove_BytesReceivedChanged(Registration->BytesReceivedChangedToken);
			Registration->Operation->remove_StateChanged(Registration->StateChangedToken);
		}
		ActiveDownloads.Remove(DownloadKey);
	}
}

void FWebView2Window::ApplyRuntimeSettings() const
{
	if (!WebView)
	{
		return;
	}

	// 运行期开关与 Environment / Controller 配置不同，可以在实例创建后写入。
	Microsoft::WRL::ComPtr<ICoreWebView2Settings> RuntimeSettings;
	WebView->get_Settings(&RuntimeSettings);

	const UWebView2Settings* Settings = UWebView2Settings::Get();
	if (!RuntimeSettings || !Settings)
	{
		return;
	}

	RuntimeSettings->put_AreDefaultContextMenusEnabled(Settings->Features.bEnableContextMenus);
	RuntimeSettings->put_AreDefaultScriptDialogsEnabled(Settings->Features.bEnableScriptDialogs);
	RuntimeSettings->put_AreDevToolsEnabled(Settings->Features.bEnableDevTools);
	RuntimeSettings->put_AreHostObjectsAllowed(Settings->Features.bAllowHostObjects);
	RuntimeSettings->put_IsBuiltInErrorPageEnabled(Settings->Features.bEnableBuiltInErrorPage);
	RuntimeSettings->put_IsScriptEnabled(Settings->Features.bEnableScript);
	RuntimeSettings->put_IsStatusBarEnabled(Settings->Features.bEnableStatusBar);
	RuntimeSettings->put_IsWebMessageEnabled(Settings->Features.bEnableWebMessage);
	RuntimeSettings->put_IsZoomControlEnabled(Settings->Features.bEnableZoomControl);

	Microsoft::WRL::ComPtr<ICoreWebView2_8> WebView8;
	if (SUCCEEDED(WebView.As(&WebView8)) && WebView8)
	{
		WebView8->put_IsMuted(Settings->Features.bMuted);
	}
}

void FWebView2Window::RegisterCoreEvents()
{
	if (!WebView || !Controller)
	{
		return;
	}

	// 统一注册全部核心事件，Close 时使用对应 Token 安全解绑。
	WebView->add_WebMessageReceived(
		Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(this, &FWebView2Window::OnMessageReceivedInternal).Get(),
		&MessageReceivedToken);

	WebView->add_NavigationCompleted(
		Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(this, &FWebView2Window::OnNavigationCompletedInternal).Get(),
		&NavigationCompletedToken);

	WebView->add_NavigationStarting(
		Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(this, &FWebView2Window::OnNavigationStartingInternal).Get(),
		&NavigationStartingToken);

	WebView->add_NewWindowRequested(
		Microsoft::WRL::Callback<ICoreWebView2NewWindowRequestedEventHandler>(this, &FWebView2Window::OnNewWindowRequestedInternal).Get(),
		&NewWindowRequestedToken);

	WebView->add_HistoryChanged(
		Microsoft::WRL::Callback<ICoreWebView2HistoryChangedEventHandler>(this, &FWebView2Window::OnHistoryChangedInternal).Get(),
		&HistoryChangedToken);

	WebView->add_DocumentTitleChanged(
		Microsoft::WRL::Callback<ICoreWebView2DocumentTitleChangedEventHandler>(this, &FWebView2Window::OnDocumentTitleChangedInternal).Get(),
		&DocumentTitleChangedToken);

	WebView->add_SourceChanged(
		Microsoft::WRL::Callback<ICoreWebView2SourceChangedEventHandler>(this, &FWebView2Window::OnSourceChangedInternal).Get(),
		&SourceChangedToken);

	if (Microsoft::WRL::ComPtr<ICoreWebView2_4> WebView4; SUCCEEDED(WebView.As(&WebView4)) && WebView4)
	{
		WebView4->add_DownloadStarting(
			Microsoft::WRL::Callback<ICoreWebView2DownloadStartingEventHandler>(this, &FWebView2Window::OnDownloadStartingInternal).Get(),
			&DownloadStartingToken);
	}

	WebView->add_PermissionRequested(
		Microsoft::WRL::Callback<ICoreWebView2PermissionRequestedEventHandler>(this, &FWebView2Window::OnPermissionRequestedInternal).Get(),
		&PermissionRequestedToken);

	Controller->add_AcceleratorKeyPressed(
		Microsoft::WRL::Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(this, &FWebView2Window::OnAcceleratorKeyPressedInternal).Get(),
		&AcceleratorKeyPressedToken);

	if (CompositionController)
	{
		CompositionController->add_CursorChanged(
			Microsoft::WRL::Callback<ICoreWebView2CursorChangedEventHandler>(this, &FWebView2Window::OnCursorChangedInternal).Get(),
			&CursorChangedToken);
	}
}