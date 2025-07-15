// Copyright 2025-Present Xiao Lan fei. All Rights Reserved.


#include "WebView2Subsystem.h"

#include "WebView2CompositionHost.h"
#include "WebView2Log.h"
#include "WebView2Manager.h"
#include "Windows/WindowsApplication.h"

class FWebviewWindowsMessageHandler :public IWindowsMessageHandler
{
public:
	virtual bool ProcessMessage(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam, int32& OutResult) override;
};

bool FWebviewWindowsMessageHandler::ProcessMessage(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam,
	int32& OutResult)
{
	//WebView自己会捕获键盘，这里不需要给他同步键盘事件
	UWebView2Subsystem* Subsystem=UWebView2Subsystem::GetWebView2Subsystem();

	if (!Subsystem->bIsMouseOverPositionArea)
	{
		return false;
	}
	
	if (msg==WM_KEYDOWN||msg==WM_KEYUP||msg==WM_SYSKEYDOWN||msg==WM_SYSKEYUP)
	{
		return false;
	}
	
	return 	 FWebView2Manager::GetInstance()->HandleWindowMessage(hwnd,msg,wParam,lParam);
	
}

static FWebviewWindowsMessageHandler MessageHandler;

void UWebView2Subsystem::Initialize(FSubsystemCollectionBase& Collection)
{

	OnStartGameInstanceDelegateHandle = FWorldDelegates::OnStartGameInstance.AddUObject(this, &UWebView2Subsystem::OnGameInstanceStarted);

	FWebView2Manager::GetInstance()->Init();
#if WITH_EDITOR
	FEditorDelegates::EndPIE.AddUObject(this, &UWebView2Subsystem::OnEndPIE);
#endif
	
	// 初始化鼠标悬停状态为 true
	bIsMouseOverPositionArea = true;
	
	Super::Initialize(Collection);

	if(FSlateApplication::IsInitialized())
	{
		TSharedPtr<FWindowsApplication> WindowsApplication = StaticCastSharedPtr<FWindowsApplication>(FSlateApplication::Get().GetPlatformApplication());
		if(WindowsApplication.IsValid())
		{
			WindowsApplication->AddMessageHandler(MessageHandler);
		}
	}
}

void UWebView2Subsystem::Deinitialize()
{
	if(FSlateApplication::IsInitialized())
	{
		TSharedPtr<FWindowsApplication> WindowsApplication = StaticCastSharedPtr<FWindowsApplication>(FSlateApplication::Get().GetPlatformApplication());
		if(WindowsApplication.IsValid())
		{
			WindowsApplication->RemoveMessageHandler(MessageHandler);
		}
	}
	
	
	FWebView2Manager::GetInstance()->Shutdown();
	
	
	Super::Deinitialize();
	
}

void UWebView2Subsystem::Tick(float DeltaTime)
{
	TSharedPtr<FWebView2CompositionHost> CompositionHost= FWebView2Manager::GetInstance()->GetMessageProcess((HWND)GEngine
		->GameViewport->GetWindow().ToSharedRef()
		->GetNativeWindow()->GetOSWindowHandle());

	if(CompositionHost)
	{
		for(TPair<FString,TSharedRef<FWebView2Window>> &Element:CompositionHost->WebViewWindowMap)
		{
			if(Element.Value->bIsMouseOverPositionArea)
			{
				bIsMouseOverPositionArea=true;
				break;
			}
			else
			{
				bIsMouseOverPositionArea=false;
			}
		}

		if(!bIsMouseOverPositionArea)
		{
			//UE_LOG(LogTemp,Warning,TEXT("%hhd"),bIsMouseOverPositionArea);
			TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
			if (CurrentWindow.IsValid())
			{
				void* NativeWindow = CurrentWindow->GetNativeWindow()->GetOSWindowHandle();
				::SetFocus(static_cast<HWND>(NativeWindow));
			}
		}
		
	}
	
}

bool UWebView2Subsystem::IsTickable() const
{
	return true;
}

TStatId UWebView2Subsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWebView2Subsystem, STATGROUP_Tickables);
}

UWebView2Subsystem* UWebView2Subsystem::GetWebView2Subsystem()
{
	if (GEngine)
	{
		return  GEngine->GetEngineSubsystem<UWebView2Subsystem>();
	}
	else
	{
		UE_LOG(LogWebView2,Error,TEXT("UWebView2Subsystem does not exist!"));
		return nullptr;
	}
}

void UWebView2Subsystem::OnEndPIE(bool bIsSimulating)
{

	if(TSharedPtr< SWindow > Window=GEngine->GameViewport->GetWindow())
	{
		FWebView2Manager::GetInstance()->Closed((HWND)Window->GetNativeWindow()->GetOSWindowHandle());
	}
}

void UWebView2Subsystem::OnGameInstanceStarted(UGameInstance* GameInstance)
{
	if (GameInstance)
	{
		// 在每次 GameInstance 启动时重置变量
		bIsMouseOverPositionArea = true;
	}
	else
	{
		UE_LOG(LogWebView2, Warning, TEXT("OnGameInstanceStarted called with null GameInstance?"));
	}
}
