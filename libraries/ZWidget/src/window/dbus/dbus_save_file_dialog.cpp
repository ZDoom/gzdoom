
#include "dbus_save_file_dialog.h"

DBusSaveFileDialog::DBusSaveFileDialog(std::string ownerHandle) : ownerHandle(ownerHandle)
{
}

bool DBusSaveFileDialog::Show()
{
	return false;
}

std::string DBusSaveFileDialog::Filename() const
{
	return filename;
}

void DBusSaveFileDialog::SetFilename(const std::string& filename)
{
	initial_filename = filename;
}

void DBusSaveFileDialog::AddFilter(const std::string& filter_description, const std::string& filter_extension)
{
	Filter f;
	f.description = filter_description;
	f.extension = filter_extension;
	filters.push_back(std::move(f));
}

void DBusSaveFileDialog::ClearFilters()
{
	filters.clear();
}

void DBusSaveFileDialog::SetFilterIndex(int filter_index)
{
	filterindex = filter_index;
}

void DBusSaveFileDialog::SetInitialDirectory(const std::string& path)
{
	initial_directory = path;
}

void DBusSaveFileDialog::SetTitle(const std::string& newtitle)
{
	title = newtitle;
}

void DBusSaveFileDialog::SetDefaultExtension(const std::string& extension)
{
	defaultext = extension;
}
