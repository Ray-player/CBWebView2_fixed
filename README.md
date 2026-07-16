# CBWebView2 使用说明

## 1. 插件简介

CBWebView2 是一个基于 Microsoft Edge WebView2 Runtime 的 Unreal Engine 插件，提供以下能力：

- UMG 控件入口，可直接放入 Widget Blueprint
- Slate 控件入口，便于 C++ 自定义界面集成
- WinComp 组合宿主，支持多网页叠加显示
- 网页消息通信
- 透明区域鼠标穿透检测
- 下载事件监听
- 页面输出 PDF
- DevTools 调试窗口
- 统一监控事件链，便于日志和埋点

当前插件主要面向 Windows 平台。

## 2. 使用前提

使用前需要满足以下条件：

1. 工程运行平台为 Win64
2. 系统已安装可用的 WebView2 Runtime
3. 插件已启用

如果运行时缺少 WebView2 Runtime，浏览器环境无法创建，页面不会正常加载。

## 3. 插件启用

在 Unreal Editor 中启用插件后，重启编辑器。

插件包含两个主要模块：

- CBWebView2：UMG / Slate 外层入口
- WebView2Utils：原生宿主、设置、消息路由、WinComp 集成

## 4. 项目设置

插件设置位于：

- Project Settings
- Plugins
- CB WebView2

主要设置项说明如下。

### 4.1 Mode

WebView 默认运行模式。

- Windowed：窗口模式
- VisualWinComp：WinComp 可视树模式，当前推荐模式
- VisualDComp：DComp 可视树模式
- TargetDComp：DComp Target 模式

说明：Mode 属于 Environment / Controller 创建前生效的配置，修改后通常需要重启编辑器才能完全生效。

### 4.2 Environment

环境级配置，创建 WebView2 Environment 时生效。

- Language：浏览器语言，例如 zh-CN
- bEnableSingleSignOn：启用单点登录
- bExclusiveUserDataFolderAccess：独占用户数据目录
- bCustomCrashReporting：启用自定义崩溃上报
- bTrackingPrevention：启用跟踪防护
- bEnableBrowserExtensions：允许浏览器扩展
- AdditionalBrowserArguments：额外浏览器启动参数数组

AdditionalBrowserArguments 现在是数组，每个元素填写一个完整参数，例如：

```ini
--disable-web-security
--allow-running-insecure-content
```

### 4.3 Controller

控制器级配置，创建 Controller 时生效。

- ProfileName：浏览器配置名称
- bInPrivate：是否启用隐私模式
- DownloadPath：默认下载路径
- ScriptLocale：脚本区域设置
- bUseOSRegion：使用系统区域设置
- bAllowHostInputProcessing：宿主先处理输入，再交给 WebView

### 4.4 Features

运行期开关，可控制常用浏览器功能：

- 默认右键菜单
- 脚本对话框
- DevTools
- HostObject
- 内置错误页
- 脚本执行
- 状态栏
- WebMessage
- 缩放控制
- 静音

## 5. UMG 中使用

### 5.1 添加控件

在 Widget Blueprint 中添加 CBWebView2Widget。

常用属性：

- InitialUrl：初始页面地址
- BackgroundColor：背景色，Alpha 为 0 可做透明背景
- bShowAddressBar：显示地址栏
- bShowControls：显示返回/前进/刷新按钮
- bShowTouchArea：显示调试触摸区域
- bEnableTransparencyHitTest：启用透明区域命中检测

### 5.2 蓝图常用调用

可直接在蓝图中调用以下函数：

- LoadURL
- ExecuteScript
- GoBack
- GoForward
- Reload
- StopLoading
- OpenDevToolsWindow
- PrintToPdf
- SetBackgroundColorEx
- SetWebViewVisibility

### 5.3 蓝图事件

控件提供以下事件：

- OnMessageReceived：收到网页消息
- OnLoadStarted：开始导航
- OnLoadCompleted：导航结束
- OnNewWindowRequested：网页请求新窗口
- OnDownloadStarting：下载开始
- OnDownloadUpdated：下载进度更新
- OnPrintToPdfCompleted：PDF 导出完成
- OnMonitoredEvent：统一监控事件

## 6. C++ 中使用

### 6.1 UMG 方式

如果你在 UUserWidget 中持有 UCBWebView2Widget，可直接调用它的公开函数。

示例：

```cpp
WebViewWidget->LoadURL(TEXT("https://www.bing.com"));

WebViewWidget->ExecuteScript(
    TEXT("document.title"),
    FOnCBWebView2ScriptExecuted::CreateLambda([](const FString& Result)
    {
        UE_LOG(LogTemp, Log, TEXT("Script Result: %s"), *Result);
    }));
```

