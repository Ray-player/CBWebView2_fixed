#include "CBWebView2.h"

DEFINE_LOG_CATEGORY(LogCBWebView2);

void FCBWebView2Module::StartupModule()
{
	// 当前模块本身不持有长生命周期状态。
	// 目前模块启动阶段不主动创建任何对象。
	// 真正的 WebView 生命周期按需由 Widget 或 Subsystem 拉起。
}

void FCBWebView2Module::ShutdownModule()
{
	// 保持空实现，避免和子模块的析构次序产生耦合。
	// 具体资源释放由各自模块与对象生命周期负责。
}

IMPLEMENT_MODULE(FCBWebView2Module, CBWebView2)