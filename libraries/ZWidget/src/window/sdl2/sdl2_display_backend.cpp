
#include "sdl2_display_backend.h"
#include "sdl2_display_window.h"

std::unique_ptr<DisplayWindow> SDL2DisplayBackend::Create(DisplayWindowHost* windowHost, bool popupWindow, DisplayWindow* owner, RenderAPI renderAPI)
{
	return std::make_unique<SDL2DisplayWindow>(windowHost, popupWindow, static_cast<SDL2DisplayWindow*>(owner), renderAPI);
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
	return SDL2DisplayWindow::GetScreenSize();
}

void* SDL2DisplayBackend::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return SDL2DisplayWindow::StartTimer(timeoutMilliseconds, std::move(onTimer));
}

void SDL2DisplayBackend::StopTimer(void* timerID)
{
	SDL2DisplayWindow::StopTimer(timerID);
}
