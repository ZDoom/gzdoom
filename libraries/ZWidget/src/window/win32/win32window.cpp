
#include "win32window.h"
#include <windowsx.h>
#include <stdexcept>
#include <cmath>
#include <vector>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC		((USHORT) 0x01)
#endif

#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE	((USHORT) 0x02)
#endif

#ifndef HID_USAGE_GENERIC_JOYSTICK
#define HID_USAGE_GENERIC_JOYSTICK	((USHORT) 0x04)
#endif

#ifndef HID_USAGE_GENERIC_GAMEPAD
#define HID_USAGE_GENERIC_GAMEPAD	((USHORT) 0x05)
#endif

#ifndef RIDEV_INPUTSINK
#define RIDEV_INPUTSINK	(0x100)
#endif

static std::string from_utf16(const std::wstring& str)
{
	if (str.empty()) return {};
	int needed = WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0, nullptr, nullptr);
	if (needed == 0)
		throw std::runtime_error("WideCharToMultiByte failed");
	std::string result;
	result.resize(needed);
	needed = WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.size(), &result[0], (int)result.size(), nullptr, nullptr);
	if (needed == 0)
		throw std::runtime_error("WideCharToMultiByte failed");
	return result;
}

static std::wstring to_utf16(const std::string& str)
{
	if (str.empty()) return {};
	int needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
	if (needed == 0)
		throw std::runtime_error("MultiByteToWideChar failed");
	std::wstring result;
	result.resize(needed);
	needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &result[0], (int)result.size());
	if (needed == 0)
		throw std::runtime_error("MultiByteToWideChar failed");
	return result;
}

Win32Window::Win32Window(DisplayWindowHost* windowHost) : WindowHost(windowHost)
{
	Windows.push_front(this);
	WindowsIterator = Windows.begin();

	WNDCLASSEX classdesc = {};
	classdesc.cbSize = sizeof(WNDCLASSEX);
	classdesc.hInstance = GetModuleHandle(0);
	classdesc.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
	classdesc.lpszClassName = L"ZWidgetWindow";
	classdesc.lpfnWndProc = &Win32Window::WndProc;
	RegisterClassEx(&classdesc);

	// Microsoft logic at its finest:
	// WS_EX_DLGMODALFRAME hides the sysmenu icon
	// WS_CAPTION shows the caption (yay! actually a flag that does what it says it does!)
	// WS_SYSMENU shows the min/max/close buttons
	// WS_THICKFRAME makes the window resizable
	CreateWindowEx(WS_EX_APPWINDOW | WS_EX_DLGMODALFRAME, L"ZWidgetWindow", L"", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, 0, 0, 100, 100, 0, 0, GetModuleHandle(0), this);

	/*
	RAWINPUTDEVICE rid;
	rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid.usUsage = HID_USAGE_GENERIC_MOUSE;
	rid.dwFlags = RIDEV_INPUTSINK;
	rid.hwndTarget = WindowHandle;
	BOOL result = RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
	*/
}

Win32Window::~Win32Window()
{
	if (WindowHandle)
	{
		DestroyWindow(WindowHandle);
		WindowHandle = 0;
	}

	Windows.erase(WindowsIterator);
}

void Win32Window::SetWindowTitle(const std::string& text)
{
	SetWindowText(WindowHandle, to_utf16(text).c_str());
}

void Win32Window::SetBorderColor(uint32_t bgra8)
{
	bgra8 = bgra8 & 0x00ffffff;
	DwmSetWindowAttribute(WindowHandle, 34/*DWMWA_BORDER_COLOR*/, &bgra8, sizeof(uint32_t));
}

void Win32Window::SetCaptionColor(uint32_t bgra8)
{
	bgra8 = bgra8 & 0x00ffffff;
	DwmSetWindowAttribute(WindowHandle, 35/*DWMWA_CAPTION_COLOR*/, &bgra8, sizeof(uint32_t));
}

void Win32Window::SetCaptionTextColor(uint32_t bgra8)
{
	bgra8 = bgra8 & 0x00ffffff;
	DwmSetWindowAttribute(WindowHandle, 36/*DWMWA_TEXT_COLOR*/, &bgra8, sizeof(uint32_t));
}

void Win32Window::SetWindowFrame(const Rect& box)
{
	double dpiscale = GetDpiScale();
	SetWindowPos(WindowHandle, nullptr, (int)std::round(box.x * dpiscale), (int)std::round(box.y * dpiscale), (int)std::round(box.width * dpiscale), (int)std::round(box.height * dpiscale), SWP_NOACTIVATE | SWP_NOZORDER);
}

