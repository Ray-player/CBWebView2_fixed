// Copyright 2025-Present Xiao Lan fei. All Rights Reserved.

#include "CBWebView2.h"

#define LOCTEXT_NAMESPACE "FCBWebView2Module"

DEFINE_LOG_CATEGORY(LogCBWebView2);
void FCBWebView2Module::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FCBWebView2Module::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCBWebView2Module, CBWebView2)