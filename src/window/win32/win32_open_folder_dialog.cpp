
#include "win32_open_folder_dialog.h"
#include "win32_display_window.h"
#include "core/widget.h"
#include <stdexcept>

Win32OpenFolderDialog::Win32OpenFolderDialog(Win32DisplayWindow* owner) : owner(owner)
{
}

bool Win32OpenFolderDialog::Show()
{
	std::wstring title16 = to_utf16(title);
	std::wstring initial_directory16 = to_utf16(initial_directory);

	HRESULT result;
	ComPtr<IFileOpenDialog> open_dialog;

	result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, open_dialog.GetIID(), open_dialog.InitPtr());
	throw_if_failed(result, "CoCreateInstance(FileOpenDialog) failed");

	result = open_dialog->SetTitle(title16.c_str());
	throw_if_failed(result, "IFileOpenDialog.SetTitle failed");

	result = open_dialog->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
	throw_if_failed(result, "IFileOpenDialog.SetOptions((FOS_PICKFOLDERS) failed");

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
			result = open_dialog->SetFolder(folder_item);
			throw_if_failed(result, "IFileOpenDialog.SetFolder failed");
		}
	}

	if (owner)
		result = open_dialog->Show(owner->WindowHandle.hwnd);
	else
		result = open_dialog->Show(0);

	if (SUCCEEDED(result))
	{
		ComPtr<IShellItem> chosen_folder;
		result = open_dialog->GetResult(chosen_folder.TypedInitPtr());
		throw_if_failed(result, "IFileOpenDialog.GetResult failed");

		WCHAR* buffer = nullptr;
		result = chosen_folder->GetDisplayName(SIGDN_FILESYSPATH, &buffer);
		throw_if_failed(result, "IShellItem.GetDisplayName failed");

		std::wstring output_directory16;
		if (buffer)
		{
			try
			{
				output_directory16 = buffer;
			}
			catch (...)
			{
				CoTaskMemFree(buffer);
				throw;
			}
		}

		CoTaskMemFree(buffer);
		selected_path = from_utf16(output_directory16);
		return true;
	}
	else
	{
		return false;
	}
}

std::string Win32OpenFolderDialog::SelectedPath() const
{
	return selected_path;
}

void Win32OpenFolderDialog::SetInitialDirectory(const std::string& path)
{
	initial_directory = path;
}

void Win32OpenFolderDialog::SetTitle(const std::string& newtitle)
{
	title = newtitle;
}

void Win32OpenFolderDialog::throw_if_failed(HRESULT result, const std::string& error)
{
	if (FAILED(result))
		throw std::runtime_error(error);
}
