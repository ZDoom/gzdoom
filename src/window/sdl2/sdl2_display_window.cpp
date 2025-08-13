
#include "sdl2_display_window.h"
#include <stdexcept>
#include <SDL2/SDL_vulkan.h>

Uint32 SDL2DisplayWindow::PaintEventNumber = 0xffffffff;
bool SDL2DisplayWindow::ExitRunLoop;
std::unordered_map<int, SDL2DisplayWindow*> SDL2DisplayWindow::WindowList;

SDL2DisplayWindow::SDL2DisplayWindow(DisplayWindowHost* windowHost, bool popupWindow, SDL2DisplayWindow* owner, RenderAPI renderAPI, double uiscale) : WindowHost(windowHost), UIScale(uiscale)
{
	unsigned int flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE /*| SDL_WINDOW_ALLOW_HIGHDPI*/;
	if (renderAPI == RenderAPI::Vulkan)
		flags |= SDL_WINDOW_VULKAN;
	else if (renderAPI == RenderAPI::OpenGL)
		flags |= SDL_WINDOW_OPENGL;
#if defined(__APPLE__)
	else if (renderAPI == RenderAPI::Metal)
		flags |= SDL_WINDOW_METAL;
#endif
	if (popupWindow)
		flags |= SDL_WINDOW_BORDERLESS;

	if (renderAPI == RenderAPI::Vulkan || renderAPI == RenderAPI::OpenGL || renderAPI == RenderAPI::Metal)
	{
		Handle.window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 320, 200, flags);
		if (!Handle.window)
			throw std::runtime_error(std::string("Unable to create SDL window:") + SDL_GetError());
	}
	else
	{
		int result = SDL_CreateWindowAndRenderer(320, 200, flags, &Handle.window, &RendererHandle);
		if (result != 0)
			throw std::runtime_error(std::string("Unable to create SDL window:") + SDL_GetError());
	}
	
	WindowList[SDL_GetWindowID(Handle.window)] = this;
}

SDL2DisplayWindow::~SDL2DisplayWindow()
{
	UnlockCursor();

	WindowList.erase(WindowList.find(SDL_GetWindowID(Handle.window)));

	if (BackBufferTexture)
	{
		SDL_DestroyTexture(BackBufferTexture);
		BackBufferTexture = nullptr;
	}

	if (RendererHandle)
		SDL_DestroyRenderer(RendererHandle);
	SDL_DestroyWindow(Handle.window);
	RendererHandle = nullptr;
	Handle.window = nullptr;
}

std::vector<std::string> SDL2DisplayWindow::GetVulkanInstanceExtensions()
{
	unsigned int extCount = 0;
	SDL_Vulkan_GetInstanceExtensions(Handle.window, &extCount, nullptr);
	std::vector<const char*> extNames(extCount);
	SDL_Vulkan_GetInstanceExtensions(Handle.window, &extCount, extNames.data());

	std::vector<std::string> result;
	result.reserve(extNames.size());
	for (const char* ext : extNames)
		result.emplace_back(ext);
	return result;
}

VkSurfaceKHR SDL2DisplayWindow::CreateVulkanSurface(VkInstance instance)
{
	VkSurfaceKHR surfaceHandle = {};
	SDL_Vulkan_CreateSurface(Handle.window, instance, &surfaceHandle);
	if (surfaceHandle)
		throw std::runtime_error("Could not create vulkan surface");
	return surfaceHandle;
}

void SDL2DisplayWindow::SetWindowTitle(const std::string& text)
{
	SDL_SetWindowTitle(Handle.window, text.c_str());
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

	SDL_SetWindowSize(Handle.window, w, h);
	SDL_SetWindowPosition(Handle.window, x, y);
}

void SDL2DisplayWindow::Show()
{
	SDL_ShowWindow(Handle.window);
}

