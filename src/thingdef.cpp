/*
** thingdef.cpp
**
** Actor definitions
**
**---------------------------------------------------------------------------
** Copyright 2002-2004 Christoph Oelckers
** Copyright 2004 Randy Heit
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

/* To add this to the DECORATE parser add the following to the
   ParseDecorate function:

	[old]   
	// Get actor class name. The A prefix is added automatically.
	while (SC_GetString ())
	{
	[new]
		if (SC_Compare ("Actor"))
		{
			ProcessActor(process);
			continue;
		}
	[old]
		if (SC_Compare ("Pickup"))
		
and add this prototype to decorations,cpp (or a header it includes):   
	void ProcessActor(void (*process)(FState *, int));

		
*/



#include "actor.h"
#include "info.h"
#include "sc_man.h"
#include "tarray.h"
#include "w_wad.h"
#include "templates.h"
#include "r_defs.h"
#include "r_draw.h"
#include "a_pickups.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "a_action.h"
#include "decallib.h"
#include "m_random.h"
#include "autosegs.h"
#include "i_system.h"
#include "p_local.h"
#include "doomerrors.h"
#include "g_doom/a_doomglobal.h"

static FRandom pr_camissile ("CustomActorfire");
static FRandom pr_camelee ("CustomMelee");
static FRandom pr_cabullet ("CustomBullet");
static FRandom pr_cajump ("CustomJump");

static const char * flagnames[]={
	"*",			// Pickup items require special handling - don't set the flag explicitly
	"SOLID",
	"SHOOTABLE",
	"NOSECTOR",
	"NOBLOCKMAP",
	"AMBUSH",
	"JUSTHIT",
	"JUSTATTACKED",
	"SPAWNCEILING",
	"NOGRAVITY",
	"DROPOFF",
	"*",			// MF_PICKUP does not make much sense
	"NOCLIP",
	"*",			// MF_SLIDE is completly unused
	"FLOAT",
	"TELEPORT",
	"MISSILE",
	"DROPPED",
	"SHADOW",
	"NOBLOOD",
	"CORPSE",
	"INFLOAT",
	"COUNTKILL",
	"COUNTITEM",
	"SKULLFLY",
	"NOTDMATCH",
	"*",			// MF_TRANSLATION1 - better use ACS for this!
	"*",			// MF_TRANSLATION2 - better use ACS for this!
	"*",			// MF_UNMORPHED - not to be used in an actor definition.
	"NOLIFTDROP",
	"STEALTH",
	"ICECORPSE",
	NULL
};
	
static const char * flag2names[]={
	"LOWGRAVITY",
	"WINDTHRUST",
	"HERETICBOUNCE",
	"*",				// BLASTED
	"*",				// MF2_FLY is for players only
	"FLOORCLIP",
	"SPAWNFLOAT",
	"NOTELEPORT",
	"RIPPER",
	"PUSHABLE",
	"SLIDESONWALLS",
	"*",				// MF2_ONMOBJ is only used internally
	"CANPASS",
	"CANNOTPUSH",
	"THRUGHOST",
	"BOSS",
	"FIREDAMAGE",
	"NODAMAGETHRUST",
	"TELESTOMP",
	"FLOATBOB",
	"HEXENBOUNCE",
	"ACTIVATEIMPACT",
	"CANPUSHWALLS",
	"ACTIVATEMCROSS",
	"ACTIVATEPCROSS",
	"CANTLEAVEFLOORPIC",
	"NONSHOOTABLE",
	"INVULNERABLE",
	"DORMANT",
	"ICEDAMAGE",
	"SEEKERMISSILE",
	"REFLECTIVE",
	NULL
};

static const char * flag3names[]=
{
	"FLOORHUGGER",
	"CEILINGHUGGER",
	"NORADIUSDMG",
	"GHOST",
	"*",
	"*",
	"DONTSPLASH",
	"*",					// NOSIGHTCHECK
	"DONTOVERLAP",
	"DONTMORPH",
	"DONTSQUASH",
	"*",					// EXPLOCOUNT
	"FULLVOLACTIVE",
	"ISMONSTER",
	"SKYEXPLODE",
	"STAYMORPHED",
	"DONTBLAST",
	"CANBLAST",
	"NOTARGET",
	"DONTGIB",
	"NOBLOCKMONST",
	"*",
	"FULLVOLDEATH",
	"CANBOUNCEWATER",
	"NOWALLBOUNCESND",
	"FOILINVUL",
	"NOTELEOTHER",
	"*",
	"*",
	"*",					// WARNBOT
	"*",					// PUFFONACTORS
	"*",					// HUNTPLAYERS
	NULL
};

static const char * flag4names[]=
{
	"*",					// NOHATEPLAYERS
	"QUICKTORETALIATE",
	"NOICEDEATH",
	"*",					// BOSSDEATH
	"RANDOMIZE",
	"*",					// NOSKIN
	"FIXMAPTHINGPOS",
	"ACTLIKEBRIDGE",
	"STRIFEDAMAGE",
	NULL
};


struct AFuncDesc
{
	const char * Name;
	void * Function;
};