### 6.2 Slate 方式

如果你使用纯 Slate，可直接创建 SCBWebView2。

```cpp
SAssignNew(MyWebView, SCBWebView2)
    .InitialUrl(TEXT("https://www.bing.com"))
    .bShowAddressBar(false)
    .bShowControls(false)
    .bEnableTransparencyHitTest(true)
    .ParentWindow(ParentWindow);
```

说明：ParentWindow 必须是有效的顶层窗口，否则无法创建底层原生宿主。

## 7. 网页消息通信

网页可通过 WebView2 的 postMessage 向宿主发送消息，插件会通过以下路径向上转发：

- 原生 FWebView2Window
- UWebView2Subsystem
- SCBWebView2
- UCBWebView2Widget

业务侧最常用的是监听以下入口：

- UCBWebView2Widget::OnMessageReceived
- UWebView2Subsystem::OnWebMessageReceived

## 8. 透明区域穿透

### 8.1 基本说明

当 bEnableTransparencyHitTest 为 true 时，插件会在页面创建时自动注入 transparency_check.js。

脚本文件位置：

- Extras/transparency_check.js

运行时优先从 Extras 加载，旧的 Content/Web/transparency_check.js 仅保留兼容回退。

### 8.2 工作方式

注入脚本会根据当前鼠标位置判断网页是否命中可交互 DOM，然后发送类似下面的消息：

```text
IsClickable:1
IsClickable:0
```

插件收到后会：

1. 更新当前 WebView 的命中状态
2. 透明区域时把控件切换为不可命中
3. 允许下层 UMG 或下层 WebView 继续接收点击

### 8.3 多个 UMG WebView 叠加时的建议

如果你有多个独立 UMG WebView，且通过 ZOrder 叠加：

1. 需要给每个需要透明穿透的网页开启 bEnableTransparencyHitTest
2. 上层网页透明时，底层网页才能继续参与命中竞争
3. 键盘输入依赖真实焦点，不要把透明状态当成焦点状态来理解

## 9. 下载功能

插件已接入下载事件链。

可获取信息包括：

- 下载地址
- MimeType
- 本地文件路径
- 已接收字节数
- 总字节数
- 当前状态

推荐监听：

- OnDownloadStarting
- OnDownloadUpdated

如果在设置中填写了 DownloadPath，插件会尝试将其设为默认下载目录。

## 10. 打印为 PDF

可通过以下接口导出当前页面为 PDF：

- UCBWebView2Widget::PrintToPdf
- SCBWebView2::PrintToPdf

参数：

- OutputPath：输出路径
- bLandscape：是否横向打印

完成后会回调：

- OnPrintToPdfCompleted

说明：该能力依赖当前 WebView2 Runtime 支持对应接口。

## 11. DevTools 调试

可通过以下接口打开开发者工具：

- UCBWebView2Widget::OpenDevToolsWindow
- SCBWebView2::OpenDevToolsWindow

适合排查：

- 页面脚本错误
- DOM 结构
- 网络请求
- postMessage 消息是否正常发送

## 12. 统一监控事件

插件内部整理了一套统一监控事件结构 FCBWebView2MonitoredEvent，可覆盖：

- NavigationStarting
- NavigationCompleted
- SourceChanged
- DocumentTitleChanged
- HistoryChanged
- WebMessageReceived
- NewWindowRequested
- DownloadStarting
- DownloadUpdated
- PermissionRequested

推荐场景：

- 做运行日志
- 做埋点统计
- 做权限或下载审计
- 在调试阶段集中观察所有网页行为

## 13. 打包与资源说明

透明命中脚本已通过构建规则加入运行时依赖，打包后会随插件一同分发。

关键资源：

- Source/WebView2/Dependency/WebView2Loader.dll
- Extras/transparency_check.js

## 14. 常见问题

### 14.1 页面不显示

优先检查：

1. 是否运行在 Win64
2. 是否安装 WebView2 Runtime
3. 是否拿到了有效的顶层窗口句柄

### 14.2 透明区域不能点穿到下层

优先检查：

1. 对应控件是否开启 bEnableTransparencyHitTest
2. 页面中是否真的存在透明区域
3. 是否能收到 IsClickable:0/1 网页消息

### 14.3 下层网页可点但不能输入

优先检查：

1. 顶层点击后是否成功转移到目标 WebView
2. 当前 WebView 是否拿到真实键盘焦点
3. 是否有业务层逻辑错误地覆盖焦点状态

## 15. 配置文件

插件默认配置文件位于：

- Config/DefaultCBWebView2.ini

项目也可以通过常规 UE 配置系统覆盖这些设置。
