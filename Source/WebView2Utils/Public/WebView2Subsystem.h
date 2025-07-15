// Copyright 2025-Present Xiao Lan fei. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "WebView2Subsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebMessageReceived, FString, Message);
DECLARE_DELEGATE_OneParam(FOnMessageReceivedNactive,const FString&)
UCLASS()
class WEBVIEW2UTILS_API UWebView2Subsystem : public UEngineSubsystem,public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//Tick
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
public:
	static UWebView2Subsystem* GetWebView2Subsystem();
public:
	UPROPERTY(BlueprintAssignable, Category = "WebView|Event")
	FOnWebMessageReceived OnWebMessageReceived;
	
	FOnMessageReceivedNactive OnMessageReceivedNactive;

	//判断鼠标是否在位置区域内的标志
	bool bIsMouseOverPositionArea = true;
	
private:
	void OnEndPIE(bool bIsSimulating);

	/** 处理 GameInstance 启动的回调函数 */
	void OnGameInstanceStarted(UGameInstance* GameInstance);

	/** 存储 GameInstance 启动委托的句柄 */
    FDelegateHandle OnStartGameInstanceDelegateHandle;
};

