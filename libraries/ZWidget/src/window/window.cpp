
#include "window/window.h"
#include <stdexcept>

#ifdef _WIN32

#include "win32/win32window.h"

std::unique_ptr<DisplayWindow> DisplayWindow::Create(DisplayWindowHost* windowHost)
{
	return std::make_unique<Win32Window>(windowHost);
}

void DisplayWindow::ProcessEvents()
{
	Win32Window::ProcessEvents();
}

void DisplayWindow::RunLoop()
{
	Win32Window::RunLoop();
}

void DisplayWindow::ExitLoop()
{
	Win32Window::ExitLoop();
}

Size DisplayWindow::GetScreenSize()
{
	return Win32Window::GetScreenSize();
}

void* DisplayWindow::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return Win32Window::StartTimer(timeoutMilliseconds, std::move(onTimer));
}

void DisplayWindow::StopTimer(void* timerID)
{
	Win32Window::StopTimer(timerID);
}

#elif defined(__APPLE__)

std::unique_ptr<DisplayWindow> DisplayWindow::Create(DisplayWindowHost* windowHost)
{
	throw std::runtime_error("DisplayWindow::Create not implemented");
}

void DisplayWindow::ProcessEvents()
{
	throw std::runtime_error("DisplayWindow::ProcessEvents not implemented");
}

void DisplayWindow::RunLoop()
{
	throw std::runtime_error("DisplayWindow::RunLoop not implemented");
}

void DisplayWindow::ExitLoop()
{
	throw std::runtime_error("DisplayWindow::ExitLoop not implemented");
}

Size DisplayWindow::GetScreenSize()
{
	throw std::runtime_error("DisplayWindow::GetScreenSize not implemented");
}

void* DisplayWindow::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	throw std::runtime_error("DisplayWindow::StartTimer not implemented");
}

void DisplayWindow::StopTimer(void* timerID)
{
	throw std::runtime_error("DisplayWindow::StopTimer not implemented");
}

#else

#include "sdl2/sdl2displaywindow.h"

std::unique_ptr<DisplayWindow> DisplayWindow::Create(DisplayWindowHost* windowHost)
{
	return std::make_unique<SDL2DisplayWindow>(windowHost);
}

void DisplayWindow::ProcessEvents()
{
	SDL2DisplayWindow::ProcessEvents();
}

void DisplayWindow::RunLoop()
{
	SDL2DisplayWindow::RunLoop();
}

void DisplayWindow::ExitLoop()
{
	SDL2DisplayWindow::ExitLoop();
}

Size DisplayWindow::GetScreenSize()
{
	return SDL2DisplayWindow::GetScreenSize();
}

void* DisplayWindow::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return SDL2DisplayWindow::StartTimer(timeoutMilliseconds, std::move(onTimer));
}

void DisplayWindow::StopTimer(void* timerID)
{
	SDL2DisplayWindow::StopTimer(timerID);
}

#endif
