// Copyright 2025-Present Xiao Lan fei. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WebView2Types.h"
#include "Engine/DeveloperSettings.h"
#include "WebView2Settings.generated.h"


UENUM(BlueprintType)
enum class EWebView2Mode:uint8
{
	WINDOWED,
	VISUAL_DCOMP,
	TARGET_DCOMP,
	VISUAL_WINCOMP
};
USTRUCT(Blueprintable)
struct WEBVIEW2UTILS_API FWebView2EnvironmentOptions
{
	GENERATED_BODY()
	/**
	 *输入用于 WebView 的语言，或留空以恢复默认值。
	 */
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2|Options")
	FString Language;

	/**
	 *WebView 请求对用户数据文件夹的独占访问权限。
	 *若true 将为所有 webview 关闭后创建的新 WebView 请求对用户数据文件夹的独占访问权限。
	 *若false将不会为所有 webview 关闭后创建的新 WebView 请求对用户数据文件夹的独占访问权限。
	 */
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2|Options")
	bool bExclusiveUserDataFolderAccess;

	/**
	 * 是否用于在 WebView 中使用 Azure Active Directory （AAD） 和个人 Microsoft 帐户 （MSA） 资源启用单一登录
	 * ture为使用aad为单一登陆
	 */
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2|Options")
	bool bAADSSOEnabled;

	/**
	 * Windows 是否会将崩溃数据发送到 Microsoft 终端节点
	 * ture则发送
	 */
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2|Options")
	bool bCustomCrashReportingEnabled;

	/**
	 * 用于在 WebView2 中启用/禁用跟踪预防功能
	 * 如果应用仅在已知安全的内容中呈现 WebView2，则可以将此属性设置为 false 以禁用跟踪防护功能。
	 * 在创建环境时禁用此功能还可以通过跳过相关代码来提高运行时性能
	 */
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2|Options")
	bool bTrackingPreventionEnabled;
	
	FWebView2EnvironmentOptions()
		:Language(TEXT("zh-cn"))
		,bExclusiveUserDataFolderAccess(false)
		,bAADSSOEnabled(false)
		,bCustomCrashReportingEnabled(false)
		,bTrackingPreventionEnabled(false)
	{
	}
};



/***
 *
 * https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2controlleroptions?view=webview2-1.0.2792.45
 */
USTRUCT(BlueprintType)
struct WEBVIEW2UTILS_API FWebViewCreateOption
{
	GENERATED_BODY()

	/** 用于指定配置文件名称，该名称只允许包含以下 ASCII 字符
	* 它的最大长度为 64 个字符，不包括 null 终止符。它是不区分大小写的 ASCII。
	* 字母字符：A-Z 和 A-Z
	* 数字字符：0-9
	* 和 '#'， '@'， '$'， '（'， '）'， '+'， '-'， '_'， '~'， '.'， ' ' （空格）。
	* 注意：文本不得以句点 '.' 或 ' ' （空格） 结尾。而且，尽管允许使用大写字母，但它们会被视为小写字母，因为配置文件名称将映射到磁盘上的实际配置文件目录路径，并且 Windows 文件系统以不区分大小写的方式处理路径名称。
	 */
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	FString  Profile;

	/**用于启用/禁用 InPrivate 模式。*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bInPrivate;

	/**下载路径*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	FString DownloadPath;

	/**默认区域设置*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	FString ScriptLocale;

	/**默认区域设置是否使用操作系统区域*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bUseOSRegion;
	
	FWebViewCreateOption()
		:bInPrivate(false)
		,bUseOSRegion(false)
	{
	}

	FWebViewCreateOption(const FWebViewCreateOption& Option)
	{
		Profile = Option.Profile;
		bInPrivate = Option.bInPrivate;
		DownloadPath = Option.DownloadPath;
		ScriptLocale = Option.ScriptLocale;
		bUseOSRegion = Option.bUseOSRegion;
	}
};

/**
 * https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2settings?view=webview2-1.0.2849.39
 * 定义启用、禁用或修改 WebView 功能的属性
 */
USTRUCT(BlueprintType)
struct WEBVIEW2UTILS_API FCoreWebView2Settings
{
	GENERATED_BODY()

	/**用于防止在 WebView 中向用户显示默认上下文菜单(右键菜单)*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bDefaultContextMenusEnabled;

	/**如果设置为 true，则 WebView2 不会呈现默认的 JavaScript 对话框 （特别是 JavaScript 警报、确认、提示函数和事件） 显示的对话框。
	 *相反，如果使用 设置事件处理程序，则 WebView 会发送一个事件，其中包含对话框的所有信息，并允许主机应用程序显示自定义 UI
	 */
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bDefaultScriptDialogsEnabled;

	/**控制用户是否能够使用上下文菜单或键盘快捷方式打开 DevTools 窗口*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bDevToolsEnabled;
	
	/**用于控制是否可以从 WebView 中的页面访问主机对象*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bHostObjectsAllowed;

	/**用于禁用导航失败和渲染过程失败的内置错误页面,禁用后，当发生相关错误时，将显示一个空白页面*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bBuiltInErrorPageEnabled;

	/**控制是否在 WebView 的所有未来导航中启用运行 JavaScript*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bScriptEnabled;

	/**控制是否显示状态栏
	 * 状态栏通常显示在 WebView 的左下角，当用户将鼠标悬停在链接上时，
	 * 会显示链接的 URI 等内容以及其他信息。默认值为 .状态栏 UI 可由 Web 内容更改，不应被视为安全
	 */
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bStatusBarEnabled;

	/**如果设置为true ，则允许使用 、 和 message 事件从 主机到 WebView 的顶级 HTML 文档的通信*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bWebMessageEnabled;

	/**如果设置为true ，则允许使用 、 和 message 事件从 主机到 WebView 的顶级 HTML 文档的通信*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	bool bZoomControlEnabled;
	
	FCoreWebView2Settings()
		:bDefaultContextMenusEnabled(false)
		,bDefaultScriptDialogsEnabled(true)
		,bDevToolsEnabled(true)
		,bHostObjectsAllowed(true)
		,bBuiltInErrorPageEnabled(true)
		,bScriptEnabled(true)
		,bStatusBarEnabled(true)
		,bWebMessageEnabled(true)
		,bZoomControlEnabled(true)
	{
		
	}	
		
	
		
	
};

UCLASS(Config = "WebView2",DefaultConfig)
class WEBVIEW2UTILS_API UWebView2Settings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWebView2Settings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	// UDeveloperSettings Interface
	virtual FName GetCategoryName() const override;

public:
	UFUNCTION(BlueprintPure, Category = "WebView2",DisplayName = "GetWebView2Settings")
	static UWebView2Settings* Get();

public:
	/**不同显示模式，修改后需重启
	* todo VISUAL_DCOMP,TARGET_DCOMP,这两个没做完后续开发
	 */
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2")
	EWebView2Mode WebView2Mode;

	/**是否静音*/
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2|Audio")
	bool bMuted;
	
	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2|Options")
	FWebView2EnvironmentOptions EnvironmentOptions;

	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2|Options")
	FWebViewCreateOption WebViewCreateOption;

	UPROPERTY(BlueprintReadOnly, Config,EditAnywhere, Category = "WebView2|Options")
	FCoreWebView2Settings CoreWebView2Settings;
};