// Prototype the code pointers
#define WEAPON(x)	
#define ACTOR(x)	void A_##x(AActor*);
ACTOR(MeleeAttack)
ACTOR(MissileAttack)
ACTOR(ComboAttack)
ACTOR(BulletAttack)
ACTOR(ScreamAndUnblock)
ACTOR(ActiveAndUnblock)
ACTOR(ActiveSound)
ACTOR(FastChase)
ACTOR(FreezeDeath)
ACTOR(FreezeDeathChunks)
ACTOR(GenericFreezeDeath)
ACTOR(IceGuyDie)
#include "d_dehackedactions.h"

// Declare the code pointer table
AFuncDesc AFTable[]=
{
#define WEAPON(x)	
#define ACTOR(x)	{"A_"#x, (void *)A_##x},
ACTOR(MeleeAttack)
ACTOR(MissileAttack)
ACTOR(ComboAttack)
ACTOR(BulletAttack)
ACTOR(ScreamAndUnblock)
ACTOR(ActiveAndUnblock)
ACTOR(ActiveSound)
ACTOR(FastChase)
ACTOR(FreezeDeath)
ACTOR(FreezeDeathChunks)
ACTOR(GenericFreezeDeath)
ACTOR(IceGuyDie)			// useful for bursting a monster into ice chunks without delay
#include "d_dehackedactions.h"
	{"A_Fall", (void*)A_NoBlocking},	// Allow Doom's old name, too, for this function
	{NULL,NULL}
};


struct FDropItem
{
	const char * Name;
	int probability;
	int amount;
	FDropItem * Next;
};

extern TArray<FActorInfo *> Decorations;

struct FMissileAttack
{
	const char *MissileName;
	fixed_t SpawnHeight;
	fixed_t Spawnofs_XY;
	angle_t Angle;
	bool aimparallel;
};

TArray<FMissileAttack> AttackList;

//==========================================================================
//
// The base class for all custom actors defined here
//
//==========================================================================
class ACustomActor : public AActor
{
	DECLARE_STATELESS_ACTOR (ACustomActor, AActor);

public:

	// This defines a few custom properties that don't exist in the base class
	// but might be useful in user defined actors.
	bool HurtShooter;
	int ExplosionRadius, ExplosionDamage;
	fixed_t DeathHeight, BurnHeight;
	const char * Obituary;
	const char * HitObituary;
	FDropItem * dropitemlist;
	int MeleeDamage;
	int MeleeSound;
	const char * MissileName;
	int MissileHeight;

	//void Serialize (FArchive &arc);
	void Die (AActor *source, AActor *inflictor);
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);

	// Normal/ranged obituary if this actor is the attacker
	virtual const char *GetObituary ()
	{
		return Obituary;
	}

	// Melee obituary if this actor is the attacker
	virtual const char *GetHitObituary ()
	{
		return HitObituary? HitObituary : Obituary;
	}

	virtual void NoBlockingSet ();
};

IMPLEMENT_ABSTRACT_ACTOR (ACustomActor)


#if 0
// Question: Do these values really need to be saved? They are supposed to
// be static for the class.
void ACustomActor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	arc << DeathHeight << BurnHeight << ExplosionRadius << ExplosionDamage
		<< HurtShooter;
}
#endif

//==========================================================================
//
// Some virtual functions which define customizable behavior
//
//==========================================================================
void ACustomActor::Die (AActor *source, AActor *inflictor)
{
	Super::Die (source, inflictor);

	// clear MF_CORPSE for non-monsters
	if (!(flags3&MF3_ISMONSTER)) flags &= ~MF_CORPSE;

	if (flags2 & MF2_FIREDAMAGE)
	{
		if (BurnHeight>=0) height = BurnHeight;
	}
	else 
	{
		if (DeathHeight>=0) height = DeathHeight;
	}
};

void ACustomActor::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
	damage = ExplosionDamage;
	dist = ExplosionRadius;
	hurtSource = HurtShooter;
}

void ACustomActor::NoBlockingSet()
{
	FDropItem * di=dropitemlist;

	while (di)
	{
		const TypeInfo * ti=TypeInfo::FindType(di->Name);
		if (ti) P_DropItem(this, ti, di->amount, di->probability);
		di=di->Next;
	}
}

//==========================================================================
//
// Customizable attack functions which use actor parameters.
// I think this is among the most requested stuff ever ;-)
//
//==========================================================================
static void DoAttack(AActor * self, bool domelee, bool domissile)
{
	if (!self->target || !self->IsKindOf(RUNTIME_CLASS(ACustomActor)))
		return;

	ACustomActor * cust=static_cast<ACustomActor*>(self);
				
	A_FaceTarget (self);
	if (domelee && P_CheckMeleeRange (self))
	{
		int damage = pr_camelee.HitDice(cust->MeleeDamage);
		if (cust->MeleeSound) S_SoundID (self, CHAN_WEAPON, cust->MeleeSound, 1, ATTN_NORM);
		P_DamageMobj (self->target, self, self, damage, MOD_HIT);
		P_TraceBleed (damage, self->target, self);
	}
	else if (domissile)
	{
		const TypeInfo * ti=TypeInfo::FindType(cust->MissileName);
		if (ti) 
		{
#if 1
			// Hmm, the result of this hack (taken from A_SkelMissile) is
			// significantly more desirable than using P_SpawnMissileZ!
			self->z+=cust->MissileHeight-32*FRACUNIT;
			AActor * missile = P_SpawnMissile (self, self->target, ti);
			self->z-=cust->MissileHeight-32*FRACUNIT;
#else
			// ZDoom has a P_SpawnMissile Z to do this.
			// Edit: why is the aiming of this so much worse than the hack I used previously???
			AActor * missile = P_SpawnMissileZ(self, self->z+cust->MissileHeight, self->target, ti);
#endif

			// automatic handling of seeker missiles
			if (missile && missile->flags2&MF2_SEEKERMISSILE)
			{
				missile->tracer=self->target;
			}
		}
	}
}

