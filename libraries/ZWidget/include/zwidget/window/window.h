#pragma once

#include <memory>
#include <string>
#include <functional>
#include <cstdint>
#include <cstdlib>
#include "../core/rect.h"

#ifndef VULKAN_H_

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;

#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T *object;
#else
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
#endif

VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSurfaceKHR)

#endif

class Widget;
class OpenFileDialog;
class SaveFileDialog;
class OpenFolderDialog;

enum class StandardCursor
{
	arrow,
	appstarting,
	cross,
	hand,
	ibeam,
	no,
	size_all,
	size_nesw,
	size_ns,
	size_nwse,
	size_we,
	uparrow,
	wait
};

enum class InputKey : uint32_t
{
	None, LeftMouse, RightMouse, Cancel,
	MiddleMouse, Unknown05, Unknown06, Unknown07,
	Backspace, Tab, Unknown0A, Unknown0B,
	Unknown0C, Enter, Unknown0E, Unknown0F,
	Shift, Ctrl, Alt, Pause,
	CapsLock, Unknown15, Unknown16, Unknown17,
	Unknown18, Unknown19, Unknown1A, Escape,
	Unknown1C, Unknown1D, Unknown1E, Unknown1F,
	Space, PageUp, PageDown, End,
	Home, Left, Up, Right,
	Down, Select, Print, Execute,
	PrintScrn, Insert, Delete, Help,
	_0, _1, _2, _3,
	_4, _5, _6, _7,
	_8, _9, Unknown3A, Unknown3B,
	Unknown3C, Unknown3D, Unknown3E, Unknown3F,
	Unknown40, A, B, C,
	D, E, F, G,
	H, I, J, K,
	L, M, N, O,
	P, Q, R, S,
	T, U, V, W,
	X, Y, Z, Unknown5B,
	Unknown5C, Unknown5D, Unknown5E, Unknown5F,
	NumPad0, NumPad1, NumPad2, NumPad3,
	NumPad4, NumPad5, NumPad6, NumPad7,
	NumPad8, NumPad9, GreyStar, GreyPlus,
	Separator, GreyMinus, NumPadPeriod, GreySlash,
	F1, F2, F3, F4,
	F5, F6, F7, F8,
	F9, F10, F11, F12,
	F13, F14, F15, F16,
	F17, F18, F19, F20,
	F21, F22, F23, F24,
	Unknown88, Unknown89, Unknown8A, Unknown8B,
	Unknown8C, Unknown8D, Unknown8E, Unknown8F,
	NumLock, ScrollLock, Unknown92, Unknown93,
	Unknown94, Unknown95, Unknown96, Unknown97,
	Unknown98, Unknown99, Unknown9A, Unknown9B,
	Unknown9C, Unknown9D, Unknown9E, Unknown9F,
	LShift, RShift, LControl, RControl,
	UnknownA4, UnknownA5, UnknownA6, UnknownA7,
	UnknownA8, UnknownA9, UnknownAA, UnknownAB,
	UnknownAC, UnknownAD, UnknownAE, UnknownAF,
	UnknownB0, UnknownB1, UnknownB2, UnknownB3,
	UnknownB4, UnknownB5, UnknownB6, UnknownB7,
	UnknownB8, UnknownB9, Semicolon, Equals,
	Comma, Minus, Period, Slash,
	Tilde, UnknownC1, UnknownC2, UnknownC3,
	UnknownC4, UnknownC5, UnknownC6, UnknownC7,
	Joy1, Joy2, Joy3, Joy4,
	Joy5, Joy6, Joy7, Joy8,
	Joy9, Joy10, Joy11, Joy12,
	Joy13, Joy14, Joy15, Joy16,
	UnknownD8, UnknownD9, UnknownDA, LeftBracket,
	Backslash, RightBracket, SingleQuote, UnknownDF,
	JoyX, JoyY, JoyZ, JoyR,
	MouseX, MouseY, MouseZ, MouseW,
	JoyU, JoyV, UnknownEA, UnknownEB,
	MouseWheelUp, MouseWheelDown, Unknown10E, Unknown10F,
	JoyPovUp, JoyPovDown, JoyPovLeft, JoyPovRight,
	UnknownF4, UnknownF5, Attn, CrSel,
	ExSel, ErEof, Play, Zoom,
	NoName, PA1, OEMClear
};

enum class RenderAPI
{
	Unspecified,
	Bitmap,
	Vulkan,
	OpenGL,
	D3D11,
	D3D12,
	Metal
};

class DisplayWindow;

