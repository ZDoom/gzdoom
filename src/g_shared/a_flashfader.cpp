/*
** a_flashfader.cpp
** User settable screen blends
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

#include "a_sharedglobal.h"
#include "g_level.h"
#include "d_player.h"
#include "serializer.h"
#include "g_levellocals.h"

IMPLEMENT_CLASS(DFlashFader, false, true)

IMPLEMENT_POINTERS_START(DFlashFader)
	IMPLEMENT_POINTER(ForWho)
IMPLEMENT_POINTERS_END

DFlashFader::DFlashFader ()
{
}

DFlashFader::DFlashFader (float r1, float g1, float b1, float a1,
						  float r2, float g2, float b2, float a2,
						  float time, AActor *who, bool terminate)
	: TotalTics ((int)(time*TICRATE)), StartTic (level.time), ForWho (who)
{
	Blends[0][0]=r1; Blends[0][1]=g1; Blends[0][2]=b1; Blends[0][3]=a1;
	Blends[1][0]=r2; Blends[1][1]=g2; Blends[1][2]=b2; Blends[1][3]=a2;
	Terminate = terminate;
}

void DFlashFader::OnDestroy ()
{
	if (Terminate) Blends[1][3] = 0.f; // Needed in order to cancel out the secondary fade.
	SetBlend (1.f);
	Super::OnDestroy();
}

void DFlashFader::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("totaltics", TotalTics)
		("starttic", StartTic)
		("forwho", ForWho)
		.Array("blends", Blends[0], 8);
}

void DFlashFader::Tick ()
{
	if (ForWho == NULL || ForWho->player == NULL)
	{
		Destroy ();
		return;
	}
	if (level.time >= StartTic+TotalTics)
	{
		SetBlend (1.f);
		Destroy ();
		return;
	}
	SetBlend ((float)(level.time - StartTic) / (float)TotalTics);
}

void DFlashFader::SetBlend (float time)
{
	if (ForWho == NULL || ForWho->player == NULL)
	{
		return;
	}
	player_t *player = ForWho->player;
	float iT = 1.f - time;
	player->BlendR = Blends[0][0]*iT + Blends[1][0]*time;
	player->BlendG = Blends[0][1]*iT + Blends[1][1]*time;
	player->BlendB = Blends[0][2]*iT + Blends[1][2]*time;
	player->BlendA = Blends[0][3]*iT + Blends[1][3]*time;
}

void DFlashFader::Cancel ()
{
	TotalTics = level.time - StartTic;
	Blends[1][3] = 0.f;
}
