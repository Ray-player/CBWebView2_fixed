#pragma once

#include "CoreMinimal.h"
#include "WebView2EventTypes.generated.h"

/** 统一事件监控中的事件类型。 */
UENUM(BlueprintType)
enum class ECBWebView2MonitoredEventType : uint8
{
	NavigationStarting UMETA(DisplayName = "导航开始"),
	NavigationCompleted UMETA(DisplayName = "导航完成"),
	SourceChanged UMETA(DisplayName = "地址变更"),
	DocumentTitleChanged UMETA(DisplayName = "标题变更"),
	HistoryChanged UMETA(DisplayName = "历史状态变更"),
	WebMessageReceived UMETA(DisplayName = "网页消息"),
	NewWindowRequested UMETA(DisplayName = "新窗口请求"),
	DownloadStarting UMETA(DisplayName = "下载开始"),
	DownloadUpdated UMETA(DisplayName = "下载更新"),
	PermissionRequested UMETA(DisplayName = "权限请求")
};

/** 统一事件监控的通用负载。 */
USTRUCT(BlueprintType)
struct WEBVIEW2UTILS_API FCBWebView2MonitoredEvent
{
	GENERATED_BODY()

	/** 事件类型。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	ECBWebView2MonitoredEventType EventType = ECBWebView2MonitoredEventType::WebMessageReceived;

	/** 事件相关 URL。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	FString Url;

	/** 页面标题。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	FString Title;

	/** 文本消息体。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	FString Message;

	/** 权限类型名称。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	FString PermissionKind;

	/** 下载输出路径。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	FString ResultFilePath;

	/** 下载状态字符串。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	FString State;

	/** 已接收下载字节数。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	int64 BytesReceived = 0;

	/** 下载总字节数。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	int64 TotalBytesToReceive = -1;

	/** 通用成功标记，常用于导航完成或下载成功。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	bool bSuccess = false;

	/** 浏览器是否允许后退。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	bool bCanGoBack = false;

	/** 浏览器是否允许前进。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	bool bCanGoForward = false;

	/** 权限请求是否由用户手势触发。 */
	UPROPERTY(BlueprintReadOnly, Category = "CBWebView2|监控")
	bool bUserInitiated = false;
};

// 原生层与蓝图层都复用这一套监控事件结构。
DECLARE_DELEGATE_OneParam(FOnWebView2MonitoredEventNative, const FCBWebView2MonitoredEvent&)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCBWebView2MonitoredEvent, const FCBWebView2MonitoredEvent&, EventInfo);