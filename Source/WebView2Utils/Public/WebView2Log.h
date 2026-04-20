#pragma once

#include "CoreMinimal.h"

// WebView2Utils 统一日志分类。
// 运行期的原生集成、消息路由、下载和权限事件都应优先打到这里。
DECLARE_LOG_CATEGORY_EXTERN(LogWebView2Utils, Log, All);