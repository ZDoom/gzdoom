/*
** d_netinfo.cpp
** Manages transport of user and "server" cvars across a network
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_netinf.h"
#include "d_net.h"
#include "d_protocol.h"
#include "d_player.h"
#include "c_dispatch.h"
#include "v_palette.h"
#include "v_video.h"
#include "i_system.h"
#include "r_state.h"
#include "sbar.h"
#include "gi.h"
#include "m_random.h"
#include "teaminfo.h"
#include "r_data/r_translate.h"
#include "templates.h"
#include "cmdlib.h"
#include "farchive.h"

static FRandom pr_pickteam ("PickRandomTeam");

extern bool st_firsttime;
EXTERN_CVAR (Bool, teamplay)

CVAR (Float,	autoaim,				5000.f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	name,					"Player",	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Color,	color,					0x40cf00,	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Int,		colorset,				0,			CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	skin,					"base",		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Int,		team,					TEAM_NONE,	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	gender,					"male",		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Bool,		neverswitchonpickup,	false,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Float,	movebob,				0.25f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Float,	stillbob,				0.f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	playerclass,			"Fighter",	CVAR_USERINFO | CVAR_ARCHIVE);

enum
{
	INFO_Name,
	INFO_Autoaim,
	INFO_Color,
	INFO_Skin,
	INFO_Team,
	INFO_Gender,
	INFO_NeverSwitchOnPickup,
	INFO_MoveBob,
	INFO_StillBob,
	INFO_PlayerClass,
	INFO_ColorSet,
};

const char *GenderNames[3] = { "male", "female", "other" };

static const char *UserInfoStrings[] =
{
	"name",
	"autoaim",
	"color",
	"skin",
	"team",
	"gender",
	"neverswitchonpickup",
	"movebob",
	"stillbob",
	"playerclass",
	"colorset",
	NULL
};

// Replace \ with %/ and % with %%
FString D_EscapeUserInfo (const char *str)
{
	FString ret;

	for (; *str != '\0'; ++str)
	{
		if (*str == '\\')
		{
			ret << '%' << '/';
		}
		else if (*str == '%')
		{
			ret << '%' << '%';
		}
		else
		{
			ret << *str;
		}
	}
	return ret;
}

// Replace %/ with \ and %% with %
FString D_UnescapeUserInfo (const char *str, size_t len)
{
	const char *end = str + len;
	FString ret;

	while (*str != '\0' && str < end)
	{
		if (*str == '%')
		{
			if (*(str + 1) == '/')
			{
				ret << '\\';
				str += 2;
				continue;
			}
			else if (*(str + 1) == '%')
			{
				str++;
			}
		}
		ret << *str++;
	}
	return ret;
}

int D_GenderToInt (const char *gender)
{
	if (!stricmp (gender, "female"))
		return GENDER_FEMALE;
	else if (!stricmp (gender, "other") || !stricmp (gender, "cyborg"))
		return GENDER_NEUTER;
	else
		return GENDER_MALE;
}

int D_PlayerClassToInt (const char *classname)
{
	if (PlayerClasses.Size () > 1)
	{
		for (unsigned int i = 0; i < PlayerClasses.Size (); ++i)
		{
			const PClass *type = PlayerClasses[i].Type;

			if (stricmp (type->Meta.GetMetaString (APMETA_DisplayName), classname) == 0)
			{
				return i;
			}
		}
		return -1;
	}
	else
	{
		return 0;
	}
}

void D_GetPlayerColor (int player, float *h, float *s, float *v, FPlayerColorSet **set)
{
	userinfo_t *info = &players[player].userinfo;
	FPlayerColorSet *colorset = NULL;
	int color;

	if (players[player].mo != NULL)
	{
		colorset = P_GetPlayerColorSet(players[player].mo->GetClass()->TypeName, info->colorset);
	}
	if (colorset != NULL)
	{
		color = GPalette.BaseColors[GPalette.Remap[colorset->RepresentativeColor]];
	}
	else
	{
		color = info->color;
	}

	RGBtoHSV (RPART(color)/255.f, GPART(color)/255.f, BPART(color)/255.f,
		h, s, v);

	if (teamplay && TeamLibrary.IsValidTeam(info->team) && !Teams[info->team].GetAllowCustomPlayerColor ())
	{
		// In team play, force the player to use the team's hue
		// and adjust the saturation and value so that the team
		// hue is visible in the final color.
		float ts, tv;
		int tcolor = Teams[info->team].GetPlayerColor ();

		RGBtoHSV (RPART(tcolor)/255.f, GPART(tcolor)/255.f, BPART(tcolor)/255.f,
			h, &ts, &tv);

		*s = clamp(ts + *s * 0.15f - 0.075f, 0.f, 1.f);
		*v = clamp(tv + *v * 0.5f - 0.25f, 0.f, 1.f);

		// Make sure not to pass back any colorset in teamplay.
		colorset = NULL;
	}
	if (set != NULL)
	{
		*set = colorset;
	}
}

// Find out which teams are present. If there is only one,
// then another team should be chosen at random.
//
// Otherwise, join whichever team has fewest players. If
// teams are tied for fewest players, pick one of those
// at random.

void D_PickRandomTeam (int player)
{
	static char teamline[8] = "\\team\\X";

	BYTE *foo = (BYTE *)teamline;
	teamline[6] = (char)D_PickRandomTeam() + '0';
	D_ReadUserInfoStrings (player, &foo, teamplay);
}

int D_PickRandomTeam ()
{
	for (unsigned int i = 0; i < Teams.Size (); i++)
	{
		Teams[i].m_iPresent = 0;
		Teams[i].m_iTies = 0;
	}

	int numTeams = 0;
	int team;

	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			if (TeamLibrary.IsValidTeam (players[i].userinfo.team))
			{
				if (Teams[players[i].userinfo.team].m_iPresent++ == 0)
				{
					numTeams++;
				}
			}
		}
	}

	if (numTeams < 2)
	{
		do
		{
			team = pr_pickteam() % Teams.Size ();
		} while (Teams[team].m_iPresent != 0);
	}
	else
	{
		int lowest = INT_MAX, lowestTie = 0;
		unsigned int i;

		for (i = 0; i < Teams.Size (); ++i)
		{
			if (Teams[i].m_iPresent > 0)
			{
				if (Teams[i].m_iPresent < lowest)
				{
					lowest = Teams[i].m_iPresent;
					lowestTie = 0;
					Teams[0].m_iTies = i;
				}
				else if (Teams[i].m_iPresent == lowest)
				{
					Teams[++lowestTie].m_iTies = i;
				}
			}
		}
		if (lowestTie == 0)
		{
			team = Teams[0].m_iTies;
		}
		else
		{
			team = Teams[pr_pickteam() % (lowestTie+1)].m_iTies;
		}
	}

	return team;
}

static void UpdateTeam (int pnum, int team, bool update)
{
	userinfo_t *info = &players[pnum].userinfo;

	if ((dmflags2 & DF2_NO_TEAM_SWITCH) && (alwaysapplydmflags || deathmatch) && TeamLibrary.IsValidTeam (info->team))
	{
		Printf ("Team changing has been disabled!\n");
		return;
	}

	int oldteam;

	if (!TeamLibrary.IsValidTeam (team))
	{
		team = TEAM_NONE;
	}
	oldteam = info->team;
	info->team = team;

	if (teamplay && !TeamLibrary.IsValidTeam (info->team))
	{ // Force players onto teams in teamplay mode
		info->team = D_PickRandomTeam ();
	}
	if (update && oldteam != info->team)
	{
		if (TeamLibrary.IsValidTeam (info->team))
			Printf ("%s joined the %s team\n", info->netname, Teams[info->team].GetName ());
		else
			Printf ("%s is now a loner\n", info->netname);
	}
	// Let the player take on the team's color
	R_BuildPlayerTranslation (pnum);
	if (StatusBar != NULL && StatusBar->GetPlayer() == pnum)
	{
		StatusBar->AttachToPlayer (&players[pnum]);
	}
	if (!TeamLibrary.IsValidTeam (info->team))
		info->team = TEAM_NONE;
}

int D_GetFragCount (player_t *player)
{
	if (!teamplay || !TeamLibrary.IsValidTeam (player->userinfo.team))
	{
		return player->fragcount;
	}
	else
	{
		// Count total frags for this player's team
		const int team = player->userinfo.team;
		int count = 0;

		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].userinfo.team == team)
			{
				count += players[i].fragcount;
			}
		}
		return count;
	}
}

void D_SetupUserInfo ()
{
	int i;
	userinfo_t *coninfo = &players[consoleplayer].userinfo;

	for (i = 0; i < MAXPLAYERS; i++)
		memset (&players[i].userinfo, 0, sizeof(userinfo_t));

	strncpy (coninfo->netname, name, MAXPLAYERNAME);
	if (teamplay && !TeamLibrary.IsValidTeam (team))
	{
		coninfo->team = D_PickRandomTeam ();
	}
	else
	{
		coninfo->team = team;
	}
	if (autoaim > 35.f || autoaim < 0.f)
	{
		coninfo->aimdist = ANGLE_1*35;
	}
	else
	{
		coninfo->aimdist = abs ((int)(autoaim * (float)ANGLE_1));
	}
	coninfo->color = color;
	coninfo->colorset = colorset;
	coninfo->skin = R_FindSkin (skin, 0);
	coninfo->gender = D_GenderToInt (gender);
	coninfo->neverswitch = neverswitchonpickup;
	coninfo->MoveBob = (fixed_t)(65536.f * movebob);
	coninfo->StillBob = (fixed_t)(65536.f * stillbob);
	coninfo->PlayerClass = D_PlayerClassToInt (playerclass);
	R_BuildPlayerTranslation (consoleplayer);
}

void D_UserInfoChanged (FBaseCVar *cvar)
{
	UCVarValue val;
	FString escaped_val;
	char foo[256];

	if (cvar == &autoaim)
	{
		if (autoaim < 0.0f)
		{
			autoaim = 0.0f;
			return;
		}
		else if (autoaim > 5000.0f)
		{
			autoaim = 5000.f;
			return;
		}
	}

	val = cvar->GetGenericRep (CVAR_String);
	escaped_val = D_EscapeUserInfo(val.String);
	if (4 + strlen(cvar->GetName()) + escaped_val.Len() > 256)
		I_Error ("User info descriptor too big");

	mysnprintf (foo, countof(foo), "\\%s\\%s", cvar->GetName(), escaped_val.GetChars());

	Net_WriteByte (DEM_UINFCHANGED);
	Net_WriteString (foo);
}

static const char *SetServerVar (char *name, ECVarType type, BYTE **stream, bool singlebit)
{
	FBaseCVar *var = FindCVar (name, NULL);
	UCVarValue value;

	if (singlebit)
	{
		if (var != NULL)
		{
			int bitdata;
			int mask;

			value = var->GetFavoriteRep (&type);
			if (type != CVAR_Int)
			{
				return NULL;
			}
			bitdata = ReadByte (stream);
			mask = 1 << (bitdata & 31);
			if (bitdata & 32)
			{
				value.Int |= mask;
			}
			else
			{
				value.Int &= ~mask;
			}
		}
	}
	else
	{
		switch (type)
		{
		case CVAR_Bool:		value.Bool = ReadByte (stream) ? 1 : 0;	break;
		case CVAR_Int:		value.Int = ReadLong (stream);			break;
		case CVAR_Float:	value.Float = ReadFloat (stream);		break;
		case CVAR_String:	value.String = ReadString (stream);		break;
		default: break;	// Silence GCC
		}
	}

	if (var)
	{
		var->ForceSet (value, type);
	}

	if (type == CVAR_String)
	{
		delete[] value.String;
	}

	if (var == &teamplay)
	{
		// Put players on teams if teamplay turned on
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				UpdateTeam (i, players[i].userinfo.team, true);
			}
		}
	}

	if (var)
	{
		value = var->GetGenericRep (CVAR_String);
		return value.String;
	}

	return NULL;
}

EXTERN_CVAR (Float, sv_gravity)

void D_SendServerInfoChange (const FBaseCVar *cvar, UCVarValue value, ECVarType type)
{
	size_t namelen;

	namelen = strlen (cvar->GetName ());

	Net_WriteByte (DEM_SINFCHANGED);
	Net_WriteByte ((BYTE)(namelen | (type << 6)));
	Net_WriteBytes ((BYTE *)cvar->GetName (), (int)namelen);
	switch (type)
	{
	case CVAR_Bool:		Net_WriteByte (value.Bool);		break;
	case CVAR_Int:		Net_WriteLong (value.Int);		break;
	case CVAR_Float:	Net_WriteFloat (value.Float);	break;
	case CVAR_String:	Net_WriteString (value.String);	break;
	default: break; // Silence GCC
	}
}

void D_SendServerFlagChange (const FBaseCVar *cvar, int bitnum, bool set)
{
	int namelen;

	namelen = (int)strlen (cvar->GetName ());

	Net_WriteByte (DEM_SINFCHANGEDXOR);
	Net_WriteByte ((BYTE)namelen);
	Net_WriteBytes ((BYTE *)cvar->GetName (), namelen);
	Net_WriteByte (BYTE(bitnum | (set << 5)));
}

void D_DoServerInfoChange (BYTE **stream, bool singlebit)
{
	const char *value;
	char name[64];
	int len;
	int type;

	len = ReadByte (stream);
	type = len >> 6;
	len &= 0x3f;
	if (len == 0)
		return;
	memcpy (name, *stream, len);
	*stream += len;
	name[len] = 0;

	if ( (value = SetServerVar (name, (ECVarType)type, stream, singlebit)) && netgame)
	{
		Printf ("%s changed to %s\n", name, value);
	}
}

void D_WriteUserInfoStrings (int i, BYTE **stream, bool compact)
{
	if (i >= MAXPLAYERS)
	{
		WriteByte (0, stream);
	}
	else
	{
		userinfo_t *info = &players[i].userinfo;

		const PClass *type = PlayerClasses[info->PlayerClass].Type;

		if (!compact)
		{
			sprintf (*((char **)stream),
					 "\\name\\%s"
					 "\\autoaim\\%g"
					 "\\color\\%x %x %x"
					 "\\colorset\\%d"
					 "\\skin\\%s"
					 "\\team\\%d"
					 "\\gender\\%s"
					 "\\neverswitchonpickup\\%d"
					 "\\movebob\\%g"
					 "\\stillbob\\%g"
					 "\\playerclass\\%s"
					 ,
					 D_EscapeUserInfo(info->netname).GetChars(),
					 (double)info->aimdist / (float)ANGLE_1,
					 RPART(info->color), GPART(info->color), BPART(info->color),
					 info->colorset,
					 D_EscapeUserInfo(skins[info->skin].name).GetChars(),
					 info->team,
					 info->gender == GENDER_FEMALE ? "female" :
						info->gender == GENDER_NEUTER ? "other" : "male",
					 info->neverswitch,
					 (float)(info->MoveBob) / 65536.f,
					 (float)(info->StillBob) / 65536.f,
					 info->PlayerClass == -1 ? "Random" :
						D_EscapeUserInfo(type->Meta.GetMetaString (APMETA_DisplayName)).GetChars()
					);
		}
		else
		{
			sprintf (*((char **)stream),
				"\\"
				"\\%s"			// name
				"\\%g"			// autoaim
				"\\%x %x %x"	// color
				"\\%s"			// skin
				"\\%d"			// team
				"\\%s"			// gender
				"\\%d"			// neverswitchonpickup
				"\\%g"			// movebob
				"\\%g"			// stillbob
				"\\%s"			// playerclass
				"\\%d"			// colorset
				,
				D_EscapeUserInfo(info->netname).GetChars(),
				(double)info->aimdist / (float)ANGLE_1,
				RPART(info->color), GPART(info->color), BPART(info->color),
				D_EscapeUserInfo(skins[info->skin].name).GetChars(),
				info->team,
				info->gender == GENDER_FEMALE ? "female" :
					info->gender == GENDER_NEUTER ? "other" : "male",
				info->neverswitch,
				(float)(info->MoveBob) / 65536.f,
				(float)(info->StillBob) / 65536.f,
				info->PlayerClass == -1 ? "Random" :
					D_EscapeUserInfo(type->Meta.GetMetaString (APMETA_DisplayName)).GetChars(),
				info->colorset
			);
		}
	}

	*stream += strlen (*((char **)stream)) + 1;
}

void D_ReadUserInfoStrings (int i, BYTE **stream, bool update)
{
	userinfo_t *info = &players[i].userinfo;
	const char *ptr = *((const char **)stream);
	const char *breakpt;
	FString value;
	bool compact;
	int infotype = -1;

	if (*ptr++ != '\\')
		return;

	compact = (*ptr == '\\') ? ptr++, true : false;

	if (i < MAXPLAYERS)
	{
		for (;;)
		{
			int j;

			breakpt = strchr (ptr, '\\');

			if (compact)
			{
				value = D_UnescapeUserInfo(ptr, breakpt != NULL ? breakpt - ptr : strlen(ptr));
				infotype++;
			}
			else
			{
				assert(breakpt != NULL);
				// A malicious remote machine could invalidate the above assert.
				if (breakpt == NULL)
				{
					break;
				}
				const char *valstart = breakpt + 1;
				if ( (breakpt = strchr (valstart, '\\')) != NULL )
				{
					value = D_UnescapeUserInfo(valstart, breakpt - valstart);
				}
				else
				{
					value = D_UnescapeUserInfo(valstart, strlen(valstart));
				}

				for (j = 0;
					 UserInfoStrings[j] && strnicmp (UserInfoStrings[j], ptr, valstart - ptr - 1) != 0;
					 ++j)
				{ }
				if (UserInfoStrings[j] == NULL)
				{
					infotype = -1;
				}
				else
				{
					infotype = j;
				}
			}
			switch (infotype)
			{
			case INFO_Autoaim: {
				double angles;

				angles = atof (value);
				if (angles > 35.f || angles < 0.f)
				{
						info->aimdist = ANGLE_1*35;
				}
				else
				{
						info->aimdist = abs ((int)(angles * (float)ANGLE_1));
				}
								}
				break;

			case INFO_Name:
				{
					char oldname[MAXPLAYERNAME+1];

					strcpy (oldname, info->netname);
					strncpy (info->netname, value, MAXPLAYERNAME);
					info->netname[MAXPLAYERNAME] = 0;
					CleanseString(info->netname);

					if (update && strcmp (oldname, info->netname) != 0)
					{
						Printf ("%s is now known as %s\n", oldname, info->netname);
					}
				}
				break;

			case INFO_Team:
				UpdateTeam (i, atoi(value), update);
				break;

			case INFO_Color:
			case INFO_ColorSet:
				if (infotype == INFO_Color)
				{
					info->color = V_GetColorFromString (NULL, value);
					info->colorset = -1;
				}
				else
				{
					info->colorset = atoi(value);
				}
				R_BuildPlayerTranslation (i);
				if (StatusBar != NULL && i == StatusBar->GetPlayer())
				{
					StatusBar->AttachToPlayer (&players[i]);
				}
				break;

			case INFO_Skin:
				info->skin = R_FindSkin (value, players[i].CurrentPlayerClass);
				if (players[i].mo != NULL)
				{
					if (players[i].cls != NULL &&
						!(players[i].mo->flags4 & MF4_NOSKIN) &&
						players[i].mo->state->sprite ==
						GetDefaultByType (players[i].cls)->SpawnState->sprite)
					{ // Only change the sprite if the player is using a standard one
						players[i].mo->sprite = skins[info->skin].sprite;
					}
				}
				// Rebuild translation in case the new skin uses a different range
				// than the old one.
				R_BuildPlayerTranslation (i);
				break;

			case INFO_Gender:
				info->gender = D_GenderToInt (value);
				break;

			case INFO_NeverSwitchOnPickup:
				if (value[0] >= '0' && value[0] <= '9')
				{
					info->neverswitch = atoi (value) ? true : false;
				}
				else if (stricmp (value, "true") == 0)
				{
					info->neverswitch = 1;
				}
				else
				{
					info->neverswitch = 0;
				}
				break;

			case INFO_MoveBob:
				info->MoveBob = (fixed_t)(atof (value) * 65536.f);
				break;

			case INFO_StillBob:
				info->StillBob = (fixed_t)(atof (value) * 65536.f);
				break;

			case INFO_PlayerClass:
				info->PlayerClass = D_PlayerClassToInt (value);
				break;

			default:
				break;
			}

			if (breakpt)
			{
				ptr = breakpt + 1;
			}
			else
			{
				break;
			}
		}
	}

	*stream += strlen (*((char **)stream)) + 1;
}

FArchive &operator<< (FArchive &arc, userinfo_t &info)
{
	if (arc.IsStoring ())
	{
		arc.Write (&info.netname, sizeof(info.netname));
	}
	else
	{
		arc.Read (&info.netname, sizeof(info.netname));
	}
	arc << info.team << info.aimdist << info.color 
		<< info.skin << info.gender << info.neverswitch
		<< info.colorset;
	return arc;
}

CCMD (playerinfo)
{
	if (argv.argc() < 2)
	{
		int i;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i])
			{
				Printf ("%d. %s\n", i, players[i].userinfo.netname);
			}
		}
	}
	else
	{
		int i = atoi (argv[1]);
		userinfo_t *ui = &players[i].userinfo;
		Printf ("Name:        %s\n",		ui->netname);
		Printf ("Team:        %s (%d)\n",	ui->team == TEAM_NONE ? "None" : Teams[ui->team].GetName (), ui->team);
		Printf ("Aimdist:     %d\n",		ui->aimdist);
		Printf ("Color:       %06x\n",		ui->color);
		Printf ("ColorSet:    %d\n",		ui->colorset);
		Printf ("Skin:        %s (%d)\n",	skins[ui->skin].name, ui->skin);
		Printf ("Gender:      %s (%d)\n",	GenderNames[ui->gender], ui->gender);
		Printf ("NeverSwitch: %d\n",		ui->neverswitch);
		Printf ("MoveBob:     %g\n",		ui->MoveBob/65536.f);
		Printf ("StillBob:    %g\n",		ui->StillBob/65536.f);
		Printf ("PlayerClass: %s (%d)\n",
			ui->PlayerClass == -1 ? "Random" : PlayerClasses[ui->PlayerClass].Type->Meta.GetMetaString (APMETA_DisplayName),
			ui->PlayerClass);
		if (argv.argc() > 2) PrintMiscActorInfo(players[i].mo);
	}
}
