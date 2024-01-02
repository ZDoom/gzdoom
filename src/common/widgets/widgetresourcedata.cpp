
#include <zwidget/core/resourcedata.h>
#include "filesystem.h"
#include "printf.h"

FResourceFile* WidgetResources;

void InitWidgetResources(const char* filename)
{
	WidgetResources = FResourceFile::OpenResourceFile(filename);
	if (!WidgetResources)
		I_FatalError("Unable to open %s", filename);
}

std::vector<uint8_t> LoadWidgetFontData(const std::string& name)
{
	auto lump = WidgetResources->FindEntry("widgets/poppins/poppins-regular.ttf");
	if (lump == -1)
		I_FatalError("Unable to find %s", name.c_str());

	auto reader = WidgetResources->GetEntryReader(lump, FileSys::READER_SHARED);
	std::vector<uint8_t> buffer(reader.GetLength());
	reader.Read(buffer.data(), buffer.size());
	return buffer;
}

std::vector<uint8_t> LoadWidgetImageData(const std::string& name)
{
	auto lump = WidgetResources->FindEntry(name.c_str());
	if (lump == -1)
		I_FatalError("Unable to find %s", name.c_str());

	auto reader = WidgetResources->GetEntryReader(lump, FileSys::READER_SHARED);
	std::vector<uint8_t> buffer(reader.GetLength());
	reader.Read(buffer.data(), buffer.size());
	return buffer;
}
