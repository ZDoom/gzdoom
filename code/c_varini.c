#include <stdio.h>

#include "cmdlib.h"
#include "c_cmds.h"
#include "c_cvars.h"
#include "dstrings.h"

extern cvar_t *CVars;

typedef struct {
	cvar_t **store; char *name; char *initval; int flags;
} cvarinit_t;

extern cvar_t	*gammalevel,
				*st_scale,
				*gameskill,
				*crosshair,
				*WI_Percents,
				*r_drawplayersprites,
				*r_drawfuzz,
				*r_viewsize,
				*r_detail,
				*r_stretchsky,
				*r_particles,
				*cl_bloodtype,
				*cl_pufftype,
				*cl_rockettrails,
				*con_notifytime,
				*con_midtime,
				*con_scaletext,
				*sv_cheats,
				*idmypos,
				*vid_defwidth,
				*vid_defheight,
				*vid_defid,
				*i_remapkeypad,
				*cl_run,
				*chat_macros[10],
				*showMessages,
				*nobfgaim,
				*chasedemo,

				*snd_surround,
				*snd_samplerate,
				*snd_pitched,
				*snd_channels,
				*snd_SfxVolume,
				*snd_MusicVolume,
				*snd_MidiVolume,
				*noisedebug,

				*sv_gravity,
				*sv_friction,
				*mouseSensitivity,
				*invertmouse,
				*freelook,
				*lookspring,
				*lookstrafe,
				*m_pitch,
				*m_yaw,
				*m_forward,
				*m_side,
				*usemouse,
				*usejoystick,
				*screenblocks,
				*developer,

				*am_rotate,
				*am_overlay,
				*am_showsecrets,
				*am_showmonsters,
				*am_showtime,
				*am_usecustomcolors,
				/* Automap colors */
				*am_backcolor,
				*am_yourcolor,
				*am_wallcolor,
				*am_tswallcolor,
				*am_fdwallcolor,
				*am_cdwallcolor,
				*am_thingcolor,
				*am_gridcolor,
				*am_xhaircolor,
				*am_notseencolor,
				*am_lockedcolor,
				*am_ovyourcolor,
				*am_ovwallcolor,
				*am_ovthingcolor,
				*am_ovotherwallscolor,
				*am_ovunseencolor,
			
				*dimamount,
				*dimcolor,
				
				*teamplay,
				*deathmatch,
				*dmflagsvar,
				*fraglimit,
				*timelimit,
				
				*autoaim,
				*name,
				*color,
				*skin,
				*team,
				*gender,
				*neverswitchonpickup,

				*chase_height,
				*chase_dist,

				*boom_pushers,
				*boom_friction,

				*splashfactor,
				*testgibs;
				


