# CBWebView2 插件文档

## 插件概述

CBWebView2是一个基于Microsoft WebView2技术的Unreal Engine插件，允许在游戏或应用中嵌入Web内容。该插件提供了在UE界面中显示网页、与网页交互以及控制WebView2浏览器实例的功能。

插件名称：CBWebView2 fixed  
插件版本：2.1  
支持平台：Windows 64位 (Win64)  
UE5版本：5.2-5.7(蓝图基于5.5,其他版本请参考最小实现)    

## 源代码目录结构

```
Source/
├── CBWebView2/ # 主模块
│ ├── Public/ 
│ │ ├── CBWebView2.h # 模块接口定义 
│ │ ├── CBWebView2Widget.h # WebView2 Widget类定义 
│ │ └── SCBWebView2.h # Slate控件类定义 
│ └── CBWebView2.Build.cs # 模块构建配置 
├── WebView2/ # WebView2 第三方SDK封装 
│ ├── Microsoft.Web.WebView2.1.0.2895-prerelease/ 
│ │ └── include/ # WebView2官方头文件 
│ │ ├── WebView2.h 
│ │ ├── WebView2EnvironmentOptions.h 
│ │ ├── WebView2Experimental.h 
│ │ └── WebView2ExperimentalEnvironmentOptions.h 
│ └── WebView2.Build.cs # WebView2模块构建配置 
└── WebView2Utils/ # WebView2工具模块  
│ ├── Public/  
│ ├── IWebView2Window.h # 窗口接口定义 
│ ├── Stdafx.h # 预编译头文件 
│ ├── WebView2ComponentBase.cpp # 组件基类实现 
│ ├── WebView2ComponentBase.h # 组件基类定义 
│ ├── WebView2CompositionHost.h # 组合主机类定义 
│ ├── WebView2FileComponent.h # 文件组件类定义 
│ ├── WebView2Log.h # 日志系统定义 
│ ├── WebView2Manager.h # WebView管理器类定义 
│ ├── WebView2Settings.h # 设置类定义 
│ ├── WebView2Subsystem.h # 子系统类定义 
│ ├── WebView2Types.h # 类型定义 
│ ├── WebView2Utils.h # 工具模块接口定义 
│ ├── WebView2ViewComponent.h # 视图组件类定义 
│ └── WebView2Window.h # 窗口类定义 
└── WebView2Utils.Build.cs # 工具模块构建配置
```

## 模块说明

### 1. CBWebView2 主模块

这是插件的主要模块，提供了一个可在UE编辑器中使用的Widget组件。

**主要类：**

- [UCBWebView2Widget](.\Source\CBWebView2\Public\CBWebView2Widget.h#L10-L119)：核心Widget类，继承自UWidget，可以在UMG中使用
- [FCBWebView2Module](.\Source\CBWebView2\Public\CBWebView2.h#L8-L15)：模块接口实现类

**主要功能：**

- 在UI中嵌入WebView2浏览器控件
- 支持加载指定URL的网页内容
- 提供浏览器控制功能（前进、后退、刷新、停止）
- 支持执行JavaScript代码
- 支持与网页进行双向通信
- 提供丰富的事件回调（加载完成、消息接收等）

### 2. WebView2Utils 工具模块

提供WebView2的核心实现和管理功能。

**主要类：**

- [UWebView2Subsystem](.\Source\WebView2Utils\Public\WebView2Subsystem.h#L11-L24)：引擎子系统，管理WebView2实例
- [FWebView2Window](.\Source\WebView2Utils\Public\WebView2Window.h#L86-L285)：WebView2窗口实现类
- [UWebView2Settings](.\CBWebView2\Source\WebView2Utils\Public\WebView2Settings.h#L191-L223)：设置类，用于配置WebView2环境选项
- [FWebView2Manager](.\CBWebView2\Source\WebView2Utils\Public\WebView2Manager.h#L3-L35)：WebView2实例管理器

**主要功能：**

- WebView2环境初始化和管理
- 浏览器窗口创建和管理
- 系统级事件处理
- 下载管理

## 主要特性

1. **Widget集成**：提供可在UMG中使用的Widget组件，方便在UI中嵌入网页
2. **双向通信**：支持UE与网页之间的双向消息传递
3. **浏览器控制**：提供完整的浏览器控制功能，包括导航、刷新等
4. **JavaScript执行**：可以在UE中执行网页中的JavaScript代码
5. **事件系统**：提供丰富的事件回调，包括加载完成、消息接收等
6. **多窗口支持**：支持创建和管理多个WebView2实例
7. **配置选项**：提供灵活的配置选项，可以自定义WebView2环境

## 使用场景

- 在UE5项目中嵌入网页内容（如UI界面、帮助文档、商城等）
- 利用Web技术的优势，实现现代化的用户界面
- 通过丰富的事件回调系统，实现与Web服务的数据交互
- 可扩展性良好，如能够创建基于Web的编辑器工具

## 优化内容：

1.增加初始创建webview窗口事件(OnCreatedWebViewWidget)，用于代替控件中的Construct事件，使LoadURL无延迟加载  
2.新增控件蓝图WBP_TestWebViewFile,更好地展示暴露给蓝图的功能  
3.(26.4更新)更新更多的前端网页示例  
4.(26.4更新)添加网页下载相关事件回调,可对接UE5下载功能  
5.(26.4更新)使用js脚本(transparency_check.js)检测网页穿透,动态调整鼠标输入应用在网页还是UE5内部  
6.(26.4更新)增加网页输入全拦截变量(InputOnlyOnWeb)以适配web端三维场景  

## 注意事项

- 插件使用了Microsoft WebView2 SDK 1.0.2895预发布版本
- 目前仅支持Windows 64位平台
- 需要安装WebView2运行时环境(Edge浏览器包含此环境)

## 最小使用示例
![bluer0](/Resources/ck_000.png)  
![bluer1](/Resources/ck_001.png)  
