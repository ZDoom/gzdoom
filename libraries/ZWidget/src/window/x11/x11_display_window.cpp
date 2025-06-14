
#include "x11_display_window.h"
#include <stdexcept>
#include <vector>
#include <cmath>
#include <map>
#include <string>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>
#include <iostream>

class X11Connection
{
public:
	X11Connection()
	{
		// If we ever want to support windows on multiple threads:
		// XInitThreads();

		display = XOpenDisplay(nullptr);
		if (!display)
			throw std::runtime_error("Could not open X11 display");

		// Make auto-repeat keys detectable
		Bool supports_detectable_autorepeat = {};
		XkbSetDetectableAutoRepeat(display, True, &supports_detectable_autorepeat);

		// Loads the XMODIFIERS environment variable to see what IME to use
		XSetLocaleModifiers("");
		xim = XOpenIM(display, 0, 0, 0);
		if (!xim)
		{
			// fallback to internal input method
			XSetLocaleModifiers("@im=none");
			xim = XOpenIM(display, 0, 0, 0);
		}
	}

	~X11Connection()
	{
		for (auto& it : standardCursors)
			XFreeCursor(display, it.second);
		if (xim)
			XCloseIM(xim);
		XCloseDisplay(display);
	}

	Display* display = nullptr;
	std::map<std::string, Atom> atoms;
	std::map<Window, X11DisplayWindow*> windows;
	std::map<StandardCursor, Cursor> standardCursors;
	bool ExitRunLoop = false;

	XIM xim = nullptr;
};

static X11Connection* GetX11Connection()
{
	static X11Connection connection;
	return &connection;
}

static Atom GetAtom(const std::string& name)
{
	auto connection = GetX11Connection();
	auto it = connection->atoms.find(name);
	if (it != connection->atoms.end())
		return it->second;
	
	Atom atom = XInternAtom(connection->display, name.c_str(), True);
	connection->atoms[name] = atom;
	return atom;
}