static const cvarinit_t Initializers[] = {
	{ &gammalevel,			"gamma",				"1",				CVAR_ARCHIVE|CVAR_CALLBACK },
	{ &testgibs,			"testgibs",				"0",				0 },
	{ &st_scale,			"st_scale",				"0",				CVAR_ARCHIVE|CVAR_CALLBACK },
	{ &dimamount,			"dimamount",			"1",				CVAR_ARCHIVE },
	{ &dimcolor,			"dimcolor",				"ff d7 00",			CVAR_ARCHIVE },
	{ &crosshair,			"crosshair",			"0",				CVAR_ARCHIVE },
	{ &developer,			"developer",			"0",				0 },
	{ &chasedemo,			"chasedemo",			"0",				0 },

	{ &snd_surround,		"snd_surround",			"1",				CVAR_ARCHIVE },
	{ &snd_SfxVolume,		"snd_sfxvolume",		"8",				CVAR_ARCHIVE|CVAR_CALLBACK },
	{ &snd_MusicVolume,		"snd_musicvolume",		"9",				CVAR_ARCHIVE|CVAR_CALLBACK },
	{ &snd_MidiVolume,		"snd_midivolume",		"0.5",				CVAR_ARCHIVE|CVAR_CALLBACK },
	{ &snd_channels,		"snd_channels",			"8",				CVAR_ARCHIVE },
	{ &snd_pitched,			"snd_pitched",			"0",				CVAR_ARCHIVE },
	{ &snd_samplerate,		"snd_samplerate",		"44100",			CVAR_ARCHIVE },
	{ &noisedebug,			"noise",				"0",				0 },

	{ &r_particles,			"r_particles",			"1",				0 },
	{ &cl_bloodtype,		"cl_bloodtype",			"0",				CVAR_ARCHIVE },
	{ &cl_pufftype,			"cl_pufftype",			"0",				CVAR_ARCHIVE },
	{ &cl_rockettrails,		"cl_rockettrails",		"1",				CVAR_ARCHIVE },

	{ &screenblocks,		"screenblocks",			"10",				CVAR_ARCHIVE|CVAR_CALLBACK },
	{ &usemouse,			"use_mouse",			"1",				CVAR_ARCHIVE },
	{ &usejoystick,			"use_joystick",			"0",				CVAR_ARCHIVE },
	{ &sv_gravity,			"sv_gravity",			"800",				CVAR_SERVERINFO },
	{ &sv_friction,			"sv_friction",			"0.90625",			CVAR_SERVERINFO },
	{ &mouseSensitivity,	"mouse_sensitivity",	"1.0",				CVAR_ARCHIVE },
	{ &invertmouse,			"invertmouse",			"0",				CVAR_ARCHIVE },
	{ &freelook,			"freelook",				"0",				CVAR_ARCHIVE },
	{ &lookspring,			"lookspring",			"1",				CVAR_ARCHIVE },
	{ &lookstrafe,			"lookstrafe",			"0",				CVAR_ARCHIVE },
	{ &m_pitch,				"m_pitch",				"1.0",				CVAR_ARCHIVE },
	{ &m_yaw,				"m_yaw",				"1.0",				CVAR_ARCHIVE },
	{ &m_forward,			"m_forward",			"1.0",				CVAR_ARCHIVE },
	{ &m_side,				"m_side",				"2.0",				CVAR_ARCHIVE },
	{ &gameskill,			"skill",				"2",				CVAR_SERVERINFO|CVAR_LATCH },
	{ &WI_Percents,			"wi_percents",			"1",				CVAR_ARCHIVE },
	{ &r_drawplayersprites,	"r_drawplayersprites",	"1",				0 },
	{ &r_drawfuzz,			"r_drawfuzz",			"1",				CVAR_ARCHIVE },
	{ &r_viewsize,			"r_viewsize",			"0",				CVAR_NOSET },
	{ &r_detail,			"r_detail",				"0",				CVAR_ARCHIVE|CVAR_CALLBACK },
	{ &r_stretchsky,		"r_stretchsky",			"1",				CVAR_ARCHIVE|CVAR_CALLBACK },
	{ &con_notifytime,		"con_notifytime",		"3",				CVAR_ARCHIVE },
	{ &con_midtime,			"con_midtime",			"3",				CVAR_ARCHIVE },
	{ &con_scaletext,		"con_scaletext",		"0",				CVAR_ARCHIVE },
	{ &sv_cheats,			"cheats",				"0",				CVAR_SERVERINFO|CVAR_LATCH },
	{ &idmypos,				"idmypos",				"0",				0 },
	{ &vid_defwidth,		"vid_defwidth",			"320",				CVAR_ARCHIVE },
	{ &vid_defheight,		"vid_defheight",		"200",				CVAR_ARCHIVE },
	{ &vid_defid,			"vid_defid",			"INDEX8",			CVAR_ARCHIVE },
	{ &i_remapkeypad,		"i_remapkeypad",		"1",				CVAR_ARCHIVE },
	{ &cl_run,				"cl_run",				"0",				CVAR_ARCHIVE },
	{ &showMessages,		"show_messages",		"1",				CVAR_ARCHIVE },

	{ &chat_macros[0],		"chatmacro0",			HUSTR_CHATMACRO0,	CVAR_ARCHIVE },
	{ &chat_macros[1],		"chatmacro1",			HUSTR_CHATMACRO1,	CVAR_ARCHIVE },
	{ &chat_macros[2],		"chatmacro2",			HUSTR_CHATMACRO2,	CVAR_ARCHIVE },
	{ &chat_macros[3],		"chatmacro3",			HUSTR_CHATMACRO3,	CVAR_ARCHIVE },
	{ &chat_macros[4],		"chatmacro4",			HUSTR_CHATMACRO4,	CVAR_ARCHIVE },
	{ &chat_macros[5],		"chatmacro5",			HUSTR_CHATMACRO5,	CVAR_ARCHIVE },
	{ &chat_macros[6],		"chatmacro6",			HUSTR_CHATMACRO6,	CVAR_ARCHIVE },
	{ &chat_macros[7],		"chatmacro7",			HUSTR_CHATMACRO7,	CVAR_ARCHIVE },
	{ &chat_macros[8],		"chatmacro8",			HUSTR_CHATMACRO8,	CVAR_ARCHIVE },
	{ &chat_macros[9],		"chatmacro9",			HUSTR_CHATMACRO9,	CVAR_ARCHIVE },

	{ &am_rotate,			"am_rotate",			"0",				CVAR_ARCHIVE },
	{ &am_overlay,			"am_overlay",			"0",				CVAR_ARCHIVE },
	{ &am_showsecrets,		"am_showsecrets",		"1",				CVAR_ARCHIVE },
	{ &am_showmonsters,		"am_showmonsters",		"1",				CVAR_ARCHIVE },
	{ &am_showtime,			"am_showtime",			"1",				CVAR_ARCHIVE },
	{ &am_usecustomcolors,	"am_usecustomcolors",	"1",				CVAR_ARCHIVE },
	{ &am_backcolor,		"am_backcolor",			"6c 54 40",			CVAR_ARCHIVE },
	{ &am_yourcolor,		"am_yourcolor",			"fc e8 d8",			CVAR_ARCHIVE },
	{ &am_wallcolor,		"am_wallcolor",			"2c 18 08",			CVAR_ARCHIVE },
	{ &am_tswallcolor,		"am_tswallcolor",		"88 88 88",			CVAR_ARCHIVE },
	{ &am_fdwallcolor,		"am_fdwallcolor",		"88 70 58",			CVAR_ARCHIVE },
	{ &am_cdwallcolor,		"am_cdwallcolor",		"4c 38 20",			CVAR_ARCHIVE },
	{ &am_thingcolor,		"am_thingcolor",		"fc fc fc",			CVAR_ARCHIVE },
	{ &am_gridcolor,		"am_gridcolor",			"8b 5a 2b",			CVAR_ARCHIVE },
	{ &am_xhaircolor,		"am_xhaircolor",		"80 80 80",			CVAR_ARCHIVE },
	{ &am_notseencolor,		"am_notseencolor",		"6c 6c 6c",			CVAR_ARCHIVE },
	{ &am_lockedcolor,		"am_lockedcolor",		"00 00 98",			CVAR_ARCHIVE },
	{ &am_ovyourcolor,		"am_ovyourcolor",		"fc e8 d8",			CVAR_ARCHIVE },
	{ &am_ovwallcolor,		"am_ovwallcolor",		"00 ff 00",			CVAR_ARCHIVE },
	{ &am_ovthingcolor,		"am_ovthingcolor",		"e8 88 00",			CVAR_ARCHIVE },
	{ &am_ovotherwallscolor,"am_ovotherwallscolor",	"00 88 44",			CVAR_ARCHIVE },
	{ &am_ovunseencolor,	"am_ovunseencolor",		"00 22 6e",			CVAR_ARCHIVE },

	{ &teamplay,			"teamplay",				"0",				CVAR_SERVERINFO },
	{ &deathmatch,			"deathmatch",			"0",				CVAR_SERVERINFO|CVAR_LATCH },
	{ &dmflagsvar,			"dmflags",				"0",				CVAR_SERVERINFO|CVAR_CALLBACK },
	{ &timelimit,			"timelimit",			"0",				CVAR_SERVERINFO },
	{ &fraglimit,			"fraglimit",			"0",				CVAR_SERVERINFO },
	{ &nobfgaim,			"nobfgaim",				"0",				CVAR_SERVERINFO },

	{ &autoaim,				"autoaim",				"5000",				CVAR_USERINFO|CVAR_ARCHIVE },
	{ &name,				"name",					"Player",			CVAR_USERINFO|CVAR_ARCHIVE },
	{ &color,				"color",				"40 cf 00",			CVAR_USERINFO|CVAR_ARCHIVE },
	{ &skin,				"skin",					"base",				CVAR_USERINFO|CVAR_ARCHIVE },
	{ &team,				"team",					"",					CVAR_USERINFO|CVAR_ARCHIVE },
	{ &gender,				"gender",				"male",				CVAR_USERINFO|CVAR_ARCHIVE },
	{ &neverswitchonpickup,	"neverswitchonpickup",	"0",				CVAR_USERINFO|CVAR_ARCHIVE },

	{ &chase_height,		"chase_height",			"-8",				CVAR_ARCHIVE },
	{ &chase_dist,			"chase_dist",			"90",				CVAR_ARCHIVE },

	{ &boom_friction,		"var_friction",			"1",				CVAR_SERVERINFO },
	{ &boom_pushers,		"var_pushers",			"1",				CVAR_SERVERINFO },

	{ &splashfactor,		"splashfactor",			"1.0",				CVAR_SERVERINFO|CVAR_CALLBACK },

	{ NULL }
};

void C_SetCVars (void)
{
	const cvarinit_t *init = Initializers;

	while (init->store) {
		*init->store = cvar (init->name, init->initval, init->flags);
		init++;
	}
}

void C_SetCVarsToDefaults (void)
{
	const cvarinit_t *init = Initializers;

	while (init->store) {
		// Only default save-able cvars
		if (init->flags & CVAR_ARCHIVE)
			cvar_set (init->name, init->initval);
		init++;
	}
}

void C_ArchiveCVars (void *f)
{
	cvar_t *cvar;

	cvar = CVars;

	while (cvar) {
		if (cvar->flags & CVAR_ARCHIVE) {
			fprintf ((FILE *)f, "set %s \"%s\"\n", cvar->name, cvar->string);
		}
		cvar = cvar->next;
	}
}
