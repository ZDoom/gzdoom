
#include "systemdialogs/open_file_dialog.h"
#include "window/window.h"
#include "core/widget.h"

std::unique_ptr<OpenFileDialog> OpenFileDialog::Create(Widget* owner)
{
	DisplayWindow* windowOwner = nullptr;
	if (owner && owner->Window())
		windowOwner = owner->Window()->DispWindow.get();
	return DisplayBackend::Get()->CreateOpenFileDialog(windowOwner);
}
