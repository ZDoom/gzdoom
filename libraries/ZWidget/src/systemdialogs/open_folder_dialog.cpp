
#include "systemdialogs/open_folder_dialog.h"
#include "window/window.h"
#include "core/widget.h"

std::unique_ptr<OpenFolderDialog> OpenFolderDialog::Create(Widget* owner)
{
	DisplayWindow* windowOwner = nullptr;
	if (owner && owner->Window())
		windowOwner = owner->Window()->DispWindow.get();
	return DisplayBackend::Get()->CreateOpenFolderDialog(windowOwner);
}