X11DisplayWindow::X11DisplayWindow(DisplayWindowHost* windowHost, bool popupWindow, X11DisplayWindow* owner, RenderAPI renderAPI) : windowHost(windowHost), owner(owner)
{
	display = GetX11Connection()->display;

	screen = XDefaultScreen(display);
	depth = XDefaultDepth(display, screen);
	visual = XDefaultVisual(display, screen);
	colormap = XDefaultColormap(display, screen);

	int disp_width_px = XDisplayWidth(display, screen);
	int disp_height_px = XDisplayHeight(display, screen);
	int disp_width_mm = XDisplayWidthMM(display, screen);
	double ppi = (disp_width_mm < 24) ? 96.0 : (25.4 * static_cast<double>(disp_width_px) / static_cast<double>(disp_width_mm));
	dpiScale = std::round(ppi / 96.0 * 4.0) / 4.0; // 100%, 125%, 150%, 175%, 200%, etc.

	XSetWindowAttributes attributes = {};
	attributes.backing_store = Always;
	attributes.override_redirect = popupWindow ? True : False;
	attributes.save_under = popupWindow ? True : False;
	attributes.colormap = colormap;
	attributes.event_mask =
		KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
		EnterWindowMask | LeaveWindowMask | PointerMotionMask | KeymapStateMask |
		ExposureMask | StructureNotifyMask | FocusChangeMask | PropertyChangeMask;
		
	unsigned long mask = CWBackingStore | CWSaveUnder | CWEventMask | CWOverrideRedirect;

	window = XCreateWindow(display, XRootWindow(display, screen), 0, 0, 100, 100, 0, depth, InputOutput, visual, mask, &attributes);
	GetX11Connection()->windows[window] = this;

	if (owner)
	{
		XSetTransientForHint(display, window, owner->window);
	}

	// Tell window manager which process this window came from
	if (GetAtom("_NET_WM_PID") != None)
	{
		int32_t pid = getpid();
		if (pid != 0)
		{
			XChangeProperty(display, window, GetAtom("_NET_WM_PID"), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
		}
	}

	// Tell window manager which machine this window came from
	if (GetAtom("WM_CLIENT_MACHINE") != None)
	{
		std::vector<char> hostname(256);
		if (gethostname(hostname.data(), hostname.size()) >= 0)
		{
			hostname.push_back(0);
			XChangeProperty(display, window, GetAtom("WM_CLIENT_MACHINE"), XA_STRING, 8, PropModeReplace, (unsigned char *)hostname.data(), strlen(hostname.data()));
		}
	}

	// Tell window manager we want to listen to close events
	if (GetAtom("WM_DELETE_WINDOW") != None)
	{
		Atom protocol = GetAtom("WM_DELETE_WINDOW");
		XSetWMProtocols(display, window, &protocol, 1);
	}

	// Tell window manager what type of window we are
	if (GetAtom("_NET_WM_WINDOW_TYPE") != None)
	{
		Atom type = None;
		if (popupWindow)
		{
			type = GetAtom("_NET_WM_WINDOW_TYPE_DROPDOWN_MENU");
			if (type == None)
				type =  GetAtom("_NET_WM_WINDOW_TYPE_POPUP_MENU");
			if (type == None)
				type = GetAtom("_NET_WM_WINDOW_TYPE_COMBO");
		}
		if (type == None)
			type = GetAtom("_NET_WM_WINDOW_TYPE_NORMAL");

		if (type != None)
		{
			XChangeProperty(display, window, GetAtom("_NET_WM_WINDOW_TYPE"), XA_ATOM, 32, PropModeReplace, (unsigned char *)&type, 1);
		}
	}

	// Create input context
	if (GetX11Connection()->xim)
	{
		xic = XCreateIC(
			GetX11Connection()->xim,
			XNInputStyle,
			XIMPreeditNothing | XIMStatusNothing,
			XNClientWindow, window,
			XNFocusWindow, window,
			nullptr);
	}
}

X11DisplayWindow::~X11DisplayWindow()
{
	if (hidden_cursor != None)
	{
		XFreeCursor(display, hidden_cursor);
		XFreePixmap(display, cursor_bitmap);
	}

	DestroyBackbuffer();
	XDestroyWindow(display, window);
	GetX11Connection()->windows.erase(GetX11Connection()->windows.find(window));
}

void X11DisplayWindow::SetWindowTitle(const std::string& text)
{
	XSetStandardProperties(display, window, text.c_str(), text.c_str(), None, nullptr, 0, nullptr);
}

void X11DisplayWindow::SetWindowFrame(const Rect& box)
{
	// To do: this requires cooperation with the window manager

	SetClientFrame(box);
}	

void X11DisplayWindow::SetClientFrame(const Rect& box)
{
	double dpiscale = GetDpiScale();
	int x = (int)std::round(box.x * dpiscale);
	int y = (int)std::round(box.y * dpiscale);
	int width = (int)std::round(box.width * dpiscale);
	int height = (int)std::round(box.height * dpiscale);

	XWindowChanges changes = {};
	changes.x = x;
	changes.y = y;
	changes.width = width;
	changes.height = height;
	unsigned int mask = CWX | CWY | CWWidth | CWHeight;

	XConfigureWindow(display, window, mask, &changes);
}

void X11DisplayWindow::Show()
{
	if (!isMapped)
	{
		XMapRaised(display, window);
		isMapped = true;
	}
}

void X11DisplayWindow::ShowFullscreen()
{
	Show();

	if (GetAtom("_NET_WM_STATE") != None && GetAtom("_NET_WM_STATE_FULLSCREEN") != None)
	{
		Atom state = GetAtom("_NET_WM_STATE_FULLSCREEN");
		XChangeProperty(display, window, GetAtom("_NET_WM_STATE"), XA_ATOM, 32, PropModeReplace, (unsigned char *)&state, 1);
		isFullscreen = true;
	}
}

bool X11DisplayWindow::IsWindowFullscreen()
{
	return isFullscreen;
}

void X11DisplayWindow::ShowMaximized()
{
	Show();
}

void X11DisplayWindow::ShowMinimized()
{
	if (!isMinimized)
	{
		Show(); // To do: can this be avoided? WMHints has an initial state that can make it show minimized
		XIconifyWindow(display, window, screen);
		isMinimized = true;
	}
}

void X11DisplayWindow::ShowNormal()
{
	Show();
	isFullscreen = false;
}

void X11DisplayWindow::Hide()
{
	if (isMapped)
	{
		XUnmapWindow(display, window);
		isMapped = false;
	}
}

void X11DisplayWindow::Activate()
{
	XRaiseWindow(display, window);
}

void X11DisplayWindow::ShowCursor(bool enable)
{
	if (isCursorEnabled != enable)
	{
		isCursorEnabled = enable;
		UpdateCursor();
	}
}

void X11DisplayWindow::LockCursor()
{
	ShowCursor(false);
}

void X11DisplayWindow::UnlockCursor()
{
	ShowCursor(true);
}

void X11DisplayWindow::CaptureMouse()
{
	ShowCursor(false);
}

void X11DisplayWindow::ReleaseMouseCapture()
{
	ShowCursor(true);
}

void X11DisplayWindow::Update()
{
	needsUpdate = true;
}

bool X11DisplayWindow::GetKeyState(InputKey key)
{
	auto it = keyState.find(key);
	return it != keyState.end() ? it->second : false;
}

void X11DisplayWindow::SetCursor(StandardCursor newcursor)
{
	if (cursor != newcursor)
	{
		cursor = newcursor;
		UpdateCursor();
	}
}

void X11DisplayWindow::UpdateCursor()
{
	if (isCursorEnabled)
	{
		Cursor& x11cursor = GetX11Connection()->standardCursors[cursor];
		if (x11cursor == None)
		{
			unsigned int index = XC_left_ptr;
			switch (cursor)
			{
			default:
			case StandardCursor::arrow: index = XC_left_ptr; break;
			case StandardCursor::appstarting: index = XC_watch; break;
			case StandardCursor::cross: index = XC_cross; break;
			case StandardCursor::hand: index = XC_hand2; break;
			case StandardCursor::ibeam: index = XC_xterm; break;
			case StandardCursor::size_all: index = XC_fleur; break;
			case StandardCursor::size_ns: index = XC_double_arrow; break;
			case StandardCursor::size_we: index = XC_sb_h_double_arrow; break;
			case StandardCursor::uparrow: index = XC_sb_up_arrow; break;
			case StandardCursor::wait: index = XC_watch; break;
			case StandardCursor::no: index = XC_X_cursor; break;
			case StandardCursor::size_nesw: break; // To do: need to map this
			case StandardCursor::size_nwse: break;
			}
			x11cursor = XCreateFontCursor(display, index);
		}
		XDefineCursor(display, window, x11cursor);
	}
	else
	{
		if (hidden_cursor == None)
		{
			char data[64] = {};
			XColor black_color = {};
			cursor_bitmap = XCreateBitmapFromData(display, window, data, 8, 8);
			hidden_cursor = XCreatePixmapCursor(display, cursor_bitmap, cursor_bitmap, &black_color, &black_color, 0, 0);
		}
		XDefineCursor(display, window, hidden_cursor);
	}
}

Rect X11DisplayWindow::GetWindowFrame() const
{
	// To do: this needs to include the window manager frame

	double dpiscale = GetDpiScale();

	Window root = {};
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int borderwidth = 0;
	unsigned int depth = 0;
	Status status = XGetGeometry(display, window, &root, &x, &y, &width, &height, &borderwidth, &depth);

	return Rect::xywh(x / dpiscale, y / dpiscale, width / dpiscale, height / dpiscale);
}

Size X11DisplayWindow::GetClientSize() const
{
	double dpiscale = GetDpiScale();

	Window root = {};
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int borderwidth = 0;
	unsigned int depth = 0;
	Status status = XGetGeometry(display, window, &root, &x, &y, &width, &height, &borderwidth, &depth);

	return Size(width / dpiscale, height / dpiscale);
}

int X11DisplayWindow::GetPixelWidth() const
{
	Window root = {};
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int borderwidth = 0;
	unsigned int depth = 0;
	Status status = XGetGeometry(display, window, &root, &x, &y, &width, &height, &borderwidth, &depth);
	return width;
}

int X11DisplayWindow::GetPixelHeight() const
{
	Window root = {};
	int x = 0;
	int y = 0;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int borderwidth = 0;
	unsigned int depth = 0;
	Status status = XGetGeometry(display, window, &root, &x, &y, &width, &height, &borderwidth, &depth);
	return height;
}

double X11DisplayWindow::GetDpiScale() const
{
	return dpiScale;
}

void X11DisplayWindow::CreateBackbuffer(int width, int height)
{
	backbuffer.pixels = malloc(width * height * sizeof(uint32_t));
	backbuffer.image = XCreateImage(display, DefaultVisual(display, screen), depth, ZPixmap, 0, (char*)backbuffer.pixels, width, height, 32, 0);
	backbuffer.pixmap = XCreatePixmap(display, window, width, height, depth);
	backbuffer.width = width;
	backbuffer.height = height;
}

void X11DisplayWindow::DestroyBackbuffer()
{
	if (backbuffer.width > 0 && backbuffer.height > 0)
	{
		XDestroyImage(backbuffer.image);
		XFreePixmap(display, backbuffer.pixmap);
		backbuffer.width = 0;
		backbuffer.height = 0;
		backbuffer.pixmap = None;
		backbuffer.image = nullptr;
		backbuffer.pixels = nullptr;
	}
}

void X11DisplayWindow::PresentBitmap(int width, int height, const uint32_t* pixels)
{
	if (backbuffer.width != width || backbuffer.height != height)
	{
		DestroyBackbuffer();
		if (width > 0 && height > 0)
			CreateBackbuffer(width, height);
	}

	if (backbuffer.width == width && backbuffer.height == height)
	{
		memcpy(backbuffer.pixels, pixels, width * height * sizeof(uint32_t));
		GC gc = XDefaultGC(display, screen);
		XPutImage(display, backbuffer.pixmap, gc, backbuffer.image, 0, 0, 0, 0, width, height);
		XCopyArea(display, backbuffer.pixmap, window, gc, 0, 0, width, height, BlackPixel(display, screen), WhitePixel(display, screen));
	}
}

void X11DisplayWindow::SetBorderColor(uint32_t bgra8)
{
}

void X11DisplayWindow::SetCaptionColor(uint32_t bgra8)
{
}

void X11DisplayWindow::SetCaptionTextColor(uint32_t bgra8)
{
}

std::vector<uint8_t> X11DisplayWindow::GetWindowProperty(Atom property, Atom &actual_type, int &actual_format, unsigned long &item_count)
{
	long read_bytes = 0;
	Atom _actual_type = actual_type;
	int  _actual_format = actual_format;
	unsigned long _item_count = item_count;
	unsigned long bytes_remaining = 0;
	unsigned char *read_data = nullptr;
	do
	{
		int result = XGetWindowProperty(
			display, window, property, 0ul, read_bytes,
			False, AnyPropertyType, &actual_type, &actual_format,
			&_item_count, &bytes_remaining, &read_data);
		if (result != Success)
		{
			actual_type = None;
			actual_format = 0;
			item_count = 0;
			return {};
		}
	} while (bytes_remaining > 0);

	item_count = _item_count;
	if (!read_data)
		return {};
	std::vector<uint8_t> buffer(read_data, read_data + read_bytes);
	XFree(read_data);
	return buffer;
}

std::string X11DisplayWindow::GetClipboardText()
{
	Atom clipboard = GetAtom("CLIPBOARD");
	if (clipboard == None)
		return {};

	XConvertSelection(display, clipboard, XA_STRING, clipboard, window, CurrentTime);
	XFlush(display);

	// Wait 500 ms for a response
	XEvent event = {};
	while (true)
	{
		if (XCheckTypedWindowEvent(display, window, SelectionNotify, &event))
			break;
		if (!WaitForEvents(500))
			return {};
	}

	Atom type = None;
	int format = 0;
	unsigned long count = 0;
	std::vector<uint8_t> data = GetWindowProperty(clipboard, type, format, count);
	if (type != XA_STRING || format != 8 || count <= 0 || data.empty())
		return {};

	data.push_back(0);
	return (char*)data.data();
}

void X11DisplayWindow::SetClipboardText(const std::string& text)
{
	clipboardText = text;

	Atom clipboard = GetAtom("CLIPBOARD");
	if (clipboard == None)
		return;

	XSetSelectionOwner(display, XA_PRIMARY, window, CurrentTime);
	XSetSelectionOwner(display, clipboard, window, CurrentTime);
}

Point X11DisplayWindow::MapFromGlobal(const Point& pos) const
{
	double dpiscale = GetDpiScale();
	Window root = XRootWindow(display, screen);
	Window child = {};
	int srcx = (int)std::round(pos.x * dpiscale);
	int srcy = (int)std::round(pos.y * dpiscale);
	int destx = 0;
	int desty = 0;
	Bool result = XTranslateCoordinates(display, root, window, srcx, srcy, &destx, &desty, &child);
	return Point(destx / dpiscale, desty / dpiscale);
}

Point X11DisplayWindow::MapToGlobal(const Point& pos) const
{
	double dpiscale = GetDpiScale();
	Window root = XRootWindow(display, screen);
	Window child = {};
	int srcx = (int)std::round(pos.x * dpiscale);
	int srcy = (int)std::round(pos.y * dpiscale);
	int destx = 0;
	int desty = 0;
	Bool result = XTranslateCoordinates(display, window, root, srcx, srcy, &destx, &desty, &child);
	return Point(destx / dpiscale, desty / dpiscale);
}

void* X11DisplayWindow::GetNativeHandle()
{
	return reinterpret_cast<void*>(window);
}

bool X11DisplayWindow::WaitForEvents(int timeout)
{
	Display* display = GetX11Connection()->display;
	int fd = XConnectionNumber(display);

	struct timeval tv;
	if (timeout > 0)
	{
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) / 1000;
	}

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	int result = select(fd + 1, &rfds, nullptr, nullptr, timeout >= 0 ? &tv : nullptr);
	return result > 0 && FD_ISSET(fd, &rfds);
}

