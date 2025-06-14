#pragma once

#include "systemdialogs/open_folder_dialog.h"
#include "win32_util.h"

class Win32DisplayWindow;

class Win32OpenFolderDialog : public OpenFolderDialog
{
public:
	Win32OpenFolderDialog(Win32DisplayWindow* owner);

	bool Show() override;
	std::string SelectedPath() const override;
	void SetInitialDirectory(const std::string& path) override;
	void SetTitle(const std::string& newtitle) override;

private:
	void throw_if_failed(HRESULT result, const std::string& error);

	Win32DisplayWindow* owner = nullptr;

	std::string selected_path;
	std::string initial_directory;
	std::string title;
};
