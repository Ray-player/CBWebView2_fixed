#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tickable.h"
#include "WebView2EventTypes.h"

#include "WebView2Subsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebView2SubsystemMessage, const FString&, Message);
DECLARE_DELEGATE_OneParam(FOnWebView2SubsystemMessageNative, const FString&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebView2SubsystemMonitorEvent, const FCBWebView2MonitoredEvent&, EventInfo);
DECLARE_DELEGATE_OneParam(FOnWebView2SubsystemMonitorEventNative, const FCBWebView2MonitoredEvent&);

/**
 * 引擎级 WebView2 子系统。
 *
 * 这一层用于注册 Windows 消息处理器，并维护当前 WebView 是否持有输入焦点。
 */
UCLASS()
class WEBVIEW2UTILS_API UWebView2Subsystem : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** 注册管理器、消息处理器以及编辑器生命周期回调。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** 卸载消息处理器并关闭原生宿主。 */
	virtual void Deinitialize() override;

	/** 处理宿主窗口焦点回收。 */
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	/** 获取全局唯一子系统实例。 */
	static UWebView2Subsystem* Get();

	/** 记录当前是否由 WebView 持有真实输入焦点。 */
	void SetWebViewFocused(bool bInFocused);
	/** 查询当前焦点状态。 */
	bool IsWebViewFocused() const;

	UPROPERTY(BlueprintAssignable, Category = "CBWebView2")
	FOnWebView2SubsystemMessage OnWebMessageReceived;

	UPROPERTY(BlueprintAssignable, Category = "CBWebView2")
	FOnWebView2SubsystemMonitorEvent OnMonitoredEvent;

	FOnWebView2SubsystemMessageNative OnWebMessageReceivedNative;
	FOnWebView2SubsystemMonitorEventNative OnMonitoredEventNative;

private:
	/** PIE 结束时清理宿主窗口对应的原生资源。 */
	void OnEndPIE(bool bIsSimulating);

	/** 仅表示真实输入焦点，不表示透明命中状态。 */
	bool bIsWebViewFocused = true;
};