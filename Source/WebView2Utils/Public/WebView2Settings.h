#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "WebView2Settings.generated.h"

struct FPropertyChangedEvent;

/**
 * WebView2 的宿主模式。
 *
 * 第一阶段优先保证 Windowed 与 VisualWinComp 可用；
 * DComp 相关模式会保留接口，但允许逐步完善。
 */
UENUM(BlueprintType)
enum class ECBWebView2Mode : uint8
{
	Windowed UMETA(DisplayName = "窗口模式"),
	VisualWinComp UMETA(DisplayName = "WinComp 可视树模式"),
	VisualDComp UMETA(DisplayName = "DComp 可视树模式"),
	TargetDComp UMETA(DisplayName = "DComp Target 模式")
};

/** WebView2 环境级设置。 */
USTRUCT(BlueprintType)
struct WEBVIEW2UTILS_API FCBWebView2EnvironmentOptions
{
	GENERATED_BODY()

	/** WebView 使用的语言，例如 zh-CN。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "环境")
	FString Language = TEXT("zh-CN");

	/** 是否启用 AAD / MSA 单点登录。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "环境")
	bool bEnableSingleSignOn = false;

	/** 是否要求用户数据目录的独占访问。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "环境")
	bool bExclusiveUserDataFolderAccess = false;

	/** 是否启用自定义崩溃上报。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "环境")
	bool bCustomCrashReporting = false;

	/** 是否启用跟踪防护。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "环境")
	bool bTrackingPrevention = true;

	/** 是否允许浏览器扩展。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "环境")
	bool bEnableBrowserExtensions = true;

	/**
	 * 额外浏览器启动参数。
	 *
	 * 每个数组元素表示一个完整参数，例如：
	 * --allow-running-insecure-content
	 * --disable-web-security
	 *
	 * 这些参数在 WebView2 Environment 创建时一次性生效；
	 * 修改后需要重启编辑器，或至少销毁并重建所有 WebView 实例。
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "环境", meta = (ConfigRestartRequired = true, TitleProperty = "{Item}"))
	TArray<FString> AdditionalBrowserArguments;
};

/** 控制器创建时使用的选项。 */
USTRUCT(BlueprintType)
struct WEBVIEW2UTILS_API FCBWebView2ControllerOptions
{
	GENERATED_BODY()

	/** Profile 名称，可为空。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "控制器")
	FString ProfileName;

	/** 是否启用 InPrivate 模式。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "控制器")
	bool bInPrivate = false;

	/** 下载目录，可为空。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "控制器")
	FString DownloadPath;

	/** 脚本区域设置，可为空。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "控制器")
	FString ScriptLocale;

	/** 是否直接使用操作系统区域设置。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "控制器")
	bool bUseOSRegion = false;

	/** 是否允许宿主先处理输入后再交给 WebView。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "控制器")
	bool bAllowHostInputProcessing = true;
};

/** 常用 WebView 功能开关。 */
USTRUCT(BlueprintType)
struct WEBVIEW2UTILS_API FCBWebView2FeatureSettings
{
	GENERATED_BODY()

	/** 是否启用网页默认右键菜单。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "功能")
	bool bEnableContextMenus = false;

	/** 是否允许网页弹出 alert / confirm / prompt 等脚本对话框。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "功能")
	bool bEnableScriptDialogs = true;

	/** 是否允许打开开发者工具。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "功能")
	bool bEnableDevTools = true;

	/** 是否允许宿主向网页注入 Host Object。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "功能")
	bool bAllowHostObjects = true;

	/** 是否启用 WebView2 内置错误页。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "功能")
	bool bEnableBuiltInErrorPage = true;

	/** 是否允许页面执行 JavaScript。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "功能")
	bool bEnableScript = true;

	/** 是否显示浏览器状态栏。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "功能")
	bool bEnableStatusBar = true;

	/** 是否启用网页与宿主之间的 WebMessage 通信。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "功能")
	bool bEnableWebMessage = true;

	/** 是否允许用户缩放网页内容。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "功能")
	bool bEnableZoomControl = true;

	/** 是否将当前 WebView 默认静音。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "功能")
	bool bMuted = false;
};

/**
 * 插件项目设置入口。
 *
 * 所有公开配置都统一放在这里，便于项目侧通过编辑器面板管理。
 */
UCLASS(Config = CBWebView2, DefaultConfig, meta = (DisplayName = "CB WebView2"))
class WEBVIEW2UTILS_API UWebView2Settings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWebView2Settings();

	virtual FName GetCategoryName() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintPure, Category = "CBWebView2")
	static const UWebView2Settings* Get();

	/** WebView 默认运行模式。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "基础", meta = (ConfigRestartRequired = true))
	ECBWebView2Mode Mode = ECBWebView2Mode::VisualWinComp;

	/** 环境级选项。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "基础", meta = (ConfigRestartRequired = true))
	FCBWebView2EnvironmentOptions Environment;

	/** 控制器级选项。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "基础", meta = (ConfigRestartRequired = true))
	FCBWebView2ControllerOptions Controller;

	/** 功能开关。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "基础")
	FCBWebView2FeatureSettings Features;

	/** 默认背景色。A 为 0 时表示透明背景。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "外观")
	FColor DefaultBackgroundColor = FColor(255, 255, 255, 255);

private:
#if WITH_EDITOR
	bool DoesChangeRequireEditorRestart(const FPropertyChangedEvent& PropertyChangedEvent) const;
#endif
};