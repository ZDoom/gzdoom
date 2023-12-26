
#include "window/window.h"

#ifdef WIN32

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

#else

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

#endif