void X11DisplayWindow::CheckNeedsUpdate()
{
	for (auto& it : GetX11Connection()->windows)
	{
		if (it.second->needsUpdate)
		{
			it.second->needsUpdate = false;
			it.second->windowHost->OnWindowPaint();
		}
	}
}

void X11DisplayWindow::ProcessEvents()
{
	CheckNeedsUpdate();
	Display* display = GetX11Connection()->display;
	while (XPending(display) > 0)
	{
		XEvent event = {};
		XNextEvent(display, &event);
		DispatchEvent(&event);
	}
}

void X11DisplayWindow::RunLoop()
{
	X11Connection* connection = GetX11Connection();
	connection->ExitRunLoop = false;
	while (!connection->ExitRunLoop && !connection->windows.empty())
	{
		CheckNeedsUpdate();
		XEvent event = {};
		XNextEvent(connection->display, &event);
		DispatchEvent(&event);
	}
}

void X11DisplayWindow::ExitLoop()
{
	X11Connection* connection = GetX11Connection();
	connection->ExitRunLoop = true;
}

void X11DisplayWindow::DispatchEvent(XEvent* event)
{
	X11Connection* connection = GetX11Connection();
	auto it = connection->windows.find(event->xany.window);
	if (it != connection->windows.end())
	{
		X11DisplayWindow* window = it->second;
		window->OnEvent(event);
	}
}

