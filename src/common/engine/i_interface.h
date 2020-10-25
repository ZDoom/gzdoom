#pragma once

#include "zstring.h"
#include "intrect.h"

struct event_t;

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
	void (*MenuDim)();
	FString(*GetPlayerName)(int i);
	bool (*DispatchEvent)(event_t* ev);
	bool (*CheckGame)(const char* nm);
	int (*GetGender)();
	void (*MenuClosed)();
	bool (*CheckMenudefOption)(const char* opt);
	void (*ConsoleToggled)(int state);
};

extern SystemCallbacks sysCallbacks;

struct WadStuff
{
	FString Path;
	FString Name;
};


extern FString endoomName;
extern bool batchrun;
extern float menuBlurAmount;
extern bool generic_ui;

void UpdateGenericUI(bool cvar);
