#include "SCBWebView2.h"

#include "WebView2CompositionHost.h"
#include "WebView2Window.h"
#include "WebView2Manager.h"
#include "WebView2Subsystem.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/SlateWrapperTypes.h"
#include "Misc/CoreMiscDefines.h"


#define LOCTEXT_NAMESPACE "CBWebView2"

SCBWebView2::SCBWebView2()
	: bShowInitialThrobber(false), bShowAddressBar(false), bShowControls(false), MousePenetrationOpacity(0.f),UniqueId(FGuid::NewGuid())
	  , WebViewWindowHandle(nullptr)
	  , Handle(FReply::Unhandled())
	  , FixSacale(1.0f)
{
}

SCBWebView2::~SCBWebView2()
{
	if (WebView2Window)
	{
		
		WebView2Window.Reset();
	}
}

void SCBWebView2::Construct(const FArguments& InArgs, TSharedRef<SWindow> InParentWindowPtr)
{
	InitializeURL=InArgs._URL;
	BackgroundColor=InArgs._Color;
	OnWebView2MessageReceived=InArgs._NewOnWebView2MessageReceived;
	OnWebView2NavigationCompleted=InArgs._NewOnWebView2NavigationCompleted;
	OnWebView2NavigationStarting=InArgs._NewOnNavigationStarting;
	OnWebView2NewWindowRequested=InArgs._NewOnNewWindowRequestedNactive;
	OnWebView2CursorChangedNactive=InArgs._NewOnCursorChangedNactive;
	//创建赋值链接
	OnWebViewCreated=InArgs._NewOnWebViewCreated;
	bShowAddressBar=InArgs._ShowAddressBar;
	bShowControls=InArgs._ShowControls;
	MousePenetrationOpacity=InArgs._SetMouseOpacity;
	bShowTouchArea=InArgs._ShowTouchArea;
	bShowInitialThrobber = InArgs._ShowInitialThrobber;
	
		ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			.Visibility((InArgs._ShowControls || InArgs._ShowAddressBar) ? EVisibility::Visible : EVisibility::Collapsed)
			+ SHorizontalBox::Slot()
			.Padding(0, 5)
			.AutoWidth()
			[
				SNew(SHorizontalBox)
				.Visibility(InArgs._ShowControls ? EVisibility::Visible : EVisibility::Collapsed)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Back","Back"))
					.IsEnabled(this, &SCBWebView2::CanGoBack)
					.OnClicked(this, &SCBWebView2::OnBackClicked)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Forward", "Forward"))
					.IsEnabled(this, &SCBWebView2::CanGoForward)
					.OnClicked(this, &SCBWebView2::OnForwardClicked)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(this, &SCBWebView2::GetReloadButtonText)
					.OnClicked(this, &SCBWebView2::OnReloadClicked)
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(5)
				[
					SNew(STextBlock)
					.Visibility(InArgs._ShowAddressBar ? EVisibility::Collapsed : EVisibility::Visible )
					.Text(this, &SCBWebView2::GetTitleText)
					.Justification(ETextJustify::Right)
				]
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.Padding(5.f, 5.f)
			[
				// @todo: A proper addressbar widget should go here, for now we use a simple textbox.
				SAssignNew(InputText, SEditableTextBox)
				.Visibility(InArgs._ShowAddressBar ? EVisibility::Visible : EVisibility::Collapsed)
				.OnTextCommitted(this, &SCBWebView2::OnUrlTextCommitted)
				.Text(this, &SCBWebView2::GetAddressBarUrlText)
				.SelectAllTextWhenFocused(true)
				.ClearKeyboardFocusOnCommit(true)
				.RevertTextOnEscape(true)
			]
		]
		+SVerticalBox::Slot()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SCircularThrobber)
				.Radius(10.0f)
				.ToolTipText(LOCTEXT("LoadingThrobberToolTip", "Loading page..."))
				.Visibility(this, &SCBWebView2::GetLoadingThrobberVisibility)
			]
			+ SOverlay::Slot()
			[
				SAssignNew(PositionOverlay, SOverlay)
			]
		]
	];
	
	if(	void* Hwnd = InParentWindowPtr->GetNativeWindow()->GetOSWindowHandle())
	{
		WebViewWindowHandle=(HWND)Hwnd;
		WebView2Window=FWebView2Manager::GetInstance()->CreateWebview(
			WebViewWindowHandle,
			UniqueId,
			InitializeURL,
			BackgroundColor);

		// 绑定WebView创建完成事件
		WebView2Window->OnWebViewCreated.BindLambda([this,InArgs](bool bSuccess)
		{
			OnWebViewCreated.ExecuteIfBound(bSuccess);
		});
		WebView2Window->OnWebView2MessageReceived.BindLambda([this,InArgs](FString Message)
		{
			OnWebView2MessageReceived.ExecuteIfBound(Message);
				// 解析JSON
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
			
			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
			{
				// 获取"position"数组
				const TArray<TSharedPtr<FJsonValue>>* Positions;
				FString Type; 
				
				if (JsonObject->TryGetStringField(TEXT("type"), Type) && Type.Equals("webPosition"))
				{
					float WebWidth=.0f;
					float WebHeight=.0f;
					JsonObject->TryGetNumberField(TEXT("resolutionW"), WebWidth);
					JsonObject->TryGetNumberField(TEXT("resolutionH"), WebHeight);
					// 获取PositionOverlay的实际尺寸
					FVector2D OverlaySize = PositionOverlay->GetCachedGeometry().GetLocalSize();
					UE_LOG(LogTemp,Warning, TEXT("FixedSacale: %f  WebViewSize: %f %f"), FixSacale,OverlaySize.X, OverlaySize.Y);
					if (JsonObject->TryGetArrayField(TEXT("position"), Positions))
					{
						// 清除现有的位置标记和区域数据
						PositionOverlay->ClearChildren();
						Images.Empty();
						// 计算缩放比例
						const float ScaleX = OverlaySize.X / WebWidth;
						const float ScaleY = OverlaySize.Y / WebHeight;
						for (const TSharedPtr<FJsonValue>& PosValue : *Positions)
						{
							const TSharedPtr<FJsonObject> PosObj = PosValue->AsObject();
							if (PosObj.IsValid())
							{
								// 提取坐标和尺寸
								float OriginalX = PosObj->GetNumberField(TEXT("x"));
								float OriginalY = PosObj->GetNumberField(TEXT("y"));
								float OriginalWidth = PosObj->GetNumberField(TEXT("width"));
								float OriginalHeight = PosObj->HasField(TEXT("high")) ? 
									PosObj->GetNumberField(TEXT("high")) : 
									PosObj->GetNumberField(TEXT("height"));
								// 转换为适应PositionOverlay的坐标和尺寸
								float X = OriginalX * ScaleX ;
								float Y = OriginalY * ScaleY ;
								float Width = OriginalWidth * ScaleX ;
								float Height = OriginalHeight * ScaleY ;

								// 创建位置标记 (视觉效果)
								TSharedPtr<SImage> NewImage;
								TSharedPtr<SBox> PositionBox = 
									SNew(SBox)
									.WidthOverride(Width)
									.HeightOverride(Height)
									[
										SAssignNew(NewImage,SImage)
										.Image( InArgs._ShowTouchArea ? (new FSlateColorBrush(FLinearColor(1, 0, 0, 0.3))) :(new FSlateColorBrush(FLinearColor(1, 0, 0, 0))))
									];

								Images.Add(NewImage);
								// 添加到PositionOverlay
								PositionOverlay->AddSlot()
									.Padding(FMargin(X, Y, 0, 0)) // 使用 Padding 设置左上角位置
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Top)
									[
										PositionBox.ToSharedRef()
									];
								}
							}
						}
					}

				}
			
		});

		WebView2Window->OnNavigationCompleted.BindLambda([this,InArgs](bool bSuccess)
		{
			OnWebView2NavigationCompleted.ExecuteIfBound(bSuccess);

			// 清除现有的位置标记和区域数据
			PositionOverlay->ClearChildren();
			Images.Empty();
			RECT Bounds=WebView2Window->GetBounds();
			float SizeY=Bounds.bottom-Bounds.top;
			float SizeX=Bounds.right-Bounds.left;
			
		// 创建位置标记
			TSharedPtr<SImage> NewImage;
			TSharedPtr<SBox> PositionBox = 
				SNew(SBox)
				[
					SAssignNew(NewImage,SImage)
					.Image( InArgs._ShowTouchArea ? (new FSlateColorBrush(FLinearColor(1, 0, 0, 0.3))) :(new FSlateColorBrush(FLinearColor(1, 0, 0, 0))))
				];
			
			Images.Add(NewImage);
		// 添加到PositionOverlay
			PositionOverlay->AddSlot()
			//.Padding(FMargin(OffsetX, OffsetY, 0, 0)) // 使用 Padding 设置左上角位置
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				PositionBox.ToSharedRef()
			];			
		});

		WebView2Window->OnNavigationStarting.BindLambda([this](FString InURL)
		{
			OnWebView2NavigationStarting.ExecuteIfBound(InURL);

			
		});
		WebView2Window->OnNewWindowRequested.BindLambda([this](FString InURL)
		{
			OnWebView2NewWindowRequested.ExecuteIfBound(InURL);
		});

		WebView2Window->OnCanGoBackNactive.BindLambda([this](bool bInCanGoBack)
		{
			bCanGoBack=bInCanGoBack;
		});

		WebView2Window->OnCanGoForwardNactive.BindLambda([this](bool bInCanGoForward)
		{
			bCanGoForward=bInCanGoForward;
		});
		WebView2Window->OnDocumentTitleChangedNactive.BindLambda([this](FString InTitile)
		{
			Title=InTitile;
		});

		WebView2Window->OnSourceChangedNactive.BindLambda([this](FString InURL)
		{
			URL=InURL;
		});
		
		WebView2Window->OnCursorChangedNactive.BindLambda([this]( EMouseCursor::Type InCursor)
		{
			SetCursor(InCursor);
		});

		
	}
}

