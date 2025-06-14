#pragma once

#include "systemdialogs/open_folder_dialog.h"

class DisplayWindow;

class StubOpenFolderDialog : public OpenFolderDialog
{
public:
	StubOpenFolderDialog(DisplayWindow* owner);

	bool Show() override;
	std::string SelectedPath() const override;
	void SetInitialDirectory(const std::string& path) override;
	void SetTitle(const std::string& newtitle) override;

private:
	DisplayWindow* owner = nullptr;

	std::string selected_path;
	std::string initial_directory;
	std::string title;
};
