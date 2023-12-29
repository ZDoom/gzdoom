#pragma once

#include <string>
#include <memory>
#include "canvas.h"
#include "rect.h"
#include "colorf.h"
#include "../window/window.h"

class Canvas;
class Timer;

enum class WidgetType
{
	Child,
	Window,
	Popup
};

class Widget : DisplayWindowHost
{
public:
	Widget(Widget* parent = nullptr, WidgetType type = WidgetType::Child);
	virtual ~Widget();

	void SetParent(Widget* parent);
	void MoveBefore(Widget* sibling);

	std::string GetWindowTitle() const;
	void SetWindowTitle(const std::string& text);

	// Icon GetWindowIcon() const;
	// void SetWindowIcon(const Icon& icon);

	// Widget content box
	Size GetSize() const;
	double GetWidth() const { return GetSize().width; }
	double GetHeight() const { return GetSize().height; }

	// Widget noncontent area
	void SetNoncontentSizes(double left, double top, double right, double bottom);

	// Widget frame box
	Rect GetFrameGeometry() const;
	void SetFrameGeometry(const Rect& geometry);
	void SetFrameGeometry(double x, double y, double width, double height) { SetFrameGeometry(Rect::xywh(x, y, width, height)); }

	void SetWindowBackground(const Colorf& color);
	void SetWindowBorderColor(const Colorf& color);
	void SetWindowCaptionColor(const Colorf& color);
	void SetWindowCaptionTextColor(const Colorf& color);

	void SetVisible(bool enable) { if (enable) Show(); else Hide(); }
	void Show();
	void ShowFullscreen();
	void ShowMaximized();
	void ShowMinimized();
	void ShowNormal();
	void Hide();

	void ActivateWindow();

	void Close();

	void Update();
	void Repaint();

	bool HasFocus();
	bool IsEnabled();
	bool IsVisible();

	void SetFocus();
	void SetEnabled(bool value);
	void SetDisabled(bool value) { SetEnabled(!value); }
	void SetHidden(bool value) { if (value) Hide(); else Show(); }

	void LockCursor();
	void UnlockCursor();
	void SetCursor(StandardCursor cursor);
	void CaptureMouse();
	void ReleaseMouseCapture();

	bool GetKeyState(EInputKey key);

	std::string GetClipboardText();
	void SetClipboardText(const std::string& text);

	Widget* Window();
	Canvas* GetCanvas();
	Widget* ChildAt(double x, double y) { return ChildAt(Point(x, y)); }
	Widget* ChildAt(const Point& pos);

	Widget* Parent() const { return ParentObj; }
	Widget* PrevSibling() const { return PrevSiblingObj; }
	Widget* NextSibling() const { return NextSiblingObj; }
	Widget* FirstChild() const { return FirstChildObj; }
	Widget* LastChild() const { return LastChildObj; }

	Point MapFrom(const Widget* parent, const Point& pos) const;
	Point MapFromGlobal(const Point& pos) const;
	Point MapFromParent(const Point& pos) const { return MapFrom(Parent(), pos); }

	Point MapTo(const Widget* parent, const Point& pos) const;
	Point MapToGlobal(const Point& pos) const;
	Point MapToParent(const Point& pos) const { return MapTo(Parent(), pos); }

	static Size GetScreenSize();

protected:
	virtual void OnPaintFrame(Canvas* canvas) { }
	virtual void OnPaint(Canvas* canvas) { }
	virtual void OnMouseMove(const Point& pos) { }
	virtual void OnMouseDown(const Point& pos, int key) { }
	virtual void OnMouseDoubleclick(const Point& pos, int key) { }
	virtual void OnMouseUp(const Point& pos, int key) { }
	virtual void OnMouseWheel(const Point& pos, EInputKey key) { }
	virtual void OnMouseLeave() { }
	virtual void OnRawMouseMove(int dx, int dy) { }
	virtual void OnKeyChar(std::string chars) { }
	virtual void OnKeyDown(EInputKey key) { }
	virtual void OnKeyUp(EInputKey key) { }
	virtual void OnGeometryChanged() { }
	virtual void OnClose() { delete this; }
	virtual void OnSetFocus() { }
	virtual void OnLostFocus() { }
	virtual void OnEnableChanged() { }

private:
	void DetachFromParent();

	void Paint(Canvas* canvas);

	// DisplayWindowHost
	void OnWindowPaint() override;
	void OnWindowMouseMove(const Point& pos) override;
	void OnWindowMouseDown(const Point& pos, EInputKey key) override;
	void OnWindowMouseDoubleclick(const Point& pos, EInputKey key) override;
	void OnWindowMouseUp(const Point& pos, EInputKey key) override;
	void OnWindowMouseWheel(const Point& pos, EInputKey key) override;
	void OnWindowRawMouseMove(int dx, int dy) override;
	void OnWindowKeyChar(std::string chars) override;
	void OnWindowKeyDown(EInputKey key) override;
	void OnWindowKeyUp(EInputKey key) override;
	void OnWindowGeometryChanged() override;
	void OnWindowClose() override;
	void OnWindowActivated() override;
	void OnWindowDeactivated() override;
	void OnWindowDpiScaleChanged() override;

	WidgetType Type = {};

	Widget* ParentObj = nullptr;
	Widget* PrevSiblingObj = nullptr;
	Widget* NextSiblingObj = nullptr;
	Widget* FirstChildObj = nullptr;
	Widget* LastChildObj = nullptr;

	Timer* FirstTimerObj = nullptr;

	Rect FrameGeometry = Rect::xywh(0.0, 0.0, 0.0, 0.0);
	Rect ContentGeometry = Rect::xywh(0.0, 0.0, 0.0, 0.0);

	Colorf WindowBackground = Colorf::fromRgba8(240, 240, 240);

	struct
	{
		double Left = 0.0;
		double Top = 0.0;
		double Right = 0.0;
		double Bottom = 0.0;
	} Noncontent;

	std::string WindowTitle;
	std::unique_ptr<DisplayWindow> DispWindow;
	std::unique_ptr<Canvas> DispCanvas;
	Widget* FocusWidget = nullptr;
	Widget* CaptureWidget = nullptr;
	Widget* HoverWidget = nullptr;

	Widget(const Widget&) = delete;
	Widget& operator=(const Widget&) = delete;

	friend class Timer;
};