void A_MeleeAttack(AActor * self)
{
	DoAttack(self, true, false);
}

void A_MissileAttack(AActor * self)
{
	DoAttack(self, false, true);
}

void A_ComboAttack(AActor * self)
{
	DoAttack(self, true, true);
}

//==========================================================================
//
// Custom sound functions. These use misc1 and misc2 in the state structure
//
//==========================================================================
void A_PlaySound(AActor * self)
{
	int soundid=self->state->GetMisc2()+256*self->state->GetMisc1();
	S_SoundID (self, CHAN_BODY, soundid, 1, ATTN_NORM);
}

void A_PlayWeaponSound(AActor * self)
{
	int soundid=self->state->GetMisc2()+256*self->state->GetMisc1();
	S_SoundID (self, CHAN_WEAPON, soundid, 1, ATTN_NORM);
}

//==========================================================================
//
// Generic seeker missile function
//
//==========================================================================
void A_SeekerMissile(AActor * self)
{
	P_SeekerMissile(self, self->state->GetMisc1()*ANGLE_1, self->state->GetMisc2()*ANGLE_1);
}
//==========================================================================
//
// Hitscan attack with a customizable amount of bullets (specified in damage)
//
//==========================================================================
void A_BulletAttack (AActor *self)
{
	int i;
	int bangle;
	int slope;
		
	if (!self->target)
		return;

	A_FaceTarget (self);
	bangle = self->angle;

	PuffType = RUNTIME_CLASS(ABulletPuff);
	slope = P_AimLineAttack (self, bangle, MISSILERANGE);

	S_SoundID (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	for (i=0 ; i<self->damage ; i++)
    {
		int angle = bangle + (pr_cabullet.Random2() << 20);
		int damage = ((pr_cabullet()%5)+1)*3;
		P_LineAttack(self, angle, MISSILERANGE, slope, damage);
    }
}

//==========================================================================
//
// State jump function
//
//==========================================================================
void A_Jump(AActor * self)
{
	if (pr_cajump()<self->state->GetMisc2())
		self->SetState(self->state+self->state->GetMisc1());
}

//==========================================================================
//
// The ultimate code pointer: Fully customizable missiles!
//
//==========================================================================
void A_CustomMissile(AActor * self)
{
	int index=self->state->GetMisc2()+256*self->state->GetMisc1();
	AActor * targ;
	AActor * missile;

	if (self->target && index>=0 && index<AttackList.Size())
	{
		FMissileAttack * att=&AttackList[index];

		const TypeInfo * ti=TypeInfo::FindType(att->MissileName);
		if (ti) 
		{
			angle_t ang = (self->angle - ANGLE_90) >> ANGLETOFINESHIFT;
			fixed_t x = att->Spawnofs_XY * finecosine[ang];
			fixed_t y = att->Spawnofs_XY * finesine[ang];
			fixed_t z = att->SpawnHeight-32*FRACUNIT;

			if (!att->aimparallel)
			{
				// same adjustment as above (in all 3 directions this time) - for better aiming!
				self->x+=x;
				self->y+=y;
				self->z+=z;
				missile = P_SpawnMissile(self, self->target, ti);
				self->x-=x;
				self->y-=y;
				self->z-=z;
			}
			else
			{
				missile = P_SpawnMissileXYZ(self->x+x, self->y+y, self->z+att->SpawnHeight, self, self->target, ti);
			}

			if (missile)
			{
				missile->angle += att->Angle;
				ang = missile->angle >> ANGLETOFINESHIFT;
				missile->momx = FixedMul (missile->Speed, finecosine[ang]);
				missile->momy = FixedMul (missile->Speed, finesine[ang]);

				// handle projectile shooting projectiles - track the
				// links back to a real owner
				if (self->flags & MF_MISSILE)
				{
					AActor * owner=self->target;
					while (owner->flags&MF_MISSILE && owner->target) owner=owner->target;
					targ=owner;
					missile->target=owner;
					// automatic handling of seeker missiles
					if (self->flags2 & missile->flags2 & MF2_SEEKERMISSILE)
					{
						missile->tracer=self->tracer;
					}
				}
				else if (missile->flags2&MF2_SEEKERMISSILE)
				{
					// automatic handling of seeker missiles
					missile->tracer=self->target;
				}
			}
		}
	}
}

//==========================================================================
//
// SC_CheckNumber
// similar to SC_GetNumber but ungets the token if it isn't a number 
// and does not print an error
//
//==========================================================================

BOOL SC_CheckNumber (void)
{
	char *stopper;

	//CheckOpen ();
	if (SC_GetString())
	{
		if (strcmp (sc_String, "MAXINT") == 0)
		{
			sc_Number = INT_MAX;
		}
		else
		{
			sc_Number = strtol (sc_String, &stopper, 0);
			if (*stopper != 0)
			{
				SC_UnGet();
				return false;
			}
		}
		sc_Float = (float)sc_Number;
		return true;
	}
	else
	{
		return false;
	}
}


//==========================================================================
//
// SC_TestToken etc.
// Some functions which check for simple tokens
//
//==========================================================================
bool SC_TestToken(const char * tok)
{
	if (SC_GetString())
	{
		if (SC_Compare(tok)) return true;
	}
	SC_UnGet();
	return false;
}

void SC_ChkToken(const char * tok)
{
	if (!SC_TestToken(tok)) SC_ScriptError("'%s' expected",&tok);
}
	
inline void ChkCom()
{
	SC_ChkToken(",");
}
	
inline void ChkBraceOpn()
{
	SC_ChkToken("{");
}
	
inline bool TestBraceCls()
{
	return SC_TestToken("}");
}


//==========================================================================
//
// Find a state address
//
//==========================================================================

// These strings must be in the same order as the respective variables in the actor struct!
static const char * mobj_statenames[]={"SPAWN","SEE","PAIN","MELEE","MISSILE","CRASH",
									   "DEATH","XDEATH", "BURN","ICE","RAISE",
									  NULL};

FState ** FindState(AActor * actor,const char * name)
{
	int i;

	for(i=0;mobj_statenames[i];i++)
	{
		if (!stricmp(mobj_statenames[i],name)) return (&actor->SpawnState)+i;
	}
	return NULL;
}


//==========================================================================
//
// Sets the default values with which an actor definition starts
//
//==========================================================================
static void ResetActor(ACustomActor * actor)
{
	memset(actor,0,sizeof(actor));

	actor->health=1000;
	actor->reactiontime=8;
	actor->radius=20*FRACUNIT;
	actor->height=16*FRACUNIT;
	actor->Mass=100;
	actor->SpawnState=&AActor::States[0];
	actor->xscale=actor->yscale=63;
	actor->RenderStyle=STYLE_Normal;
	actor->alpha=FRACUNIT;
	actor->DeathHeight=actor->BurnHeight=-1;
	actor->ExplosionDamage=actor->ExplosionRadius=128;
	actor->HurtShooter=true;
	actor->MissileHeight=32*FRACUNIT;
}



//==========================================================================
//
// Finds a parent class 
// this has to search the uninitialized data because the actors haven't been
// fully initialized yet.
//
//==========================================================================
static FActorInfo * FindClass(const char * name)
{
	int i;

	// First search all existing custom actors
	for (i=0;i<TypeInfo::m_RuntimeActors.Size();i++)
	{
		if (!strcmp(name, TypeInfo::m_RuntimeActors[i]->Name+1))
		{
			return TypeInfo::m_RuntimeActors[i]->ActorInfo;
		}
	}

	// Then search the internal ones
	TAutoSegIterator<FActorInfo *, &ARegHead, &ARegTail> reg;
	while (++reg != NULL)
	{
		if (!strcmp(name,reg->Class->Name+1)) return reg;
	}

	// I'll do this later!
	return NULL;
}




//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
static FActorInfo * CreateNewActor(FActorInfo ** parentc)
{
	char * typeName;

	SC_MustGetString();

	if (TypeInfo::IFindType (sc_String) != NULL)
	{
		SC_ScriptError ("Actor %s is already defined.", (const char **)&sc_String);
	}

	typeName=new char[strlen(sc_String)+2];
	sprintf(typeName,"A%s",sc_String);

	TypeInfo * parent = RUNTIME_CLASS(ACustomActor);
	TypeInfo * ti = parent->CreateDerivedClass (typeName, parent->SizeOf);
	FActorInfo * info=ti->ActorInfo;

	Decorations.Push (info);
	info->NumOwnedStates=0;
	info->OwnedStates=NULL;
	info->SpawnID=0;

	ResetActor((ACustomActor*)info->Defaults);

	if (parentc)
	{
		*parentc=NULL;
		SC_MustGetString();
		if (SC_Compare(":"))
		{
			SC_MustGetString();
			FActorInfo * parenti=FindClass(sc_String);

			if (!parenti)
			{
				Printf("Warning: Parent Type '%s' not found in %s\n",sc_String,typeName+1);
			}
			else
			{
				memcpy(info->Defaults,parenti->Defaults,parenti->Class->SizeOf);
				if (parentc) *parentc=parenti;
			}
		}
		else SC_UnGet();
	}

	info->DoomEdNum=-1;
	if (SC_CheckNumber()) 
	{
		if (sc_Number>=-1 && sc_Number<32768) info->DoomEdNum=sc_Number;
		else SC_ScriptError ("DoomEdNum must be in the range [-1,32767]");
	}

	return info;
}

//==========================================================================
//
// State processing
//
//==========================================================================

static int ProcessStates(FActorInfo * actor, ACustomActor * defaults)
{
	char statestring[256];
	int count=0;
	FState state;
	FState * laststate=NULL;
	int lastlabel;
	FState ** stp;
	TArray<FState> StateArray;
	int minrequiredstate=0;

	statestring[255] = 0;

	ChkBraceOpn();
	while (!TestBraceCls() && !sc_End)
	{
		memset(&state,0,sizeof(state));
		SC_MustGetString();
		if (SC_Compare("GOTO"))
		{
			SC_MustGetString();
			if (!laststate) 
			{
				SC_ScriptError("GOTO before first state");
				continue;
			}
			strncpy (statestring, sc_String, 255);
			SC_MustGetString ();
			if (SC_Compare ("+"))
			{
				SC_MustGetNumber ();
				strcat (statestring, "+");
				strcat (statestring, sc_String);
			}
			else
			{
				SC_UnGet ();
			}
			// copy the text - this must be resolved later!
			laststate->NextState=(FState*)strdup(statestring);	
		}
		else if (SC_Compare("STOP"))
		{
			if (!laststate) 
			{
				SC_ScriptError("STOP before first state");
				continue;
			}
			laststate->NextState=(FState*)-1;
		}
		else if (SC_Compare("WAIT"))
		{
			if (!laststate) 
			{
				SC_ScriptError("WAIT before first state");
				continue;
			}
			laststate->NextState=(FState*)-2;
		}
		else if (SC_Compare("LOOP"))
		{
			if (!laststate) 
			{
				SC_ScriptError("LOOP before first state");
				continue;
			}
			laststate->NextState=(FState*)(lastlabel+1);
		}
		else
		{
			char * statestrp;

			strncpy(statestring, sc_String, 255);
			SC_MustGetString();

			while (SC_Compare (":"))
			{
				lastlabel = count;
				stp = FindState(defaults, statestring);
				if (stp) *stp=(FState *) (count+1);
				else 
					SC_ScriptError("Unknown state label %s",(const char **)&statestring);
				SC_MustGetString ();
				strncpy(statestring, sc_String, 255);
				SC_MustGetString ();
			}

			SC_UnGet ();

			if (strlen (statestring) != 4)
			{
				SC_ScriptError ("Sprite names must be exactly 4 characters\n");
			}

			memcpy(state.sprite.name, statestring, 4);
			SC_MustGetString();
			strncpy(statestring, sc_String + 1, 255);
			statestrp = statestring;
			state.Frame=(*sc_String&223)-'A';
			if ((*sc_String&223)<'A' || (*sc_String&223)>']')
			{
				SC_ScriptError ("Frames must be A-Z, [, \\, or ]");
				state.Frame=0;
			}

			SC_MustGetNumber();
			sc_Number++;
			state.Tics=sc_Number&255;
			state.Misc1=(sc_Number>>8)&255;
			if (state.Misc1) state.Frame|=SF_BIGTIC;

			while (SC_GetString() && !sc_Crossed)
			{
				if (SC_Compare("BRIGHT")) 
				{
					state.Frame|=SF_FULLBRIGHT;
					continue;
				}

				// These three functions are special because they require a parameter
				if (SC_Compare("A_PlaySound"))
				{
					if (state.Frame&SF_BIGTIC)
					{
						SC_ScriptError("You cannot use A_PlaySound with a state duration larger than 254!");
					}
					SC_MustGetStringName ("(");
					SC_MustGetString();
					int v=S_FindSound(sc_String);
					SC_MustGetStringName (")");
					state.Misc2=v&255;
					state.Misc1=v>>8;
					state.Action.acvoid=A_PlaySound;
					goto endofstate;
				}
				if (SC_Compare("A_PlayWeaponSound"))
				{
					if (state.Frame&SF_BIGTIC)
					{
						SC_ScriptError("You cannot use A_PlayWeaponSound with a state duration larger than 254!");
					}
					SC_MustGetStringName ("(");
					SC_MustGetString();
					int v=S_FindSound(sc_String);
					SC_MustGetStringName (")");
					state.Misc2=v&255;
					state.Misc1=v>>8;
					state.Action.acvoid=A_PlayWeaponSound;
					goto endofstate;
				}
				if (SC_Compare("A_SeekerMissile"))
				{
					if (state.Frame&SF_BIGTIC)
					{
						SC_ScriptError("You cannot use A_SeekerMissile with a state duration larger than 254!");
					}
					SC_MustGetStringName ("(");
					SC_MustGetNumber();
					if (sc_Number<0 || sc_Number>90)
					{
						SC_ScriptError("A_SeekerMissile parameters must be in the range [0..90]");
					}
					state.Misc1=sc_Number;
					SC_MustGetStringName (",");
					SC_MustGetNumber();
					if (sc_Number<0 || sc_Number>90)
					{
						SC_ScriptError("A_SeekerMissile parameters must be in the range [0..90]");
					}
					state.Misc2=sc_Number;
					SC_MustGetStringName (")");
					state.Action.acvoid=A_SeekerMissile;
					goto endofstate;
				}
				if (SC_Compare("A_Jump"))
				{
					if (strlen(statestring)>0)
					{
						SC_ScriptError("A_Jump may not be used on a multiple state definition!");
					}
					if (state.Frame&SF_BIGTIC)
					{
						SC_ScriptError("You cannot use A_Jump with a state duration larger than 254!");
					}
					SC_MustGetStringName("(");
					SC_MustGetNumber();
					state.Misc2=sc_Number;
					if (sc_Number<0 || sc_Number>255)
					{
						SC_ScriptError("The first parameter of A_Jump must be in the range [0..255]");
					}
					SC_MustGetStringName (",");
					SC_MustGetNumber();
					if (sc_Number<=0)
					{
						SC_ScriptError("The second parameter of A_Jump must be greater than 0");
					}
					state.Misc1=sc_Number;
					SC_MustGetStringName (")");
					minrequiredstate=count+state.Misc1;
					state.Action.acvoid=A_Jump;
					goto endofstate;
				}
				if (SC_Compare("A_CustomMissile"))
				{
					FMissileAttack att;

					if (state.Frame&SF_BIGTIC)
					{
						SC_ScriptError("You cannot use A_Jump with a state duration larger than 254!");
					}
					SC_MustGetStringName("(");
					SC_MustGetString();
					att.MissileName=strdup(sc_String);
					SC_MustGetStringName (",");
					SC_MustGetNumber();
					att.SpawnHeight=sc_Number*FRACUNIT;
					SC_MustGetStringName (",");
					SC_MustGetNumber();
					att.Spawnofs_XY=sc_Number;
					SC_MustGetStringName (",");
					SC_MustGetNumber();
					att.Angle=sc_Number*ANGLE_1;

					if (SC_TestToken(","))
					{
						SC_MustGetNumber();
						att.aimparallel=!!sc_Number;
					}
					else att.aimparallel = false;

					SC_MustGetStringName (")");

					int v=AttackList.Push(att);
					state.Misc2=v&255;
					state.Misc1=v>>8;
					state.Action.acvoid=A_CustomMissile;
					goto endofstate;
				}

				for(int i=0;AFTable[i].Name;i++) if (SC_Compare(AFTable[i].Name))
				{
					state.Action.acvoid=AFTable[i].Function;
					goto endofstate;
				}
				SC_ScriptError("Invalid state parameter");
			}
			SC_UnGet();
endofstate:
			StateArray.Push(state);
			while (*statestrp)
			{
				int frame=((*statestrp++)&223)-'A';

				if (frame<0 || frame>28)
				{
					SC_ScriptError ("Frames must be A-Z, [, \\, or ]");
					frame=0;
				}

				state.Frame=(state.Frame&(SF_FULLBRIGHT|SF_BIGTIC))|frame;
				StateArray.Push(state);
				count++;
			}
			laststate=&StateArray[count];
			count++;
		}
	}
	if (count<=minrequiredstate)
	{
		SC_ScriptError("A_Jump offset out of range in %s",(const char **)&actor->Class->Name);
	}

	FState * realstates=new FState[count];
	int i;

	memcpy(realstates,&StateArray[0],count*sizeof(FState));
	actor->OwnedStates=realstates;
	actor->NumOwnedStates=count;

	// adjust the state pointers
	for(stp=&defaults->SpawnState;stp<=&defaults->RaiseState;stp++)
	{
		int v=(int)*stp;
		if (v>=1 && v<0x10000) *stp=actor->OwnedStates+v-1;
	}

	for(i=0;i<count;i++)
	{
		// resolve labels and jumps
		switch((int)realstates[i].NextState)
		{
		case 0:		// next
			realstates[i].NextState=(i<count-1? &realstates[i+1]:&realstates[0]);
			break;

		case -1:	// stop
			realstates[i].NextState=NULL;
			break;

		case -2:	// wait
			realstates[i].NextState=&realstates[i];
			break;

		default:	// loop
			if (((int)realstates[i].NextState)<0x10000)
			{
				realstates[i].NextState=&realstates[(int)realstates[i].NextState-1];
			}
			else	// goto
			{
				char * p=strtok((char*)realstates[i].NextState,"+");
				char * q=strtok(NULL,"+");
				int v=0;
				if (q) v=strtol(q,NULL,0);

				stp=FindState(defaults,p);
				free(realstates[i].NextState);		// free the allocated string buffer
				if (stp)
				{
					if (*stp) realstates[i].NextState=*stp+v;
					else 
					{
						realstates[i].NextState=NULL;
						if (v)
						{
							SC_ScriptError("Attempt to get invalid state from actor %s\n",(const char**)(&actor->Class->Name));
							return NULL;
						}
					}
				}
				else 
					SC_ScriptError("Unknown state label %s",(const char **)&p);
			}
		}
	}
	return count;
}

//==========================================================================
//
// For getting a state address from the parent
//
//==========================================================================
static FState * CheckState(int statenum,FActorInfo * parent)
{
	if (SC_GetString() && !sc_Crossed)
	{
		if (SC_Compare("0")) return NULL;
		else if (SC_Compare("PARENT"))
		{
			if (!parent)
			{
				SC_ScriptError("Attempt to read parent state without valid parent\n");
				return NULL;
			}
			SC_MustGetString();

			{
				FState * basestate;
				FState ** stp=FindState((AActor*)parent->Defaults, sc_String);
				int v = 0;

				if (stp) basestate =*stp;
				else 
				{
					SC_ScriptError("Unknown state label %s",(const char **)&sc_String);
					return NULL;
				}

				if (SC_GetString ())
				{
					if (SC_Compare ("+"))
					{
						SC_MustGetNumber ();
						v = sc_Number;
					}
					else
					{
						SC_UnGet ();
					}
				}

				if (!basestate && !v) return NULL;
				basestate+=v;

				if (v && !basestate)
				{
					SC_ScriptError("Attempt to get invalid state from actor %s\n",(const char**)(&parent->Class->Name));
					return NULL;
				}
				return basestate;
			}
		}
		else SC_ScriptError("Invalid state assignment");
	}
	return NULL;
}



//==========================================================================
//
// Reads an actor definition
//
//==========================================================================
inline char * MS_Strdup(const char * s)
{
	return *s? strdup(s):NULL;
}


void ProcessActor(void (*process)(FState *, int))
{
	FActorInfo * info=NULL;
	ACustomActor * defaults;
	try
	{
		int currentstate=0;
		FActorInfo * parent=NULL;
		int index;
		bool stateset=false;
		bool dropitemset=false;

		info=CreateNewActor(&parent);
		defaults=(ACustomActor*)info->Defaults;

		SC_SetCMode (true);

		ChkBraceOpn();
		while (!TestBraceCls() && !sc_End)
		{
			SC_GetString();

			if (SC_Compare("SKIP_SUPER"))
			{
				ResetActor(defaults);
			}
			else if (SC_Compare("SPAWNID"))
			{
				SC_MustGetNumber();
				if (sc_Number<0 || sc_Number>255)
				{
					SC_ScriptError ("SpawnNum must be in the range [0,255]");
				}
				else info->SpawnID=(BYTE)sc_Number;
			}
			else if (SC_Compare("HEALTH"))
			{
				SC_MustGetNumber();
				defaults->health=sc_Number;
			}
			else if (SC_Compare("REACTIONTIME"))
			{
				SC_MustGetNumber();
				defaults->reactiontime=sc_Number;
			}
			else if (SC_Compare("PAINCHANCE"))
			{
				SC_MustGetNumber();
				defaults->PainChance=sc_Number;
			}
			else if (SC_Compare("DAMAGE"))
			{
				SC_MustGetNumber();
				defaults->damage=sc_Number;
			}
			else if (SC_Compare("SPEED"))
			{
				SC_MustGetFloat();
				defaults->Speed=sc_Float*FRACUNIT;
			}
			else if (SC_Compare("RADIUS"))
			{
				SC_MustGetFloat();
				defaults->radius=sc_Float*FRACUNIT;
			}
			else if (SC_Compare("HEIGHT"))
			{
				SC_MustGetFloat();
				defaults->height=sc_Float*FRACUNIT;
			}
			else if (SC_Compare("MASS"))
			{
				SC_MustGetNumber();
				defaults->Mass=sc_Number;
			}
			else if (SC_Compare("XSCALE"))
			{
				SC_MustGetFloat();
				defaults->xscale=sc_Float*63;
			}
			else if (SC_Compare("YSCALE"))
			{
				SC_MustGetFloat();
				defaults->yscale=sc_Float*63;
			}
			else if (SC_Compare("SCALE"))
			{
				SC_MustGetFloat();
				defaults->xscale=defaults->yscale=sc_Float*63;
			}
			else if (SC_Compare("SEESOUND"))
			{
				SC_MustGetString();
				defaults->SeeSound=S_FindSound(sc_String);
			}
			else if (SC_Compare("ATTACKSOUND"))
			{
				SC_MustGetString();
				defaults->AttackSound=S_FindSound(sc_String);
			}
			else if (SC_Compare("PAINSOUND"))
			{
				SC_MustGetString();
				defaults->PainSound=S_FindSound(sc_String);
			}
			else if (SC_Compare("DEATHSOUND"))
			{
				SC_MustGetString();
				defaults->DeathSound=S_FindSound(sc_String);
			}
			else if (SC_Compare("ACTIVESOUND"))
			{
				SC_MustGetString();
				defaults->ActiveSound=S_FindSound(sc_String);
			}

			else if (SC_Compare("DROPITEM"))
			{
				// create a linked list of dropitems
				if (!dropitemset)
				{
					dropitemset=true;
					defaults->dropitemlist=NULL;
				}

				FDropItem * di=new FDropItem;

				SC_MustGetString();
				di->Name=strdup(sc_String);
				di->probability=255;
				di->amount=-1;
				if (SC_CheckNumber())
				{
					di->probability=sc_Number;
					if (SC_CheckNumber())
					{
						di->amount=sc_Number;
					}
				}
				di->Next=defaults->dropitemlist;
				defaults->dropitemlist=di;
			}

			else if (SC_Compare("SPAWN"))	defaults->SpawnState=CheckState(currentstate,parent);
			else if (SC_Compare("SEE"))		defaults->SeeState=CheckState(currentstate,parent);
			else if (SC_Compare("MELEE"))	defaults->MeleeState=CheckState(currentstate,parent);
			else if (SC_Compare("MISSILE"))	defaults->MissileState=CheckState(currentstate,parent);
			else if (SC_Compare("PAIN"))	defaults->PainState=CheckState(currentstate,parent);
			else if (SC_Compare("DEATH"))	defaults->DeathState=CheckState(currentstate,parent);
			else if (SC_Compare("XDEATH"))	defaults->XDeathState=CheckState(currentstate,parent);
			else if (SC_Compare("BURN"))	defaults->BDeathState=CheckState(currentstate,parent);
			else if (SC_Compare("ICE"))		defaults->IDeathState=CheckState(currentstate,parent);
			else if (SC_Compare("RAISE"))	defaults->RaiseState=CheckState(currentstate,parent);
			else if (SC_Compare("CRASH"))	defaults->CrashState=CheckState(currentstate,parent);
			else if (SC_Compare("STATES"))	
			{
				if (!stateset) ProcessStates(info, defaults);
				else SC_ScriptError("Multiple state declarations not allowed");
				stateset=true;
			}

			else if (SC_Compare("RENDERSTYLE"))
			{
				static const char * renderstyles[]={
					"NONE","NORMAL","FUZZY","SOULTRANS","OPTFUZZY","STENCIL","TRANSLUCENT", "ADD",NULL};

				static int renderstyle_values[]={
					STYLE_None, STYLE_Normal, STYLE_Fuzzy, STYLE_SoulTrans, STYLE_OptFuzzy,
						STYLE_Stencil, STYLE_Translucent, STYLE_Add};

				SC_MustGetString();
				index=SC_MustMatchString(renderstyles);
				defaults->RenderStyle=renderstyle_values[index>=0 && index<=7 ? index: 1];
			}
			else if (SC_Compare("ALPHA"))
			{
				SC_MustGetFloat();
				defaults->alpha=sc_Float*FRACUNIT;
			}

			// The following properties are only exposed through virtual functions.
			// This means they cannot be inherited from internal actors!
			else if (SC_Compare("OBITUARY") )
			{
				SC_MustGetString();
				defaults->Obituary=MS_Strdup(sc_String);
			}
			else if (SC_Compare("HITOBITUARY"))
			{
				SC_MustGetString();
				defaults->HitObituary=MS_Strdup(sc_String);
			}
			else if (SC_Compare("DONTHURTSHOOTER"))
			{
				defaults->HurtShooter=false;
			}
			else if (SC_Compare("EXPLOSIONRADIUS"))
			{
				SC_MustGetNumber();
				defaults->ExplosionRadius=sc_Number;
			}
			else if (SC_Compare("EXPLOSIONDAMAGE"))
			{
				SC_MustGetNumber();
				defaults->ExplosionDamage=sc_Number;
			}
			else if (SC_Compare("DEATHHEIGHT"))
			{
				SC_MustGetFloat();
				defaults->DeathHeight=sc_Float*FRACUNIT;
			}
			else if (SC_Compare("BURNHEIGHT"))
			{
				SC_MustGetFloat();
				defaults->BurnHeight=sc_Float*FRACUNIT;
			}
			else if (SC_Compare("MELEEDAMAGE"))
			{
				SC_MustGetNumber();
				defaults->MeleeDamage=sc_Number;
			}
			else if (SC_Compare("MELEESOUND"))
			{
				SC_MustGetString();
				defaults->MeleeSound=S_FindSound(sc_String);
			}
			else if (SC_Compare("MISSILETYPE"))
			{
				SC_MustGetString();
				defaults->MissileName=MS_Strdup(sc_String);
			}
			else if (SC_Compare("MISSILEHEIGHT"))
			{
				SC_MustGetFloat();
				defaults->MissileHeight=sc_Float*FRACUNIT;
			}
			else if (SC_Compare ("Translation"))
			{
				SC_MustGetNumber ();
				if (sc_Number < 0 || sc_Number > 2)
				{
					SC_ScriptError ("Translation must be in the range [0,2]");
				}
				defaults->Translation = TRANSLATION(TRANSLATION_Standard, sc_Number);
			}


			else if (SC_Compare("CLEARFLAGS"))
			{
				defaults->flags=defaults->flags2=defaults->flags3=defaults->flags4=0;
			}
			else if (SC_Compare("MONSTER"))
			{
				// sets the standard flag for a monster
				defaults->flags|=MF_SHOOTABLE|MF_COUNTKILL|MF_SOLID; 
				defaults->flags2|=MF2_PUSHWALL|MF2_MCROSS|MF2_PASSMOBJ;
				defaults->flags3|=MF3_ISMONSTER;
			}
			else if (SC_Compare("PROJECTILE"))
			{
				// sets the standard flags for a projectile
				defaults->flags|=MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE; 
				defaults->flags2|=MF2_IMPACT|MF2_PCROSS|MF2_NOTELEPORT;
			}

			else
			{
				char mod = *sc_String;
				if (mod == '+' || mod == '-')
				{
					int v;

					SC_MustGetString ();
					// This isn't a real flag but should be usable as one!
					if (SC_Compare ("DOOMBOUNCE"))
					{
						if (mod=='+') defaults->flags2|=MF2_DOOMBOUNCE;
						else defaults->flags2&=~MF2_DOOMBOUNCE;
					}
					else if ((v=SC_MatchString(flagnames))!=-1)
					{
						unsigned fmod=1<<v;
						if (mod=='+') defaults->flags|=fmod;
						else defaults->flags&=~fmod;
					}
					else if ((v=SC_MatchString(flag2names))!=-1)
					{
						unsigned long fmod=1<<v;
						if (mod=='+') defaults->flags2|=fmod;
						else defaults->flags2&=~fmod;
					}
					else if ((v=SC_MatchString(flag3names))!=-1)
					{
						unsigned long fmod=1<<v;
						if (mod=='+') defaults->flags3|=fmod;
						else defaults->flags3&=~fmod;
					}
					else if ((v=SC_MatchString(flag4names))!=-1)
					{
						unsigned long fmod=1<<v;
						if (mod=='+') defaults->flags4|=fmod;
						else defaults->flags4&=~fmod;
					}
					else SC_ScriptError("\"%s\" is an unknown flag\n",(const char**)&sc_String);

				}
				else
				{
					SC_ScriptError("\"%s\" is an unknown actor property\n",(const char**)&sc_String);
				}
			}
		}
		process(info->OwnedStates, info->NumOwnedStates);
	}

	catch(CRecoverableError & e)
	{
		throw e;
	}
	// I think this is better than a crash log.
	catch (...)
	{
		if (info)
			I_Error("Unexpected error during parsing of actor %s",info->Class->Name+1);
		else
			I_Error("Unexpected error during parsing of actor definitions");
	}

	SC_SetCMode (false);
}



