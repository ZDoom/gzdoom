
#include "sdl2_display_backend.h"
#include "sdl2_display_window.h"
#include <stdexcept>
#include <SDL2/SDL_video.h>
#ifndef WIN32
#include <dlfcn.h>
#endif

namespace X11DPI
{
	typedef struct _XDisplay Display;
	typedef Display *(*XOpenDisplayPtr)(const char*);
	typedef char *(*XGetDefaultPtr)(Display*, const char*, const char*);
	typedef int (*XCloseDisplayPtr)(Display*);
}

SDL2DisplayBackend::SDL2DisplayBackend()
{
	int result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	if (result != 0)
		throw std::runtime_error(std::string("Unable to initialize SDL:") + SDL_GetError());

	SDL2DisplayWindow::PaintEventNumber = SDL_RegisterEvents(1);

	// SDL2 doesn't have proper native hidpi support for Linux. Cheat a bit here by asking X11 ourselves.
#if !defined(WIN32) && !defined(__APPLE__)
	const char* driver = SDL_GetCurrentVideoDriver();
	if (driver && strcmp(driver, "x11") == 0)
	{
		void* module = dlopen("libX11.so", RTLD_NOW | RTLD_LOCAL);
		if (module)
		{
			using namespace X11DPI;
			auto XOpenDisplay = (XOpenDisplayPtr)dlsym(module, "XOpenDisplay");
			auto XGetDefault = (XGetDefaultPtr)dlsym(module, "XGetDefault");
			auto XCloseDisplay = (XCloseDisplayPtr)dlsym(module, "XCloseDisplay");
			if (XOpenDisplay && XGetDefault && XCloseDisplay)
			{
				auto display = XOpenDisplay(nullptr);
				if (display)
				{
					char* value = XGetDefault(display, "Xft", "dpi");
					if (value)
					{
						int dpi = std::atoi(value);
						if (dpi != 0)
						{
							UIScale = dpi / 96.0;
						}
					}
					XCloseDisplay(display);
				}
			}
		}
	}
#endif
}

std::unique_ptr<DisplayWindow> SDL2DisplayBackend::Create(DisplayWindowHost* windowHost, bool popupWindow, DisplayWindow* owner, RenderAPI renderAPI)
{
	return std::make_unique<SDL2DisplayWindow>(windowHost, popupWindow, static_cast<SDL2DisplayWindow*>(owner), renderAPI, UIScale);
}

void SDL2DisplayBackend::ProcessEvents()
{
	SDL2DisplayWindow::ProcessEvents();
}

void SDL2DisplayBackend::RunLoop()
{
	SDL2DisplayWindow::RunLoop();
}

void SDL2DisplayBackend::ExitLoop()
{
	SDL2DisplayWindow::ExitLoop();
}

Size SDL2DisplayBackend::GetScreenSize()
{
	SDL_Rect rect = {};
	int result = SDL_GetDisplayBounds(0, &rect);
	if (result != 0)
		throw std::runtime_error(std::string("Unable to get screen size:") + SDL_GetError());

	return Size(rect.w / UIScale, rect.h / UIScale);
}

void* SDL2DisplayBackend::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return SDL2DisplayWindow::StartTimer(timeoutMilliseconds, std::move(onTimer));
}

void SDL2DisplayBackend::StopTimer(void* timerID)
{
	SDL2DisplayWindow::StopTimer(timerID);
}
