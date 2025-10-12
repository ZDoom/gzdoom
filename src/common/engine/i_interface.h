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
class FArgs;
struct FTranslationID;

extern FString GameUUID;
FString GenerateUUID();

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
	FTranslationID(*RemapTranslation)(FTranslationID trans);
};

extern SystemCallbacks sysCallbacks;

struct WadStuff
{
	FString Path;
	FString Name;
};

struct FStartupSelectionInfo
{
	const TArray<WadStuff>* Wads = nullptr;
	FArgs* Args = nullptr;

	// Local game info
	int DefaultIWAD = 0;
	FString DefaultArgs = {};
	bool bSaveArgs = true;

	// Settings
	int DefaultStartFlags = 0;
	bool DefaultQueryIWAD = true;
	FString DefaultLanguage = "auto";
	int DefaultBackend = 1;
	bool DefaultFullscreen = true;

	// Net game info
	int DefaultNetIWAD = 0;
	bool bNetStart = false;
	bool bHosting = false;
	bool bSaveNetFile = false;
	bool bSaveNetArgs = true;
	int DefaultNetPage = 0;
	FString DefaultNetArgs = {};
	FString AdditionalNetArgs = {}; // These ones shouldn't be saved.
	FString DefaultNetSaveFile = {};
	int DefaultNetHostTeam = 255;
	int DefaultNetPlayers = 8;
	int DefaultNetHostPort = 0;
	int DefaultNetTicDup = 0;
	bool DefaultNetExtraTic = false;
	int DefaultNetMode = 0;
	int DefaultNetGameMode = 0;
	bool DefaultNetAltDM = false;

	FString DefaultNetAddress = {};
	int DefaultNetJoinPort = 0;
	int DefaultNetJoinTeam = 255;

	FStartupSelectionInfo() = delete;
	FStartupSelectionInfo(const TArray<WadStuff>& wads, FArgs& args, int startFlags);
	int SaveInfo();
};


extern FString endoomName;
extern bool batchrun;
extern float menuBlurAmount;
extern bool generic_ui;
extern bool special_i;
extern int 	paused;
extern bool pauseext;

void UpdateGenericUI(bool cvar);
