#pragma once

#include <string>
#include <memory>
#include <variant>
#include <unordered_map>
#include <unordered_set>
#include "canvas.h"
#include "rect.h"
#include "colorf.h"
#include "../window/window.h"

class Canvas;
class Timer;
class Dropdown;

enum class WidgetType
{
	Child,
	Window,
	Popup
};

enum class WidgetEvent
{
	VisibilityChange,

	// TODO: add more events
};

class Widget : DisplayWindowHost
{
public:
	Widget(Widget* parent = nullptr, WidgetType type = WidgetType::Child, RenderAPI api = RenderAPI::Unspecified);
	virtual ~Widget();

	void SetParent(Widget* parent);
	void MoveBefore(Widget* sibling);

	std::string GetWindowTitle() const;
	void SetWindowTitle(const std::string& text);

	std::vector<std::shared_ptr<Image>> GetWindowIcon() const;
	void SetWindowIcon(const std::vector<std::shared_ptr<Image>>& images);

	// Widget content box
	Size GetSize() const;
	double GetWidth() const { return GetSize().width; }
	double GetHeight() const { return GetSize().height; }

	// Widget noncontent area
	void SetNoncontentSizes(double left, double top, double right, double bottom);
	double GetNoncontentLeft() const { return GridFitSize(GetStyleDouble("noncontent-left")); }
	double GetNoncontentTop() const { return GridFitSize(GetStyleDouble("noncontent-top")); }
	double GetNoncontentRight() const { return GridFitSize(GetStyleDouble("noncontent-right")); }
	double GetNoncontentBottom() const { return GridFitSize(GetStyleDouble("noncontent-bottom")); }

	// Get the DPI scale factor for the window the widget is located on
	double GetDpiScale() const;

	// Align point to the nearest physical screen pixel
	double GridFitPoint(double p) const;
	Point GridFitPoint(double x, double y) const { return GridFitPoint(Point(x, y)); }
	Point GridFitPoint(Point p) const { return Point(GridFitPoint(p.x), GridFitPoint(p.y)); }

	// Convert size to exactly covering physical screen pixels
	double GridFitSize(double s) const;
	Size GridFitSize(double w, double h) const { return GridFitSize(Size(w, h)); }
	Size GridFitSize(Size s) const { return Size(GridFitSize(s.width), GridFitSize(s.height)); }

	// Widget frame box
	Rect GetFrameGeometry() const;
	void SetFrameGeometry(const Rect& geometry);
	void SetFrameGeometry(double x, double y, double width, double height) { SetFrameGeometry(Rect::xywh(x, y, width, height)); }

	// Style properties
	void SetStyleClass(const std::string& styleClass);
	const std::string& GetStyleClass() const { return StyleClass; }
	void SetStyleState(const std::string& state);
	const std::string& GetStyleState() const { return StyleState; }
	void SetStyleBool(const std::string& propertyName, bool value);
	void SetStyleInt(const std::string& propertyName, int value);
	void SetStyleDouble(const std::string& propertyName, double value);
	void SetStyleString(const std::string& propertyName, const std::string& value);
	void SetStyleColor(const std::string& propertyName, const Colorf& value);
	bool GetStyleBool(const std::string& propertyName) const;
	int GetStyleInt(const std::string& propertyName) const;
	double GetStyleDouble(const std::string& propertyName) const;
	std::string GetStyleString(const std::string& propertyName) const;
	Colorf GetStyleColor(const std::string& propertyName) const;

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

	void Subscribe(Widget* subscriber);
	void Unsubscribe(Widget* subscriber);

	void ActivateWindow();

	void Close();

	void Update();
	void Repaint();

	bool HasFocus();
	bool IsEnabled();
	bool IsVisible();
	bool IsHidden();
	bool IsFullscreen();

	void SetFocus();
	void SetEnabled(bool value);
	void SetDisabled(bool value) { SetEnabled(!value); }
	void SetHidden(bool value) { if (value) Hide(); else Show(); }

	void LockCursor();
	void UnlockCursor();
	void SetCursor(StandardCursor cursor);

	void SetPointerCapture();
	void ReleasePointerCapture();

