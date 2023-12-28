
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
	auto lump = WidgetResources->FindLump("widgets/poppins/poppins-regular.ttf");
	if (!lump)
		I_FatalError("Unable to find %s", name.c_str());

	auto data = lump->Lock();
	std::vector<uint8_t> buffer;
	buffer.resize(lump->Size());
	memcpy(buffer.data(), data, buffer.size());
	lump->Unlock();
	return buffer;
}

std::vector<uint8_t> LoadWidgetImageData(const std::string& name)
{
	auto lump = WidgetResources->FindLump(name.c_str());
	if (!lump)
		I_FatalError("Unable to find %s", name.c_str());

	auto data = lump->Lock();
	std::vector<uint8_t> buffer;
	buffer.resize(lump->Size());
	memcpy(buffer.data(), data, buffer.size());
	lump->Unlock();
	return buffer;
}
