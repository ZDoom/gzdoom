#include "actor.h"
#include "info.h"

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

	TypeInfo *classtype;
	const char *datastr;
	int dataint;
	FState *datastate;
	BYTE defnum;

	while ((defnum = *parser) != ADEF_EOL)
	{
		BYTE type = defnum >> 6;
		defnum &= 63;

		if (defnum <= ADEF_LastString)
		{
			datastr = (const char *)(parser + 1);
			parser = (const BYTE *)datastr + strlen (datastr) + 1;
		}
		else if (defnum >= ADEF_FirstCommand)
		{
			switch (defnum)
			{
			case ADEF_SkipSuper:
				parser += 2;
				break;
			case ADEF_StateBase:
				classtype = *((TypeInfo **)(parser + 1));
				parser += 5;
				if (classtype == NULL)
				{
					classtype = Class;
				}
				states = DefaultStates (classtype);
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
		case ADEF_SeeSound:		actor->SeeSound = datastr;		break;
		case ADEF_AttackSound:	actor->AttackSound = datastr;	break;
		case ADEF_PainSound:	actor->PainSound = datastr;		break;
		case ADEF_DeathSound:	actor->DeathSound = datastr;	break;
		case ADEF_ActiveSound:	actor->ActiveSound = datastr;	break;

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
		case ADEF_FlagsSet:		actor->flags |= dataint;		break;
		case ADEF_Flags2Set:	actor->flags2 |= dataint;		break;
		case ADEF_Flags3Set:	actor->flags3 |= dataint;		break;
		case ADEF_FlagsClear:	actor->flags &= ~dataint;		break;
		case ADEF_Flags2Clear:	actor->flags2 &= ~dataint;		break;
		case ADEF_Flags3Clear:	actor->flags3 &= ~dataint;		break;
		case ADEF_Alpha:		actor->alpha = dataint;			break;
		case ADEF_RenderStyle:	actor->RenderStyle = dataint;	break;
		case ADEF_RenderFlags:	actor->renderflags = dataint;	break;

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
}
