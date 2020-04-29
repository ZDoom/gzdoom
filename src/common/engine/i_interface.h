#pragma once

#include "zstring.h"
#include "intrect.h"

struct SystemCallbacks
{
	bool (*WantGuiCapture)();
	bool (*WantLeftButton)();
	bool (*NetGame)();
	bool (*WantNativeMouse)();
	bool (*CaptureModeInGame)();
	void (*CrashInfo)(char* buffer, size_t bufflen, const char* lfstr);
	void (*PlayStartupSound)(const char* name);
	bool (*IsSpecialUI)();
	bool (*DisableTextureFilter)();
	void (*OnScreenSizeChanged)();
	IntRect(*GetSceneRect)();
	FString(*GetLocationDescription)();
};

extern SystemCallbacks *sysCallbacks;

struct WadStuff
{
	FString Path;
	FString Name;
};


extern FString endoomName;
extern bool batchrun;
