#pragma once

#include "win32_util.h"

#include <list>
#include <unordered_map>
#include <zwidget/window/window.h>
#include <zwidget/window/win32nativehandle.h>

class Win32DisplayWindow : public DisplayWindow
{
public:
	Win32DisplayWindow(DisplayWindowHost* windowHost, bool popupWindow, Win32DisplayWindow* owner, RenderAPI renderAPI);
	~Win32DisplayWindow();

	void SetWindowTitle(const std::string& text) override;
	void SetWindowIcon(const std::vector<std::shared_ptr<Image>>& images) override;
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
	void UpdateCursor();

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

	Point GetLParamPos(LPARAM lparam) const;

	void* GetNativeHandle() override { return &WindowHandle; }

	std::vector<std::string> GetVulkanInstanceExtensions() override;
	VkSurfaceKHR CreateVulkanSurface(VkInstance instance) override;

	static void ProcessEvents();
	static void RunLoop();
	static void ExitLoop();
	static Size GetScreenSize();

	static void* StartTimer(int timeoutMilliseconds, std::function<void()> onTimer);
	static void StopTimer(void* timerID);

	static bool ExitRunLoop;
	static std::list<Win32DisplayWindow*> Windows;
	std::list<Win32DisplayWindow*>::iterator WindowsIterator;

	static std::unordered_map<UINT_PTR, std::function<void()>> Timers;

	LRESULT OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam);
	static LRESULT CALLBACK WndProc(HWND windowhandle, UINT msg, WPARAM wparam, LPARAM lparam);

	DisplayWindowHost* WindowHost = nullptr;
	bool PopupWindow = false;

	Win32NativeHandle WindowHandle;
	bool Fullscreen = false;

	bool MouseLocked = false;
	POINT MouseLockPos = {};

	bool TrackMouseActive = false;

	HDC PaintDC = 0;

	HICON SmallIcon = {};
	HICON LargeIcon = {};

	StandardCursor CurrentCursor = StandardCursor::arrow;
};