void X11DisplayWindow::OnEvent(XEvent* event)
{
	if (event->type == ConfigureNotify)
		OnConfigureNotify(event);
	else if (event->type == ClientMessage)
		OnClientMessage(event);
	else if (event->type == Expose)
		OnExpose(event);
	else if (event->type == FocusIn)
		OnFocusIn(event);
	else if (event->type == FocusOut)
		OnFocusOut(event);
	else if (event->type == PropertyNotify)
		OnPropertyNotify(event);
	else if (event->type == KeyPress)
		OnKeyPress(event);
	else if (event->type == KeyRelease)
		OnKeyRelease(event);
	else if (event->type == ButtonPress)
		OnButtonPress(event);
	else if (event->type == ButtonRelease)
		OnButtonRelease(event);
	else if (event->type == MotionNotify)
		OnMotionNotify(event);
	else if (event->type == LeaveNotify)
		OnLeaveNotify(event);
	else if (event->type == SelectionClear)
		OnSelectionClear(event);
	else if (event->type == SelectionNotify)
		OnSelectionNotify(event);
	else if (event->type == SelectionRequest)
		OnSelectionRequest(event);
}

void X11DisplayWindow::OnConfigureNotify(XEvent* event)
{
	ClientSizeX = event->xconfigure.width;
	ClientSizeY = event->xconfigure.height;
	windowHost->OnWindowGeometryChanged();
}

