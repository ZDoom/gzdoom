/*
** d_netinf.h
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

#ifndef __D_NETINFO_H__
#define __D_NETINFO_H__

#include "c_cvars.h"

EXTERN_CVAR (Float, autoaim)

int D_GenderToInt (const char *gender);
extern const char *GenderNames[3];

int D_PlayerClassToInt (const char *classname);

void D_SetupUserInfo (void);

void D_UserInfoChanged (FBaseCVar *info);

void D_SendServerInfoChange (const FBaseCVar *cvar, UCVarValue value, ECVarType type);
void D_SendServerFlagChange (const FBaseCVar *cvar, int bitnum, bool set);
void D_DoServerInfoChange (BYTE **stream, bool singlebit);

void D_WriteUserInfoStrings (int player, BYTE **stream, bool compact=false);
void D_ReadUserInfoStrings (int player, BYTE **stream, bool update);

struct FPlayerColorSet;
void D_GetPlayerColor (int player, float *h, float *s, float *v, FPlayerColorSet **colorset);
void D_PickRandomTeam (int player);
int D_PickRandomTeam ();
class player_t;
int D_GetFragCount (player_t *player);

#endif //__D_CLIENTINFO_H__
