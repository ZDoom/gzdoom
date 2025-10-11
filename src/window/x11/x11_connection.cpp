
#include "x11_connection.h"
#include "x11_display_window.h"
#include <stdexcept>
#include <chrono>

X11Connection* GetX11Connection()
{
	static X11Connection connection;
	return &connection;
}

X11Connection::X11Connection()
{
	// This is required by vulkan
	XInitThreads();

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

	// Look for XInput support
	int event = 0, error = 0;
	if (XQueryExtension(display, "XInputExtension", &XInputOpcode, &event, &error))
	{
		// We need XInput 2.0 support
		int major = 2, minor = 0;
		if (XIQueryVersion(display, &major, &minor) != BadRequest)
		{
			// And we need a master pointer ID
			int ndevices = 0;
			XIDeviceInfo *devices = XIQueryDevice(display, XIAllDevices, &ndevices);
			if (devices)
			{
				for (int i = 0; i < ndevices; i++)
				{
					if (devices[i].use == XIMasterPointer)
					{
						MasterPointerID = devices[i].deviceid;
						XInput2Supported = true;
						break;
					}
				}
				XIFreeDeviceInfo(devices);
			}
		}
	}

	// This XInput API is the gift that just keeps on giving. Instead of routing events to the
	// focus window it sends it to the root window. Thanks!
	if (XInput2Supported)
	{
		unsigned char mask[3] = { 0 };
		XISetMask(mask, XI_RawMotion);
		XIEventMask eventmask;
		eventmask.deviceid = MasterPointerID;
		eventmask.mask_len = sizeof(mask);
		eventmask.mask = mask;
		Window root = XRootWindow(display, XDefaultScreen(display));
		XISelectEvents(display, root, &eventmask, 1);
	}
}

X11Connection::~X11Connection()
{
	for (auto& it : standardCursors)
		XFreeCursor(display, it.second);
	if (xim)
		XCloseIM(xim);
	XCloseDisplay(display);
}

Atom X11Connection::GetAtom(const std::string& name)
{
	auto it = atoms.find(name);
	if (it != atoms.end())
		return it->second;
	
	Atom atom = XInternAtom(display, name.c_str(), True);
	atoms[name] = atom;
	return atom;
}

void X11Connection::ProcessEvents()
{
	CheckNeedsUpdate();
	while (XPending(display) > 0)
	{
		XEvent event = {};
		XNextEvent(display, &event);
		DispatchEvent(&event);
	}
	CheckTimers();
}

void X11Connection::RunLoop()
{
	X11Connection* connection = GetX11Connection();
	connection->ExitRunLoop = false;
	while (!connection->ExitRunLoop && !connection->windows.empty())
	{
		ProcessEvents();
		WaitForEvents(GetTimerTimeout());
	}
}

void X11Connection::ExitLoop()
{
	X11Connection* connection = GetX11Connection();
	connection->ExitRunLoop = true;
}

bool X11Connection::WaitForEvents(int timeout)
{
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

void X11Connection::CheckNeedsUpdate()
{
	for (auto& it : windows)
	{
		if (it.second->needsUpdate)
		{
			it.second->needsUpdate = false;
			it.second->windowHost->OnWindowPaint();
		}
	}
}

void X11Connection::DispatchEvent(XEvent* event)
{
	if (XInput2Supported)
	{
		// XInput sends all raw input to the root window. We want it routed to the focused window
		if (event->xcookie.type == GenericEvent &&
			event->xcookie.extension == XInputOpcode &&
			XGetEventData(display, &event->xcookie))
		{
			for (auto& it : windows)
			{
				X11DisplayWindow* window = it.second;
				if (window->OnXInputEvent(event))
					break;
			}
			XFreeEventData(display, &event->xcookie);
		}
	}

	auto it = windows.find(event->xany.window);
	if (it != windows.end())
	{
		X11DisplayWindow* window = it->second;
		window->OnEvent(event);
	}
}

Size X11Connection::GetScreenSize()
{
	int screen = XDefaultScreen(display);

	int disp_width_px = XDisplayWidth(display, screen);
	int disp_height_px = XDisplayHeight(display, screen);
	int disp_width_mm = XDisplayWidthMM(display, screen);
	double ppi = (disp_width_mm < 24) ? 96.0 : (25.4 * static_cast<double>(disp_width_px) / static_cast<double>(disp_width_mm));
	double dpiScale = ppi / 96.0;

	return Size(disp_width_px / dpiScale, disp_height_px / dpiScale);
}

static int64_t GetTimePoint()
{
	using namespace std::chrono;
	return (int64_t)(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void* X11Connection::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	timeoutMilliseconds = std::max(timeoutMilliseconds, 1);
	timers.push_back(std::make_shared<X11Timer>(timeoutMilliseconds, onTimer, GetTimePoint() + timeoutMilliseconds));
	return timers.back().get();
}

void X11Connection::StopTimer(void* timerID)
{
	for (auto it = timers.begin(); it != timers.end(); ++it)
	{
		if (it->get() == timerID)
		{
			timers.erase(it);
			return;
		}
	}
}

void X11Connection::CheckTimers()
{
	int64_t now = GetTimePoint();

	// The callback may stop timers. Iterators might invalidate.
	while (true)
	{
		std::shared_ptr<X11Timer> foundTimer;
		for (auto& timer : timers)
		{
			if (timer->nextTime < now)
			{
				foundTimer = timer;
				break;
			}
		}

		if (!foundTimer)
			break;

		// Not very precise, but these aren't high precision timers
		foundTimer->nextTime = now + foundTimer->timeoutMilliseconds;
		foundTimer->onTimer();
	}
}

int X11Connection::GetTimerTimeout()
{
	if (timers.empty())
		return 0;

	int64_t nextTime = timers.front()->nextTime;
	for (auto& timer : timers)
	{
		nextTime = std::min(nextTime, timer->nextTime);
	}

	int64_t now = GetTimePoint();
	return (int)std::max(nextTime - now, (int64_t)1);
}
