#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_netinf.h"
#include "d_net.h"
#include "d_protocol.h"
#include "c_dispatch.h"
#include "v_palette.h"
#include "v_video.h"
#include "i_system.h"
#include "r_draw.h"
#include "r_state.h"
#include "sbar.h"
#include "gi.h"

extern BOOL st_firsttime;

CVAR (Float,	autoaim,				5000.f,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	name,					"Player",	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Color,	color,					0x40cf00,	CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	skin,					"base",		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Int,		team,					255,		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (String,	gender,					"male",		CVAR_USERINFO | CVAR_ARCHIVE);
CVAR (Bool,		neverswitchonpickup,	false,		CVAR_USERINFO | CVAR_ARCHIVE);

enum
{
	INFO_Name,
	INFO_Autoaim,
	INFO_Color,
	INFO_Skin,
	INFO_Team,
	INFO_Gender,
	INFO_NeverSwitchOnPickup
};

const char *TeamNames[NUM_TEAMS] =
{
	"Red", "Blue", "Green", "Gold"
};

const char *GenderNames[3] = { "male", "female", "cyborg" };

static const char *UserInfoStrings[] =
{
	"name",
	"autoaim",
	"color",
	"skin",
	"team",
	"gender",
	"neverswitchonpickup",
	NULL
};

int D_GenderToInt (const char *gender)
{
	if (!stricmp (gender, "female"))
		return GENDER_FEMALE;
	else if (!stricmp (gender, "cyborg"))
		return GENDER_NEUTER;
	else
		return GENDER_MALE;
}

void D_SetupUserInfo (void)
{
	int i;
	userinfo_t *coninfo = &players[consoleplayer].userinfo;

	for (i = 0; i < MAXPLAYERS; i++)
		memset (&players[i].userinfo, 0, sizeof(userinfo_t));

	strncpy (coninfo->netname, name, MAXPLAYERNAME);
	coninfo->team = team;
	coninfo->aimdist = abs ((int)(autoaim * (float)ANGLE_1));
	if (coninfo->aimdist > ANGLE_1*32)
	{
		coninfo->aimdist = ANGLE_1*32;
	}
	coninfo->color = color;
	coninfo->skin = R_FindSkin (skin);
	coninfo->gender = D_GenderToInt (gender);
	coninfo->neverswitch = neverswitchonpickup;
	R_BuildPlayerTranslation (consoleplayer, coninfo->color);
}

void D_UserInfoChanged (FBaseCVar *cvar)
{
	UCVarValue val;
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
	if (4 + strlen (cvar->GetName ()) + strlen (val.String) > 256)
		I_Error ("User info descriptor too big");

	sprintf (foo, "\\%s\\%s", cvar->GetName (), val.String);

	Net_WriteByte (DEM_UINFCHANGED);
	Net_WriteString (foo);
}

static const char *SetServerVar (char *name, ECVarType type, byte **stream)
{
	FBaseCVar *var = FindCVar (name, NULL);
	UCVarValue value;

	switch (type)
	{
	case CVAR_Bool:		value.Bool = ReadByte (stream) ? 1 : 0;	break;
	case CVAR_Int:		value.Int = ReadLong (stream);			break;
	case CVAR_Float:	value.Float = ReadFloat (stream);		break;
	case CVAR_String:	value.String = ReadString (stream);		break;
	default: break;	// Silence GCC
	}

	if (var)
	{
		var->ForceSet (value, type);
	}

	if (type == CVAR_String)
	{
		delete[] value.String;
	}

	if (var)
	{
		value = var->GetGenericRep (CVAR_String);
		return value.String;
	}

	return NULL;
}

void D_SendServerInfoChange (const FBaseCVar *cvar, UCVarValue value, ECVarType type)
{
	int namelen;

	namelen = strlen (cvar->GetName ());

	Net_WriteByte (DEM_SINFCHANGED);
	Net_WriteByte (namelen | (type << 6));
	Net_WriteBytes ((BYTE *)cvar->GetName (), namelen);
	switch (type)
	{
	case CVAR_Bool:		Net_WriteByte (value.Bool);		break;
	case CVAR_Int:		Net_WriteLong (value.Int);		break;
	case CVAR_Float:	Net_WriteFloat (value.Float);	break;
	case CVAR_String:	Net_WriteString (value.String);	break;
	default: break; // Silence GCC
	}
}

void D_DoServerInfoChange (byte **stream)
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

	if ( (value = SetServerVar (name, (ECVarType)type, stream)) && netgame)
	{
		Printf ("%s changed to %s\n", name, value);
	}
}

