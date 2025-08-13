
#include "stub_open_folder_dialog.h"

StubOpenFolderDialog::StubOpenFolderDialog(DisplayWindow* owner) : owner(owner)
{
}

bool StubOpenFolderDialog::Show()
{
	return false;
}

std::string StubOpenFolderDialog::SelectedPath() const
{
	return selected_path;
}

void StubOpenFolderDialog::SetInitialDirectory(const std::string& path)
{
	initial_directory = path;
}

void StubOpenFolderDialog::SetTitle(const std::string& newtitle)
{
	title = newtitle;
}
