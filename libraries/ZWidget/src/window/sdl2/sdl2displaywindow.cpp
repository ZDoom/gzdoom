
#include "sdl2displaywindow.h"
#include <stdexcept>

Uint32 SDL2DisplayWindow::PaintEventNumber = 0xffffffff;
bool SDL2DisplayWindow::ExitRunLoop;
std::unordered_map<int, SDL2DisplayWindow*> SDL2DisplayWindow::WindowList;

class InitSDL
{
public:
	InitSDL()
	{
		int result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
		if (result != 0)
			throw std::runtime_error(std::string("Unable to initialize SDL:") + SDL_GetError());

		SDL2DisplayWindow::PaintEventNumber = SDL_RegisterEvents(1);
	}
};

static void CheckInitSDL()
{
	static InitSDL initsdl;
}

SDL2DisplayWindow::SDL2DisplayWindow(DisplayWindowHost* windowHost) : WindowHost(windowHost)
{
	CheckInitSDL();

	int result = SDL_CreateWindowAndRenderer(320, 200, SDL_WINDOW_HIDDEN /*| SDL_WINDOW_ALLOW_HIGHDPI*/, &WindowHandle, &RendererHandle);
	if (result != 0)
		throw std::runtime_error(std::string("Unable to create SDL window:") + SDL_GetError());
	
	WindowList[SDL_GetWindowID(WindowHandle)] = this;
}

SDL2DisplayWindow::~SDL2DisplayWindow()
{
	WindowList.erase(WindowList.find(SDL_GetWindowID(WindowHandle)));

	if (BackBufferTexture)
	{
		SDL_DestroyTexture(BackBufferTexture);
		BackBufferTexture = nullptr;
	}

	SDL_DestroyRenderer(RendererHandle);
	SDL_DestroyWindow(WindowHandle);
	RendererHandle = nullptr;
	WindowHandle = nullptr;
}

void SDL2DisplayWindow::SetWindowTitle(const std::string& text)
{
	SDL_SetWindowTitle(WindowHandle, text.c_str());
}

void SDL2DisplayWindow::SetWindowFrame(const Rect& box)
{
	// SDL2 doesn't really seem to have an API for this.
	// The docs aren't clear what you're setting when calling SDL_SetWindowSize.
	SetClientFrame(box);
}

void SDL2DisplayWindow::SetClientFrame(const Rect& box)
{
	// Is there a way to set both in one call?

	double uiscale = GetDpiScale();
	int x = (int)std::round(box.x * uiscale);
	int y = (int)std::round(box.y * uiscale);
	int w = (int)std::round(box.width * uiscale);
	int h = (int)std::round(box.height * uiscale);

	SDL_SetWindowPosition(WindowHandle, x, y);
	SDL_SetWindowSize(WindowHandle, w, h);
}

void SDL2DisplayWindow::Show()
{
	SDL_ShowWindow(WindowHandle);
}

