#pragma once
#include "WebView2ComponentBase.h"

class FWebView2FileComponent :public FWebView2ComponentBase
{
public:
	void SaveScreenshot();
	void PritToPdf(bool enableLandscape);
	bool IsPrintToPdfInProgress();
};