void SDL2DisplayWindow::ShowFullscreen()
{
	SDL_ShowWindow(Handle.window);
	SDL_SetWindowFullscreen(Handle.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	isFullscreen = true;
}

void SDL2DisplayWindow::ShowMaximized()
{
	SDL_ShowWindow(Handle.window);
	SDL_MaximizeWindow(Handle.window);
}

void SDL2DisplayWindow::ShowMinimized()
{
	SDL_ShowWindow(Handle.window);
	SDL_MinimizeWindow(Handle.window);
}

void SDL2DisplayWindow::ShowNormal()
{
	SDL_ShowWindow(Handle.window);
	SDL_SetWindowFullscreen(Handle.window, 0);
	isFullscreen = false;
}

bool SDL2DisplayWindow::IsWindowFullscreen()
{
	return isFullscreen;
}

void SDL2DisplayWindow::Hide()
{
	SDL_HideWindow(Handle.window);
}

void SDL2DisplayWindow::Activate()
{
	SDL_RaiseWindow(Handle.window);
}

void SDL2DisplayWindow::ShowCursor(bool enable)
{
	SDL_ShowCursor(enable);
}

void SDL2DisplayWindow::LockCursor()
{
	if (!CursorLocked)
	{
		SDL_SetRelativeMouseMode(SDL_TRUE);
		CursorLocked = true;
	}
}

void SDL2DisplayWindow::UnlockCursor()
{
	if (CursorLocked)
	{
		SDL_SetRelativeMouseMode(SDL_FALSE);
		CursorLocked = false;
	}
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
	event.user.windowID = SDL_GetWindowID(Handle.window);
	SDL_PushEvent(&event);
}

bool SDL2DisplayWindow::GetKeyState(InputKey key)
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
	SDL_GetWindowPosition(Handle.window, &x, &y);
	SDL_GetWindowSize(Handle.window, &w, &h);
	return Rect::xywh(x / uiscale, y / uiscale, w / uiscale, h / uiscale);
}

Point SDL2DisplayWindow::MapFromGlobal(const Point& pos) const
{
	int x = 0;
	int y = 0;
	double uiscale = GetDpiScale();
	SDL_GetWindowPosition(Handle.window, &x, &y);
	return Point(pos.x - x / uiscale, pos.y - y / uiscale);
}

Point SDL2DisplayWindow::MapToGlobal(const Point& pos) const
{
	int x = 0;
	int y = 0;
	double uiscale = GetDpiScale();
	SDL_GetWindowPosition(Handle.window, &x, &y);
	return Point(pos.x + x / uiscale, pos.y + y / uiscale);
}

Size SDL2DisplayWindow::GetClientSize() const
{
	int w = 0;
	int h = 0;
	double uiscale = GetDpiScale();
	SDL_GetWindowSize(Handle.window, &w, &h);
	return Size(w / uiscale, h / uiscale);
}

int SDL2DisplayWindow::GetPixelWidth() const
{
	int w = 0;
	int h = 0;
	if (RendererHandle)
		SDL_GetRendererOutputSize(RendererHandle, &w, &h);
	else
		SDL_GL_GetDrawableSize(Handle.window, &w, &h);
	return w;
}

int SDL2DisplayWindow::GetPixelHeight() const
{
	int w = 0;
	int h = 0;
	if (RendererHandle)
		SDL_GetRendererOutputSize(RendererHandle, &w, &h);
	else
		SDL_GL_GetDrawableSize(Handle.window, &w, &h);
	return h;
}

double SDL2DisplayWindow::GetDpiScale() const
{
	// SDL2 doesn't really support this properly. SDL_GetDisplayDPI returns the wrong information according to the docs.
	// We currently cheat by asking X11 directly. See the SDL2DisplayBackend constructor.
	return UIScale;
}

void SDL2DisplayWindow::PresentBitmap(int width, int height, const uint32_t* pixels)
{
	if (!RendererHandle)
		return;

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
	SDL_Event event;
	while (SDL_PollEvent(&event) != 0)
	{
		DispatchEvent(event);
	}
}