void SDL2DisplayWindow::ShowFullscreen()
{
	SDL_SetWindowFullscreen(WindowHandle, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void SDL2DisplayWindow::ShowMaximized()
{
	SDL_ShowWindow(WindowHandle);
	SDL_MaximizeWindow(WindowHandle);
}

void SDL2DisplayWindow::ShowMinimized()
{
	SDL_ShowWindow(WindowHandle);
	SDL_MinimizeWindow(WindowHandle);
}

void SDL2DisplayWindow::ShowNormal()
{
	SDL_ShowWindow(WindowHandle);
	SDL_SetWindowFullscreen(WindowHandle, 0);
}

void SDL2DisplayWindow::Hide()
{
	SDL_HideWindow(WindowHandle);
}

void SDL2DisplayWindow::Activate()
{
	SDL_RaiseWindow(WindowHandle);
}

void SDL2DisplayWindow::ShowCursor(bool enable)
{
	SDL_ShowCursor(enable);
}

void SDL2DisplayWindow::LockCursor()
{
	SDL_SetWindowGrab(WindowHandle, SDL_TRUE);
	SDL_ShowCursor(0);
}

void SDL2DisplayWindow::UnlockCursor()
{
	SDL_SetWindowGrab(WindowHandle, SDL_FALSE);
	SDL_ShowCursor(1);
}

void SDL2DisplayWindow::CaptureMouse()
{
}

void SDL2DisplayWindow::ReleaseMouseCapture()
{
}

void SDL2DisplayWindow::SetCursor(StandardCursor cursor)
{
}

void SDL2DisplayWindow::Update()
{
	SDL_Event event = {};
	event.type = PaintEventNumber;
	event.user.windowID = SDL_GetWindowID(WindowHandle);
	SDL_PushEvent(&event);
}

bool SDL2DisplayWindow::GetKeyState(EInputKey key)
{
	int numkeys = 0;
	const Uint8* state = SDL_GetKeyboardState(&numkeys);
	if (!state) return false;

	SDL_Scancode index = InputKeyToScancode(key);
	return (index < numkeys) ? state[index] != 0 : false;
}

Rect SDL2DisplayWindow::GetWindowFrame() const
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	double uiscale = GetDpiScale();
	SDL_GetWindowPosition(WindowHandle, &x, &y);
	SDL_GetWindowSize(WindowHandle, &w, &h);
	return Rect::xywh(x / uiscale, y / uiscale, w / uiscale, h / uiscale);
}

Size SDL2DisplayWindow::GetClientSize() const
{
	int w = 0;
	int h = 0;
	double uiscale = GetDpiScale();
	SDL_GetWindowSize(WindowHandle, &w, &h);
	return Size(w / uiscale, h / uiscale);
}

int SDL2DisplayWindow::GetPixelWidth() const
{
	int w = 0;
	int h = 0;
	int result = SDL_GetRendererOutputSize(RendererHandle, &w, &h);
	return w;
}

int SDL2DisplayWindow::GetPixelHeight() const
{
	int w = 0;
	int h = 0;
	int result = SDL_GetRendererOutputSize(RendererHandle, &w, &h);
	return h;
}

double SDL2DisplayWindow::GetDpiScale() const
{
	// SDL2 doesn't really support this properly. SDL_GetDisplayDPI returns the wrong information according to the docs.
	return 1.0;
}

void SDL2DisplayWindow::PresentBitmap(int width, int height, const uint32_t* pixels)
{
	if (!BackBufferTexture || BackBufferWidth != width || BackBufferHeight != height)
	{
		if (BackBufferTexture)
		{
			SDL_DestroyTexture(BackBufferTexture);
			BackBufferTexture = nullptr;
		}

		BackBufferTexture = SDL_CreateTexture(RendererHandle, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
		if (!BackBufferTexture)
			return;

		BackBufferWidth = width;
		BackBufferHeight = height;
	}

	int destpitch = 0;
	void* dest = nullptr;
	int result = SDL_LockTexture(BackBufferTexture, nullptr, &dest, &destpitch);
	if (result != 0) return;
	for (int y = 0; y < height; y++)
	{
		const void* sline = pixels + y * width;
		void* dline = (uint8_t*)dest + y * destpitch;
		memcpy(dline, sline, width << 2);
	}
	SDL_UnlockTexture(BackBufferTexture);

	SDL_RenderCopy(RendererHandle, BackBufferTexture, nullptr, nullptr);
	SDL_RenderPresent(RendererHandle);
}

void SDL2DisplayWindow::SetBorderColor(uint32_t bgra8)
{
	// SDL doesn't have this
}

void SDL2DisplayWindow::SetCaptionColor(uint32_t bgra8)
{
	// SDL doesn't have this
}

void SDL2DisplayWindow::SetCaptionTextColor(uint32_t bgra8)
{
	// SDL doesn't have this
}

std::string SDL2DisplayWindow::GetClipboardText()
{
	char* buffer = SDL_GetClipboardText();
	if (!buffer)
		return {};
	std::string text = buffer;
	SDL_free(buffer);
	return text;
}

void SDL2DisplayWindow::SetClipboardText(const std::string& text)
{
	SDL_SetClipboardText(text.c_str());
}

void SDL2DisplayWindow::ProcessEvents()
{
	CheckInitSDL();

	SDL_Event event;
	while (SDL_PollEvent(&event) != 0)
	{
		DispatchEvent(event);
	}
}

void SDL2DisplayWindow::RunLoop()
{
	CheckInitSDL();

	while (!ExitRunLoop)
	{
		SDL_Event event;
		int result = SDL_WaitEvent(&event);
		if (result == 0)
			throw std::runtime_error(std::string("SDL_WaitEvent failed:") + SDL_GetError());
		DispatchEvent(event);
	}
}

void SDL2DisplayWindow::ExitLoop()
{
	CheckInitSDL();

	ExitRunLoop = true;
}

Size SDL2DisplayWindow::GetScreenSize()
{
	CheckInitSDL();

	SDL_Rect rect = {};
	int result = SDL_GetDisplayBounds(0, &rect);
	if (result != 0)
		throw std::runtime_error(std::string("Unable to get screen size:") + SDL_GetError());

	double uiscale = 1.0; // SDL2 doesn't really support this properly. SDL_GetDisplayDPI returns the wrong information according to the docs.
	return Size(rect.w / uiscale, rect.h / uiscale);
}

void* SDL2DisplayWindow::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	CheckInitSDL();

	// To do: implement timers

	return nullptr;
}

void SDL2DisplayWindow::StopTimer(void* timerID)
{
	CheckInitSDL();

	// To do: implement timers
}

SDL2DisplayWindow* SDL2DisplayWindow::FindEventWindow(const SDL_Event& event)
{
	int windowID;
	switch (event.type)
	{
	case SDL_WINDOWEVENT: windowID = event.window.windowID; break;
	case SDL_TEXTINPUT: windowID = event.text.windowID; break;
	case SDL_KEYUP: windowID = event.key.windowID; break;
	case SDL_KEYDOWN: windowID = event.key.windowID; break;
	case SDL_MOUSEBUTTONUP: windowID = event.button.windowID; break;
	case SDL_MOUSEBUTTONDOWN: windowID = event.button.windowID; break;
	case SDL_MOUSEWHEEL: windowID = event.wheel.windowID; break;
	case SDL_MOUSEMOTION: windowID = event.motion.windowID; break;
	default:
		if (event.type == PaintEventNumber) windowID = event.user.windowID;
		else return nullptr;
	}

	auto it = WindowList.find(windowID);
	return it != WindowList.end() ? it->second : nullptr;
}

void SDL2DisplayWindow::DispatchEvent(const SDL_Event& event)
{
	SDL2DisplayWindow* window = FindEventWindow(event);
	if (!window) return;

	switch (event.type)
	{
	case SDL_WINDOWEVENT: window->OnWindowEvent(event.window); break;
	case SDL_TEXTINPUT: window->OnTextInput(event.text); break;
	case SDL_KEYUP: window->OnKeyUp(event.key); break;
	case SDL_KEYDOWN: window->OnKeyDown(event.key); break;
	case SDL_MOUSEBUTTONUP: window->OnMouseButtonUp(event.button); break;
	case SDL_MOUSEBUTTONDOWN: window->OnMouseButtonDown(event.button); break;
	case SDL_MOUSEWHEEL: window->OnMouseWheel(event.wheel); break;
	case SDL_MOUSEMOTION: window->OnMouseMotion(event.motion); break;
	default:
		if (event.type == PaintEventNumber) window->OnPaintEvent();
	}
}

void SDL2DisplayWindow::OnWindowEvent(const SDL_WindowEvent& event)
{
	switch (event.event)
	{
		case SDL_WINDOWEVENT_CLOSE: WindowHost->OnWindowClose(); break;
		case SDL_WINDOWEVENT_MOVED: WindowHost->OnWindowGeometryChanged(); break;
		case SDL_WINDOWEVENT_RESIZED: WindowHost->OnWindowGeometryChanged(); break;
		case SDL_WINDOWEVENT_SHOWN: WindowHost->OnWindowPaint(); break;
		case SDL_WINDOWEVENT_EXPOSED: WindowHost->OnWindowPaint(); break;
		case SDL_WINDOWEVENT_FOCUS_GAINED: WindowHost->OnWindowActivated(); break;
		case SDL_WINDOWEVENT_FOCUS_LOST: WindowHost->OnWindowDeactivated(); break;
	}
}

void SDL2DisplayWindow::OnTextInput(const SDL_TextInputEvent& event)
{
	WindowHost->OnWindowKeyChar(event.text);
}

void SDL2DisplayWindow::OnKeyUp(const SDL_KeyboardEvent& event)
{
	WindowHost->OnWindowKeyUp(ScancodeToInputKey(event.keysym.scancode));
}

void SDL2DisplayWindow::OnKeyDown(const SDL_KeyboardEvent& event)
{
	WindowHost->OnWindowKeyDown(ScancodeToInputKey(event.keysym.scancode));
}

EInputKey SDL2DisplayWindow::GetMouseButtonKey(const SDL_MouseButtonEvent& event)
{
	switch (event.button)
	{
		case SDL_BUTTON_LEFT: return IK_LeftMouse;
		case SDL_BUTTON_MIDDLE: return IK_MiddleMouse;
		case SDL_BUTTON_RIGHT: return IK_RightMouse;
		// case SDL_BUTTON_X1: return IK_XButton1;
		// case SDL_BUTTON_X2: return IK_XButton2;
		default: return IK_None;
	}
}

void SDL2DisplayWindow::OnMouseButtonUp(const SDL_MouseButtonEvent& event)
{
	EInputKey key = GetMouseButtonKey(event);
	if (key != IK_None)
	{
		WindowHost->OnWindowMouseUp(GetMousePos(event), key);
	}
}

void SDL2DisplayWindow::OnMouseButtonDown(const SDL_MouseButtonEvent& event)
{
	EInputKey key = GetMouseButtonKey(event);
	if (key != IK_None)
	{
		WindowHost->OnWindowMouseDown(GetMousePos(event), key);
	}
}

void SDL2DisplayWindow::OnMouseWheel(const SDL_MouseWheelEvent& event)
{
	EInputKey key = (event.y > 0) ? IK_MouseWheelUp : (event.y < 0) ? IK_MouseWheelDown : IK_None;
	if (key != IK_None)
	{
		WindowHost->OnWindowMouseWheel(GetMousePos(event), key);
	}
}

void SDL2DisplayWindow::OnMouseMotion(const SDL_MouseMotionEvent& event)
{
	WindowHost->OnWindowMouseMove(GetMousePos(event));
}

void SDL2DisplayWindow::OnPaintEvent()
{
	WindowHost->OnWindowPaint();
}

EInputKey SDL2DisplayWindow::ScancodeToInputKey(SDL_Scancode keycode)
{
	switch (keycode)
	{
		case SDL_SCANCODE_BACKSPACE: return IK_Backspace;
		case SDL_SCANCODE_TAB: return IK_Tab;
		case SDL_SCANCODE_CLEAR: return IK_OEMClear;
		case SDL_SCANCODE_RETURN: return IK_Enter;
		case SDL_SCANCODE_MENU: return IK_Alt;
		case SDL_SCANCODE_PAUSE: return IK_Pause;
		case SDL_SCANCODE_ESCAPE: return IK_Escape;
		case SDL_SCANCODE_SPACE: return IK_Space;
		case SDL_SCANCODE_END: return IK_End;
		case SDL_SCANCODE_HOME: return IK_Home;
		case SDL_SCANCODE_LEFT: return IK_Left;
		case SDL_SCANCODE_UP: return IK_Up;
		case SDL_SCANCODE_RIGHT: return IK_Right;
		case SDL_SCANCODE_DOWN: return IK_Down;
		case SDL_SCANCODE_SELECT: return IK_Select;
		case SDL_SCANCODE_PRINTSCREEN: return IK_Print;
		case SDL_SCANCODE_EXECUTE: return IK_Execute;
		case SDL_SCANCODE_INSERT: return IK_Insert;
		case SDL_SCANCODE_DELETE: return IK_Delete;
		case SDL_SCANCODE_HELP: return IK_Help;
		case SDL_SCANCODE_0: return IK_0;
		case SDL_SCANCODE_1: return IK_1;
		case SDL_SCANCODE_2: return IK_2;
		case SDL_SCANCODE_3: return IK_3;
		case SDL_SCANCODE_4: return IK_4;
		case SDL_SCANCODE_5: return IK_5;
		case SDL_SCANCODE_6: return IK_6;
		case SDL_SCANCODE_7: return IK_7;
		case SDL_SCANCODE_8: return IK_8;
		case SDL_SCANCODE_9: return IK_9;
		case SDL_SCANCODE_A: return IK_A;
		case SDL_SCANCODE_B: return IK_B;
		case SDL_SCANCODE_C: return IK_C;
		case SDL_SCANCODE_D: return IK_D;
		case SDL_SCANCODE_E: return IK_E;
		case SDL_SCANCODE_F: return IK_F;
		case SDL_SCANCODE_G: return IK_G;
		case SDL_SCANCODE_H: return IK_H;
		case SDL_SCANCODE_I: return IK_I;
		case SDL_SCANCODE_J: return IK_J;
		case SDL_SCANCODE_K: return IK_K;
		case SDL_SCANCODE_L: return IK_L;
		case SDL_SCANCODE_M: return IK_M;
		case SDL_SCANCODE_N: return IK_N;
		case SDL_SCANCODE_O: return IK_O;
		case SDL_SCANCODE_P: return IK_P;
		case SDL_SCANCODE_Q: return IK_Q;
		case SDL_SCANCODE_R: return IK_R;
		case SDL_SCANCODE_S: return IK_S;
		case SDL_SCANCODE_T: return IK_T;
		case SDL_SCANCODE_U: return IK_U;
		case SDL_SCANCODE_V: return IK_V;
		case SDL_SCANCODE_W: return IK_W;
		case SDL_SCANCODE_X: return IK_X;
		case SDL_SCANCODE_Y: return IK_Y;
		case SDL_SCANCODE_Z: return IK_Z;
		case SDL_SCANCODE_KP_0: return IK_NumPad0;
		case SDL_SCANCODE_KP_1: return IK_NumPad1;
		case SDL_SCANCODE_KP_2: return IK_NumPad2;
		case SDL_SCANCODE_KP_3: return IK_NumPad3;
		case SDL_SCANCODE_KP_4: return IK_NumPad4;
		case SDL_SCANCODE_KP_5: return IK_NumPad5;
		case SDL_SCANCODE_KP_6: return IK_NumPad6;
		case SDL_SCANCODE_KP_7: return IK_NumPad7;
		case SDL_SCANCODE_KP_8: return IK_NumPad8;
		case SDL_SCANCODE_KP_9: return IK_NumPad9;
		// case SDL_SCANCODE_KP_ENTER: return IK_NumPadEnter;
		// case SDL_SCANCODE_KP_MULTIPLY: return IK_Multiply;
		// case SDL_SCANCODE_KP_PLUS: return IK_Add;
		case SDL_SCANCODE_SEPARATOR: return IK_Separator;
		// case SDL_SCANCODE_KP_MINUS: return IK_Subtract;
		case SDL_SCANCODE_KP_PERIOD: return IK_NumPadPeriod;
		// case SDL_SCANCODE_KP_DIVIDE: return IK_Divide;
		case SDL_SCANCODE_F1: return IK_F1;
		case SDL_SCANCODE_F2: return IK_F2;
		case SDL_SCANCODE_F3: return IK_F3;
		case SDL_SCANCODE_F4: return IK_F4;
		case SDL_SCANCODE_F5: return IK_F5;
		case SDL_SCANCODE_F6: return IK_F6;
		case SDL_SCANCODE_F7: return IK_F7;
		case SDL_SCANCODE_F8: return IK_F8;
		case SDL_SCANCODE_F9: return IK_F9;
		case SDL_SCANCODE_F10: return IK_F10;
		case SDL_SCANCODE_F11: return IK_F11;
		case SDL_SCANCODE_F12: return IK_F12;
		case SDL_SCANCODE_F13: return IK_F13;
		case SDL_SCANCODE_F14: return IK_F14;
		case SDL_SCANCODE_F15: return IK_F15;
		case SDL_SCANCODE_F16: return IK_F16;
		case SDL_SCANCODE_F17: return IK_F17;
		case SDL_SCANCODE_F18: return IK_F18;
		case SDL_SCANCODE_F19: return IK_F19;
		case SDL_SCANCODE_F20: return IK_F20;
		case SDL_SCANCODE_F21: return IK_F21;
		case SDL_SCANCODE_F22: return IK_F22;
		case SDL_SCANCODE_F23: return IK_F23;
		case SDL_SCANCODE_F24: return IK_F24;
		case SDL_SCANCODE_NUMLOCKCLEAR: return IK_NumLock;
		case SDL_SCANCODE_SCROLLLOCK: return IK_ScrollLock;
		case SDL_SCANCODE_LSHIFT: return IK_LShift;
		case SDL_SCANCODE_RSHIFT: return IK_RShift;
		case SDL_SCANCODE_LCTRL: return IK_LControl;
		case SDL_SCANCODE_RCTRL: return IK_RControl;
		case SDL_SCANCODE_GRAVE: return IK_Tilde;
		default: return IK_None;
	}
}

SDL_Scancode SDL2DisplayWindow::InputKeyToScancode(EInputKey inputkey)
{
	switch (inputkey)
	{
		case IK_Backspace: return SDL_SCANCODE_BACKSPACE;
		case IK_Tab: return SDL_SCANCODE_TAB;
		case IK_OEMClear: return SDL_SCANCODE_CLEAR;
		case IK_Enter: return SDL_SCANCODE_RETURN;
		case IK_Alt: return SDL_SCANCODE_MENU;
		case IK_Pause: return SDL_SCANCODE_PAUSE;
		case IK_Escape: return SDL_SCANCODE_ESCAPE;
		case IK_Space: return SDL_SCANCODE_SPACE;
		case IK_End: return SDL_SCANCODE_END;
		case IK_Home: return SDL_SCANCODE_HOME;
		case IK_Left: return SDL_SCANCODE_LEFT;
		case IK_Up: return SDL_SCANCODE_UP;
		case IK_Right: return SDL_SCANCODE_RIGHT;
		case IK_Down: return SDL_SCANCODE_DOWN;
		case IK_Select: return SDL_SCANCODE_SELECT;
		case IK_Print: return SDL_SCANCODE_PRINTSCREEN;
		case IK_Execute: return SDL_SCANCODE_EXECUTE;
		case IK_Insert: return SDL_SCANCODE_INSERT;
		case IK_Delete: return SDL_SCANCODE_DELETE;
		case IK_Help: return SDL_SCANCODE_HELP;
		case IK_0: return SDL_SCANCODE_0;
		case IK_1: return SDL_SCANCODE_1;
		case IK_2: return SDL_SCANCODE_2;
		case IK_3: return SDL_SCANCODE_3;
		case IK_4: return SDL_SCANCODE_4;
		case IK_5: return SDL_SCANCODE_5;
		case IK_6: return SDL_SCANCODE_6;
		case IK_7: return SDL_SCANCODE_7;
		case IK_8: return SDL_SCANCODE_8;
		case IK_9: return SDL_SCANCODE_9;
		case IK_A: return SDL_SCANCODE_A;
		case IK_B: return SDL_SCANCODE_B;
		case IK_C: return SDL_SCANCODE_C;
		case IK_D: return SDL_SCANCODE_D;
		case IK_E: return SDL_SCANCODE_E;
		case IK_F: return SDL_SCANCODE_F;
		case IK_G: return SDL_SCANCODE_G;
		case IK_H: return SDL_SCANCODE_H;
		case IK_I: return SDL_SCANCODE_I;
		case IK_J: return SDL_SCANCODE_J;
		case IK_K: return SDL_SCANCODE_K;
		case IK_L: return SDL_SCANCODE_L;
		case IK_M: return SDL_SCANCODE_M;
		case IK_N: return SDL_SCANCODE_N;
		case IK_O: return SDL_SCANCODE_O;
		case IK_P: return SDL_SCANCODE_P;
		case IK_Q: return SDL_SCANCODE_Q;
		case IK_R: return SDL_SCANCODE_R;
		case IK_S: return SDL_SCANCODE_S;
		case IK_T: return SDL_SCANCODE_T;
		case IK_U: return SDL_SCANCODE_U;
		case IK_V: return SDL_SCANCODE_V;
		case IK_W: return SDL_SCANCODE_W;
		case IK_X: return SDL_SCANCODE_X;
		case IK_Y: return SDL_SCANCODE_Y;
		case IK_Z: return SDL_SCANCODE_Z;
		case IK_NumPad0: return SDL_SCANCODE_KP_0;
		case IK_NumPad1: return SDL_SCANCODE_KP_1;
		case IK_NumPad2: return SDL_SCANCODE_KP_2;
		case IK_NumPad3: return SDL_SCANCODE_KP_3;
		case IK_NumPad4: return SDL_SCANCODE_KP_4;
		case IK_NumPad5: return SDL_SCANCODE_KP_5;
		case IK_NumPad6: return SDL_SCANCODE_KP_6;
		case IK_NumPad7: return SDL_SCANCODE_KP_7;
		case IK_NumPad8: return SDL_SCANCODE_KP_8;
		case IK_NumPad9: return SDL_SCANCODE_KP_9;
		// case IK_NumPadEnter: return SDL_SCANCODE_KP_ENTER;
		// case IK_Multiply return SDL_SCANCODE_KP_MULTIPLY:;
		// case IK_Add: return SDL_SCANCODE_KP_PLUS;
		case IK_Separator: return SDL_SCANCODE_SEPARATOR;
		// case IK_Subtract: return SDL_SCANCODE_KP_MINUS;
		case IK_NumPadPeriod: return SDL_SCANCODE_KP_PERIOD;
		// case IK_Divide: return SDL_SCANCODE_KP_DIVIDE;
		case IK_F1: return SDL_SCANCODE_F1;
		case IK_F2: return SDL_SCANCODE_F2;
		case IK_F3: return SDL_SCANCODE_F3;
		case IK_F4: return SDL_SCANCODE_F4;
		case IK_F5: return SDL_SCANCODE_F5;
		case IK_F6: return SDL_SCANCODE_F6;
		case IK_F7: return SDL_SCANCODE_F7;
		case IK_F8: return SDL_SCANCODE_F8;
		case IK_F9: return SDL_SCANCODE_F9;
		case IK_F10: return SDL_SCANCODE_F10;
		case IK_F11: return SDL_SCANCODE_F11;
		case IK_F12: return SDL_SCANCODE_F12;
		case IK_F13: return SDL_SCANCODE_F13;
		case IK_F14: return SDL_SCANCODE_F14;
		case IK_F15: return SDL_SCANCODE_F15;
		case IK_F16: return SDL_SCANCODE_F16;
		case IK_F17: return SDL_SCANCODE_F17;
		case IK_F18: return SDL_SCANCODE_F18;
		case IK_F19: return SDL_SCANCODE_F19;
		case IK_F20: return SDL_SCANCODE_F20;
		case IK_F21: return SDL_SCANCODE_F21;
		case IK_F22: return SDL_SCANCODE_F22;
		case IK_F23: return SDL_SCANCODE_F23;
		case IK_F24: return SDL_SCANCODE_F24;
		case IK_NumLock: return SDL_SCANCODE_NUMLOCKCLEAR;
		case IK_ScrollLock: return SDL_SCANCODE_SCROLLLOCK;
		case IK_LShift: return SDL_SCANCODE_LSHIFT;
		case IK_RShift: return SDL_SCANCODE_RSHIFT;
		case IK_LControl: return SDL_SCANCODE_LCTRL;
		case IK_RControl: return SDL_SCANCODE_RCTRL;
		case IK_Tilde: return SDL_SCANCODE_GRAVE;
		default: return (SDL_Scancode)0;
	}	
}
