#include "WebView2Utils.h"

#include "WebView2Log.h"

#define LOCTEXT_NAMESPACE "FWebView2UtilsModule"


DEFINE_LOG_CATEGORY(LogWebView2);
void FWebView2UtilsModule::StartupModule()
{
	FString Path=FPaths::Combine(FPaths::ProjectDir(),TEXT("Binaries"),TEXT("Win64"),TEXT("WebView2Loader.dll"));
	void* Handle = FPlatformProcess::GetDllHandle(*Path);
	if (!Handle)
	{
		int32 ErrorNum = FPlatformMisc::GetLastError();
		TCHAR ErrorMsg[1024];
		FPlatformMisc::GetSystemErrorMessage(ErrorMsg, 1024, ErrorNum);
		UE_LOG(LogWebView2, Error, TEXT("Failed to get CEF3 DLL handle for %s: %s (%d)"), *Path, ErrorMsg, ErrorNum);
	}
}

void FWebView2UtilsModule::ShutdownModule()
{
	if(DllHandle)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
	}
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FWebView2UtilsModule, WebView2Utils)