class DisplayWindowHost
{
public:
	virtual void OnWindowPaint() = 0;
	virtual void OnWindowMouseMove(const Point& pos) = 0;
	virtual void OnWindowMouseLeave() = 0;
	virtual void OnWindowMouseDown(const Point& pos, InputKey key) = 0;
	virtual void OnWindowMouseDoubleclick(const Point& pos, InputKey key) = 0;
	virtual void OnWindowMouseUp(const Point& pos, InputKey key) = 0;
	virtual void OnWindowMouseWheel(const Point& pos, InputKey key) = 0;
	virtual void OnWindowRawMouseMove(int dx, int dy) = 0;
	virtual void OnWindowKeyChar(std::string chars) = 0;
	virtual void OnWindowKeyDown(InputKey key) = 0;
	virtual void OnWindowKeyUp(InputKey key) = 0;
	virtual void OnWindowGeometryChanged() = 0;
	virtual void OnWindowClose() = 0;
	virtual void OnWindowActivated() = 0;
	virtual void OnWindowDeactivated() = 0;
	virtual void OnWindowDpiScaleChanged() = 0;
};

class DisplayWindow
{
public:
	static std::unique_ptr<DisplayWindow> Create(DisplayWindowHost* windowHost, bool popupWindow, DisplayWindow* owner, RenderAPI renderAPI);

	static void ProcessEvents();
	static void RunLoop();
	static void ExitLoop();

	static void* StartTimer(int timeoutMilliseconds, std::function<void()> onTimer);
	static void StopTimer(void* timerID);

	static Size GetScreenSize();

	virtual ~DisplayWindow() = default;

	virtual void SetWindowTitle(const std::string& text) = 0;
	virtual void SetWindowFrame(const Rect& box) = 0;
	virtual void SetClientFrame(const Rect& box) = 0;
	virtual void Show() = 0;
	virtual void ShowFullscreen() = 0;
	virtual void ShowMaximized() = 0;
	virtual void ShowMinimized() = 0;
	virtual void ShowNormal() = 0;
	virtual bool IsWindowFullscreen() = 0;
	virtual void Hide() = 0;
	virtual void Activate() = 0;
	virtual void ShowCursor(bool enable) = 0;
	virtual void LockCursor() = 0;
	virtual void UnlockCursor() = 0;
	virtual void CaptureMouse() = 0;
	virtual void ReleaseMouseCapture() = 0;
	virtual void Update() = 0;
	virtual bool GetKeyState(InputKey key) = 0;

	virtual void SetCursor(StandardCursor cursor) = 0;

	virtual Rect GetWindowFrame() const = 0;
	virtual Size GetClientSize() const = 0;
	virtual int GetPixelWidth() const = 0;
	virtual int GetPixelHeight() const = 0;
	virtual double GetDpiScale() const = 0;

	virtual Point MapFromGlobal(const Point& pos) const = 0;
	virtual Point MapToGlobal(const Point& pos) const = 0;

	virtual void SetBorderColor(uint32_t bgra8) = 0;
	virtual void SetCaptionColor(uint32_t bgra8) = 0;
	virtual void SetCaptionTextColor(uint32_t bgra8) = 0;

	virtual void PresentBitmap(int width, int height, const uint32_t* pixels) = 0;

	virtual std::string GetClipboardText() = 0;
	virtual void SetClipboardText(const std::string& text) = 0;

	virtual void* GetNativeHandle() = 0;

	virtual std::vector<std::string> GetVulkanInstanceExtensions() = 0;
	virtual VkSurfaceKHR CreateVulkanSurface(VkInstance instance) = 0;
};

class DisplayBackend
{
public:
	static DisplayBackend* Get();
	static void Set(std::unique_ptr<DisplayBackend> instance);

	static std::unique_ptr<DisplayBackend> TryCreateWin32();
	static std::unique_ptr<DisplayBackend> TryCreateSDL2();
	static std::unique_ptr<DisplayBackend> TryCreateX11();
	static std::unique_ptr<DisplayBackend> TryCreateWayland();

	static std::unique_ptr<DisplayBackend> TryCreateBackend();

	virtual ~DisplayBackend() = default;

	virtual bool IsWin32() { return false; }
	virtual bool IsSDL2() { return false; }
	virtual bool IsX11() { return false; }
	virtual bool IsWayland() { return false; }

	virtual std::unique_ptr<DisplayWindow> Create(DisplayWindowHost* windowHost, bool popupWindow, DisplayWindow* owner, RenderAPI renderAPI) = 0;
	virtual void ProcessEvents() = 0;
	virtual void RunLoop() = 0;
	virtual void ExitLoop() = 0;

	virtual void* StartTimer(int timeoutMilliseconds, std::function<void()> onTimer) = 0;
	virtual void StopTimer(void* timerID) = 0;

	virtual Size GetScreenSize() = 0;

	virtual std::unique_ptr<OpenFileDialog> CreateOpenFileDialog(DisplayWindow* owner);
	virtual std::unique_ptr<SaveFileDialog> CreateSaveFileDialog(DisplayWindow* owner);
	virtual std::unique_ptr<OpenFolderDialog> CreateOpenFolderDialog(DisplayWindow* owner);
};