	void SetModalCapture();
	void ReleaseModalCapture();

	bool GetKeyState(InputKey key);

	std::string GetClipboardText();
	void SetClipboardText(const std::string& text);

	Widget* Window() const;
	Canvas* GetCanvas() const;
	Widget* ChildAt(double x, double y) { return ChildAt(Point(x, y)); }
	Widget* ChildAt(const Point& pos);

	bool IsParent(const Widget* w) const;
	bool IsChild(const Widget* w) const;

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

	void SetCanvas(std::unique_ptr<Canvas> canvas);
	void* GetNativeHandle();
	int GetNativePixelWidth();
	int GetNativePixelHeight();

	// Vulkan support:
	std::vector<std::string> GetVulkanInstanceExtensions() { return Window()->DispWindow->GetVulkanInstanceExtensions(); }
	VkSurfaceKHR CreateVulkanSurface(VkInstance instance) { return Window()->DispWindow->CreateVulkanSurface(instance); }

protected:
	virtual void OnPaintFrame(Canvas* canvas);
	virtual void OnPaint(Canvas* canvas) { }
	virtual bool OnMouseDown(const Point& pos, InputKey key) { return false; }
	virtual bool OnMouseDoubleclick(const Point& pos, InputKey key) { return false; }
	virtual bool OnMouseUp(const Point& pos, InputKey key) { return false; }
	virtual bool OnMouseWheel(const Point& pos, InputKey key) { return false; }
	virtual void OnMouseMove(const Point& pos) { }
	virtual void OnMouseLeave() { }
	virtual void OnRawMouseMove(int dx, int dy) { }
	virtual void OnKeyChar(std::string chars) { }
	virtual void OnKeyDown(InputKey key) { }
	virtual void OnKeyUp(InputKey key) { }
	virtual void OnGeometryChanged() { }
	virtual void OnClose() { delete this; }
	virtual void OnSetFocus() { }
	virtual void OnLostFocus() { }
	virtual void OnEnableChanged() { }

	virtual void Notify(Widget* source, const WidgetEvent type) { };

private:
	void DetachFromParent();

	void Paint(Canvas* canvas);

	// DisplayWindowHost
	void OnWindowPaint() override;
	void OnWindowMouseMove(const Point& pos) override;
	void OnWindowMouseLeave() override;
	void OnWindowMouseDown(const Point& pos, InputKey key) override;
	void OnWindowMouseDoubleclick(const Point& pos, InputKey key) override;
	void OnWindowMouseUp(const Point& pos, InputKey key) override;
	void OnWindowMouseWheel(const Point& pos, InputKey key) override;
	void OnWindowRawMouseMove(int dx, int dy) override;
	void OnWindowKeyChar(std::string chars) override;
	void OnWindowKeyDown(InputKey key) override;
	void OnWindowKeyUp(InputKey key) override;
	void OnWindowGeometryChanged() override;
	void OnWindowClose() override;
	void OnWindowActivated() override;
	void OnWindowDeactivated() override;
	void OnWindowDpiScaleChanged() override;

	void NotifySubscribers(const WidgetEvent type);

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

	std::string WindowTitle;
	std::vector<std::shared_ptr<Image>> WindowIcon;
	std::unique_ptr<DisplayWindow> DispWindow;
	std::unique_ptr<Canvas> DispCanvas;
	Widget* FocusWidget = nullptr;
	Widget* CaptureWidget = nullptr;
	Widget* HoverWidget = nullptr;
	bool HiddenFlag = false;

	StandardCursor CurrentCursor = StandardCursor::arrow;

	std::string StyleClass = "widget";
	std::string StyleState;
	typedef std::variant<bool, int, double, std::string, Colorf> PropertyVariant;
	std::unordered_map<std::string, PropertyVariant> StyleProperties;

	Widget(const Widget&) = delete;
	Widget& operator=(const Widget&) = delete;

	std::unordered_set<Widget*> Subscribers;
	std::unordered_set<Widget*> Subscriptions;

	friend class Timer;
	friend class OpenFileDialog;
	friend class OpenFolderDialog;
	friend class SaveFileDialog;
	friend class Dropdown;
};
