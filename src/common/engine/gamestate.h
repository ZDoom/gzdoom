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
	GS_CUTSCENE,

	GS_MENUSCREEN = GS_DEMOSCREEN,

	GS_FORCEWIPE = -1,
	GS_FORCEWIPEFADE = -2,
	GS_FORCEWIPEBURN = -3,
	GS_FORCEWIPEMELT = -4
};


extern	gamestate_t 	gamestate;
