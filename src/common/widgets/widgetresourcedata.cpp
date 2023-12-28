
#include <zwidget/core/resourcedata.h>

// To do: this should load data from lumps

#ifdef WIN32
#include <Windows.h>
#include <stdexcept>
static std::wstring to_utf16(const std::string& str)
{
	if (str.empty()) return {};
	int needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
	if (needed == 0)
		throw std::runtime_error("MultiByteToWideChar failed");
	std::wstring result;
	result.resize(needed);
	needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &result[0], (int)result.size());
	if (needed == 0)
		throw std::runtime_error("MultiByteToWideChar failed");
	return result;
}

static std::vector<uint8_t> ReadAllBytes(const std::string& filename)
{
	HANDLE handle = CreateFile(to_utf16(filename).c_str(), 0x0001/*FILE_READ_ACCESS*/, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (handle == INVALID_HANDLE_VALUE)
		throw std::runtime_error("Could not open " + filename);

	LARGE_INTEGER fileSize;
	BOOL result = GetFileSizeEx(handle, &fileSize);
	if (result == FALSE)
	{
		CloseHandle(handle);
		throw std::runtime_error("GetFileSizeEx failed");
	}

	std::vector<uint8_t> buffer(fileSize.QuadPart);

	DWORD bytesRead = 0;
	result = ReadFile(handle, buffer.data(), (DWORD)buffer.size(), &bytesRead, nullptr);
	if (result == FALSE || bytesRead != buffer.size())
	{
		CloseHandle(handle);
		throw std::runtime_error("ReadFile failed");
	}

	CloseHandle(handle);

	return buffer;
}

std::vector<uint8_t> LoadWidgetFontData(const std::string& name)
{
	return ReadAllBytes("C:\\Windows\\Fonts\\segoeui.ttf");
}

#else
std::vector<uint8_t> LoadWidgetFontData(const std::string& name)
{
	return {};
}
#endif
