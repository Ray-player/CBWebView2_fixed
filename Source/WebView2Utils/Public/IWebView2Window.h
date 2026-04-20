#pragma once

#include "CoreMinimal.h"

/** 当前文档的加载状态。 */
enum class ECBWebView2DocumentState : uint8
{
	NoDocument,
	Loading,
	Completed,
	Error
};

/**
 * WebView 原生窗口抽象接口。
 *
 * 这一层用于隔离 Slate / UMG 和具体 WebView2 宿主实现。
 */
class IWebView2Window
{
public:
	virtual ~IWebView2Window() = default;

	/** 导航控制。 */
	virtual void LoadURL(const FString& InUrl) = 0;
	virtual void GoForward() const = 0;
	virtual void GoBack() const = 0;
	virtual void Reload() const = 0;
	virtual void Stop() const = 0;

	/** 布局与可见性控制。 */
	virtual void SetBounds(const RECT& InRect) = 0;
	virtual void SetBounds(const POINT& InOffset, const POINT& InSize) = 0;
	virtual RECT GetBounds() const = 0;

	virtual void SetVisible(ESlateVisibility InVisibility) = 0;
	virtual ESlateVisibility GetVisible() const = 0;

	/** 当前文档加载状态。 */
	virtual ECBWebView2DocumentState GetDocumentLoadingState() const = 0;

	/** 输入焦点与鼠标/键盘消息转发。 */
	virtual void MoveFocus(bool bFocus) = 0;
	virtual void SendMouseButton(const FVector2D& Point, bool bIsLeft, bool bIsDown) = 0;
	virtual void SendMouseMove(const FVector2D& Point) = 0;
	virtual void SendMouseWheel(const FVector2D& Point, float Delta) = 0;
	virtual void SendKeyboardMessage(uint32 Msg, uint64 WParam, int64 LParam) = 0;
};