void X11DisplayWindow::OnClientMessage(XEvent* event)
{
	Atom protocolsAtom = GetAtom("WM_PROTOCOLS");
	if (protocolsAtom != None && event->xclient.message_type == protocolsAtom)
	{
		Atom deleteAtom = GetAtom("WM_DELETE_WINDOW");
		Atom pingAtom = GetAtom("_NET_WM_PING");

		Atom protocol = event->xclient.data.l[0];
		if (deleteAtom != None && protocol == deleteAtom)
		{
			windowHost->OnWindowClose();
		}
		else if (pingAtom != None && protocol == pingAtom)
		{
			XSendEvent(display, RootWindow(display, screen), False, SubstructureNotifyMask | SubstructureRedirectMask, event);
		}
	}
}

void X11DisplayWindow::OnExpose(XEvent* event)
{
	windowHost->OnWindowPaint();
}

void X11DisplayWindow::OnFocusIn(XEvent* event)
{
	if (xic)
		XSetICFocus(xic);

	windowHost->OnWindowActivated();
}

void X11DisplayWindow::OnFocusOut(XEvent* event)
{
	windowHost->OnWindowDeactivated();
}

void X11DisplayWindow::OnPropertyNotify(XEvent* event)
{
	// Sent when window is minimized, maximized, etc.
}