void Win32Window::SetClientFrame(const Rect& box)
{
	// This function is currently unused but needs to be disabled because it contains Windows API calls that were only added in Windows 10.
#if 0
	double dpiscale = GetDpiScale();

	RECT rect = {};
	rect.left = (int)std::round(box.x * dpiscale);
	rect.top = (int)std::round(box.y * dpiscale);
	rect.right = rect.left + (int)std::round(box.width * dpiscale);
	rect.bottom = rect.top + (int)std::round(box.height * dpiscale);

	DWORD style = (DWORD)GetWindowLongPtr(WindowHandle, GWL_STYLE);
	DWORD exstyle = (DWORD)GetWindowLongPtr(WindowHandle, GWL_EXSTYLE);
	AdjustWindowRectExForDpi(&rect, style, FALSE, exstyle, GetDpiForWindow(WindowHandle));

	SetWindowPos(WindowHandle, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOACTIVATE | SWP_NOZORDER);
#endif
}

void Win32Window::Show()
{
	ShowWindow(WindowHandle, SW_SHOW);
}

void Win32Window::ShowFullscreen()
{
	HDC screenDC = GetDC(0);
	int width = GetDeviceCaps(screenDC, HORZRES);
	int height = GetDeviceCaps(screenDC, VERTRES);
	ReleaseDC(0, screenDC);
	SetWindowLongPtr(WindowHandle, GWL_EXSTYLE, WS_EX_APPWINDOW);
	SetWindowLongPtr(WindowHandle, GWL_STYLE, WS_OVERLAPPED);
	SetWindowPos(WindowHandle, HWND_TOP, 0, 0, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	Fullscreen = true;
}

void Win32Window::ShowMaximized()
{
	ShowWindow(WindowHandle, SW_SHOWMAXIMIZED);
}

void Win32Window::ShowMinimized()
{
	ShowWindow(WindowHandle, SW_SHOWMINIMIZED);
}

void Win32Window::ShowNormal()
{
	ShowWindow(WindowHandle, SW_NORMAL);
}

void Win32Window::Hide()
{
	ShowWindow(WindowHandle, SW_HIDE);
}

void Win32Window::Activate()
{
	SetFocus(WindowHandle);
}

void Win32Window::ShowCursor(bool enable)
{
}

void Win32Window::LockCursor()
{
	if (!MouseLocked)
	{
		MouseLocked = true;
		GetCursorPos(&MouseLockPos);
		::ShowCursor(FALSE);
	}
}

void Win32Window::UnlockCursor()
{
	if (MouseLocked)
	{
		MouseLocked = false;
		SetCursorPos(MouseLockPos.x, MouseLockPos.y);
		::ShowCursor(TRUE);
	}
}

void Win32Window::CaptureMouse()
{
	SetCapture(WindowHandle);
}

void Win32Window::ReleaseMouseCapture()
{
	ReleaseCapture();
}

void Win32Window::Update()
{
	InvalidateRect(WindowHandle, nullptr, FALSE);
}

bool Win32Window::GetKeyState(EInputKey key)
{
	return ::GetKeyState((int)key) & 0x8000; // High bit (0x8000) means key is down, Low bit (0x0001) means key is sticky on (like Caps Lock, Num Lock, etc.)
}

void Win32Window::SetCursor(StandardCursor cursor)
{
	if (cursor != CurrentCursor)
	{
		CurrentCursor = cursor;
		UpdateCursor();
	}
}

Rect Win32Window::GetWindowFrame() const
{
	RECT box = {};
	GetWindowRect(WindowHandle, &box);
	double dpiscale = GetDpiScale();
	return Rect(box.left / dpiscale, box.top / dpiscale, box.right / dpiscale, box.bottom / dpiscale);
}

Size Win32Window::GetClientSize() const
{
	RECT box = {};
	GetClientRect(WindowHandle, &box);
	double dpiscale = GetDpiScale();
	return Size(box.right / dpiscale, box.bottom / dpiscale);
}

int Win32Window::GetPixelWidth() const
{
	RECT box = {};
	GetClientRect(WindowHandle, &box);
	return box.right;
}

int Win32Window::GetPixelHeight() const
{
	RECT box = {};
	GetClientRect(WindowHandle, &box);
	return box.bottom;
}

typedef UINT(WINAPI* GetDpiForWindow_t)(HWND);
double Win32Window::GetDpiScale() const
{
	static GetDpiForWindow_t pGetDpiForWindow = nullptr;
	static bool done = false;
	if (!done)
	{
		HMODULE hMod = GetModuleHandleA("User32.dll");
		if (hMod != nullptr) pGetDpiForWindow = reinterpret_cast<GetDpiForWindow_t>(GetProcAddress(hMod, "GetDpiForWindow"));
		done = true;
	}

	if (pGetDpiForWindow)
		return pGetDpiForWindow(WindowHandle) / 96.0;
	else
		return 1.0;
}

std::string Win32Window::GetClipboardText()
{
	BOOL result = OpenClipboard(WindowHandle);
	if (result == FALSE)
		throw std::runtime_error("Unable to open clipboard");

	HANDLE handle = GetClipboardData(CF_UNICODETEXT);
	if (handle == 0)
	{
		CloseClipboard();
		return std::string();
	}

	std::wstring::value_type* data = (std::wstring::value_type*)GlobalLock(handle);
	if (data == 0)
	{
		CloseClipboard();
		return std::string();
	}
	std::string str = from_utf16(data);
	GlobalUnlock(handle);

	CloseClipboard();
	return str;
}

void Win32Window::SetClipboardText(const std::string& text)
{
	std::wstring text16 = to_utf16(text);

	BOOL result = OpenClipboard(WindowHandle);
	if (result == FALSE)
		throw std::runtime_error("Unable to open clipboard");

	result = EmptyClipboard();
	if (result == FALSE)
	{
		CloseClipboard();
		throw std::runtime_error("Unable to empty clipboard");
	}

	unsigned int length = (text16.length() + 1) * sizeof(std::wstring::value_type);
	HANDLE handle = GlobalAlloc(GMEM_MOVEABLE, length);
	if (handle == 0)
	{
		CloseClipboard();
		throw std::runtime_error("Unable to allocate clipboard memory");
	}

	void* data = GlobalLock(handle);
	if (data == 0)
	{
		GlobalFree(handle);
		CloseClipboard();
		throw std::runtime_error("Unable to lock clipboard memory");
	}
	memcpy(data, text16.c_str(), length);
	GlobalUnlock(handle);

	HANDLE data_result = SetClipboardData(CF_UNICODETEXT, handle);

	if (data_result == 0)
	{
		GlobalFree(handle);
		CloseClipboard();
		throw std::runtime_error("Unable to set clipboard data");
	}

	CloseClipboard();
}

void Win32Window::PresentBitmap(int width, int height, const uint32_t* pixels)
{
	BITMAPV5HEADER header = {};
	header.bV5Size = sizeof(BITMAPV5HEADER);
	header.bV5Width = width;
	header.bV5Height = -height;
	header.bV5Planes = 1;
	header.bV5BitCount = 32;
	header.bV5Compression = BI_BITFIELDS;
	header.bV5AlphaMask = 0xff000000;
	header.bV5RedMask = 0x00ff0000;
	header.bV5GreenMask = 0x0000ff00;
	header.bV5BlueMask = 0x000000ff;
	header.bV5SizeImage = width * height * sizeof(uint32_t);
	header.bV5CSType = LCS_sRGB;

	HDC dc = PaintDC;
	if (dc != 0)
	{
		int result = SetDIBitsToDevice(dc, 0, 0, width, height, 0, 0, 0, height, pixels, (const BITMAPINFO*)&header, BI_RGB);
		ReleaseDC(WindowHandle, dc);
	}
}

LRESULT Win32Window::OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
	LPARAM result = 0;
	if (DwmDefWindowProc(WindowHandle, msg, wparam, lparam, &result))
		return result;

	if (msg == WM_INPUT)
	{
		bool hasFocus = GetFocus() != 0;

		HRAWINPUT handle = (HRAWINPUT)lparam;
		UINT size = 0;
		UINT result = GetRawInputData(handle, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
		if (result == 0 && size > 0)
		{
			size *= 2;
			std::vector<uint8_t*> buffer(size);
			result = GetRawInputData(handle, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER));
			if (result >= 0)
			{
				RAWINPUT* rawinput = (RAWINPUT*)buffer.data();
				if (rawinput->header.dwType == RIM_TYPEMOUSE)
				{
					if (hasFocus)
						WindowHost->OnWindowRawMouseMove(rawinput->data.mouse.lLastX, rawinput->data.mouse.lLastY);
				}
			}
		}
		return DefWindowProc(WindowHandle, msg, wparam, lparam);
	}
	else if (msg == WM_PAINT)
	{
		PAINTSTRUCT paintStruct = {};
		PaintDC = BeginPaint(WindowHandle, &paintStruct);
		if (PaintDC)
		{
			WindowHost->OnWindowPaint();
			EndPaint(WindowHandle, &paintStruct);
			PaintDC = 0;
		}
		return 0;
	}
	else if (msg == WM_ACTIVATE)
	{
		WindowHost->OnWindowActivated();
	}
	else if (msg == WM_MOUSEMOVE)
	{
		if (MouseLocked && GetFocus() != 0)
		{
			RECT box = {};
			GetClientRect(WindowHandle, &box);

			POINT center = {};
			center.x = box.right / 2;
			center.y = box.bottom / 2;
			ClientToScreen(WindowHandle, &center);

			SetCursorPos(center.x, center.y);
		}
		else
		{
			UpdateCursor();
		}

		WindowHost->OnWindowMouseMove(GetLParamPos(lparam));
	}
	else if (msg == WM_LBUTTONDOWN)
	{
		WindowHost->OnWindowMouseDown(GetLParamPos(lparam), IK_LeftMouse);
	}
	else if (msg == WM_LBUTTONDBLCLK)
	{
		WindowHost->OnWindowMouseDoubleclick(GetLParamPos(lparam), IK_LeftMouse);
	}
	else if (msg == WM_LBUTTONUP)
	{
		WindowHost->OnWindowMouseUp(GetLParamPos(lparam), IK_LeftMouse);
	}
	else if (msg == WM_MBUTTONDOWN)
	{
		WindowHost->OnWindowMouseDown(GetLParamPos(lparam), IK_MiddleMouse);
	}
	else if (msg == WM_MBUTTONDBLCLK)
	{
		WindowHost->OnWindowMouseDoubleclick(GetLParamPos(lparam), IK_MiddleMouse);
	}
	else if (msg == WM_MBUTTONUP)
	{
		WindowHost->OnWindowMouseUp(GetLParamPos(lparam), IK_MiddleMouse);
	}
	else if (msg == WM_RBUTTONDOWN)
	{
		WindowHost->OnWindowMouseDown(GetLParamPos(lparam), IK_RightMouse);
	}
	else if (msg == WM_RBUTTONDBLCLK)
	{
		WindowHost->OnWindowMouseDoubleclick(GetLParamPos(lparam), IK_RightMouse);
	}
	else if (msg == WM_RBUTTONUP)
	{
		WindowHost->OnWindowMouseUp(GetLParamPos(lparam), IK_RightMouse);
	}
	else if (msg == WM_MOUSEWHEEL)
	{
		double delta = GET_WHEEL_DELTA_WPARAM(wparam) / (double)WHEEL_DELTA;

		// Note: WM_MOUSEWHEEL uses screen coordinates. GetLParamPos assumes client coordinates.
		double dpiscale = GetDpiScale();
		POINT pos;
		pos.x = GET_X_LPARAM(lparam);
		pos.y = GET_Y_LPARAM(lparam);
		ScreenToClient(WindowHandle, &pos);

		WindowHost->OnWindowMouseWheel(Point(pos.x / dpiscale, pos.y / dpiscale), delta < 0.0 ? IK_MouseWheelDown : IK_MouseWheelUp);
	}
	else if (msg == WM_CHAR)
	{
		wchar_t buf[2] = { (wchar_t)wparam, 0 };
		WindowHost->OnWindowKeyChar(from_utf16(buf));
	}
	else if (msg == WM_KEYDOWN)
	{
		WindowHost->OnWindowKeyDown((EInputKey)wparam);
	}
	else if (msg == WM_KEYUP)
	{
		WindowHost->OnWindowKeyUp((EInputKey)wparam);
	}
	else if (msg == WM_SETFOCUS)
	{
		if (MouseLocked)
		{
			::ShowCursor(FALSE);
		}
	}
	else if (msg == WM_KILLFOCUS)
	{
		if (MouseLocked)
		{
			::ShowCursor(TRUE);
		}
	}
	else if (msg == WM_CLOSE)
	{
		WindowHost->OnWindowClose();
		return 0;
	}
	else if (msg == WM_SIZE)
	{
		WindowHost->OnWindowGeometryChanged();
		return 0;
	}
	/*else if (msg == WM_NCCALCSIZE && wparam == TRUE) // calculate client area for the window
	{
		NCCALCSIZE_PARAMS* calcsize = (NCCALCSIZE_PARAMS*)lparam;
		return WVR_REDRAW;
	}*/

	return DefWindowProc(WindowHandle, msg, wparam, lparam);
}

