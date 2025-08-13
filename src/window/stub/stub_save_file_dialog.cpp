
#include "stub_save_file_dialog.h"

StubSaveFileDialog::StubSaveFileDialog(DisplayWindow* owner) : owner(owner)
{
}

bool StubSaveFileDialog::Show()
{
	return false;
}

std::string StubSaveFileDialog::Filename() const
{
	return filename;
}

void StubSaveFileDialog::SetFilename(const std::string& filename)
{
	initial_filename = filename;
}

void StubSaveFileDialog::AddFilter(const std::string& filter_description, const std::string& filter_extension)
{
	Filter f;
	f.description = filter_description;
	f.extension = filter_extension;
	filters.push_back(std::move(f));
}

void StubSaveFileDialog::ClearFilters()
{
	filters.clear();
}

void StubSaveFileDialog::SetFilterIndex(int filter_index)
{
	filterindex = filter_index;
}

void StubSaveFileDialog::SetInitialDirectory(const std::string& path)
{
	initial_directory = path;
}

void StubSaveFileDialog::SetTitle(const std::string& newtitle)
{
	title = newtitle;
}

void StubSaveFileDialog::SetDefaultExtension(const std::string& extension)
{
	defaultext = extension;
}