InputKey X11DisplayWindow::GetInputKey(XEvent* event)
{
	if (event->type == KeyPress || event->type == KeyRelease)
	{
		KeySym keysymbol = XkbKeycodeToKeysym(display, event->xkey.keycode, 0, 0);
		switch (keysymbol)
		{
		case XK_BackSpace: return InputKey::Backspace;
		case XK_Tab: return InputKey::Tab;
		case XK_Return: return InputKey::Enter;
		// To do: should we merge them or not? Windows merges them
		case XK_Shift_L: return InputKey::Shift; // InputKey::LShift
		case XK_Shift_R: return InputKey::Shift; // InputKey::RShift
		case XK_Control_L: return InputKey::Ctrl; // InputKey::LControl
		case XK_Control_R: return InputKey::Ctrl; // InputKey::RControl
		case XK_Meta_L: return InputKey::Alt;
		case XK_Meta_R: return InputKey::Alt;
		case XK_Pause: return InputKey::Pause;
		case XK_Caps_Lock: return InputKey::CapsLock;
		case XK_Escape: return InputKey::Escape;
		case XK_space: return InputKey::Space;
		case XK_Page_Up: return InputKey::PageUp;
		case XK_Page_Down: return InputKey::PageDown;
		case XK_End: return InputKey::End;
		case XK_Home: return InputKey::Home;
		case XK_Left: return InputKey::Left;
		case XK_Up: return InputKey::Up;
		case XK_Right: return InputKey::Right;
		case XK_Down: return InputKey::Down;
		case XK_Print: return InputKey::Print;
		case XK_Execute: return InputKey::Execute;
		// case XK_Print_Screen: return InputKey::PrintScrn;
		case XK_Insert: return InputKey::Insert;
		case XK_Delete: return InputKey::Delete;
		case XK_Help: return InputKey::Help;
		case XK_0: return InputKey::_0;
		case XK_1: return InputKey::_1;
		case XK_2: return InputKey::_2;
		case XK_3: return InputKey::_3;
		case XK_4: return InputKey::_4;
		case XK_5: return InputKey::_5;
		case XK_6: return InputKey::_6;
		case XK_7: return InputKey::_7;
		case XK_8: return InputKey::_8;
		case XK_9: return InputKey::_9;
		case XK_A: case XK_a:  return InputKey::A;
		case XK_B: case XK_b:  return InputKey::B;
		case XK_C: case XK_c:  return InputKey::C;
		case XK_D: case XK_d:  return InputKey::D;
		case XK_E: case XK_e:  return InputKey::E;
		case XK_F: case XK_f:  return InputKey::F;
		case XK_G: case XK_g:  return InputKey::G;
		case XK_H: case XK_h:  return InputKey::H;
		case XK_I: case XK_i:  return InputKey::I;
		case XK_J: case XK_j:  return InputKey::J;
		case XK_K: case XK_k:  return InputKey::K;
		case XK_L: case XK_l:  return InputKey::L;
		case XK_M: case XK_m:  return InputKey::M;
		case XK_N: case XK_n:  return InputKey::N;
		case XK_O: case XK_o:  return InputKey::O;
		case XK_P: case XK_p: return InputKey::P;
		case XK_Q: case XK_q: return InputKey::Q;
		case XK_R: case XK_r: return InputKey::R;
		case XK_S: case XK_s: return InputKey::S;
		case XK_T: case XK_t: return InputKey::T;
		case XK_U: case XK_u: return InputKey::U;
		case XK_V: case XK_v: return InputKey::V;
		case XK_W: case XK_w: return InputKey::W;
		case XK_X: case XK_x: return InputKey::X;
		case XK_Y: case XK_y: return InputKey::Y;
		case XK_Z: case XK_z: return InputKey::Z;
		case XK_KP_0: return InputKey::NumPad0;
		case XK_KP_1: return InputKey::NumPad1;
		case XK_KP_2: return InputKey::NumPad2;
		case XK_KP_3: return InputKey::NumPad3;
		case XK_KP_4: return InputKey::NumPad4;
		case XK_KP_5: return InputKey::NumPad5;
		case XK_KP_6: return InputKey::NumPad6;
		case XK_KP_7: return InputKey::NumPad7;
		case XK_KP_8: return InputKey::NumPad8;
		case XK_KP_9: return InputKey::NumPad9;
		case XK_KP_Multiply: return InputKey::GreyStar;
		case XK_KP_Add: return InputKey::GreyPlus;
		case XK_KP_Separator: return InputKey::Separator;
		case XK_KP_Subtract: return InputKey::GreyMinus;
		case XK_KP_Decimal: return InputKey::NumPadPeriod;
		case XK_KP_Divide: return InputKey::GreySlash;
		case XK_F1: return InputKey::F1;
		case XK_F2: return InputKey::F2;
		case XK_F3: return InputKey::F3;
		case XK_F4: return InputKey::F4;
		case XK_F5: return InputKey::F5;
		case XK_F6: return InputKey::F6;
		case XK_F7: return InputKey::F7;
		case XK_F8: return InputKey::F8;
		case XK_F9: return InputKey::F9;
		case XK_F10: return InputKey::F10;
		case XK_F11: return InputKey::F11;
		case XK_F12: return InputKey::F12;
		case XK_F13: return InputKey::F13;
		case XK_F14: return InputKey::F14;
		case XK_F15: return InputKey::F15;
		case XK_F16: return InputKey::F16;
		case XK_F17: return InputKey::F17;
		case XK_F18: return InputKey::F18;
		case XK_F19: return InputKey::F19;
		case XK_F20: return InputKey::F20;
		case XK_F21: return InputKey::F21;
		case XK_F22: return InputKey::F22;
		case XK_F23: return InputKey::F23;
		case XK_F24: return InputKey::F24;
		case XK_Num_Lock: return InputKey::NumLock;
		case XK_Scroll_Lock: return InputKey::ScrollLock;
		case XK_semicolon: return InputKey::Semicolon;
		case XK_equal: return InputKey::Equals;
		case XK_comma: return InputKey::Comma;
		case XK_minus: return InputKey::Minus;
		case XK_period: return InputKey::Period;
		case XK_slash: return InputKey::Slash;
		case XK_dead_tilde: return InputKey::Tilde;
		case XK_bracketleft: return InputKey::LeftBracket;
		case XK_backslash: return InputKey::Backslash;
		case XK_bracketright: return InputKey::RightBracket;
		case XK_apostrophe: return InputKey::SingleQuote;
		default: return (InputKey)(((uint32_t)keysymbol) << 8);
		}
	}
	else if (event->type == ButtonPress || event->type == ButtonRelease)
	{
		switch (event->xbutton.button)
		{
		case 1: return InputKey::LeftMouse;
		case 2: return InputKey::MiddleMouse;
		case 3: return InputKey::RightMouse;
		case 4: return InputKey::MouseWheelUp;
		case 5: return InputKey::MouseWheelDown;
		// case 6: return InputKey::XButton1;
		// case 7: return InputKey::XButton2;
		default: break;
		}
	}
	return {};
}

