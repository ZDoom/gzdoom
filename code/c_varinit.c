#include <stdio.h>

#include "cmdlib.h"
#include "c_commands.h"
#include "c_cvars.h"
#include "dstrings.h"

extern cvar_t *CVars;

typedef struct {
	cvar_t **store; char *name; char *initval; int flags;
} cvarinit_t;

extern cvar_t	*gameskill,
				*WI_Percents,
				*I_ShowFPS,
				*r_drawplayersprites,
				*r_viewsize,
				*NotifyTime,
				*sv_cheats,
				*idmypos,
				*vid_defwidth,
				*vid_defheight,
				*vid_defdriver,
				*i_remapkeypad,
				*cl_run,
				*chat_macros[10],
				*showMessages,
				*snd_samplerate,
				*mouseSensitivity,
				*usemouse,
				*usejoystick,
				*screenblocks,
				*snd_channels,
				*snd_SfxVolume,
				*snd_MusicVolume;

static const cvarinit_t Initializers[] = {
	&snd_SfxVolume,			"snd_sfxvolume",		"8",	CVAR_ARCHIVE,
	&snd_MusicVolume,		"snd_musicvolume",		"9",	CVAR_ARCHIVE,
	&snd_channels,			"snd_channels",			"8",	CVAR_ARCHIVE,
	&screenblocks,			"screenblocks",			"10",	CVAR_ARCHIVE,
	&usemouse,				"use_mouse",			"1",	CVAR_ARCHIVE,
	&usejoystick,			"use_joystick",			"0",	CVAR_ARCHIVE,
	&mouseSensitivity,		"mouse_sensitivity",	"1.0",	CVAR_ARCHIVE,
	&gameskill,				"skill",				"2",	CVAR_SERVERINFO,
	&WI_Percents,			"wi_percents",			"1",	CVAR_ARCHIVE,
	&I_ShowFPS,				"vid_fps",				"0",	0,
	&r_drawplayersprites,	"r_drawplayersprites",	"1",	0,
	&r_viewsize,			"r_viewsize",			"0",	CVAR_NOSET,
	&NotifyTime,			"con_notifytime",		"3",	0,
	&sv_cheats,				"cheats",				"0",	CVAR_SERVERINFO,
	&idmypos,				"idmypos",				"0",	0,
	&vid_defwidth,			"vid_defwidth",			"320",	CVAR_ARCHIVE,
	&vid_defheight,			"vid_defheight",		"200",	CVAR_ARCHIVE,
	&vid_defdriver,			"vid_defdriver",		"NULL",	CVAR_ARCHIVE,
	&i_remapkeypad,			"i_remapkeypad",		"1",	CVAR_ARCHIVE,
	&cl_run,				"cl_run",				"0",	CVAR_ARCHIVE,
	&showMessages,			"show_messages",		"1",	CVAR_ARCHIVE,
	&snd_samplerate,		"snd_samplerate",		"44100",CVAR_ARCHIVE,

	&chat_macros[0],		"chatmacro0",			HUSTR_CHATMACRO0,	CVAR_ARCHIVE,
	&chat_macros[1],		"chatmacro1",			HUSTR_CHATMACRO1,	CVAR_ARCHIVE,
	&chat_macros[2],		"chatmacro2",			HUSTR_CHATMACRO2,	CVAR_ARCHIVE,
	&chat_macros[3],		"chatmacro3",			HUSTR_CHATMACRO3,	CVAR_ARCHIVE,
	&chat_macros[4],		"chatmacro4",			HUSTR_CHATMACRO4,	CVAR_ARCHIVE,
	&chat_macros[5],		"chatmacro5",			HUSTR_CHATMACRO5,	CVAR_ARCHIVE,
	&chat_macros[6],		"chatmacro6",			HUSTR_CHATMACRO6,	CVAR_ARCHIVE,
	&chat_macros[7],		"chatmacro7",			HUSTR_CHATMACRO7,	CVAR_ARCHIVE,
	&chat_macros[8],		"chatmacro8",			HUSTR_CHATMACRO8,	CVAR_ARCHIVE,
	&chat_macros[9],		"chatmacro9",			HUSTR_CHATMACRO9,	CVAR_ARCHIVE,
	NULL
};

void C_SetCVars (void)
{
	const cvarinit_t *init = Initializers;

	while (init->store) {
		*init->store = cvar (init->name, init->initval, init->flags);
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
