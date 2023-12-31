#pragma once

#include <memory>
#include <string>
#include <functional>
#include "../core/rect.h"

class Engine;

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

enum EInputKey
{
	IK_None, IK_LeftMouse, IK_RightMouse, IK_Cancel,
	IK_MiddleMouse, IK_Unknown05, IK_Unknown06, IK_Unknown07,
	IK_Backspace, IK_Tab, IK_Unknown0A, IK_Unknown0B,
	IK_Unknown0C, IK_Enter, IK_Unknown0E, IK_Unknown0F,
	IK_Shift, IK_Ctrl, IK_Alt, IK_Pause,
	IK_CapsLock, IK_Unknown15, IK_Unknown16, IK_Unknown17,
	IK_Unknown18, IK_Unknown19, IK_Unknown1A, IK_Escape,
	IK_Unknown1C, IK_Unknown1D, IK_Unknown1E, IK_Unknown1F,
	IK_Space, IK_PageUp, IK_PageDown, IK_End,
	IK_Home, IK_Left, IK_Up, IK_Right,
	IK_Down, IK_Select, IK_Print, IK_Execute,
	IK_PrintScrn, IK_Insert, IK_Delete, IK_Help,
	IK_0, IK_1, IK_2, IK_3,
	IK_4, IK_5, IK_6, IK_7,
	IK_8, IK_9, IK_Unknown3A, IK_Unknown3B,
	IK_Unknown3C, IK_Unknown3D, IK_Unknown3E, IK_Unknown3F,
	IK_Unknown40, IK_A, IK_B, IK_C,
	IK_D, IK_E, IK_F, IK_G,
	IK_H, IK_I, IK_J, IK_K,
	IK_L, IK_M, IK_N, IK_O,
	IK_P, IK_Q, IK_R, IK_S,
	IK_T, IK_U, IK_V, IK_W,
	IK_X, IK_Y, IK_Z, IK_Unknown5B,
	IK_Unknown5C, IK_Unknown5D, IK_Unknown5E, IK_Unknown5F,
	IK_NumPad0, IK_NumPad1, IK_NumPad2, IK_NumPad3,
	IK_NumPad4, IK_NumPad5, IK_NumPad6, IK_NumPad7,
	IK_NumPad8, IK_NumPad9, IK_GreyStar, IK_GreyPlus,
	IK_Separator, IK_GreyMinus, IK_NumPadPeriod, IK_GreySlash,
	IK_F1, IK_F2, IK_F3, IK_F4,
	IK_F5, IK_F6, IK_F7, IK_F8,
	IK_F9, IK_F10, IK_F11, IK_F12,
	IK_F13, IK_F14, IK_F15, IK_F16,
	IK_F17, IK_F18, IK_F19, IK_F20,
	IK_F21, IK_F22, IK_F23, IK_F24,
	IK_Unknown88, IK_Unknown89, IK_Unknown8A, IK_Unknown8B,
	IK_Unknown8C, IK_Unknown8D, IK_Unknown8E, IK_Unknown8F,
	IK_NumLock, IK_ScrollLock, IK_Unknown92, IK_Unknown93,
	IK_Unknown94, IK_Unknown95, IK_Unknown96, IK_Unknown97,
	IK_Unknown98, IK_Unknown99, IK_Unknown9A, IK_Unknown9B,
	IK_Unknown9C, IK_Unknown9D, IK_Unknown9E, IK_Unknown9F,
	IK_LShift, IK_RShift, IK_LControl, IK_RControl,
	IK_UnknownA4, IK_UnknownA5, IK_UnknownA6, IK_UnknownA7,
	IK_UnknownA8, IK_UnknownA9, IK_UnknownAA, IK_UnknownAB,
	IK_UnknownAC, IK_UnknownAD, IK_UnknownAE, IK_UnknownAF,
	IK_UnknownB0, IK_UnknownB1, IK_UnknownB2, IK_UnknownB3,
	IK_UnknownB4, IK_UnknownB5, IK_UnknownB6, IK_UnknownB7,
	IK_UnknownB8, IK_UnknownB9, IK_Semicolon, IK_Equals,
	IK_Comma, IK_Minus, IK_Period, IK_Slash,
	IK_Tilde, IK_UnknownC1, IK_UnknownC2, IK_UnknownC3,
	IK_UnknownC4, IK_UnknownC5, IK_UnknownC6, IK_UnknownC7,
	IK_Joy1, IK_Joy2, IK_Joy3, IK_Joy4,
	IK_Joy5, IK_Joy6, IK_Joy7, IK_Joy8,
	IK_Joy9, IK_Joy10, IK_Joy11, IK_Joy12,
	IK_Joy13, IK_Joy14, IK_Joy15, IK_Joy16,
	IK_UnknownD8, IK_UnknownD9, IK_UnknownDA, IK_LeftBracket,
	IK_Backslash, IK_RightBracket, IK_SingleQuote, IK_UnknownDF,
	IK_JoyX, IK_JoyY, IK_JoyZ, IK_JoyR,
	IK_MouseX, IK_MouseY, IK_MouseZ, IK_MouseW,
	IK_JoyU, IK_JoyV, IK_UnknownEA, IK_UnknownEB,
	IK_MouseWheelUp, IK_MouseWheelDown, IK_Unknown10E, IK_Unknown10F,
	IK_JoyPovUp, IK_JoyPovDown, IK_JoyPovLeft, IK_JoyPovRight,
	IK_UnknownF4, IK_UnknownF5, IK_Attn, IK_CrSel,
	IK_ExSel, IK_ErEof, IK_Play, IK_Zoom,
	IK_NoName, IK_PA1, IK_OEMClear
};