Point X11DisplayWindow::GetMousePos(XEvent* event)
{
	double dpiScale = GetDpiScale();
	int x = event->xbutton.x;
	int y = event->xbutton.y;
	return Point(x / dpiScale, y / dpiScale);
}

void X11DisplayWindow::OnKeyPress(XEvent* event)
{
	// If we ever want to track keypress repeat:
	// char keyboard_state[32];
	// XQueryKeymap(display, keyboard_state);
	// unsigned int keycode = event->xkey.keycode;
	// bool isrepeat = event->type == KeyPress && keyboard_state[keycode / 8] & (1 << keycode % 8);

	InputKey key = GetInputKey(event);
	keyState[key] = true;
	windowHost->OnWindowKeyDown(key);

	std::string text;
	if (xic) // utf-8 text input
	{
		Status status = {};
		KeySym keysym = NoSymbol;
		char buffer[32] = {};
		Xutf8LookupString(xic, &event->xkey, buffer, sizeof(buffer) - 1, &keysym, &status);
		if (status == XLookupChars || status == XLookupBoth)
			text = buffer;
	}
	else // latin-1 input fallback
	{
		const int buff_size = 16;
		char buff[buff_size];
		int result = XLookupString(&event->xkey, buff, buff_size - 1, nullptr, nullptr);
		if (result < 0) result = 0;
		if (result > (buff_size - 1)) result = buff_size - 1;
		buff[result] = 0;
		text = std::string(buff, result);

		// Lazy way to convert to utf-8
		for (char& c : text)
		{
			if (c < 0)
				c = '?';
		}
	}

	if (!text.empty())
		windowHost->OnWindowKeyChar(std::move(text));
}

void X11DisplayWindow::OnKeyRelease(XEvent* event)
{
	InputKey key = GetInputKey(event);
	keyState[key] = false;
	windowHost->OnWindowKeyUp(key);
}

void X11DisplayWindow::OnButtonPress(XEvent* event)
{
	InputKey key = GetInputKey(event);
	keyState[key] = true;
	windowHost->OnWindowMouseDown(GetMousePos(event), key);
	// if (lastClickWithin400ms)
	//	windowHost->OnWindowMouseDoubleclick(GetMousePos(event), InputKey::LeftMouse);
}

void X11DisplayWindow::OnButtonRelease(XEvent* event)
{
	InputKey key = GetInputKey(event);
	keyState[key] = false;
	windowHost->OnWindowMouseUp(GetMousePos(event), key);
}

void X11DisplayWindow::OnMotionNotify(XEvent* event)
{
	double dpiScale = GetDpiScale();
	int x = event->xmotion.x;
	int y = event->xmotion.y;
	if (isCursorEnabled)
	{
		windowHost->OnWindowMouseMove(Point(x / dpiScale, y / dpiScale));
	}
	else
	{
		MouseX = ClientSizeX / 2;
		MouseY = ClientSizeY / 2;

		if (MouseX != -1 && MouseY != -1)
		{
			windowHost->OnWindowRawMouseMove(x - MouseX, y - MouseY);
		}

		// Warp pointer to the center of the window
		XWarpPointer(display, window, window, 0, 0, ClientSizeX, ClientSizeY, ClientSizeX / 2, ClientSizeY / 2);
	}

}

void X11DisplayWindow::OnLeaveNotify(XEvent* event)
{
	windowHost->OnWindowMouseLeave();
}

void X11DisplayWindow::OnSelectionClear(XEvent* event)
{
	clipboardText.clear();
}

void X11DisplayWindow::OnSelectionNotify(XEvent* event)
{
	// This is handled in GetClipboardText
}

