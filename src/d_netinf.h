/*
** d_netinf.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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

#ifndef __D_NETINFO_H__
#define __D_NETINFO_H__

#include "c_cvars.h"

EXTERN_CVAR (Float, autoaim)

#define MAXPLAYERNAME	15

enum
{
	GENDER_MALE,
	GENDER_FEMALE,
	GENDER_NEUTER
};

enum
{
	PLAYERCLASS_Random = -1,
	PLAYERCLASS_Fighter,
	PLAYERCLASS_Cleric,
	PALYERCLASS_Mage
};
extern const char *PlayerClassNames[4];

int D_GenderToInt (const char *gender);
extern const char *GenderNames[3];

struct userinfo_s
{
	char		netname[MAXPLAYERNAME+1];
	int			team;
	int			aimdist;
	int			color;
	int			skin;
	int			gender;
	bool		neverswitch;
	fixed_t		MoveBob, StillBob;
	int			PlayerClass;
};
typedef struct userinfo_s userinfo_t;

enum ETeams
{
	TEAM_Red,
	TEAM_Blue,
	TEAM_Green,
	TEAM_Gold,

	NUM_TEAMS,
	TEAM_None		= 255
};

extern const char *TeamNames[NUM_TEAMS];
extern float TeamHues[NUM_TEAMS];

FArchive &operator<< (FArchive &arc, userinfo_t &info);

void D_SetupUserInfo (void);

void D_UserInfoChanged (FBaseCVar *info);

void D_SendServerInfoChange (const FBaseCVar *cvar, UCVarValue value, ECVarType type);
void D_DoServerInfoChange (byte **stream);

void D_WriteUserInfoStrings (int player, byte **stream, bool compact=false);
void D_ReadUserInfoStrings (int player, byte **stream, bool update);

void D_GetPlayerColor (int player, float *h, float *s, float *v);
void D_PickRandomTeam (int player);
int D_PickRandomTeam ();
class player_s;
int D_GetFragCount (player_s *player);

#endif //__D_CLIENTINFO_H__
