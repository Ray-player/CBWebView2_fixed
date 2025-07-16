#include "CBWebView2Widget.h"

#include "SCBWebView2.h"



UCBWebView2Widget::UCBWebView2Widget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, URL(TEXT(""))
	, MousePenetrationOpacity(0.0f)
    , BackgroundColor(FColor(255, 255, 255, 255)), bShowAddressBar(false),bShowControls(false)
{
}

void UCBWebView2Widget::ReleaseSlateResources(bool bReleaseChildren)
{
	
	Super::ReleaseSlateResources(bReleaseChildren);

	if(CBWebView2)
	{
		CBWebView2->BeginDestroy();
	}
	CBWebView2.Reset();

}

TSharedRef<SWidget> UCBWebView2Widget::RebuildWidget()
{
	if(GEngine && GEngine->GameViewport)
	{
		return
			SAssignNew(CBWebView2,SCBWebView2,GEngine->GameViewport->GetWindow().ToSharedRef())
			.URL(URL)
			.Color(BackgroundColor)
			.ShowAddressBar(bShowAddressBar)
			.ShowControls(bShowControls)
			.ShowTouchArea(bShowTouchArea)
			.SetMouseOpacity(MousePenetrationOpacity)
			.NewOnWebView2MessageReceived_UObject(this,&UCBWebView2Widget::OnWebView2MessageReceivedCallback)
			.NewOnWebView2NavigationCompleted_UObject(this,&UCBWebView2Widget::OnWebView2LoadCompletedCallback)
			.NewOnNavigationStarting_UObject(this,&UCBWebView2Widget::OnWebView2NavigationStartingCallback)
			.NewOnNewWindowRequestedNactive_UObject(this,&UCBWebView2Widget::OnWebView2NewWindowRequestedCallback)
			.NewOnWebViewCreated_UObject(this, &UCBWebView2Widget::OnWebViewCreatedCallback);
	}
	/**if(GEngine && GEngine->IsEditor())
	{
		if(FSlateApplication::Get().IsActive())
		{
			const TSharedRef<SWindow> Window =FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef();
		
			return SAssignNew(CBWebView2,SCBWebView2,Window)
			.URL(URL)
			.Color(BackgroundColor)
			.NewOnWebView2MessageReceived_UObject(this,&UCBWebView2Widget::OnWebView2MessageReceivedCallback);
		}
	
	}*/
	{
		return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("this is webview2 ui")))
		];
	}
	
}


void UCBWebView2Widget::SetVisible(ESlateVisibility InVisibility)
{
	if(CBWebView2)
	{
		CBWebView2->SetVisible(InVisibility);
	}

	SetVisibility(InVisibility);
}

void UCBWebView2Widget::SetBackgroundColor(FColor InBackgroundColor)
{
	if(CBWebView2)
	{
		CBWebView2->SetBackgroundColor(InBackgroundColor);
	}
}

void UCBWebView2Widget::ExecuteScript(const FString& Script, FWebView2ScriptCallback Callback) const
{
	if(CBWebView2)
	{
		CBWebView2->ExecuteScript(Script,FWebView2ScriptCallbackNative::CreateLambda([Callback](const FString& Data)
		{
			if(Callback.IsBound())
			{
				//UE_LOG(LogTemp,Warning,TEXT("%s"),*Data);
				Callback.Execute(Data);
			}
		}));
	}
}

void UCBWebView2Widget::LoadURL(const FString InURL)
{
	if(CBWebView2)
	{
		CBWebView2->LoadURL(InURL);
	}
}

void UCBWebView2Widget::GoForward() const
{
	if(CBWebView2)
	{
		CBWebView2->GoForward();
	}
}

void UCBWebView2Widget::GoBack() const
{
	if(CBWebView2)
	{
		CBWebView2->GoBack();
	}
}

void UCBWebView2Widget::ReLoad() const
{
	if(CBWebView2)
	{
		CBWebView2->ReLoad();
	}
}

void UCBWebView2Widget::Stop() const
{
	if(CBWebView2)
	{
		CBWebView2->Stop();
	}
}

void UCBWebView2Widget::OnWebView2MessageReceivedCallback(const FString& NewMessage)
{
	OnWebView2MessageReceived.Broadcast(NewMessage);
}

void UCBWebView2Widget::OnWebView2LoadCompletedCallback(const bool& bSuccess)
{
	OnWebView2LoadCompleted.Broadcast(bSuccess);
}

void UCBWebView2Widget::OnWebView2NavigationStartingCallback(const FString& NewURL)
{
	OnWebView2LoadStrat.Broadcast(NewURL);
}

void UCBWebView2Widget::OnWebView2NewWindowRequestedCallback(const FString& NewURL)
{
	OnNewWindowRequested.Broadcast(NewURL);
}

void UCBWebView2Widget::OnWebViewCreatedCallback(const bool& bIsCreat)
{
	OnCreatedWebViewWidget.Broadcast(bIsCreat);
}
