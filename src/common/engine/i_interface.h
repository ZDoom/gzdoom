#pragma once

#include "zstring.h"
#include "intrect.h"
#include "name.h"

struct event_t;
class FRenderState;
class FGameTexture;
class FTextureID;
enum EUpscaleFlags : int;
class FConfigFile;

struct SystemCallbacks
{
	bool (*G_Responder)(event_t* ev);	// this MUST be set, otherwise nothing will work
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
	bool (*PreBindTexture)(FRenderState* state, FGameTexture*& tex, EUpscaleFlags& flags, int& scaleflags, int& clampmode, int& translation, int& overrideshader);
	void (*FontCharCreated)(FGameTexture* base, FGameTexture* untranslated);
	void (*ToggleFullConsole)();
	void (*StartCutscene)(bool blockui);
	void (*SetTransition)(int type);
	bool (*CheckCheatmode)(bool printmsg, bool sponly);
	void (*HudScaleChanged)();
	bool (*SetSpecialMenu)(FName& menu, int param);
	void (*OnMenuOpen)(bool makesound);
	void (*LanguageChanged)(const char*);
	bool (*OkForLocalization)(FTextureID, const char*);
	FConfigFile* (*GetConfig)();
	bool (*WantEscape)();
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
extern int 	paused;
extern bool pauseext;

void UpdateGenericUI(bool cvar);
