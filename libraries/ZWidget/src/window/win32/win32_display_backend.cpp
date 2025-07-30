
#include "win32_display_backend.h"
#include "win32_display_window.h"
#include "win32_open_file_dialog.h"
#include "win32_save_file_dialog.h"
#include "win32_open_folder_dialog.h"

std::unique_ptr<DisplayWindow> Win32DisplayBackend::Create(DisplayWindowHost* windowHost, bool popupWindow, DisplayWindow* owner, RenderAPI renderAPI)
{
	return std::make_unique<Win32DisplayWindow>(windowHost, popupWindow, static_cast<Win32DisplayWindow*>(owner), renderAPI);
}

void Win32DisplayBackend::ProcessEvents()
{
	Win32DisplayWindow::ProcessEvents();
}

void Win32DisplayBackend::RunLoop()
{
	Win32DisplayWindow::RunLoop();
}

void Win32DisplayBackend::ExitLoop()
{
	Win32DisplayWindow::ExitLoop();
}

Size Win32DisplayBackend::GetScreenSize()
{
	return Win32DisplayWindow::GetScreenSize();
}

void* Win32DisplayBackend::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return Win32DisplayWindow::StartTimer(timeoutMilliseconds, std::move(onTimer));
}

void Win32DisplayBackend::StopTimer(void* timerID)
{
	Win32DisplayWindow::StopTimer(timerID);
}

std::unique_ptr<OpenFileDialog> Win32DisplayBackend::CreateOpenFileDialog(DisplayWindow* owner)
{
	return std::make_unique<Win32OpenFileDialog>(static_cast<Win32DisplayWindow*>(owner));
}

std::unique_ptr<SaveFileDialog> Win32DisplayBackend::CreateSaveFileDialog(DisplayWindow* owner)
{
	return std::make_unique<Win32SaveFileDialog>(static_cast<Win32DisplayWindow*>(owner));
}

std::unique_ptr<OpenFolderDialog> Win32DisplayBackend::CreateOpenFolderDialog(DisplayWindow* owner)
{
	return std::make_unique<Win32OpenFolderDialog>(static_cast<Win32DisplayWindow*>(owner));
}
