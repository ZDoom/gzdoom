/*
** a_secrettrigger.cpp
** A thing that counts toward the secret count when activated
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

#include "actor.h"
#include "g_level.h"
#include "c_console.h"
#include "info.h"
#include "s_sound.h"
#include "d_player.h"
#include "doomstat.h"
#include "v_font.h"
#include "p_spec.h"

class ASecretTrigger : public AActor
{
	DECLARE_CLASS (ASecretTrigger, AActor)
public:
	void PostBeginPlay ();
	void Activate (AActor *activator);
};

IMPLEMENT_CLASS (ASecretTrigger)

void ASecretTrigger::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	level.total_secrets++;
}

void ASecretTrigger::Activate (AActor *activator)
{
	P_GiveSecret(activator, args[0] <= 1, (args[0] == 0 || args[0] == 2), -1);
	Destroy ();
}

