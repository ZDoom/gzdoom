/*
** thingdef.cpp
**
** Actor definitions
**
**---------------------------------------------------------------------------
** Copyright 2002-2008 Christoph Oelckers
** Copyright 2004-2008 Randy Heit
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
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
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

#include "gi.h"
#include "actor.h"
#include "info.h"
#include "sc_man.h"
#include "tarray.h"
#include "w_wad.h"
#include "templates.h"
#include "r_defs.h"
#include "a_pickups.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "a_action.h"
#include "decallib.h"
#include "m_random.h"
#include "i_system.h"
#include "m_argv.h"
#include "p_local.h"
#include "doomerrors.h"
#include "a_weaponpiece.h"
#include "p_conversation.h"
#include "v_text.h"
#include "thingdef.h"
#include "codegeneration/thingdef_exp.h"
#include "a_sharedglobal.h"
#include "vmbuilder.h"
#include "stats.h"

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------
void InitThingdef();

// STATIC FUNCTION PROTOTYPES --------------------------------------------
PClassActor *QuestItemClasses[31];

//==========================================================================
//
// SetImplicitArgs
//
// Adds the parameters implied by the function flags.
//
//==========================================================================

void SetImplicitArgs(TArray<PType *> *args, TArray<DWORD> *argflags, PClass *cls, DWORD funcflags)
{
	// Must be called before adding any other arguments.
	assert(args == NULL || args->Size() == 0);
	assert(argflags == NULL || argflags->Size() == 0);

	if (funcflags & VARF_Method)
	{
		// implied self pointer
		if (args != NULL)		args->Push(NewClassPointer(cls)); 
		if (argflags != NULL)	argflags->Push(VARF_Implicit);
	}
	if (funcflags & VARF_Action)
	{
		// implied caller and callingstate pointers
		if (args != NULL)
		{
			args->Insert(0, NewClassPointer(RUNTIME_CLASS(AActor)));	// the caller must go before self due to an old design mistake.
			args->Push(TypeState);
		}
		if (argflags != NULL)
		{
			argflags->Push(VARF_Implicit);
			argflags->Push(VARF_Implicit);
		}
	}
}

//==========================================================================
//
// LoadActors
//
// Called from FActor::StaticInit()
//
//==========================================================================
void ParseScripts();
void ParseAllDecorate();

void LoadActors ()
{
	cycle_t timer;

	timer.Reset(); timer.Clock();
	FScriptPosition::ResetErrorCounter();

	InitThingdef();
	ParseScripts();
	ParseAllDecorate();

	FunctionBuildList.Build();

	if (FScriptPosition::ErrorCounter > 0)
	{
		I_Error("%d errors while parsing DECORATE scripts", FScriptPosition::ErrorCounter);
	}

	int errorcount = 0;
	for (auto ti : PClassActor::AllActorClasses)
	{
		if (ti->Size == TentativeClass)
		{
			Printf(TEXTCOLOR_RED "Class %s referenced but not defined\n", ti->TypeName.GetChars());
			errorcount++;
			continue;
		}

		if (GetDefaultByType(ti) == nullptr)
		{
			Printf(TEXTCOLOR_RED "No ActorInfo defined for class '%s'\n", ti->TypeName.GetChars());
			errorcount++;
			continue;
		}
	}
	if (errorcount > 0)
	{
		I_Error("%d errors during actor postprocessing", errorcount);
	}

	timer.Unclock();
	if (!batchrun) Printf("DECORATE parsing took %.2f ms\n", timer.TimeMS());
	// Base time: ~52 ms

	// Since these are defined in DECORATE now the table has to be initialized here.
	for (int i = 0; i < 31; i++)
	{
		char fmt[20];
		mysnprintf(fmt, countof(fmt), "QuestItem%d", i + 1);
		QuestItemClasses[i] = PClass::FindActor(fmt);
	}
}
