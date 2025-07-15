#pragma once

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#include <windows.h>

#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"


class FWebView2ComponentBase
{
public:
	
	virtual bool HandleWindowMessage(
		HWND Hwnd,
		UINT Message,
		WPARAM Wparam,
		LPARAM Lparam,
		LRESULT* Result)
	{
		return false;
	}
	virtual ~FWebView2ComponentBase(){}
};
