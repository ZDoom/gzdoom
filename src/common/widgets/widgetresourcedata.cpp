
#include <zwidget/core/resourcedata.h>
#include <zwidget/core/theme.h>
#include "c_cvars.h"
#include "filesystem.h"
#include "printf.h"
#include "zstring.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

CUSTOM_CVARD(Int, ui_theme, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "launcher theme. 0: auto, 1: dark, 2: light")
{
	if (self < 0) self = 0;
	if (self > 2) self = 2;
}

FResourceFile* WidgetResources;

bool IsZWidgetAvailable()
{
	return WidgetResources;
}

void InitWidgetResources(const char* filename)
{
	WidgetResources = FResourceFile::OpenResourceFile(filename);
	if (!WidgetResources)
		I_FatalError("Unable to open %s", filename);

	bool use_dark = ui_theme != 2;

	if (ui_theme == 0)
	{
		// TODO: detect system theme
	}

	if (use_dark) // light
	{
		// TODO: make a nice theme
		WidgetTheme::SetTheme(std::make_unique<DarkWidgetTheme>());
	}
	else
	{
		WidgetTheme::SetTheme(std::unique_ptr<WidgetTheme>(new WidgetTheme{{
			Colorf::fromRgb(0xeee8d5), // background
			Colorf::fromRgb(0x000000), // text
			Colorf::fromRgb(0xfdf6e3), // headers / inputs
			Colorf::fromRgb(0x000000), // headers / inputs text
			Colorf::fromRgb(0xd7d2bf), // interactive elements
			Colorf::fromRgb(0x000000), // interactive elements text
			Colorf::fromRgb(0xa4c2e9), // hover / highlight
			Colorf::fromRgb(0x000000), // hover / highlight text
			Colorf::fromRgb(0x7ca2e9), // click
			Colorf::fromRgb(0x000000), // click text
			Colorf::fromRgb(0x586e75), // around elements
			Colorf::fromRgb(0xbdb8a7)  // between elements
		}}));
	}
}

void CloseWidgetResources()
{
	delete WidgetResources;
	WidgetResources = nullptr;
}

static std::vector<uint8_t> LoadFile(const char* name)
{
	if (!WidgetResources)
		I_FatalError("InitWidgetResources has not been called");

	auto lump = WidgetResources->FindEntry(name);
	if (lump == -1)
		I_FatalError("Unable to find %s", name);

	auto reader = WidgetResources->GetEntryReader(lump, FileSys::READER_SHARED);
	std::vector<uint8_t> buffer(reader.GetLength());
	reader.Read(buffer.data(), buffer.size());
	return buffer;
}

// this must be allowed to fail without throwing.
static std::vector<uint8_t> LoadDiskFile(const char* name)
{
	std::vector<uint8_t> buffer;
	FileSys::FileReader lump;
	if (lump.OpenFile(name))
	{
		buffer.resize(lump.GetLength());
		lump.Read(buffer.data(), buffer.size());
	}
	return buffer;
}

// This interface will later require some significant redesign.
std::vector<SingleFontData> LoadWidgetFontData(const std::string& name)
{
	std::vector<SingleFontData> returnv;
	if (!stricmp(name.c_str(), "notosans"))
	{
		returnv.resize(3);
		returnv[0].fontdata = LoadFile("widgets/noto/notosans-regular.ttf");
		returnv[1].fontdata = LoadFile("widgets/noto/notosansarmenian-regular.ttf");
		returnv[2].fontdata = LoadFile("widgets/noto/notosansgeorgian-regular.ttf");
#ifdef _WIN32
		wchar_t wbuffer[256];
		if (GetWindowsDirectoryW(wbuffer, 256))
		{
			returnv.resize(5);
			FString windir(wbuffer);
			returnv[3].fontdata = LoadDiskFile((windir + "/fonts/yugothm.ttc").GetChars());
			returnv[3].language = "ja";
			returnv[4].fontdata = LoadDiskFile((windir + "/fonts/malgun.ttf").GetChars());
			returnv[4].language = "ko";
			// Don't fail if these cannot be found
			if (returnv[4].fontdata.size() == 0) returnv.erase(returnv.begin() + 4);
			if (returnv[3].fontdata.size() == 0) returnv.erase(returnv.begin() + 3);
		}
#endif
		return returnv;

	}
	returnv.resize(1);
	std::string fn = "widgets/font/" +name + ".ttf";
	returnv[0].fontdata = LoadFile(fn.c_str());
	return returnv;
}

std::vector<uint8_t> LoadWidgetData(const std::string& name)
{
	return LoadFile(name.c_str());
}