FReply SCBWebView2::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UWebView2Subsystem* Subsystem = UWebView2Subsystem::GetWebView2Subsystem();

	if(!WebView2Window)
	{
		return  SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
	}
	
	// 直接使用 Tick 中计算好的状态
	if (WebView2Window->bIsMouseOverPositionArea)
	{
		// 根据您的修改，如果在区域内，事件不被处理 (Unhandled)，允许传递
		UE_LOG(LogTemp, Log, TEXT("Mouse button down over position area - Unhandled"));
		// 如果您希望捕获事件，则返回 Handled
		 return FReply::Handled(); 
	}
	
	// 如果不在区域内，则调用基类默认行为
	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

void SCBWebView2::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	
}

void SCBWebView2::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
}

bool SCBWebView2::HasKeyboardFocus() const
{
	return SCompoundWidget::HasKeyboardFocus();
}

void SCBWebView2::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	

	// 获取当前鼠标屏幕坐标
	FVector2D ScreenMousePos = FSlateApplication::Get().GetCursorPos();
	

	// 遍历每个SImage
	for (TSharedPtr<SImage> ImageWidget : Images)
	{
		if (!ImageWidget.IsValid())
		{
			continue;
		}
		
		// 判断鼠标是否在这个Geometry范围内
		if (ImageWidget->GetCachedGeometry().IsUnderLocation(ScreenMousePos))
		{
			if(WebView2Window)
			{
				WebView2Window->bIsMouseOverPositionArea = true;
			}
			
			break; // 找到一个匹配区域即可退出循环
		}else
		{

			if(WebView2Window)
			{
				WebView2Window->bIsMouseOverPositionArea = false;
			}
		}
	}
	
	
}

