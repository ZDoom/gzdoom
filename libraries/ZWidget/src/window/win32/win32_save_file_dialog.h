#pragma once

#include "systemdialogs/save_file_dialog.h"
#include "win32_util.h"

class Win32DisplayWindow;

class Win32SaveFileDialog : public SaveFileDialog
{
public:
	Win32SaveFileDialog(Win32DisplayWindow* owner);

	bool Show() override;
	std::string Filename() const override;
	void SetFilename(const std::string& filename) override;
	void AddFilter(const std::string& filter_description, const std::string& filter_extension) override;
	void ClearFilters() override;
	void SetFilterIndex(int filter_index) override;
	void SetInitialDirectory(const std::string& path) override;
	void SetTitle(const std::string& newtitle) override;
	void SetDefaultExtension(const std::string& extension) override;

private:
	void throw_if_failed(HRESULT result, const std::string& error);

	Win32DisplayWindow* owner = nullptr;

	std::string filename;
	std::string initial_directory;
	std::string initial_filename;
	std::string title;
	std::vector<std::string> filenames;

	struct Filter
	{
		std::string description;
		std::string extension;
	};
	std::vector<Filter> filters;
	int filterindex = 0;
	std::string defaultext;
};