void SDL2DisplayWindow::RunLoop()
{
	ExitRunLoop = false;
	while (!ExitRunLoop)
	{
		SDL_Event event = {};
		int result = SDL_WaitEvent(&event);
		if (result == 1)
			DispatchEvent(event); // Silently ignore if it fails and pray it doesn't busy loop, because SDL and Linux utterly sucks!
	}
}

void SDL2DisplayWindow::ExitLoop()
{
	ExitRunLoop = true;
}

std::unordered_map<void *, std::function<void()>> SDL2DisplayWindow::Timers;
std::unordered_map<void *, void *> SDL2DisplayWindow::TimerHandles;
unsigned long SDL2DisplayWindow::TimerIDs = 0;
Uint32 TimerEventID = SDL_RegisterEvents(1);

Uint32 SDL2DisplayWindow::ExecTimer(Uint32 interval, void* execID)
{
	// cancel event and stop loop if function not found
	if (Timers.find(execID) == Timers.end())
		return 0;

	SDL_Event timerEvent;
	SDL_zero(timerEvent);

	timerEvent.user.type = TimerEventID;
	timerEvent.user.data1 = execID;

	SDL_PushEvent(&timerEvent);

	return interval;
}

void* SDL2DisplayWindow::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	// is this guard needed?
	// CheckInitSDL();

	void* execID = (void*)(uintptr_t)++TimerIDs;
	void* id = (void*)(uintptr_t)SDL_AddTimer(timeoutMilliseconds, SDL2DisplayWindow::ExecTimer, execID);

	if (!id) return id;

	Timers.insert({execID, onTimer});
	TimerHandles.insert({id, execID});

	return id;
}

