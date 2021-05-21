#pragma once

// The current state of the game: whether we are
// playing, gazing at the intermission screen,
// the game final animation, or a demo. 
enum gamestate_t : int
{
	GS_LEVEL,
	GS_INTERMISSION,
	GS_FINALE,
	GS_DEMOSCREEN,
	GS_FULLCONSOLE,		// [RH]	Fullscreen console
	GS_HIDECONSOLE,		// [RH] The menu just did something that should hide fs console
	GS_STARTUP,			// [RH] Console is fullscreen, and game is just starting
	GS_TITLELEVEL,		// [RH] A combination of GS_LEVEL and GS_DEMOSCREEN
	GS_INTRO,
	GS_MENUSCREEN = GS_DEMOSCREEN,

	GS_FORCEWIPE = -1,
	GS_FORCEWIPEFADE = -2,
	GS_FORCEWIPEBURN = -3,
	GS_FORCEWIPEMELT = -4
};

extern	gamestate_t 	gamestate;

enum gameaction_t : int
{
	ga_nothing,
	ga_loadlevel, // not used.
	ga_newgame,
	ga_newgame2,
	ga_recordgame,
	ga_loadgame,
	ga_loadgamehidecon,
	ga_loadgameplaydemo,
	ga_autoloadgame,
	ga_savegame,
	ga_autosave,
	ga_playdemo,
	ga_completed,
	ga_slideshow,
	ga_worlddone,
	ga_screenshot,
	ga_togglemap,
	ga_fullconsole,
	ga_resumeconversation,
	ga_intro,
	ga_intermission,
};

extern	gameaction_t 	gameaction;