int32 SCBWebView2::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	if (WebView2Window)
	{
		FixSacale=AllottedGeometry.Scale;
		FVector2D Offset = AllottedGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		FVector2D Size = AllottedGeometry.GetDrawSize();

		if(Size.X>16000.f || Size.X<0.f)
		{
			Size.X=100.f;
		}

		if(Size.Y>16000.f || Size.Y<0.f)
		{
			Size.Y=100.f;
		}
		
		POINT Of;
		Of.x = Offset.X;
		Of.y = bShowAddressBar ||bShowControls ? Offset.Y+30*AllottedGeometry.Scale : Offset.Y;
		POINT Si;
		Si.x = Size.X;
		Si.y = bShowAddressBar || bShowControls ? Size.Y-30*AllottedGeometry.Scale : Size.Y;
		
		WebView2Window->SetBounds(Of, Si);
		//UE_LOG(LogTemp,Warning,TEXT("%f,%f"), Size.X, Size.Y);
		if(WebView2Window->GetWebView2CompositionHost())
		{
			if(LayerId !=WebView2Window->GetLayerID())
			{
				WebView2Window->SetLayerID(LayerId);
				WebView2Window->GetWebView2CompositionHost()->RefreshWebViewVisual();
				//UE_LOG(LogTemp,Warning,TEXT("%d"),LayerId);
			}
		}
		
	}
	
	int32 Layer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	return Layer;
}



FVector2D SCBWebView2::ComputeDesiredSize(float) const
{
		if (GetVisibility() == EVisibility::Collapsed)
		{
			return FVector2D::ZeroVector;
		}
		if (WebView2Window)
		{
			RECT bound = WebView2Window->GetBounds();
			return FVector2D(
				(static_cast<float>(bound.right - bound.left)),
				(static_cast<float>(bound.bottom - bound.top)));
		}
		return FVector2D::ZeroVector;
}

FCursorReply SCBWebView2::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return SCompoundWidget::OnCursorQuery(MyGeometry, CursorEvent);
}

void SCBWebView2::BeginDestroy()
{
	WebView2Window->CloseWindow();
}

