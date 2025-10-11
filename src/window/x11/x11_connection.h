#pragma once

#include "window/window.h"
#include <string>
#include <map>
#include <memory>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysymdef.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XInput2.h>

class X11DisplayWindow;

class X11Timer
{
public:
	X11Timer(int timeoutMilliseconds, std::function<void()> onTimer, int64_t nextTime) : timeoutMilliseconds(timeoutMilliseconds), onTimer(onTimer), nextTime(nextTime) {}

	int timeoutMilliseconds = 0;
	std::function<void()> onTimer;
	int64_t nextTime = 0;
};

class X11Connection
{
public:
	X11Connection();
	~X11Connection();

	void ProcessEvents();
	void RunLoop();
	void ExitLoop();
	bool WaitForEvents(int timeout);

	void* StartTimer(int timeoutMilliseconds, std::function<void()> onTimer);
	void StopTimer(void* timerID);

	Size GetScreenSize();

	Atom GetAtom(const std::string& name);

	Display* display = nullptr;
	std::map<std::string, Atom> atoms;
	std::map<Window, X11DisplayWindow*> windows;
	std::map<StandardCursor, Cursor> standardCursors;
	bool ExitRunLoop = false;

	XIM xim = nullptr;
	bool XInput2Supported = false;
	int XInputOpcode = 0;
	int MasterPointerID = 0;

private:
	void DispatchEvent(XEvent* event);
	void CheckNeedsUpdate();
	void CheckTimers();
	int GetTimerTimeout();

	std::vector<std::shared_ptr<X11Timer>> timers;
};

X11Connection* GetX11Connection();
