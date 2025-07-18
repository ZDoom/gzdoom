#pragma once
#include <functional>
#include "dobject.h"
#include "v_2ddrawer.h"
#include "d_eventbase.h"
#include "s_soundinternal.h"
#include "gamestate.h"
#include "zstring.h"
#include "c_cvars.h"

EXTERN_CVAR(Bool, inter_subtitles)

using CompletionFunc = std::function<void(bool)>;

void Job_Init();

enum
{
	SJ_BLOCKUI = 1,
};

enum
{
	ST_VOTE,
	ST_MUST_BE_SKIPPABLE,
	ST_UNSKIPPABLE,
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
bool ScreenJobValidate();

struct CutsceneDef;
bool StartCutscene(const char* s, int flags, const CompletionFunc& completion);
bool StartCutscene(CutsceneDef& cs, int flags, const CompletionFunc& completion_);
bool CanWipe();

VMFunction* LookupFunction(const char* qname, bool validate = true);
void CallCreateFunction(const char* qname, DObject* runner);
DObject* CreateRunner(bool clearbefore = true, int skipType = ST_VOTE);
void AddGenericVideo(DObject* runner, const FString& fn, int soundid, int fps);

struct CutsceneState
{
	DObject* runner;
	PClass* runnerclass;
	PType* runnerclasstype;
	CompletionFunc completion;
};

extern CutsceneState cutscene;
