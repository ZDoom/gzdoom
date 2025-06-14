
#include "dbus_open_folder_dialog.h"

DBusOpenFolderDialog::DBusOpenFolderDialog(std::string ownerHandle) : ownerHandle(ownerHandle)
{
}

bool DBusOpenFolderDialog::Show()
{
	return false;
}

std::string DBusOpenFolderDialog::SelectedPath() const
{
	return selected_path;
}

void DBusOpenFolderDialog::SetInitialDirectory(const std::string& path)
{
	initial_directory = path;
}

void DBusOpenFolderDialog::SetTitle(const std::string& newtitle)
{
	title = newtitle;
}
