#pragma once

#include <stdint.h>
#include "zstring.h"

struct FStartupInfo
{
	FString Name;
	uint32_t FgColor;			// Foreground color for title banner
	uint32_t BkColor;			// Background color for title banner
	FString Song;
	int Type;
	int LoadLights = -1;
	int LoadBrightmaps = -1;
	int LoadWidescreen = -1;
	int modern = 0;
	enum
	{
		DefaultStartup,
		DoomStartup,
		HereticStartup,
		HexenStartup,
		StrifeStartup,
	};
};


extern FStartupInfo GameStartupInfo;	