enum EInputType
{
	IST_None,
	IST_Press,
	IST_Hold,
	IST_Release,
	IST_Axis
};

class KeyEvent
{
public:
	EInputKey Key;
	bool Ctrl = false;
	bool Alt = false;
	bool Shift = false;
	Point MousePos = Point(0.0, 0.0);
};

class DisplayWindow;

class DisplayWindowHost
{
public:
	virtual void OnWindowPaint() = 0;
	virtual void OnWindowMouseMove(const Point& pos) = 0;
	virtual void OnWindowMouseDown(const Point& pos, EInputKey key) = 0;
	virtual void OnWindowMouseDoubleclick(const Point& pos, EInputKey key) = 0;
	virtual void OnWindowMouseUp(const Point& pos, EInputKey key) = 0;
	virtual void OnWindowMouseWheel(const Point& pos, EInputKey key) = 0;
	virtual void OnWindowRawMouseMove(int dx, int dy) = 0;
	virtual void OnWindowKeyChar(std::string chars) = 0;
	virtual void OnWindowKeyDown(EInputKey key) = 0;
	virtual void OnWindowKeyUp(EInputKey key) = 0;
	virtual void OnWindowGeometryChanged() = 0;
	virtual void OnWindowClose() = 0;
	virtual void OnWindowActivated() = 0;
	virtual void OnWindowDeactivated() = 0;
	virtual void OnWindowDpiScaleChanged() = 0;
};

class DisplayWindow
{
public:
	static std::unique_ptr<DisplayWindow> Create(DisplayWindowHost* windowHost);

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
	virtual void Hide() = 0;
	virtual void Activate() = 0;
	virtual void ShowCursor(bool enable) = 0;
	virtual void LockCursor() = 0;
	virtual void UnlockCursor() = 0;
	virtual void Update() = 0;
	virtual bool GetKeyState(EInputKey key) = 0;

	virtual Rect GetWindowFrame() const = 0;
	virtual Size GetClientSize() const = 0;
	virtual int GetPixelWidth() const = 0;
	virtual int GetPixelHeight() const = 0;
	virtual double GetDpiScale() const = 0;

	virtual void SetBorderColor(uint32_t bgra8) = 0;
	virtual void SetCaptionColor(uint32_t bgra8) = 0;
	virtual void SetCaptionTextColor(uint32_t bgra8) = 0;

	virtual void PresentBitmap(int width, int height, const uint32_t* pixels) = 0;

	virtual std::string GetClipboardText() = 0;
	virtual void SetClipboardText(const std::string& text) = 0;
};
