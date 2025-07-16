# CBWebView2_fixed  
一个基于webview的ue5浏览器插件,本项目作为个人学习使用,蓝图版本为UE5.5,源码fork自 https://github.com/SteveSantoso/CBWebView2  
## 主要更改内容：  
1.增加初始创建webview窗口事件(OnCreatedWebViewWidget)，用于代替控件中的Construct事件，使LoadURL无延迟加载  
2.新增控件蓝图WBP_TestWebViewFile,更好地展示暴露给蓝图的功能  
3.重新编写前端示例index.html，并将关键js代码提取到ruler.js  
4.更改页面穿透功能，配合前端使用类名标记需要输入响应的区域，并且增加了页面的缩放匹配功能  