void Win32Window::UpdateCursor()
{
	LPCWSTR cursor = IDC_ARROW;
	switch (CurrentCursor)
	{
	case StandardCursor::arrow: cursor = IDC_ARROW; break;
	case StandardCursor::appstarting: cursor = IDC_APPSTARTING; break;
	case StandardCursor::cross: cursor = IDC_CROSS; break;
	case StandardCursor::hand: cursor = IDC_HAND; break;
	case StandardCursor::ibeam: cursor = IDC_IBEAM; break;
	case StandardCursor::no: cursor = IDC_NO; break;
	case StandardCursor::size_all: cursor = IDC_SIZEALL; break;
	case StandardCursor::size_nesw: cursor = IDC_SIZENESW; break;
	case StandardCursor::size_ns: cursor = IDC_SIZENS; break;
	case StandardCursor::size_nwse: cursor = IDC_SIZENWSE; break;
	case StandardCursor::size_we: cursor = IDC_SIZEWE; break;
	case StandardCursor::uparrow: cursor = IDC_UPARROW; break;
	case StandardCursor::wait: cursor = IDC_WAIT; break;
	default: break;
	}

	::SetCursor((HCURSOR)LoadImage(0, cursor, IMAGE_CURSOR, LR_DEFAULTSIZE, LR_DEFAULTSIZE, LR_SHARED));
}