void SCBWebView2::ExecuteScript(const FString& Script, FWebView2ScriptCallbackNative ScriptCallback) const
{
	if(WebView2Window)
	{
		WebView2Window->ExecuteScript(Script,[ScriptCallback](const FString Reslut)
		{
			if(ScriptCallback.IsBound())
			{
				ScriptCallback.Execute(Reslut);
			}
		});
	}
}

void SCBWebView2::LoadURL(const FString InURL)
{
	if(WebView2Window)
	{
		WebView2Window->LoadURL(InURL);
	}
	else
	{
		UE_LOG(LogTemp,Warning,TEXT("WebView2Window is null"));
	}
}

void SCBWebView2::GoForward() const
{
	if(WebView2Window)
	{
		WebView2Window->GoForward();
	}
}

void SCBWebView2::GoBack() const
{
	if(WebView2Window)
	{
		WebView2Window->GoBack();
	}
}

bool SCBWebView2::CanGoBack() const
{
	return bCanGoBack;
}

bool SCBWebView2::CanGoForward() const
{
	return bCanGoForward;
}

void SCBWebView2::ReLoad() const
{
	if(WebView2Window)
	{
		WebView2Window->ReLoad();
	}
}

void SCBWebView2::Stop() const
{
	if(WebView2Window)
	{
		WebView2Window->Stop();
	}
}

bool SCBWebView2::IsLoading() const
{
	if(WebView2Window)
	{
		if (WebView2Window->GetDocumentLoadingState() == EWebView2DocumentState::Loading)
		{
			return true;
		}
	}

	return false;
}

void SCBWebView2::StopLoad()
{
	if(WebView2Window)
	{
		WebView2Window->Stop();
	}
}

void SCBWebView2::Reload()
{
	if(WebView2Window)
	{
		WebView2Window->ReLoad();
	}
}

FText SCBWebView2::GetTitleText() const
{
	if (WebView2Window)
	{
		UE_LOG(LogTemp, Display, TEXT("OnDocumentTitleChangedNactive: %s"), *Title);
		return FText::FromString(Title);
	}
    return LOCTEXT("InvalidWindow", "Browser Window is not valid/supported");
}

FText SCBWebView2::GetAddressBarUrlText() const
{
	if (WebView2Window.IsValid())
	{
		return FText::FromString(URL);
	}
	return FText::GetEmpty();
}

void SCBWebView2::SetVisible(ESlateVisibility InVisibility)
{
	if (WebView2Window.IsValid())
	{
		WebView2Window->SetVisible(InVisibility);
	}
	TAttribute<EVisibility>  EVisib;
	switch (InVisibility)
	{
	case ESlateVisibility::Collapsed:
		EVisib.Set(EVisibility::Collapsed);
		break;
	case ESlateVisibility::Hidden:
		EVisib.Set(EVisibility::Hidden);
		break;
	case ESlateVisibility::Visible:
		EVisib.Set(EVisibility::Visible);
		break;
	case ESlateVisibility::HitTestInvisible:
		EVisib.Set(EVisibility::HitTestInvisible);
		break;
	case ESlateVisibility::SelfHitTestInvisible:
		EVisib.Set(EVisibility::SelfHitTestInvisible);
		break;
	}
	
	SetVisibility(EVisib);
	
}

ESlateVisibility SCBWebView2::GetVisible()
{
	if (WebView2Window.IsValid())
	{
		return WebView2Window->GetVisible();
	}
	return ESlateVisibility::Hidden;
}

FReply SCBWebView2::OnBackClicked()
{
	GoBack();
	return FReply::Handled();
}

FReply SCBWebView2::OnForwardClicked()
{
	GoForward();
	return FReply::Handled();
}

FText SCBWebView2::GetReloadButtonText() const
{
	static FText ReloadText = LOCTEXT("Reload", "Reload");
	static FText StopText = LOCTEXT("StopText", "Stop");

	if (WebView2Window.IsValid())
	{
		if (IsLoading())
		{
			return StopText;
		}
	}
	return ReloadText;
}

FReply SCBWebView2::OnReloadClicked()
{
	if (IsLoading())
	{
		StopLoad();
	}
	else
	{
		Reload();
	}
	return FReply::Handled();
}

void SCBWebView2::OnUrlTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if(CommitType == ETextCommit::OnEnter)
	{
		LoadURL(NewText.ToString());
	}
}

EVisibility SCBWebView2::GetLoadingThrobberVisibility() const
{
	if (bShowInitialThrobber && !WebView2Window->IsInitialized())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}


void SCBWebView2::SetBackgroundColor(FColor InBackgroundColor)
{
	if (WebView2Window)
	{
		WebView2Window->SetBackgroundColor(InBackgroundColor);
	}
}

#undef LOCTEXT_NAMESPACE