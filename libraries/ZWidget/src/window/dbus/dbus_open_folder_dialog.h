#pragma once

#include "systemdialogs/open_folder_dialog.h"

class DBusOpenFolderDialog : public OpenFolderDialog
{
public:
	DBusOpenFolderDialog(std::string ownerHandle);

	bool Show() override;
	std::string SelectedPath() const override;
	void SetInitialDirectory(const std::string& path) override;
	void SetTitle(const std::string& newtitle) override;

private:
	static std::string unescapeUri(const std::string& s);

	std::string ownerHandle;
	std::string title;
	std::string initialDirectory;
	std::string selectedPath;
};