Point Win32Window::GetLParamPos(LPARAM lparam) const
{
	double dpiscale = GetDpiScale();
	return Point(GET_X_LPARAM(lparam) / dpiscale, GET_Y_LPARAM(lparam) / dpiscale);
}

LRESULT Win32Window::WndProc(HWND windowhandle, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_CREATE)
	{
		CREATESTRUCT* createstruct = (CREATESTRUCT*)lparam;
		Win32Window* viewport = (Win32Window*)createstruct->lpCreateParams;
		viewport->WindowHandle = windowhandle;
		SetWindowLongPtr(windowhandle, GWLP_USERDATA, (LONG_PTR)viewport);
		return viewport->OnWindowMessage(msg, wparam, lparam);
	}
	else
	{
		Win32Window* viewport = (Win32Window*)GetWindowLongPtr(windowhandle, GWLP_USERDATA);
		if (viewport)
		{
			LRESULT result = viewport->OnWindowMessage(msg, wparam, lparam);
			if (msg == WM_DESTROY)
			{
				SetWindowLongPtr(windowhandle, GWLP_USERDATA, 0);
				viewport->WindowHandle = 0;
			}
			return result;
		}
		else
		{
			return DefWindowProc(windowhandle, msg, wparam, lparam);
		}
	}
}

