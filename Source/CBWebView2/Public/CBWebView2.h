#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCBWebView2, Log, All);

/**
	* 插件的 UE 外层模块。
 *
 * 这一层只负责模块生命周期和对外 UMG/Slate 入口，
 * 真正的 WebView2 原生集成逻辑放在 WebView2Utils 模块中。
 */
class CBWEBVIEW2_API FCBWebView2Module : public IModuleInterface
{
public:
	/** 模块启动时预留入口，当前不主动创建任何实例。 */
	virtual void StartupModule() override;
	/** 模块关闭时预留入口，具体释放由对象生命周期完成。 */
	virtual void ShutdownModule() override;
};