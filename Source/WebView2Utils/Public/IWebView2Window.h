#pragma once

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"


#include <dcomp.h>



#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"


enum class EWebView2DocumentState
{
	Completed,
	Error,
	Loading,
	NoDocument
};

class IWebView2Window
{
public:
	virtual ~IWebView2Window() = default;
	/**加载网页*/ 
	virtual void LoadURL(const FString URL) = 0;

	/*** 如果浏览器可以向前导航则返回 true。*/
	//virtual bool CanGoForward() const = 0;
	/**前进*/
	virtual void GoForward() const = 0;
	/**后退*/
	virtual void GoBack() const = 0;

	/** 如果浏览器可以向后导航则返回 true*/
	//virtual bool CanGoBack() const = 0;
	
	/**重新加载*/
	virtual void ReLoad() const = 0;
	/**停止加载*/
	virtual void Stop() const = 0;
	/**设置webview大小*/
	virtual void SetBounds(RECT Rect) = 0;
	/**设置webview大小*/
	virtual void SetBounds(POINT Offset, POINT Size) = 0;
	
	/**获取webview大小*/
	virtual RECT GetBounds() const = 0;
	/**设置可见性*/
	virtual void SetVisible(ESlateVisibility InVisibility) = 0;
	/**获取可见性*/
	virtual ESlateVisibility  GetVisible() const = 0;
	/**禁用鼠标*/

	/** 获取当前文档的加载状态。 */
	virtual EWebView2DocumentState GetDocumentLoadingState() const = 0;
	
};