void SDL2DisplayWindow::StopTimer(void* timerID)
{
	// is this guard needed?
	// CheckInitSDL();

	SDL_RemoveTimer((SDL_TimerID)(uintptr_t)timerID);

	auto execID = TimerHandles.find(timerID);
	if (execID == TimerHandles.end())
		return;

	Timers.erase(execID->second);
	TimerHandles.erase(timerID);
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
	// timers are created in a non-window context
	if (event.type == TimerEventID)
		return OnTimerEvent(event.user);

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

InputKey SDL2DisplayWindow::GetMouseButtonKey(const SDL_MouseButtonEvent& event)
{
	switch (event.button)
	{
		case SDL_BUTTON_LEFT: return InputKey::LeftMouse;
		case SDL_BUTTON_MIDDLE: return InputKey::MiddleMouse;
		case SDL_BUTTON_RIGHT: return InputKey::RightMouse;
		// case SDL_BUTTON_X1: return InputKey::XButton1;
		// case SDL_BUTTON_X2: return InputKey::XButton2;
		default: return InputKey::None;
	}
}

void SDL2DisplayWindow::OnMouseButtonUp(const SDL_MouseButtonEvent& event)
{
	InputKey key = GetMouseButtonKey(event);
	if (key != InputKey::None)
	{
		WindowHost->OnWindowMouseUp(GetMousePos(event), key);
	}
}

void SDL2DisplayWindow::OnMouseButtonDown(const SDL_MouseButtonEvent& event)
{
	InputKey key = GetMouseButtonKey(event);
	if (key != InputKey::None)
	{
		WindowHost->OnWindowMouseDown(GetMousePos(event), key);
	}
}

void SDL2DisplayWindow::OnMouseWheel(const SDL_MouseWheelEvent& event)
{
	int x = 0, y = 0;
	SDL_GetMouseState(&x, &y);
	double uiscale = GetDpiScale();
	Point mousepos(x / uiscale, y / uiscale);

	if (event.y > 0)
		WindowHost->OnWindowMouseWheel(mousepos, InputKey::MouseWheelUp);
	else if (event.y < 0)
		WindowHost->OnWindowMouseWheel(mousepos, InputKey::MouseWheelDown);
}

void SDL2DisplayWindow::OnMouseMotion(const SDL_MouseMotionEvent& event)
{
	if (CursorLocked)
	{
		WindowHost->OnWindowRawMouseMove(event.xrel, event.yrel);
	}
	else
	{
		WindowHost->OnWindowMouseMove(GetMousePos(event));
	}
}

void SDL2DisplayWindow::OnPaintEvent()
{
	WindowHost->OnWindowPaint();
}

void SDL2DisplayWindow::OnTimerEvent(const SDL_UserEvent& event)
{
	auto func = Timers.find(event.data1);

	// incase timer was cancelled before we get here
	if (func == Timers.end())
		return;

	func->second();
}

InputKey SDL2DisplayWindow::ScancodeToInputKey(SDL_Scancode keycode)
{
	switch (keycode)
	{
		case SDL_SCANCODE_BACKSPACE: return InputKey::Backspace;
		case SDL_SCANCODE_TAB: return InputKey::Tab;
		case SDL_SCANCODE_CLEAR: return InputKey::OEMClear;
		case SDL_SCANCODE_RETURN: return InputKey::Enter;
		case SDL_SCANCODE_MENU: return InputKey::Alt;
		case SDL_SCANCODE_PAUSE: return InputKey::Pause;
		case SDL_SCANCODE_ESCAPE: return InputKey::Escape;
		case SDL_SCANCODE_SPACE: return InputKey::Space;
		case SDL_SCANCODE_END: return InputKey::End;
		case SDL_SCANCODE_PAGEDOWN: return InputKey::PageDown;
		case SDL_SCANCODE_HOME: return InputKey::Home;
		case SDL_SCANCODE_PAGEUP: return InputKey::PageUp;
		case SDL_SCANCODE_LEFT: return InputKey::Left;
		case SDL_SCANCODE_UP: return InputKey::Up;
		case SDL_SCANCODE_RIGHT: return InputKey::Right;
		case SDL_SCANCODE_DOWN: return InputKey::Down;
		case SDL_SCANCODE_SELECT: return InputKey::Select;
		case SDL_SCANCODE_PRINTSCREEN: return InputKey::Print;
		case SDL_SCANCODE_EXECUTE: return InputKey::Execute;
		case SDL_SCANCODE_INSERT: return InputKey::Insert;
		case SDL_SCANCODE_DELETE: return InputKey::Delete;
		case SDL_SCANCODE_HELP: return InputKey::Help;
		case SDL_SCANCODE_0: return InputKey::_0;
		case SDL_SCANCODE_1: return InputKey::_1;
		case SDL_SCANCODE_2: return InputKey::_2;
		case SDL_SCANCODE_3: return InputKey::_3;
		case SDL_SCANCODE_4: return InputKey::_4;
		case SDL_SCANCODE_5: return InputKey::_5;
		case SDL_SCANCODE_6: return InputKey::_6;
		case SDL_SCANCODE_7: return InputKey::_7;
		case SDL_SCANCODE_8: return InputKey::_8;
		case SDL_SCANCODE_9: return InputKey::_9;
		case SDL_SCANCODE_A: return InputKey::A;
		case SDL_SCANCODE_B: return InputKey::B;
		case SDL_SCANCODE_C: return InputKey::C;
		case SDL_SCANCODE_D: return InputKey::D;
		case SDL_SCANCODE_E: return InputKey::E;
		case SDL_SCANCODE_F: return InputKey::F;
		case SDL_SCANCODE_G: return InputKey::G;
		case SDL_SCANCODE_H: return InputKey::H;
		case SDL_SCANCODE_I: return InputKey::I;
		case SDL_SCANCODE_J: return InputKey::J;
		case SDL_SCANCODE_K: return InputKey::K;
		case SDL_SCANCODE_L: return InputKey::L;
		case SDL_SCANCODE_M: return InputKey::M;
		case SDL_SCANCODE_N: return InputKey::N;
		case SDL_SCANCODE_O: return InputKey::O;
		case SDL_SCANCODE_P: return InputKey::P;
		case SDL_SCANCODE_Q: return InputKey::Q;
		case SDL_SCANCODE_R: return InputKey::R;
		case SDL_SCANCODE_S: return InputKey::S;
		case SDL_SCANCODE_T: return InputKey::T;
		case SDL_SCANCODE_U: return InputKey::U;
		case SDL_SCANCODE_V: return InputKey::V;
		case SDL_SCANCODE_W: return InputKey::W;
		case SDL_SCANCODE_X: return InputKey::X;
		case SDL_SCANCODE_Y: return InputKey::Y;
		case SDL_SCANCODE_Z: return InputKey::Z;
		case SDL_SCANCODE_KP_0: return InputKey::NumPad0;
		case SDL_SCANCODE_KP_1: return InputKey::NumPad1;
		case SDL_SCANCODE_KP_2: return InputKey::NumPad2;
		case SDL_SCANCODE_KP_3: return InputKey::NumPad3;
		case SDL_SCANCODE_KP_4: return InputKey::NumPad4;
		case SDL_SCANCODE_KP_5: return InputKey::NumPad5;
		case SDL_SCANCODE_KP_6: return InputKey::NumPad6;
		case SDL_SCANCODE_KP_7: return InputKey::NumPad7;
		case SDL_SCANCODE_KP_8: return InputKey::NumPad8;
		case SDL_SCANCODE_KP_9: return InputKey::NumPad9;
		// case SDL_SCANCODE_KP_ENTER: return InputKey::NumPadEnter;
		// case SDL_SCANCODE_KP_MULTIPLY: return InputKey::Multiply;
		// case SDL_SCANCODE_KP_PLUS: return InputKey::Add;
		case SDL_SCANCODE_SEPARATOR: return InputKey::Separator;
		// case SDL_SCANCODE_KP_MINUS: return InputKey::Subtract;
		case SDL_SCANCODE_KP_PERIOD: return InputKey::NumPadPeriod;
		// case SDL_SCANCODE_KP_DIVIDE: return InputKey::Divide;
		case SDL_SCANCODE_F1: return InputKey::F1;
		case SDL_SCANCODE_F2: return InputKey::F2;
		case SDL_SCANCODE_F3: return InputKey::F3;
		case SDL_SCANCODE_F4: return InputKey::F4;
		case SDL_SCANCODE_F5: return InputKey::F5;
		case SDL_SCANCODE_F6: return InputKey::F6;
		case SDL_SCANCODE_F7: return InputKey::F7;
		case SDL_SCANCODE_F8: return InputKey::F8;
		case SDL_SCANCODE_F9: return InputKey::F9;
		case SDL_SCANCODE_F10: return InputKey::F10;
		case SDL_SCANCODE_F11: return InputKey::F11;
		case SDL_SCANCODE_F12: return InputKey::F12;
		case SDL_SCANCODE_F13: return InputKey::F13;
		case SDL_SCANCODE_F14: return InputKey::F14;
		case SDL_SCANCODE_F15: return InputKey::F15;
		case SDL_SCANCODE_F16: return InputKey::F16;
		case SDL_SCANCODE_F17: return InputKey::F17;
		case SDL_SCANCODE_F18: return InputKey::F18;
		case SDL_SCANCODE_F19: return InputKey::F19;
		case SDL_SCANCODE_F20: return InputKey::F20;
		case SDL_SCANCODE_F21: return InputKey::F21;
		case SDL_SCANCODE_F22: return InputKey::F22;
		case SDL_SCANCODE_F23: return InputKey::F23;
		case SDL_SCANCODE_F24: return InputKey::F24;
		case SDL_SCANCODE_NUMLOCKCLEAR: return InputKey::NumLock;
		case SDL_SCANCODE_SCROLLLOCK: return InputKey::ScrollLock;
		case SDL_SCANCODE_LSHIFT: return InputKey::LShift;
		case SDL_SCANCODE_RSHIFT: return InputKey::RShift;
		case SDL_SCANCODE_LCTRL: return InputKey::LControl;
		case SDL_SCANCODE_RCTRL: return InputKey::RControl;
		case SDL_SCANCODE_GRAVE: return InputKey::Tilde;
		default: return InputKey::None;
	}
}

SDL_Scancode SDL2DisplayWindow::InputKeyToScancode(InputKey inputkey)
{
	switch (inputkey)
	{
		case InputKey::Backspace: return SDL_SCANCODE_BACKSPACE;
		case InputKey::Tab: return SDL_SCANCODE_TAB;
		case InputKey::OEMClear: return SDL_SCANCODE_CLEAR;
		case InputKey::Enter: return SDL_SCANCODE_RETURN;
		case InputKey::Alt: return SDL_SCANCODE_MENU;
		case InputKey::Pause: return SDL_SCANCODE_PAUSE;
		case InputKey::Escape: return SDL_SCANCODE_ESCAPE;
		case InputKey::Space: return SDL_SCANCODE_SPACE;
		case InputKey::End: return SDL_SCANCODE_END;
		case InputKey::Home: return SDL_SCANCODE_HOME;
		case InputKey::Left: return SDL_SCANCODE_LEFT;
		case InputKey::Up: return SDL_SCANCODE_UP;
		case InputKey::Right: return SDL_SCANCODE_RIGHT;
		case InputKey::Down: return SDL_SCANCODE_DOWN;
		case InputKey::Select: return SDL_SCANCODE_SELECT;
		case InputKey::Print: return SDL_SCANCODE_PRINTSCREEN;
		case InputKey::Execute: return SDL_SCANCODE_EXECUTE;
		case InputKey::Insert: return SDL_SCANCODE_INSERT;
		case InputKey::Delete: return SDL_SCANCODE_DELETE;
		case InputKey::Help: return SDL_SCANCODE_HELP;
		case InputKey::_0: return SDL_SCANCODE_0;
		case InputKey::_1: return SDL_SCANCODE_1;
		case InputKey::_2: return SDL_SCANCODE_2;
		case InputKey::_3: return SDL_SCANCODE_3;
		case InputKey::_4: return SDL_SCANCODE_4;
		case InputKey::_5: return SDL_SCANCODE_5;
		case InputKey::_6: return SDL_SCANCODE_6;
		case InputKey::_7: return SDL_SCANCODE_7;
		case InputKey::_8: return SDL_SCANCODE_8;
		case InputKey::_9: return SDL_SCANCODE_9;
		case InputKey::A: return SDL_SCANCODE_A;
		case InputKey::B: return SDL_SCANCODE_B;
		case InputKey::C: return SDL_SCANCODE_C;
		case InputKey::D: return SDL_SCANCODE_D;
		case InputKey::E: return SDL_SCANCODE_E;
		case InputKey::F: return SDL_SCANCODE_F;
		case InputKey::G: return SDL_SCANCODE_G;
		case InputKey::H: return SDL_SCANCODE_H;
		case InputKey::I: return SDL_SCANCODE_I;
		case InputKey::J: return SDL_SCANCODE_J;
		case InputKey::K: return SDL_SCANCODE_K;
		case InputKey::L: return SDL_SCANCODE_L;
		case InputKey::M: return SDL_SCANCODE_M;
		case InputKey::N: return SDL_SCANCODE_N;
		case InputKey::O: return SDL_SCANCODE_O;
		case InputKey::P: return SDL_SCANCODE_P;
		case InputKey::Q: return SDL_SCANCODE_Q;
		case InputKey::R: return SDL_SCANCODE_R;
		case InputKey::S: return SDL_SCANCODE_S;
		case InputKey::T: return SDL_SCANCODE_T;
		case InputKey::U: return SDL_SCANCODE_U;
		case InputKey::V: return SDL_SCANCODE_V;
		case InputKey::W: return SDL_SCANCODE_W;
		case InputKey::X: return SDL_SCANCODE_X;
		case InputKey::Y: return SDL_SCANCODE_Y;
		case InputKey::Z: return SDL_SCANCODE_Z;
		case InputKey::NumPad0: return SDL_SCANCODE_KP_0;
		case InputKey::NumPad1: return SDL_SCANCODE_KP_1;
		case InputKey::NumPad2: return SDL_SCANCODE_KP_2;
		case InputKey::NumPad3: return SDL_SCANCODE_KP_3;
		case InputKey::NumPad4: return SDL_SCANCODE_KP_4;
		case InputKey::NumPad5: return SDL_SCANCODE_KP_5;
		case InputKey::NumPad6: return SDL_SCANCODE_KP_6;
		case InputKey::NumPad7: return SDL_SCANCODE_KP_7;
		case InputKey::NumPad8: return SDL_SCANCODE_KP_8;
		case InputKey::NumPad9: return SDL_SCANCODE_KP_9;
		// case InputKey::NumPadEnter: return SDL_SCANCODE_KP_ENTER;
		// case InputKey::Multiply return SDL_SCANCODE_KP_MULTIPLY:;
		// case InputKey::Add: return SDL_SCANCODE_KP_PLUS;
		case InputKey::Separator: return SDL_SCANCODE_SEPARATOR;
		// case InputKey::Subtract: return SDL_SCANCODE_KP_MINUS;
		case InputKey::NumPadPeriod: return SDL_SCANCODE_KP_PERIOD;
		// case InputKey::Divide: return SDL_SCANCODE_KP_DIVIDE;
		case InputKey::F1: return SDL_SCANCODE_F1;
		case InputKey::F2: return SDL_SCANCODE_F2;
		case InputKey::F3: return SDL_SCANCODE_F3;
		case InputKey::F4: return SDL_SCANCODE_F4;
		case InputKey::F5: return SDL_SCANCODE_F5;
		case InputKey::F6: return SDL_SCANCODE_F6;
		case InputKey::F7: return SDL_SCANCODE_F7;
		case InputKey::F8: return SDL_SCANCODE_F8;
		case InputKey::F9: return SDL_SCANCODE_F9;
		case InputKey::F10: return SDL_SCANCODE_F10;
		case InputKey::F11: return SDL_SCANCODE_F11;
		case InputKey::F12: return SDL_SCANCODE_F12;
		case InputKey::F13: return SDL_SCANCODE_F13;
		case InputKey::F14: return SDL_SCANCODE_F14;
		case InputKey::F15: return SDL_SCANCODE_F15;
		case InputKey::F16: return SDL_SCANCODE_F16;
		case InputKey::F17: return SDL_SCANCODE_F17;
		case InputKey::F18: return SDL_SCANCODE_F18;
		case InputKey::F19: return SDL_SCANCODE_F19;
		case InputKey::F20: return SDL_SCANCODE_F20;
		case InputKey::F21: return SDL_SCANCODE_F21;
		case InputKey::F22: return SDL_SCANCODE_F22;
		case InputKey::F23: return SDL_SCANCODE_F23;
		case InputKey::F24: return SDL_SCANCODE_F24;
		case InputKey::NumLock: return SDL_SCANCODE_NUMLOCKCLEAR;
		case InputKey::ScrollLock: return SDL_SCANCODE_SCROLLLOCK;
		case InputKey::LShift: return SDL_SCANCODE_LSHIFT;
		case InputKey::RShift: return SDL_SCANCODE_RSHIFT;
		case InputKey::LControl: return SDL_SCANCODE_LCTRL;
		case InputKey::RControl: return SDL_SCANCODE_RCTRL;
		case InputKey::Tilde: return SDL_SCANCODE_GRAVE;
		default: return (SDL_Scancode)0;
	}	
}
