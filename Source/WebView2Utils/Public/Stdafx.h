#pragma once

// 该头文件统一收口 WebView2 / WinRT / WRL 相关第三方包含，
// 以减少各源码文件重复处理平台宏与告警抑制。
#pragma warning(disable : 4668 4005 4191 4530)

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#include <SDKDDKVer.h>

THIRD_PARTY_INCLUDES_START
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>

#include <wrl.h>

// WebView2 官方 SDK 头文件。
#include "WebView2.h"
#include "WebView2EnvironmentOptions.h"
#include "WebView2Experimental.h"
#include "WebView2ExperimentalEnvironmentOptions.h"
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"