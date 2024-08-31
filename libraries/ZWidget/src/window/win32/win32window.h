#pragma once

#define NOMINMAX
#define WIN32_MEAN_AND_LEAN
#ifndef WINVER
#define WINVER 0x0605
#endif
#include <Windows.h>

#include <list>
#include <unordered_map>
#include <zwidget/window/window.h>

class Win32Window : public DisplayWindow
{
public:
	Win32Window(DisplayWindowHost* windowHost);
	~Win32Window();

	void SetWindowTitle(const std::string& text) override;
	void SetWindowFrame(const Rect& box) override;
	void SetClientFrame(const Rect& box) override;
	void Show() override;
	void ShowFullscreen() override;
	void ShowMaximized() override;
	void ShowMinimized() override;
	void ShowNormal() override;
	void Hide() override;
	void Activate() override;
	void ShowCursor(bool enable) override;
	void LockCursor() override;
	void UnlockCursor() override;
	void CaptureMouse() override;
	void ReleaseMouseCapture() override;
	void Update() override;
	bool GetKeyState(EInputKey key) override;

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

	Point GetLParamPos(LPARAM lparam) const;

	static void ProcessEvents();
	static void RunLoop();
	static void ExitLoop();
	static Size GetScreenSize();

	static void* StartTimer(int timeoutMilliseconds, std::function<void()> onTimer);
	static void StopTimer(void* timerID);

	static bool ExitRunLoop;
	static std::list<Win32Window*> Windows;
	std::list<Win32Window*>::iterator WindowsIterator;

	static std::unordered_map<UINT_PTR, std::function<void()>> Timers;

	LRESULT OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam);
	static LRESULT CALLBACK WndProc(HWND windowhandle, UINT msg, WPARAM wparam, LPARAM lparam);

	DisplayWindowHost* WindowHost = nullptr;

	HWND WindowHandle = 0;
	bool Fullscreen = false;

	bool MouseLocked = false;
	POINT MouseLockPos = {};

	HDC PaintDC = 0;

	StandardCursor CurrentCursor = StandardCursor::arrow;
};
