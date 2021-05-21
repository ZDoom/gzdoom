#pragma once
#include <functional>
#include "dobject.h"
#include "v_2ddrawer.h"
#include "d_eventbase.h"
#include "s_soundinternal.h"
#include "gamestate.h"
#include "zstring.h"

using CompletionFunc = std::function<void(bool)>;

void Job_Init();

enum
{
	SJ_BLOCKUI = 1,
	SJ_DELAY = 2,
};

struct CutsceneDef
{
	FString video;
	FString function;
	FString soundName;
	int soundID = -1;	// ResID not SoundID!
	int framespersec = 0; // only relevant for ANM.
	bool transitiononly = false; // only play when transitioning between maps, but not when starting on a map or ending a game.

	void Create(DObject* runner);
	bool isdefined() { return video.IsNotEmpty() || function.IsNotEmpty(); }
	int GetSound();
};

void EndScreenJob();
void DeleteScreenJob();
bool ScreenJobResponder(event_t* ev);
bool ScreenJobTick();
void ScreenJobDraw();

struct CutsceneDef;
bool StartCutscene(const char* s, int flags, const CompletionFunc& completion);

extern int intermissiondelay;
