
#include "stub_open_file_dialog.h"

StubOpenFileDialog::StubOpenFileDialog(DisplayWindow* owner) : owner(owner)
{
}

bool StubOpenFileDialog::Show()
{
	return false;
}

std::string StubOpenFileDialog::Filename() const
{
	return !filenames.empty() ? filenames.front() : std::string();
}

std::vector<std::string> StubOpenFileDialog::Filenames() const
{
	return filenames;
}

void StubOpenFileDialog::SetMultiSelect(bool new_multi_select)
{
	multi_select = new_multi_select;
}

void StubOpenFileDialog::SetFilename(const std::string& filename)
{
	initial_filename = filename;
}

void StubOpenFileDialog::AddFilter(const std::string& filter_description, const std::string& filter_extension)
{
	Filter f;
	f.description = filter_description;
	f.extension = filter_extension;
	filters.push_back(std::move(f));
}

void StubOpenFileDialog::ClearFilters()
{
	filters.clear();
}

void StubOpenFileDialog::SetFilterIndex(int filter_index)
{
	filterindex = filter_index;
}

void StubOpenFileDialog::SetInitialDirectory(const std::string& path)
{
	initial_directory = path;
}

void StubOpenFileDialog::SetTitle(const std::string& newtitle)
{
	title = newtitle;
}

void StubOpenFileDialog::SetDefaultExtension(const std::string& extension)
{
	defaultext = extension;
}
