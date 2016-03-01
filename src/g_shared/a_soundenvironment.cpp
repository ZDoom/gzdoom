/*
** a_soundenvironment.cpp
** Actor that controls the reverb settings in its zone
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

#include "info.h"
#include "r_defs.h"
#include "s_sound.h"
#include "r_state.h"

class ASoundEnvironment : public AActor
{
	DECLARE_CLASS (ASoundEnvironment, AActor)
public:
	void PostBeginPlay ();
	void Deactivate (AActor *activator);
	void Activate (AActor *deactivator);
};

IMPLEMENT_CLASS (ASoundEnvironment)

void ASoundEnvironment::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	if (!(flags2 & MF2_DORMANT))
	{
		Activate (this);
	}
}

void ASoundEnvironment::Activate (AActor *activator)
{
	zones[Sector->ZoneNumber].Environment = S_FindEnvironment ((args[0]<<8) | (args[1]));
}

// Deactivate just exists so that you can flag the thing as dormant in an editor
// and not have it take effect. This is so you can use multiple environments in
// a single zone, with only one set not-dormant, so you know which one will take
// effect at the start.
void ASoundEnvironment::Deactivate (AActor *deactivator)
{
	flags2 |= MF2_DORMANT;
}
