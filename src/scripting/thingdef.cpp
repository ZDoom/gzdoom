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

TDeletingArray<class FxExpression *> ActorDamageFuncs;


// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------
void InitThingdef();
void ParseDecorate(FScanner &ctx);

// STATIC FUNCTION PROTOTYPES --------------------------------------------
PClassActor *QuestItemClasses[31];

//==========================================================================
//
// Do some postprocessing after everything has been defined
//
//==========================================================================

static void DumpFunction(FILE *dump, VMScriptFunction *sfunc, const char *label, int labellen)
{
	const char *marks = "=======================================================";
	fprintf(dump, "\n%.*s %s %.*s", MAX(3, 38 - labellen / 2), marks, label, MAX(3, 38 - labellen / 2), marks);
	fprintf(dump, "\nInteger regs: %-3d  Float regs: %-3d  Address regs: %-3d  String regs: %-3d\nStack size: %d\n",
		sfunc->NumRegD, sfunc->NumRegF, sfunc->NumRegA, sfunc->NumRegS, sfunc->MaxParam);
	VMDumpConstants(dump, sfunc);
	fprintf(dump, "\nDisassembly @ %p:\n", sfunc->Code);
	VMDisasm(dump, sfunc->Code, sfunc->CodeSize, sfunc);
}

static void FinishThingdef()
{
	int errorcount = 0;
	unsigned i;
	int codesize = 0;
	FILE *dump = NULL;

	if (Args->CheckParm("-dumpdisasm")) dump = fopen("disasm.txt", "w");

	for (i = 0; i < StateTempCalls.Size(); ++i)
	{
		FStateTempCall *tcall = StateTempCalls[i];
		VMFunction *func = nullptr;

		assert(tcall->Code != NULL);

		// We don't know the return type in advance for anonymous functions.
		FCompileContext ctx(tcall->ActorClass, nullptr);
		tcall->Code = tcall->Code->Resolve(ctx);
		tcall->Proto = ctx.ReturnProto;

		// Make sure resolving it didn't obliterate it.
		if (tcall->Code != nullptr)
		{
			// Can we call this function directly without wrapping it in an
			// anonymous function? e.g. Are we passing any parameters to it?
			func = tcall->Code->GetDirectFunction();

			if (func == nullptr)
			{
				VMFunctionBuilder buildit(true);

				assert(tcall->Proto != nullptr);

				// Allocate registers used to pass parameters in.
				// self, stateowner, state (all are pointers)
				buildit.Registers[REGT_POINTER].Get(3);

				// Emit code
				tcall->Code->Emit(&buildit);

				VMScriptFunction *sfunc = buildit.MakeFunction();
				sfunc->NumArgs = NAP;
				
				// Generate prototype for this anonymous function
				TArray<PType *> args(3);
				SetImplicitArgs(&args, NULL, tcall->ActorClass, VARF_Method | VARF_Action);
				sfunc->Proto = NewPrototype(tcall->Proto->ReturnTypes, args);

				func = sfunc;

				if (dump != NULL)
				{
					char label[64];
					int labellen = mysnprintf(label, countof(label), "Function %s.States[%d] (*%d)",
						tcall->ActorClass->TypeName.GetChars(), tcall->FirstState, tcall->NumStates);
					DumpFunction(dump, sfunc, label, labellen);
					codesize += sfunc->CodeSize;
				}
			}

			delete tcall->Code;
			tcall->Code = nullptr;
			for (int k = 0; k < tcall->NumStates; ++k)
			{
				tcall->ActorClass->OwnedStates[tcall->FirstState + k].SetAction(func);
			}
		}
	}

	for (i = 0; i < PClassActor::AllActorClasses.Size(); i++)
	{
		PClassActor *ti = PClassActor::AllActorClasses[i];

		if (ti->Size == TentativeClass)
		{
			Printf(TEXTCOLOR_RED "Class %s referenced but not defined\n", ti->TypeName.GetChars());
			errorcount++;
			continue;
		}

		AActor *def = GetDefaultByType(ti);

		if (!def)
		{
			Printf(TEXTCOLOR_RED "No ActorInfo defined for class '%s'\n", ti->TypeName.GetChars());
			errorcount++;
			continue;
		}

		if (def->DamageFunc != nullptr)
		{
			FxDamageValue *dmg = (FxDamageValue *)ActorDamageFuncs[(uintptr_t)def->DamageFunc - 1];
			VMScriptFunction *sfunc;
			sfunc = dmg->GetFunction();
			if (sfunc == nullptr)
			{
				FCompileContext ctx(ti);
				dmg = static_cast<FxDamageValue *>(dmg->Resolve(ctx));

				if (dmg != nullptr)
				{
					VMFunctionBuilder buildit;
					buildit.Registers[REGT_POINTER].Get(1);		// The self pointer
					dmg->Emit(&buildit);
					sfunc = buildit.MakeFunction();
					sfunc->NumArgs = 1;
					sfunc->Proto = nullptr;		///FIXME: Need a proper prototype here
					// Save this function in case this damage value was reused
					// (which happens quite easily with inheritance).
					dmg->SetFunction(sfunc);
				}
			}
			def->DamageFunc = sfunc;

			if (dump != nullptr && sfunc != nullptr)
			{
				char label[64];
				int labellen = mysnprintf(label, countof(label), "Function %s.Damage",
					ti->TypeName.GetChars());
				DumpFunction(dump, sfunc, label, labellen);
				codesize += sfunc->CodeSize;
			}
		}
	}
	if (dump != NULL)
	{
		fprintf(dump, "\n*************************************************************************\n%i code bytes\n", codesize * 4);
		fclose(dump);
	}
	if (errorcount > 0)
	{
		I_Error("%d errors during actor postprocessing", errorcount);
	}

	ActorDamageFuncs.DeleteAndClear();
	StateTempCalls.DeleteAndClear();
}



//==========================================================================
//
// LoadActors
//
// Called from FActor::StaticInit()
//
//==========================================================================
void ParseScripts();

void LoadActors ()
{
	int lastlump, lump;
	cycle_t timer;

	timer.Reset(); timer.Clock();
	ActorDamageFuncs.Clear();
	InitThingdef();
	lastlump = 0;
	ParseScripts();

	FScriptPosition::ResetErrorCounter();
	while ((lump = Wads.FindLump ("DECORATE", &lastlump)) != -1)
	{
		FScanner sc(lump);
		ParseDecorate (sc);
	}
	FinishThingdef();
	if (FScriptPosition::ErrorCounter > 0)
	{
		I_Error("%d errors while parsing DECORATE scripts", FScriptPosition::ErrorCounter);
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
