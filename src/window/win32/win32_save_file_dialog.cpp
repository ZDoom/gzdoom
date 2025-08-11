
#include "win32_save_file_dialog.h"
#include "win32_display_window.h"
#include "core/widget.h"

Win32SaveFileDialog::Win32SaveFileDialog(Win32DisplayWindow* owner) : owner(owner)
{
}

bool Win32SaveFileDialog::Show()
{
	std::wstring title16 = to_utf16(title);
	std::wstring initial_directory16 = to_utf16(initial_directory);

	HRESULT result;
	ComPtr<IFileSaveDialog> save_dialog;

	result = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, save_dialog.GetIID(), save_dialog.InitPtr());
	throw_if_failed(result, "CoCreateInstance(FileSaveDialog) failed");

	result = save_dialog->SetTitle(title16.c_str());
	throw_if_failed(result, "IFileSaveDialog.SetTitle failed");

	if (!initial_filename.empty())
	{
		result = save_dialog->SetFileName(to_utf16(initial_filename).c_str());
		throw_if_failed(result, "IFileSaveDialog.SetFileName failed");
	}

	FILEOPENDIALOGOPTIONS options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
	result = save_dialog->SetOptions(options);
	throw_if_failed(result, "IFileSaveDialog.SetOptions() failed");

	if (!filters.empty())
	{
		std::vector<COMDLG_FILTERSPEC> filterspecs(filters.size());
		std::vector<std::wstring> descriptions(filters.size());
		std::vector<std::wstring> extensions(filters.size());
		for (size_t i = 0; i < filters.size(); i++)
		{
			descriptions[i] = to_utf16(filters[i].description);
			extensions[i] = to_utf16(filters[i].extension);
			COMDLG_FILTERSPEC& spec = filterspecs[i];
			spec.pszName = descriptions[i].c_str();
			spec.pszSpec = extensions[i].c_str();
		}
		result = save_dialog->SetFileTypes((UINT)filterspecs.size(), filterspecs.data());
		throw_if_failed(result, "IFileOpenDialog.SetFileTypes() failed");

		if ((size_t)filterindex < filters.size())
		{
			result = save_dialog->SetFileTypeIndex(filterindex);
			throw_if_failed(result, "IFileOpenDialog.SetFileTypeIndex() failed");
		}
	}

	if (!defaultext.empty())
	{
		result = save_dialog->SetDefaultExtension(to_utf16(defaultext).c_str());
		throw_if_failed(result, "IFileOpenDialog.SetDefaultExtension() failed");
	}

	if (initial_directory16.length() > 0)
	{
		LPITEMIDLIST item_id_list = nullptr;
		SFGAOF flags = 0;
		result = SHParseDisplayName(initial_directory16.c_str(), nullptr, &item_id_list, SFGAO_FILESYSTEM, &flags);
		throw_if_failed(result, "SHParseDisplayName failed");

		ComPtr<IShellItem> folder_item;
		result = SHCreateShellItem(nullptr, nullptr, item_id_list, folder_item.TypedInitPtr());
		ILFree(item_id_list);
		throw_if_failed(result, "SHCreateItemFromParsingName failed");

		/* This code requires Windows Vista or newer:
		ComPtr<IShellItem> folder_item;
		result = SHCreateItemFromParsingName(initial_directory16.c_str(), nullptr, folder_item.GetIID(), folder_item.InitPtr());
		throw_if_failed(result, "SHCreateItemFromParsingName failed");
		*/

		if (folder_item)
		{
			result = save_dialog->SetFolder(folder_item);
			throw_if_failed(result, "IFileSaveDialog.SetFolder failed");
		}
	}

	if (owner)
		result = save_dialog->Show(owner->WindowHandle.hwnd);
	else
		result = save_dialog->Show(0);

	if (SUCCEEDED(result))
	{
		ComPtr<IShellItem> chosen_folder;
		result = save_dialog->GetResult(chosen_folder.TypedInitPtr());
		throw_if_failed(result, "IFileSaveDialog.GetResult failed");

		WCHAR* buffer = nullptr;
		result = chosen_folder->GetDisplayName(SIGDN_FILESYSPATH, &buffer);
		throw_if_failed(result, "IShellItem.GetDisplayName failed");

		std::wstring output16;
		if (buffer)
		{
			try
			{
				output16 = buffer;
			}
			catch (...)
			{
				CoTaskMemFree(buffer);
				throw;
			}
		}

		CoTaskMemFree(buffer);
		filename = from_utf16(output16);
		return true;
	}
	else
	{
		return false;
	}
}

std::string Win32SaveFileDialog::Filename() const
{
	return filename;
}

void Win32SaveFileDialog::SetFilename(const std::string& filename)
{
	initial_filename = filename;
}

void Win32SaveFileDialog::AddFilter(const std::string& filter_description, const std::string& filter_extension)
{
	Filter f;
	f.description = filter_description;
	f.extension = filter_extension;
	filters.push_back(std::move(f));
}

void Win32SaveFileDialog::ClearFilters()
{
	filters.clear();
}

void Win32SaveFileDialog::SetFilterIndex(int filter_index)
{
	filterindex = filter_index;
}

void Win32SaveFileDialog::SetInitialDirectory(const std::string& path)
{
	initial_directory = path;
}

void Win32SaveFileDialog::SetTitle(const std::string& newtitle)
{
	title = newtitle;
}

void Win32SaveFileDialog::SetDefaultExtension(const std::string& extension)
{
	defaultext = extension;
}

void Win32SaveFileDialog::throw_if_failed(HRESULT result, const std::string& error)
{
	if (FAILED(result))
		throw std::runtime_error(error);
}
