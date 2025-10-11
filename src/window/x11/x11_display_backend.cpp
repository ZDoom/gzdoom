
#include "x11_display_backend.h"
#include "x11_display_window.h"
#include "x11_connection.h"
#include <stdexcept>

#ifdef USE_DBUS
#include "window/dbus/dbus_open_file_dialog.h"
#include "window/dbus/dbus_save_file_dialog.h"
#include "window/dbus/dbus_open_folder_dialog.h"
#endif

std::unique_ptr<DisplayWindow> X11DisplayBackend::Create(DisplayWindowHost* windowHost, bool popupWindow, DisplayWindow* owner, RenderAPI renderAPI)
{
	return std::make_unique<X11DisplayWindow>(windowHost, popupWindow, static_cast<X11DisplayWindow*>(owner), renderAPI);
}

void X11DisplayBackend::ProcessEvents()
{
	GetX11Connection()->ProcessEvents();
}

void X11DisplayBackend::RunLoop()
{
	GetX11Connection()->RunLoop();
}

void X11DisplayBackend::ExitLoop()
{
	GetX11Connection()->ExitLoop();
}

Size X11DisplayBackend::GetScreenSize()
{
	return GetX11Connection()->GetScreenSize();
}

void* X11DisplayBackend::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	return GetX11Connection()->StartTimer(timeoutMilliseconds, onTimer);
}

void X11DisplayBackend::StopTimer(void* timerID)
{
	GetX11Connection()->StopTimer(timerID);
}

#ifdef USE_DBUS
std::unique_ptr<OpenFileDialog> X11DisplayBackend::CreateOpenFileDialog(DisplayWindow* owner)
{
	std::string ownerHandle;
	if (owner)
		ownerHandle = "x11:" + std::to_string(static_cast<X11DisplayWindow*>(owner)->window);
	return std::make_unique<DBusOpenFileDialog>(ownerHandle);
}

std::unique_ptr<SaveFileDialog> X11DisplayBackend::CreateSaveFileDialog(DisplayWindow* owner)
{
	std::string ownerHandle;
	if (owner)
		ownerHandle = "x11:" + std::to_string(static_cast<X11DisplayWindow*>(owner)->window);
	return std::make_unique<DBusSaveFileDialog>(ownerHandle);
}

std::unique_ptr<OpenFolderDialog> X11DisplayBackend::CreateOpenFolderDialog(DisplayWindow* owner)
{
	std::string ownerHandle;
	if (owner)
		ownerHandle = "x11:" + std::to_string(static_cast<X11DisplayWindow*>(owner)->window);
	return std::make_unique<DBusOpenFolderDialog>(ownerHandle);
}
#endif
