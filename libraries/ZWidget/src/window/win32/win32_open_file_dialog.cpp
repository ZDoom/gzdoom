
#include "win32_open_file_dialog.h"
#include "win32_display_window.h"
#include "core/widget.h"
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

Win32OpenFileDialog::Win32OpenFileDialog(Win32DisplayWindow* owner) : owner(owner)
{
}

bool Win32OpenFileDialog::Show()
{
	std::wstring title16 = to_utf16(title);
	std::wstring initial_directory16 = to_utf16(initial_directory);

	HRESULT result;
	ComPtr<IFileOpenDialog> open_dialog;

	result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, open_dialog.GetIID(), open_dialog.InitPtr());
	throw_if_failed(result, "CoCreateInstance(FileOpenDialog) failed");

	result = open_dialog->SetTitle(title16.c_str());
	throw_if_failed(result, "IFileOpenDialog.SetTitle failed");

	if (!initial_filename.empty())
	{
		result = open_dialog->SetFileName(to_utf16(initial_filename).c_str());
		throw_if_failed(result, "IFileOpenDialog.SetFileName failed");
	}

	FILEOPENDIALOGOPTIONS options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
	if (multi_select)
		options |= FOS_ALLOWMULTISELECT;
	result = open_dialog->SetOptions(options);
	throw_if_failed(result, "IFileOpenDialog.SetOptions() failed");

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
		result = open_dialog->SetFileTypes((UINT)filterspecs.size(), filterspecs.data());
		throw_if_failed(result, "IFileOpenDialog.SetFileTypes() failed");

		if ((size_t)filterindex < filters.size())
		{
			result = open_dialog->SetFileTypeIndex(filterindex);
			throw_if_failed(result, "IFileOpenDialog.SetFileTypeIndex() failed");
		}
	}

	if (!defaultext.empty())
	{
		result = open_dialog->SetDefaultExtension(to_utf16(defaultext).c_str());
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
			result = open_dialog->SetFolder(folder_item);
			throw_if_failed(result, "IFileOpenDialog.SetFolder failed");
		}
	}

	// For some reason this can hang deep inside Win32 if we do it on the calling thread!
	{
		bool done = false;
		std::mutex mutex;
		std::condition_variable condvar;

		std::thread thread([&]() {

			if (owner)
				result = open_dialog->Show(owner->WindowHandle.hwnd);
			else
				result = open_dialog->Show(0);

			std::unique_lock lock(mutex);
			done = true;
			condvar.notify_all();

			});

		std::unique_lock lock(mutex);
		while (!done)
		{
			DisplayBackend::Get()->ProcessEvents();
			using namespace std::chrono_literals;
			condvar.wait_for(lock, 50ms, [&]() { return done; });
		}
		lock.unlock();
		thread.join();
	}

	if (SUCCEEDED(result))
	{
		ComPtr<IShellItemArray> items;
		result = open_dialog->GetResults(items.TypedInitPtr());
		throw_if_failed(result, "IFileOpenDialog.GetSelectedItems failed");

		DWORD num_items = 0;
		result = items->GetCount(&num_items);
		throw_if_failed(result, "IShellItemArray.GetCount failed");

		for (DWORD i = 0; i < num_items; i++)
		{
			ComPtr<IShellItem> item;
			result = items->GetItemAt(i, item.TypedInitPtr());
			throw_if_failed(result, "IShellItemArray.GetItemAt failed");

			WCHAR* buffer = nullptr;
			result = item->GetDisplayName(SIGDN_FILESYSPATH, &buffer);
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
			filenames.push_back(from_utf16(output16));
		}
		return true;
	}
	else
	{
		return false;
	}
}

std::string Win32OpenFileDialog::Filename() const
{
	return !filenames.empty() ? filenames.front() : std::string();
}

std::vector<std::string> Win32OpenFileDialog::Filenames() const
{
	return filenames;
}

void Win32OpenFileDialog::SetMultiSelect(bool new_multi_select)
{
	multi_select = new_multi_select;
}

void Win32OpenFileDialog::SetFilename(const std::string& filename)
{
	initial_filename = filename;
}

void Win32OpenFileDialog::AddFilter(const std::string& filter_description, const std::string& filter_extension)
{
	Filter f;
	f.description = filter_description;
	f.extension = filter_extension;
	filters.push_back(std::move(f));
}

void Win32OpenFileDialog::ClearFilters()
{
	filters.clear();
}

void Win32OpenFileDialog::SetFilterIndex(int filter_index)
{
	filterindex = filter_index;
}

void Win32OpenFileDialog::SetInitialDirectory(const std::string& path)
{
	initial_directory = path;
}

void Win32OpenFileDialog::SetTitle(const std::string& newtitle)
{
	title = newtitle;
}

void Win32OpenFileDialog::SetDefaultExtension(const std::string& extension)
{
	defaultext = extension;
}

void Win32OpenFileDialog::throw_if_failed(HRESULT result, const std::string& error)
{
	if (FAILED(result))
		throw std::runtime_error(error);
}
