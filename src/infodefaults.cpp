/*
** infodefaults.cpp
** Parses default lists to create default copies of actors
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

#include "actor.h"
#include "info.h"
#include "s_sound.h"

void FActorInfo::BuildDefaults ()
{
	if (Defaults == NULL)
	{
		Defaults = new BYTE[Class->SizeOf];
		if (Class == RUNTIME_CLASS(AActor))
		{
			memset (Defaults, 0, Class->SizeOf);
		}
		else
		{
			TypeInfo *parent;

			if (DefaultList[0] == ADEF_SkipSuper)
			{
				parent = RUNTIME_CLASS(AActor);
			}
			else
			{
				parent = Class->ParentType;
			}
			parent->ActorInfo->BuildDefaults ();
			memcpy (Defaults, parent->ActorInfo->Defaults, parent->SizeOf);
			if (Class->SizeOf > parent->SizeOf)
			{
				memset (Defaults + parent->SizeOf, 0, Class->SizeOf - parent->SizeOf);
			}
			if (parent == RUNTIME_CLASS(AActor) && OwnedStates == NULL)
			{ // Stateless actors that are direct subclasses of AActor
			  // have their spawnstate default to something that won't
			  // immediately destroy them.
				((AActor *)(Defaults))->SpawnState = &AActor::States[0];
			}
		}
		ApplyDefaults (Defaults);
	}
}

static FState *DefaultStates (TypeInfo *type)
{
	FState *states = type->ActorInfo->OwnedStates;

	if (states == NULL)
	{
		do
		{
			type = type->ParentType;
			states = type->ActorInfo->OwnedStates;
		} while (states == NULL && type != RUNTIME_CLASS(AActor));
	}
	return states;
}

void FActorInfo::ApplyDefaults (BYTE *defaults)
{
	AActor *actor = (AActor *)defaults;
	FState *states = DefaultStates (Class);
	const BYTE *parser = DefaultList;

	const char *datastr;
	int dataint = 0;
	FState *datastate = NULL;
	int datasound = 0;
	BYTE defnum;

	while ((defnum = *parser) != ADEF_EOL)
	{
		BYTE type = defnum >> 6;
		defnum &= 63;
		if (defnum <= ADEF_LastString)
		{
			datastr = (const char *)(parser + 1);
			parser = (const BYTE *)datastr + strlen (datastr) + 1;
			if (defnum <= ADEF_ActiveSound)
			{
				datasound = S_FindSound (datastr);
			}
		}
		else if (defnum >= ADEF_FirstCommand)
		{
			switch (defnum)
			{
			case ADEF_SkipSuper:
				parser += 2;
				break;
			}
		}
		else
		{
			switch (type)
			{
			case 0:		// byte
				dataint = parser[1];
				parser += 2;
				break;

			case 1:		// byte * FRACUNIT;
				dataint = parser[1] << FRACBITS;
				parser += 2;
				break;

			case 2:		// word
				dataint = *((WORD *)(parser + 1));
				parser += 3;
				break;

			case 3:		// long
				dataint = *((DWORD *)(parser + 1));
				parser += 5;
				break;
			}
			defnum &= ~ADEFTYPE_MASK;
			datastate = dataint < 255 ? states + dataint : NULL;
		}

		switch (defnum)
		{
		case ADEF_SeeSound:		actor->SeeSound = datasound;	break;
		case ADEF_AttackSound:	actor->AttackSound = datasound;	break;
		case ADEF_PainSound:	actor->PainSound = datasound;	break;
		case ADEF_DeathSound:	actor->DeathSound = datasound;	break;
		case ADEF_ActiveSound:	actor->ActiveSound = datasound;	break;
		case ADEF_Tag:			/* Do nothing for now */		break;

		case ADEF_XScale:		actor->xscale = dataint;		break;
		case ADEF_YScale:		actor->yscale = dataint;		break;
		case ADEF_SpawnHealth:	actor->health = dataint;		break;
		case ADEF_ReactionTime:	actor->reactiontime = dataint;	break;
		case ADEF_PainChance:	actor->PainChance = dataint;	break;
		case ADEF_Speed:		actor->Speed = dataint;			break;
		case ADEF_Radius:		actor->radius = dataint;		break;
		case ADEF_Height:		actor->height = dataint;		break;
		case ADEF_Mass:			actor->Mass = dataint;			break;
		case ADEF_Damage:		actor->damage = dataint;		break;
		case ADEF_Flags:		actor->flags = dataint;			break;
		case ADEF_Flags2:		actor->flags2 = dataint;		break;
		case ADEF_Flags3:		actor->flags3 = dataint;		break;
		case ADEF_Flags4:		actor->flags4 = dataint;		break;
		case ADEF_FlagsSet:		actor->flags |= dataint;		break;
		case ADEF_Flags2Set:	actor->flags2 |= dataint;		break;
		case ADEF_Flags3Set:	actor->flags3 |= dataint;		break;
		case ADEF_Flags4Set:	actor->flags4 |= dataint;		break;
		case ADEF_FlagsClear:	actor->flags &= ~dataint;		break;
		case ADEF_Flags2Clear:	actor->flags2 &= ~dataint;		break;
		case ADEF_Flags3Clear:	actor->flags3 &= ~dataint;		break;
		case ADEF_Flags4Clear:	actor->flags4 &= ~dataint;		break;
		case ADEF_Alpha:		actor->alpha = dataint;			break;
		case ADEF_RenderStyle:	actor->RenderStyle = dataint;	break;
		case ADEF_RenderFlags:	actor->renderflags = dataint;	break;
		case ADEF_Translation:	actor->Translation = dataint;	break;

		case ADEF_SpawnState:	actor->SpawnState = datastate;	break;
		case ADEF_SeeState:		actor->SeeState = datastate;	break;
		case ADEF_PainState:	actor->PainState = datastate;	break;
		case ADEF_MeleeState:	actor->MeleeState = datastate;	break;
		case ADEF_MissileState:	actor->MissileState = datastate; break;
		case ADEF_CrashState:	actor->CrashState = datastate;	break;
		case ADEF_DeathState:	actor->DeathState = datastate;	break;
		case ADEF_XDeathState:	actor->XDeathState = datastate;	break;
		case ADEF_BDeathState:	actor->BDeathState = datastate;	break;
		case ADEF_IDeathState:	actor->IDeathState = datastate;	break;
		case ADEF_RaiseState:	actor->RaiseState = datastate;	break;
		}
	}
	// Anything that is CountKill is also a monster, even if it doesn't specify it.
	if (actor->flags & MF_COUNTKILL)
	{
		actor->flags3 |= MF3_ISMONSTER;
	}
}