void D_WriteUserInfoStrings (int i, byte **stream, bool compact)
{
	if (i >= MAXPLAYERS)
	{
		WriteByte (0, stream);
	}
	else
	{
		userinfo_t *info = &players[i].userinfo;

		if (!compact)
		{
			sprintf (*((char **)stream),
					 "\\name\\%s"
					 "\\autoaim\\%g"
					 "\\color\\%x %x %x"
					 "\\skin\\%s"
					 "\\team\\%d"
					 "\\gender\\%s"
					 "\\neverswitchonpickup\\%d",
					 info->netname,
					 (double)info->aimdist / (float)ANGLE_1,
					 RPART(info->color), GPART(info->color), BPART(info->color),
					 skins[info->skin].name, info->team,
					 info->gender == GENDER_FEMALE ? "female" :
						info->gender == GENDER_NEUTER ? "cyborg" : "male",
					 info->neverswitch
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
				,
				info->netname,
				(double)info->aimdist / (float)ANGLE_1,
				RPART(info->color), GPART(info->color), BPART(info->color),
				skins[info->skin].name,
				info->team,
				info->gender == GENDER_FEMALE ? "female" :
					info->gender == GENDER_NEUTER ? "cyborg" : "male",
				info->neverswitch
			);
		}
	}

	*stream += strlen (*((char **)stream)) + 1;
}

void D_ReadUserInfoStrings (int i, byte **stream, bool update)
{
	userinfo_t *info = &players[i].userinfo;
	char *ptr = *((char **)stream);
	char *breakpt;
	char *value;
	bool compact;
	int infotype = -1;

	if (*ptr++ != '\\')
		return;

	compact = (*ptr == '\\') ? ptr++, true : false;

	if (i < MAXPLAYERS)
	{
		for (;;)
		{
			breakpt = strchr (ptr, '\\');

			if (breakpt != NULL)
				*breakpt = 0;

			if (compact)
			{
				value = ptr;
				infotype++;
			}
			else
			{
				value = breakpt + 1;
				if ( (breakpt = strchr (value, '\\')) )
					*breakpt = 0;

				int j = 0;
				while (UserInfoStrings[j] && stricmp (UserInfoStrings[j], ptr) != 0)
					j++;
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
			case INFO_Autoaim:
				info->aimdist = abs ((int)(atof (value) * (float)ANGLE_1));
				if (info->aimdist > ANGLE_1*32)
				{
					info->aimdist = ANGLE_1*32;
				}
				break;

			case INFO_Name:
				{
					char oldname[MAXPLAYERNAME+1];

					strncpy (oldname, info->netname, MAXPLAYERNAME);
					oldname[MAXPLAYERNAME] = 0;
					strncpy (info->netname, value, MAXPLAYERNAME);
					info->netname[MAXPLAYERNAME] = 0;

					if (update)
						Printf ("%s is now known as %s\n", oldname, info->netname);
				}
				break;

			case INFO_Team:
				info->team = atoi (value);
				if ((unsigned)info->team >= NUM_TEAMS)
					info->team = TEAM_None;
				if (update)
				{
					if (info->team != TEAM_None)
						Printf ("%s is now on %s\n", info->netname, TeamNames[info->team]);
					else
						Printf ("%s is now a loner\n", info->netname);
				}
				break;

			case INFO_Color:
				info->color = V_GetColorFromString (NULL, value);
				R_BuildPlayerTranslation (i, info->color);
				if (StatusBar != NULL && i == displayplayer)
				{
					StatusBar->AttachToPlayer (&players[i]);
				}
				break;

			case INFO_Skin:
				info->skin = R_FindSkin (value);
				if (players[i].mo != NULL)
				{
					players[i].mo->sprite = skins[info->skin].sprite;
					players[i].mo->skin = &skins[info->skin];
				}
				if (StatusBar != NULL && i == displayplayer)
				{
					StatusBar->SetFace (&skins[info->skin]);
				}
				break;

			case INFO_Gender:
				info->gender = D_GenderToInt (value);
				break;

			case INFO_NeverSwitchOnPickup:
				info->neverswitch = atoi (value) ? true : false;
				break;

			default:
				break;
			}

			if (!compact)
			{
				*(value - 1) = '\\';
			}
			if (breakpt)
			{
				*breakpt = '\\';
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
		arc.Write (&info.team, sizeof(info.team));
	}
	else
	{
		arc.Read (&info.netname, sizeof(info.netname));
		arc.Read (&info.team, sizeof(info.team));
	}
	arc << info.aimdist << info.color << info.skin << info.gender << info.neverswitch;
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
		Printf ("Name:        %s\n", players[i].userinfo.netname);
		Printf ("Team:        %d\n", players[i].userinfo.team);
		Printf ("Aimdist:     %d\n", players[i].userinfo.aimdist);
		Printf ("Color:       %06x\n", players[i].userinfo.color);
		Printf ("Skin:        %d\n", players[i].userinfo.skin);
		Printf ("Gender:      %d\n", players[i].userinfo.gender);
		Printf ("NeverSwitch: %d\n", players[i].userinfo.neverswitch);
	}
}
