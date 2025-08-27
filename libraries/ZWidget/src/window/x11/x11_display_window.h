#pragma once

#include <zwidget/window/window.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XInput2.h>
#include <map>

class X11DisplayWindow : public DisplayWindow
{
public:
	X11DisplayWindow(DisplayWindowHost* windowHost, bool popupWindow, X11DisplayWindow* owner, RenderAPI renderAPI);
	~X11DisplayWindow();

	void SetWindowTitle(const std::string& text) override;
	void SetWindowFrame(const Rect& box) override;
	void SetClientFrame(const Rect& box) override;
	void Show() override;
	void ShowFullscreen() override;
	void ShowMaximized() override;
	void ShowMinimized() override;
	void ShowNormal() override;
	bool IsWindowFullscreen() override;
	void Hide() override;
	void Activate() override;
	void ShowCursor(bool enable) override;
	void LockCursor() override;
	void UnlockCursor() override;
	void CaptureMouse() override;
	void ReleaseMouseCapture() override;
	void Update() override;
	bool GetKeyState(InputKey key) override;

	void SetCursor(StandardCursor cursor) override;

	Rect GetWindowFrame() const override;
	Size GetClientSize() const override;
	int GetPixelWidth() const override;
	int GetPixelHeight() const override;
	double GetDpiScale() const override;

	void PresentBitmap(int width, int height, const uint32_t* pixels) override;

	void SetBorderColor(uint32_t bgra8) override;
	void SetCaptionColor(uint32_t bgra8) override;
	void SetCaptionTextColor(uint32_t bgra8) override;

	std::string GetClipboardText() override;
	void SetClipboardText(const std::string& text) override;

	Point MapFromGlobal(const Point& pos) const override;
	Point MapToGlobal(const Point& pos) const override;

	void* GetNativeHandle() override;

	std::vector<std::string> GetVulkanInstanceExtensions() override;
	VkSurfaceKHR CreateVulkanSurface(VkInstance instance) override;

	static void ProcessEvents();
	static void RunLoop();
	static void ExitLoop();
	static Size GetScreenSize();
	static void* StartTimer(int timeoutMilliseconds, std::function<void()> onTimer);
	static void StopTimer(void* timerID);

private:
	void UpdateCursor();

	void OnEvent(XEvent* event);
	void OnConfigureNotify(XEvent* event);
	void OnClientMessage(XEvent* event);
	void OnExpose(XEvent* event);
	void OnFocusIn(XEvent* event);
	void OnFocusOut(XEvent* event);
	void OnPropertyNotify(XEvent* event);
	void OnKeyPress(XEvent* event);
	void OnKeyRelease(XEvent* event);
	void OnButtonPress(XEvent* event);
	void OnButtonRelease(XEvent* event);
	void OnMotionNotify(XEvent* event);
	void OnLeaveNotify(XEvent* event);
	void OnSelectionClear(XEvent* event);
	void OnSelectionNotify(XEvent* event);
	void OnSelectionRequest(XEvent* event);
	void OnXInputEvent(XEvent* event);

	void CreateBackbuffer(int width, int height);
	void DestroyBackbuffer();

	InputKey GetInputKey(XEvent* event);
	Point GetMousePos(XEvent* event);

	static bool WaitForEvents(int timeout);
	static void DispatchEvent(XEvent* event);

	static void CheckNeedsUpdate();

	std::vector<uint8_t> GetWindowProperty(Atom property, Atom &actual_type, int &actual_format, unsigned long &item_count);

	DisplayWindowHost* windowHost = nullptr;
	X11DisplayWindow* owner = nullptr;
	Display* display = nullptr;
	Window window = {};
	int screen = 0;
	int depth = 0;
	Visual* visual = nullptr;
	Colormap colormap = {};
	XIC xic = nullptr;
	StandardCursor cursor = {};
	bool isCursorEnabled = true;
	bool isMapped = false;
	bool isMinimized = false;
	bool isFullscreen = false;
	double dpiScale = 1.0;

	int ClientSizeX = 0;
	int ClientSizeY = 0;

	struct
	{
		int LastX = -1;
		int LastY = -1;
		bool Focused = false;
	} RawInput;

	Pixmap cursor_bitmap = None;
	Cursor hidden_cursor = None;

	std::map<InputKey, bool> keyState;

	std::string clipboardText;

	struct
	{
		Pixmap pixmap = None;
		XImage* image = nullptr;
		void* pixels = nullptr;
		int width = 0;
		int height = 0;
	} backbuffer;

	bool needsUpdate = false;

	friend class X11DisplayBackend;
};