void X11DisplayWindow::OnSelectionRequest(XEvent* event)
{
	Atom requestor = event->xselectionrequest.requestor;
	if (requestor == window)
		return;

	Atom targetsAtom = GetAtom("TARGETS");
	Atom multipleAtom = GetAtom("MULTIPLE");

	struct Request { Window target; Atom property; };
	std::vector<Request> requests;

	if (event->xselectionrequest.target == multipleAtom)
	{
		Atom actualType = None;
		int actualFormat = 0;
		unsigned long itemCount = 0;
		std::vector<uint8_t> data = GetWindowProperty(requestor, actualType, actualFormat, itemCount);
		if (data.size() < itemCount * sizeof(Atom))
			return;

		Atom* atoms = (Atom*)data.data();
		for (unsigned long i = 0; i + 1 < itemCount; i += 2)
		{
			requests.push_back({ atoms[i], atoms[i + 1]});
		}
	}
	else
	{
		requests.push_back({ event->xselectionrequest.target, event->xselectionrequest.property });
	}

	for (const Request& request : requests)
	{
		Window xtarget = request.target;
		Atom xproperty = request.property;

		XEvent response = {};
		response.xselection.type = SelectionNotify;
		response.xselection.display = event->xselectionrequest.display;
		response.xselection.requestor = event->xselectionrequest.requestor;
		response.xselection.selection = event->xselectionrequest.selection;
		response.xselection.target = event->xselectionrequest.target;
		response.xselection.property = xproperty;
		response.xselection.time = event->xselectionrequest.time;

		if (xtarget == targetsAtom)
		{
			Atom newTargets = XA_STRING;
			XChangeProperty(display, requestor, xproperty, targetsAtom, 32, PropModeReplace, (unsigned char *)&newTargets, 1);
		}
		else if (xtarget == XA_STRING)
		{
			XChangeProperty(display, requestor, xproperty, xtarget, 8, PropModeReplace, (const unsigned char*)clipboardText.c_str(), clipboardText.size());
		}
		else
		{
			response.xselection.property = None; // Is this correct?
		}

		XSendEvent(display, requestor, False, 0, &response);
	}
}

Size X11DisplayWindow::GetScreenSize()
{
	X11Connection* connection = GetX11Connection();
	Display* display = connection->display;
	int screen = XDefaultScreen(display);

	int disp_width_px = XDisplayWidth(display, screen);
	int disp_height_px = XDisplayHeight(display, screen);
	int disp_width_mm = XDisplayWidthMM(display, screen);
	double ppi = (disp_width_mm < 24) ? 96.0 : (25.4 * static_cast<double>(disp_width_px) / static_cast<double>(disp_width_mm));
	double dpiScale = ppi / 96.0;

	return Size(disp_width_px / dpiScale, disp_height_px / dpiScale);
}

void* X11DisplayWindow::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return nullptr;
}

void X11DisplayWindow::StopTimer(void* timerID)
{
}

// This is to avoid needing all the Vulkan headers and the volk binding library just for this:
#ifndef VK_VERSION_1_0

#define VKAPI_CALL
#define VKAPI_PTR VKAPI_CALL

typedef uint32_t VkFlags;
typedef enum VkStructureType { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR = 1000004000, VK_OBJECT_TYPE_MAX_ENUM = 0x7FFFFFFF } VkStructureType;
typedef enum VkResult { VK_SUCCESS = 0, VK_RESULT_MAX_ENUM = 0x7FFFFFFF } VkResult;
typedef struct VkAllocationCallbacks VkAllocationCallbacks;

typedef void (VKAPI_PTR* PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction(VKAPI_PTR* PFN_vkGetInstanceProcAddr)(VkInstance instance, const char* pName);

#ifndef VK_KHR_xlib_surface

typedef VkFlags VkXlibSurfaceCreateFlagsKHR;
typedef struct VkXlibSurfaceCreateInfoKHR
{
    VkStructureType                sType;
    const void*                    pNext;
    VkXlibSurfaceCreateFlagsKHR    flags;
    Display*                       dpy;
    Window                         window;
} VkXlibSurfaceCreateInfoKHR;

typedef VkResult(VKAPI_PTR* PFN_vkCreateXlibSurfaceKHR)(VkInstance instance, const VkXlibSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);

#endif
#endif

class ZWidgetX11VulkanLoader
{
public:
	ZWidgetX11VulkanLoader()
	{
#if defined(__APPLE__)
		module = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
		if (!module)
			module = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
		if (!module)
			module = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
#else
		module = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
		if (!module)
			module = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
#endif

		if (!module)
			throw std::runtime_error("Could not load vulkan");

		vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(module, "vkGetInstanceProcAddr");
		if (!vkGetInstanceProcAddr)
		{
			dlclose(module);
			throw std::runtime_error("vkGetInstanceProcAddr not found");
		}
	}

	~ZWidgetX11VulkanLoader()
	{
		dlclose(module);
	}

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
	void* module = nullptr;
};

VkSurfaceKHR X11DisplayWindow::CreateVulkanSurface(VkInstance instance)
{
	static ZWidgetX11VulkanLoader loader;

	auto vkCreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)loader.vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
	if (!vkCreateXlibSurfaceKHR)
		throw std::runtime_error("Could not create vulkan surface");

	VkXlibSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };
	createInfo.dpy = display;
	createInfo.window = window;

	VkSurfaceKHR surface = {};
	VkResult result = vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Could not create vulkan surface");
	return surface;
}

std::vector<std::string> X11DisplayWindow::GetVulkanInstanceExtensions()
{
	return { "VK_KHR_surface", "VK_KHR_xlib_surface" };
}