void Win32Window::ProcessEvents()
{
	while (true)
	{
		MSG msg = {};
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE) <= 0)
			break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void Win32Window::RunLoop()
{
	while (!ExitRunLoop && !Windows.empty())
	{
		MSG msg = {};
		if (GetMessage(&msg, 0, 0, 0) <= 0)
			break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	ExitRunLoop = false;
}

void Win32Window::ExitLoop()
{
	ExitRunLoop = true;
}

Size Win32Window::GetScreenSize()
{
	HDC screenDC = GetDC(0);
	int screenWidth = GetDeviceCaps(screenDC, HORZRES);
	int screenHeight = GetDeviceCaps(screenDC, VERTRES);
	double dpiScale = GetDeviceCaps(screenDC, LOGPIXELSX) / 96.0;
	ReleaseDC(0, screenDC);

	return Size(screenWidth / dpiScale, screenHeight / dpiScale);
}

static void CALLBACK Win32TimerCallback(HWND handle, UINT message, UINT_PTR timerID, DWORD timestamp)
{
	auto it = Win32Window::Timers.find(timerID);
	if (it != Win32Window::Timers.end())
	{
		it->second();
	}
}

void* Win32Window::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	UINT_PTR result = SetTimer(0, 0, timeoutMilliseconds, Win32TimerCallback);
	if (result == 0)
		throw std::runtime_error("Could not create timer");
	Timers[result] = std::move(onTimer);
	return (void*)result;
}

void Win32Window::StopTimer(void* timerID)
{
	auto it = Timers.find((UINT_PTR)timerID);
	if (it != Timers.end())
	{
		Timers.erase(it);
		KillTimer(0, (UINT_PTR)timerID);
	}
}

std::list<Win32Window*> Win32Window::Windows;
bool Win32Window::ExitRunLoop;

std::unordered_map<UINT_PTR, std::function<void()>> Win32Window::Timers;
