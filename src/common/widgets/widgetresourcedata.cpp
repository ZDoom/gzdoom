
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

static std::vector<uint8_t> LoadFile(const std::string& name)
{
	auto lump = WidgetResources->FindEntry(name.c_str());
	if (lump == -1)
		I_FatalError("Unable to find %s", name.c_str());

	auto reader = WidgetResources->GetEntryReader(lump, FileSys::READER_SHARED);
	std::vector<uint8_t> buffer(reader.GetLength());
	reader.Read(buffer.data(), buffer.size());
	return buffer;
}

// This interface will later require some significant redesign.
std::vector<SingleFontData> LoadWidgetFontData(const std::string& name)
{
	std::vector<SingleFontData> returnv;
	if (!stricmp(name.c_str(), "notosans"))
	{
		returnv.resize(3);
		returnv[2].fontdata = LoadFile("widgets/noto/notosans-regular.ttf");
		returnv[0].fontdata = LoadFile("widgets/noto/notosansarmenian-regular.ttf");
		returnv[1].fontdata = LoadFile("widgets/noto/notosansgeorgian-regular.ttf");
		returnv[0].ranges.push_back(std::make_pair(0x531, 0x58f));
		returnv[1].ranges.push_back(std::make_pair(0x10a0, 0x10ff));
		returnv[1].ranges.push_back(std::make_pair(0x1c90, 0x1cbf));
		returnv[1].ranges.push_back(std::make_pair(0x2d00, 0x2d2f));
		// todo: load system CJK fonts here
		return returnv;

	}
	returnv.resize(1);
	std::string fn = "widgets/font/" +name + ".ttf";
	returnv[0].fontdata = LoadFile(fn);
	return returnv;
}

std::vector<uint8_t> LoadWidgetImageData(const std::string& name)
{
	return LoadFile(name);
}
