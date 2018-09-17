/*
** thingdef_codeptr.cpp
**
** Code pointers for Actor definitions
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Christoph Oelckers
** Copyright 2004-2006 Randy Heit
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
#include "g_level.h"
#include "actor.h"
#include "r_defs.h"
#include "a_pickups.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "p_effect.h"
#include "p_enemy.h"
#include "decallib.h"
#include "p_local.h"
#include "c_console.h"
#include "doomerrors.h"
#include "a_sharedglobal.h"
#include "v_font.h"
#include "doomstat.h"
#include "g_shared/a_specialspot.h"
#include "actorptrselect.h"
#include "p_trace.h"
#include "p_setup.h"
#include "gstrings.h"
#include "d_player.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "v_text.h"
#include "thingdef.h"
#include "g_levellocals.h"
#include "r_utility.h"
#include "sbar.h"
#include "actorinlines.h"
#include "types.h"

AActor *SingleActorFromTID(int tid, AActor *defactor);


static FRandom pr_camissile ("CustomActorfire");
static FRandom pr_cabullet ("CustomBullet");
static FRandom pr_cajump ("CustomJump");
static FRandom pr_cwbullet ("CustomWpBullet");
static FRandom pr_cwjump ("CustomWpJump");
static FRandom pr_cwpunch ("CustomWpPunch");
static FRandom pr_grenade ("ThrowGrenade");
static FRandom pr_crailgun ("CustomRailgun");
static FRandom pr_spawndebris ("SpawnDebris");
static FRandom pr_spawnitemex ("SpawnItemEx");
static FRandom pr_burst ("Burst");
static FRandom pr_monsterrefire ("MonsterRefire");
static FRandom pr_teleport("A_Teleport");
static FRandom pr_bfgselfdamage("BFGSelfDamage");

//==========================================================================
//
// ACustomInventory :: CallStateChain
//
// Executes the code pointers in a chain of states
// until there is no next state
//
//==========================================================================

bool AStateProvider::CallStateChain (AActor *actor, FState *state)
{
	INTBOOL result = false;
	int counter = 0;
	VMValue params[3] = { actor, this, 0 };

	// We accept return types of `state`, `(int|bool)` or `state, (int|bool)`.
	// The last one is for the benefit of A_Warp and A_Teleport.
	int retval, numret;
	FState *nextstate;
	VMReturn ret[2];
	ret[0].PointerAt((void **)&nextstate);
	ret[1].IntAt(&retval);

	FState *savedstate = this->state;

	while (state != NULL)
	{
		if (!(state->UseFlags & SUF_ITEM))
		{
			Printf(TEXTCOLOR_RED "State %s not flagged for use in CustomInventory state chains.\n", FState::StaticGetStateName(state).GetChars());
			return false;
		}

		this->state = state;
		nextstate = NULL;	// assume no jump

		if (state->ActionFunc != NULL)
		{
			if (state->ActionFunc->Unsafe)
			{
				// If an unsafe function (i.e. one that accesses user variables) is being detected, print a warning once and remove the bogus function. We may not call it because that would inevitably crash.
				auto owner = FState::StaticFindStateOwner(state);
				Printf(TEXTCOLOR_RED "Unsafe state call in state %s to %s which accesses user variables. The action function has been removed from this state\n",
					FState::StaticGetStateName(state).GetChars(), state->ActionFunc->PrintableName.GetChars());
				state->ActionFunc = nullptr;
			}

			PPrototype *proto = state->ActionFunc->Proto;
			VMReturn *wantret;
			FStateParamInfo stp = { state, STATE_StateChain, PSP_WEAPON };

			params[2] = VMValue(&stp);
			retval = true;		// assume success
			wantret = NULL;		// assume no return value wanted
			numret = 0;

			// For functions that return nothing (or return some type
			// we don't care about), we pretend they return true,
			// thanks to the values set just above.

			if (proto->ReturnTypes.Size() >= 2 && 
				proto->ReturnTypes[0] == TypeState &&
				(proto->ReturnTypes[1] == TypeSInt32 || proto->ReturnTypes[0] == TypeUInt32 || proto->ReturnTypes[1] == TypeBool))
			{ // Function returns a state and an int or bool
				wantret = &ret[0];
				numret = 2;
			}
			else if (proto->ReturnTypes.Size() == 1 && proto->ReturnTypes[0] == TypeState)
			{ // Function returns a state
				wantret = &ret[0];
				retval = false;	// this is a jump function which never affects the success state.
				numret = 1;
			}
			else if (proto->ReturnTypes.Size() >= 1 &&
				(proto->ReturnTypes[0] == TypeSInt32 || proto->ReturnTypes[0] == TypeUInt32 || proto->ReturnTypes[0] == TypeBool))
			{ // Function returns an int or bool
				wantret = &ret[1];
				numret = 1;
			}
			try
			{
				VMCall(state->ActionFunc, params, state->ActionFunc->ImplicitArgs, wantret, numret);
			}
			catch (CVMAbortException &err)
			{
				err.MaybePrintMessage();
				err.stacktrace.AppendFormat("Called from state %s in inventory state chain in %s\n", FState::StaticGetStateName(state).GetChars(), GetClass()->TypeName.GetChars());
				throw;
			}

			// As long as even one state succeeds, the whole chain succeeds unless aborted below.
			// A state that wants to jump does not count as "succeeded".
			if (nextstate == NULL)
			{
				result |= retval;
			}
		}

		// Since there are no delays it is a good idea to check for infinite loops here!
		counter++;
		if (counter >= 10000)	break;

		if (nextstate == NULL) 
		{
			nextstate = state->GetNextState();

			if (state == nextstate) 
			{ // Abort immediately if the state jumps to itself!
				result = false;
				break;
			}
		}
		state = nextstate;
	}
	this->state = savedstate;
	return !!result;
}

DEFINE_ACTION_FUNCTION(ACustomInventory, CallStateChain)
{
	PARAM_SELF_PROLOGUE(AStateProvider);
	PARAM_OBJECT(affectee, AActor);
	PARAM_POINTER(state, FState);
	ACTION_RETURN_BOOL(self->CallStateChain(affectee, state));
}

//==========================================================================
//
// GetPointer
//
// resolve AAPTR_*
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, GetPointer)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(ptr);
	ACTION_RETURN_OBJECT(COPY_AAPTR(self, ptr));
}

//==========================================================================
//
// CheckClass
//
// NON-ACTION function to check a pointer's class.
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, CheckClass)
{
	if (numret > 0)
	{
		assert(ret != NULL);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_CLASS		(checktype, AActor);
		PARAM_INT_DEF	(pick_pointer);
		PARAM_BOOL_DEF	(match_superclass);

		self = COPY_AAPTR(self, pick_pointer);
		if (self == nullptr || checktype == nullptr)
		{
			ret->SetInt(false);
		}
		else if (match_superclass)
		{
			ret->SetInt(self->IsKindOf(checktype));
		}
		else
		{
			ret->SetInt(self->GetClass() == checktype);
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// CheckClass
//
// NON-ACTION function to calculate missile damage for the given actor
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, GetMissileDamage)
{
	if (numret > 0)
	{
		assert(ret != NULL);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_INT(mask);
		PARAM_INT(add);
		PARAM_INT_DEF(pick_pointer);

		self = COPY_AAPTR(self, pick_pointer);
		if (self == NULL)
		{
			ret->SetInt(0);
		}
		else
		{
			ret->SetInt(self->GetMissileDamage(mask, add));
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// CountInv
//
// NON-ACTION function to return the inventory count of an item.
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, CountInv)
{
	if (numret > 0)
	{
		assert(ret != NULL);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_CLASS(itemtype, AInventory);
		PARAM_INT_DEF(pick_pointer);

		self = COPY_AAPTR(self, pick_pointer);
		if (self == NULL || itemtype == NULL)
		{
			ret->SetInt(0);
		}
		else
		{
			AInventory *item = self->FindInventory(itemtype);
			ret->SetInt(item ? item->Amount : 0);
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// GetDistance
//
// NON-ACTION function to get the distance in double.
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, GetDistance)
{
	if (numret > 0)
	{
		assert(ret != NULL);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_BOOL(checkz);
		PARAM_INT_DEF(ptr);

		AActor *target = COPY_AAPTR(self, ptr);

		if (!target || target == self)
		{
			ret->SetFloat(0);
		}
		else
		{
			DVector3 diff = self->Vec3To(target);
			if (checkz)
				diff.Z += (target->Height - self->Height) / 2;
			else
				diff.Z = 0.;

			ret->SetFloat(diff.Length());
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// GetAngle
//
// NON-ACTION function to get the angle in degrees (normalized to -180..180)
//
//==========================================================================
enum GAFlags
{
	GAF_RELATIVE =			1,
	GAF_SWITCH =			1 << 1,
};

DEFINE_ACTION_FUNCTION(AActor, GetAngle)
{
	if (numret > 0)
	{
		assert(ret != NULL);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_INT(flags);
		PARAM_INT_DEF(ptr)

		AActor *target = COPY_AAPTR(self, ptr);

		if (!target || target == self)
		{
			ret->SetFloat(0);
		}
		else
		{
			DVector3 diff = (flags & GAF_SWITCH) ? target->Vec3To(self) : self->Vec3To(target);
			DAngle angto = diff.Angle();
			DAngle yaw = (flags & GAF_SWITCH) ? target->Angles.Yaw : self->Angles.Yaw;
			if (flags & GAF_RELATIVE) angto = deltaangle(yaw, angto);
			ret->SetFloat(angto.Degrees);
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// GetSpawnHealth
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, GetSpawnHealth)
{
	if (numret > 0)
	{
		PARAM_SELF_PROLOGUE(AActor);
		ret->SetInt(self->SpawnHealth());
		return 1;
	}
	return 0;
}

//==========================================================================
//
// GetSpriteAngle
//
// NON-ACTION function returns the sprite angle of a pointer.
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, GetSpriteAngle)
{
	if (numret > 0)
	{
		assert(ret != NULL);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_INT_DEF(ptr);

		AActor *target = COPY_AAPTR(self, ptr);
		if (target == nullptr)
		{
			ret->SetFloat(0.0);
		}
		else
		{
			const double ang = target->SpriteAngle.Degrees;
			ret->SetFloat(ang);
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// GetSpriteRotation
//
// NON-ACTION function returns the sprite rotation of a pointer.
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, GetSpriteRotation)
{
	if (numret > 0)
	{
		assert(ret != NULL);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_INT_DEF(ptr);

		AActor *target = COPY_AAPTR(self, ptr);
		if (target == nullptr)
		{
			ret->SetFloat(0.0);
		}
		else
		{
			const double ang = target->SpriteRotation.Degrees;
			ret->SetFloat(ang);
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// GetZAt
//
// NON-ACTION function to get the floor or ceiling z at (x, y) with 
// relativity being an option.
//==========================================================================
enum GZFlags
{
	GZF_ABSOLUTEPOS =			1,			// Use the absolute position instead of an offsetted one.
	GZF_ABSOLUTEANG =			1 << 1,		// Don't add the actor's angle to the parameter.
	GZF_CEILING =				1 << 2,		// Check the ceiling instead of the floor.
	GZF_3DRESTRICT =			1 << 3,		// Ignore midtextures and 3D floors above the pointer's z.
	GZF_NOPORTALS =				1 << 4,		// Don't pass through any portals.
	GZF_NO3DFLOOR =				1 << 5,		// Pass all 3D floors.
};

DEFINE_ACTION_FUNCTION(AActor, GetZAt)
{
	if (numret > 0)
	{
		assert(ret != NULL);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_FLOAT_DEF(px);
		PARAM_FLOAT_DEF(py);
		PARAM_ANGLE_DEF(angle);
		PARAM_INT_DEF(flags);
		PARAM_INT_DEF(pick_pointer);

		AActor *mobj = COPY_AAPTR(self, pick_pointer);
		if (mobj == nullptr)
		{
			ret->SetFloat(0);
		}
		else
		{
			// [MC] At any time, the NextLowest/Highest functions could be changed to include
			// more FFC flags to check. Don't risk it by just passing flags straight to it.

			DVector2 pos = { px, py };
			double z = 0.;
			int pflags = (flags & GZF_3DRESTRICT) ? FFCF_3DRESTRICT : 0;
			if (flags & GZF_NOPORTALS)			pflags |= FFCF_NOPORTALS;

			if (!(flags & GZF_ABSOLUTEPOS))
			{
				if (!(flags & GZF_ABSOLUTEANG))
				{
					angle += mobj->Angles.Yaw;
				}

				double s = angle.Sin();
				double c = angle.Cos();
				pos = mobj->Vec2Offset(pos.X * c + pos.Y * s, pos.X * s - pos.Y * c);
			}
			sector_t *sec = P_PointInSector(pos);

			if (sec)
			{
				if (flags & GZF_CEILING)
				{
					if ((flags & GZF_NO3DFLOOR) && (flags & GZF_NOPORTALS))
					{
						z = sec->ceilingplane.ZatPoint(pos);
					}
					else if (flags & GZF_NO3DFLOOR)
					{
						z = sec->HighestCeilingAt(pos);
					}
					else
					{	// [MC] Handle strict 3D floors and portal toggling via the flags passed to it.
						z = sec->NextHighestCeilingAt(pos.X, pos.Y, mobj->Z(), mobj->Top(), pflags);
					}
				}
				else
				{
					if ((flags & GZF_NO3DFLOOR) && (flags & GZF_NOPORTALS))
					{
						z = sec->floorplane.ZatPoint(pos);
					}
					else if (flags & GZF_NO3DFLOOR)
					{
						z = sec->LowestFloorAt(pos);
					}
					else
					{
						z = sec->NextLowestFloorAt(pos.X, pos.Y, mobj->Z(), pflags, mobj->MaxStepHeight);
					}
				}
			}
			ret->SetFloat(z);
			return 1;
		}
	}
	return 0;
}

//==========================================================================
//
// GetCrouchFactor
//
// NON-ACTION function to retrieve a player's crouching factor.
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, GetCrouchFactor)
{
	if (numret > 0)
	{
		assert(ret != NULL);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_INT_DEF(ptr);
		AActor *mobj = COPY_AAPTR(self, ptr);

		if (!mobj || !mobj->player)
		{
			ret->SetFloat(1);
		}
		else
		{
			ret->SetFloat(mobj->player->crouchfactor);
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// GetCVar
//
// NON-ACTION function that works like ACS's GetCVar.
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, GetCVar)
{
	if (numret > 0)
	{
		assert(ret != nullptr);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_STRING(cvarname);

		FBaseCVar *cvar = GetCVar(self, cvarname);
		if (cvar == nullptr)
		{
			ret->SetFloat(0);
		}
		else
		{
			ret->SetFloat(cvar->GetGenericRep(CVAR_Float).Float);
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// GetCVar
//
// NON-ACTION function that works like ACS's GetCVar.
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, GetCVarString)
{
	if (numret > 0)
	{
		assert(ret != nullptr);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_STRING(cvarname);

		FBaseCVar *cvar = GetCVar(self, cvarname);
		if (cvar == nullptr)
		{
			ret->SetString("");
		}
		else
		{
			ret->SetString(cvar->GetGenericRep(CVAR_String).String);
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// GetPlayerInput
//
// NON-ACTION function that works like ACS's GetPlayerInput.
// Takes a pointer as anyone may or may not be a player.
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, GetPlayerInput)
{
	if (numret > 0)
	{
		assert(ret != nullptr);
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_INT		(inputnum);
		PARAM_INT_DEF(ptr);

		AActor *mobj = COPY_AAPTR(self, ptr);

		//Need a player.
		if (!mobj || !mobj->player)
		{
			ret->SetInt(0);
		}
		else
		{
			ret->SetInt(P_Thing_CheckInputNum(mobj->player, inputnum));
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// CountProximity
//
// NON-ACTION function of A_CheckProximity that returns how much it counts.
// Takes a pointer as anyone may or may not be a player.
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, CountProximity)
{
	if (numret > 0)
	{
		PARAM_SELF_PROLOGUE(AActor);
		PARAM_CLASS(classname, AActor);
		PARAM_FLOAT(distance);
		PARAM_INT_DEF(flags);
		PARAM_INT_DEF(ptr);

		AActor *mobj = COPY_AAPTR(self, ptr);
		if (mobj == nullptr)
		{
			ret->SetInt(0);
		}
		else
		{
			ret->SetInt(P_Thing_CheckProximity(self, classname, distance, 0, flags, ptr, true));
		}
		return 1;
	}
	return 0;
}

//===========================================================================
//
// __decorate_internal_int__
// __decorate_internal_bool__
// __decorate_internal_float__
//
// Placeholders for forcing DECORATE to cast numbers. If actually called,
// returns whatever was passed.
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, __decorate_internal_int__)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(returnme);
	ACTION_RETURN_INT(returnme);
}

DEFINE_ACTION_FUNCTION(AActor, __decorate_internal_bool__)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_BOOL(returnme);
	ACTION_RETURN_BOOL(returnme);
}

DEFINE_ACTION_FUNCTION(AActor, __decorate_internal_float__)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(returnme);
	if (numret > 0)
	{
		ret->SetFloat(returnme);
		return 1;
	}
	return 0;
}

//==========================================================================
//
// A_RearrangePointers
//
// Allow an actor to change its relationship to other actors by
// copying pointers freely between TARGET MASTER and TRACER.
// Can also assign null value, but does not duplicate A_ClearTarget.
//
//==========================================================================


DEFINE_ACTION_FUNCTION(AActor, A_RearrangePointers)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(ptr_target);
	PARAM_INT_DEF	(ptr_master);
	PARAM_INT_DEF	(ptr_tracer);
	PARAM_INT_DEF	(flags);

	// Rearrange pointers internally

	// Fetch all values before modification, so that all fields can get original values
	AActor
		*gettarget = self->target,
		*getmaster = self->master,
		*gettracer = self->tracer;

	switch (ptr_target) // pick the new target
	{
	case AAPTR_MASTER:
		self->target = getmaster;
		if (!(PTROP_UNSAFETARGET & flags)) VerifyTargetChain(self);
		break;
	case AAPTR_TRACER:
		self->target = gettracer;
		if (!(PTROP_UNSAFETARGET & flags)) VerifyTargetChain(self);
		break;
	case AAPTR_NULL:
		self->target = NULL;
		// THIS IS NOT "A_ClearTarget", so no other targeting info is removed
		break;
	}

	// presently permitting non-monsters to set master
	switch (ptr_master) // pick the new master
	{
	case AAPTR_TARGET:
		self->master = gettarget;
		if (!(PTROP_UNSAFEMASTER & flags)) VerifyMasterChain(self);
		break;
	case AAPTR_TRACER:
		self->master = gettracer;
		if (!(PTROP_UNSAFEMASTER & flags)) VerifyMasterChain(self);
		break;
	case AAPTR_NULL:
		self->master = NULL;
		break;
	}

	switch (ptr_tracer) // pick the new tracer
	{
	case AAPTR_TARGET:
		self->tracer = gettarget;
		break; // no verification deemed necessary; the engine never follows a tracer chain(?)
	case AAPTR_MASTER:
		self->tracer = getmaster;
		break; // no verification deemed necessary; the engine never follows a tracer chain(?)
	case AAPTR_NULL:
		self->tracer = NULL;
		break;
	}
	return 0;
}

//==========================================================================
//
// A_TransferPointer
//
// Copy one pointer (MASTER, TARGET or TRACER) from this actor (SELF),
// or from this actor's MASTER, TARGET or TRACER.
//
// You can copy any one of that actor's pointers
//
// Assign the copied pointer to any one pointer in SELF,
// MASTER, TARGET or TRACER.
//
// Any attempt to make an actor point to itself will replace the pointer
// with a null value.
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_TransferPointer)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(ptr_source);
	PARAM_INT		(ptr_recipient);
	PARAM_INT		(ptr_sourcefield);
	PARAM_INT_DEF	(ptr_recipientfield);
	PARAM_INT_DEF	(flags);

	AActor *source, *recipient;

	// Exchange pointers with actors to whom you have pointers (or with yourself, if you must)
	source = COPY_AAPTR(self, ptr_source);
	recipient = COPY_AAPTR(self, ptr_recipient);	// pick an actor to store the provided pointer value
	if (recipient == NULL)
	{
		return 0;
	}

	// convert source from dataprovider to data
	source = COPY_AAPTR(source, ptr_sourcefield);
	if (source == recipient)
	{ // The recepient should not acquire a pointer to itself; will write NULL}
		source = NULL;
	}
	if (ptr_recipientfield == AAPTR_DEFAULT)
	{ // If default: Write to same field as data was read from
		ptr_recipientfield = ptr_sourcefield;
	}
	ASSIGN_AAPTR(recipient, ptr_recipientfield, source, flags);
	return 0;
}

//==========================================================================
//
// A_CopyFriendliness
//
// Join forces with one of the actors you are pointing to (MASTER by default)
//
// Normal CopyFriendliness reassigns health. This function will not.
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CopyFriendliness)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF	(ptr_source);
	
	if (self->player != NULL)
	{
		return 0;
	}

	AActor *source = COPY_AAPTR(self, ptr_source);
	if (source != NULL)
	{ // No change in current target or health
		self->CopyFriendliness(source, false, false);
	}
	return 0;
}

//==========================================================================
//
// Custom sound functions. 
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PlaySound)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_SOUND_DEF	(soundid);
	PARAM_INT_DEF	(channel);
	PARAM_FLOAT_DEF	(volume);
	PARAM_BOOL_DEF	(looping);
	PARAM_FLOAT_DEF	(attenuation);
	PARAM_BOOL_DEF	(local);

	if (!looping)
	{
		if (!(channel & CHAN_NOSTOP) || !S_IsActorPlayingSomething(self, channel & 7, soundid))
		{
			S_PlaySound(self, channel, soundid, (float)volume, (float)attenuation, local);
		}
	}
	else
	{
		if (!S_IsActorPlayingSomething (self, channel&7, soundid))
		{
			S_PlaySound(self, channel | CHAN_LOOP, soundid, (float)volume, (float)attenuation, local);
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_StopSound)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(slot);

	S_StopSound(self, slot);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SoundVolume)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(channel);
	PARAM_FLOAT(volume);
	S_ChangeSoundVolume(self, channel, static_cast<float>(volume));
	return 0;
}


//==========================================================================
//
// These come from a time when DECORATE constants did not exist yet and
// the sound interface was less flexible. As a result the parameters are
// not optimal and these functions have been deprecated in favor of extending
// A_PlaySound and A_StopSound.
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PlaySoundEx)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_SOUND		(soundid);
	PARAM_NAME		(channel);
	PARAM_BOOL_DEF	(looping);
	PARAM_INT_DEF	(attenuation_raw);

	float attenuation;
	switch (attenuation_raw)
	{
		case -1: attenuation = ATTN_STATIC;	break; // drop off rapidly
		default:
		case  0: attenuation = ATTN_NORM;	break; // normal
		case  1:
		case  2: attenuation = ATTN_NONE;	break; // full volume
	}

	if (channel < NAME_Auto || channel > NAME_SoundSlot7)
	{
		channel = NAME_Auto;
	}

	if (!looping)
	{
		S_Sound (self, int(channel) - NAME_Auto, soundid, 1, attenuation);
	}
	else
	{
		if (!S_IsActorPlayingSomething (self, int(channel) - NAME_Auto, soundid))
		{
			S_Sound (self, (int(channel) - NAME_Auto) | CHAN_LOOP, soundid, 1, attenuation);
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_StopSoundEx)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME(channel);

	if (channel > NAME_Auto && channel <= NAME_SoundSlot7)
	{
		S_StopSound (self, int(channel) - NAME_Auto);
	}
	return 0;
}

//==========================================================================
//
// Generic seeker missile function
//
//==========================================================================
static FRandom pr_seekermissile ("SeekerMissile");
enum
{
	SMF_LOOK = 1,
	SMF_PRECISE = 2,
	SMF_CURSPEED = 4,
};
DEFINE_ACTION_FUNCTION(AActor, A_SeekerMissile)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(ang1);
	PARAM_INT(ang2);
	PARAM_INT_DEF(flags);
	PARAM_INT_DEF(chance);
	PARAM_INT_DEF(distance);

	if ((flags & SMF_LOOK) && (self->tracer == nullptr) && (pr_seekermissile()<chance))
	{
		self->tracer = P_RoughMonsterSearch (self, distance, true);
	}
	if (!P_SeekerMissile(self, clamp<int>(ang1, 0, 90), clamp<int>(ang2, 0, 90), !!(flags & SMF_PRECISE), !!(flags & SMF_CURSPEED)))
	{
		if (flags & SMF_LOOK)
		{ // This monster is no longer seekable, so let us look for another one next time.
			self->tracer = NULL;
		}
	}
	return 0;
}

//==========================================================================
//
// Hitscan attack with a customizable amount of bullets (specified in damage)
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_BulletAttack)
{
	PARAM_SELF_PROLOGUE(AActor);

	int i;
		
	if (!self->target) return 0;

	A_FaceTarget (self);

	DAngle slope = P_AimLineAttack (self, self->Angles.Yaw, MISSILERANGE);

	S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
	for (i = self->GetMissileDamage (0, 1); i > 0; --i)
    {
		DAngle angle = self->Angles.Yaw + pr_cabullet.Random2() * (5.625 / 256.);
		int damage = ((pr_cabullet()%5)+1)*3;
		P_LineAttack(self, angle, MISSILERANGE, slope, damage,
			NAME_Hitscan, NAME_BulletPuff);
    }
	return 0;
}


//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_Jump)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(maxchance);

	paramnum++;		// Increment paramnum to point at the first jump target
	int count = numparam - paramnum;
	if (count > 0 && (maxchance >= 256 || pr_cajump() < maxchance))
	{
		int jumpnum = (count == 1 ? 0 : (pr_cajump() % count));
		PARAM_STATE_ACTION_AT(paramnum + jumpnum, jumpto);
		ACTION_RETURN_STATE(jumpto);
	}
	ACTION_RETURN_STATE(NULL);
}

//==========================================================================
//
// State jump function
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, CheckInventory)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS		(itemtype, AInventory);
	PARAM_INT		(itemamount);
	PARAM_INT_DEF	(setowner);

	if (itemtype == nullptr)
	{
		ACTION_RETURN_BOOL(false);
	}
	AActor *owner = COPY_AAPTR(self, setowner);
	if (owner == nullptr)
	{
		ACTION_RETURN_BOOL(false);
	}

	AInventory *item = owner->FindInventory(itemtype);

	if (item)
	{
		if (itemamount > 0)
		{
			if (item->Amount >= itemamount)
			{
				ACTION_RETURN_BOOL(true);
			}
		}
		else if (item->Amount >= item->MaxAmount)
		{
			ACTION_RETURN_BOOL(true);
		}
	}
	ACTION_RETURN_BOOL(false);
}


//==========================================================================
//
// Parameterized version of A_Explode
//
//==========================================================================

enum
{
	XF_HURTSOURCE =		1,
	XF_NOTMISSILE =		4,
	XF_NOACTORTYPE =	1 << 3,
	XF_NOSPLASH	=		16,
};

DEFINE_ACTION_FUNCTION(AActor, A_Explode)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF	(damage);
	PARAM_INT_DEF	(distance);
	PARAM_INT_DEF	(flags);
	PARAM_BOOL_DEF	(alert);
	PARAM_INT_DEF	(fulldmgdistance);
	PARAM_INT_DEF	(nails);
	PARAM_INT_DEF	(naildamage);
	PARAM_CLASS_DEF	(pufftype, AActor);
	PARAM_NAME_DEF	(damagetype);

	if (damage < 0)	// get parameters from metadata
	{
		damage = self->IntVar(NAME_ExplosionDamage);
		distance = self->IntVar(NAME_ExplosionRadius);
		flags = !self->BoolVar(NAME_DontHurtShooter);
		alert = false;
	}
	if (distance <= 0) distance = damage;

	// NailBomb effect, from SMMU but not from its source code: instead it was implemented and
	// generalized from the documentation at http://www.doomworld.com/eternity/engine/codeptrs.html

	if (nails)
	{
		DAngle ang;
		for (int i = 0; i < nails; i++)
		{
			ang = i*360./nails;
			// Comparing the results of a test wad with Eternity, it seems A_NailBomb does not aim
			P_LineAttack(self, ang, MISSILERANGE, 0.,
				//P_AimLineAttack (self, ang, MISSILERANGE), 
				naildamage, NAME_Hitscan, pufftype, (self->flags & MF_MISSILE) ? LAF_TARGETISSOURCE : 0);
		}
	}

	if (!(flags & XF_NOACTORTYPE) && damagetype == NAME_None)
	{
		damagetype = self->DamageType;
	}

	int pflags = 0;
	if (flags & XF_HURTSOURCE)	pflags |= RADF_HURTSOURCE;
	if (flags & XF_NOTMISSILE)	pflags |= RADF_SOURCEISSPOT;

	int count = P_RadiusAttack (self, self->target, damage, distance, damagetype, pflags, fulldmgdistance);
	if (!(flags & XF_NOSPLASH)) P_CheckSplash(self, distance);
	if (alert && self->target != NULL && self->target->player != NULL)
	{
		P_NoiseAlert(self->target, self);
	}
	ACTION_RETURN_INT(count);
}

//==========================================================================
//
// A_RadiusThrust
//
//==========================================================================

enum
{
	RTF_AFFECTSOURCE =			1,
	RTF_NOIMPACTDAMAGE =		2,
	RTF_NOTMISSILE =			4,
};

DEFINE_ACTION_FUNCTION(AActor, A_RadiusThrust)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF	(force);
	PARAM_INT_DEF	(distance);
	PARAM_INT_DEF	(flags);
	PARAM_INT_DEF	(fullthrustdistance);

	bool sourcenothrust = false;

	if (force == 0) force = 128;
	if (distance <= 0) distance = abs(force);

	// Temporarily negate MF2_NODMGTHRUST on the shooter, since it renders this function useless.
	if (!(flags & RTF_NOTMISSILE) && self->target != NULL && self->target->flags2 & MF2_NODMGTHRUST)
	{
		sourcenothrust = true;
		self->target->flags2 &= ~MF2_NODMGTHRUST;
	}

	P_RadiusAttack (self, self->target, force, distance, self->DamageType, flags | RADF_NODAMAGE, fullthrustdistance);
	P_CheckSplash(self, distance);

	if (sourcenothrust)
	{
		self->target->flags2 |= MF2_NODMGTHRUST;
	}
	return 0;
}

//==========================================================================
//
// A_RadiusDamageSelf
//
//==========================================================================
enum
{
	RDSF_BFGDAMAGE = 1,
};

DEFINE_ACTION_FUNCTION(AActor, A_RadiusDamageSelf)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(damage);
	PARAM_FLOAT_DEF(distance);
	PARAM_INT_DEF(flags);
	PARAM_CLASS_DEF(flashtype, AActor);

	int 				i;
	int 				damageSteps;
	int 				actualDamage;
	double 				actualDistance;

	actualDistance = self->Distance3D(self->target);
	if (actualDistance < distance)
	{
		// [XA] Decrease damage with distance. Use the BFG damage
		//      calculation formula if the flag is set (essentially
		//      a generalization of SMMU's BFG11K behavior, used
		//      with fraggle's blessing.)
		damageSteps = damage - int(damage * actualDistance / distance);
		if (flags & RDSF_BFGDAMAGE)
		{
			actualDamage = 0;
			for (i = 0; i < damageSteps; ++i)
				actualDamage += (pr_bfgselfdamage() & 7) + 1;
		}
		else
		{
			actualDamage = damageSteps;
		}

		// optional "flash" effect -- spawn an actor on
		// the player to indicate bad things happened.
		AActor *flash = NULL;
		if(flashtype != NULL)
			flash = Spawn(flashtype, self->target->PosPlusZ(self->target->Height / 4), ALLOW_REPLACE);

		int dmgFlags = 0;
		FName dmgType = NAME_BFGSplash;

		if (flash != NULL)
		{
			if (flash->flags5 & MF5_PUFFGETSOWNER) flash->target = self->target;
			if (flash->flags3 & MF3_FOILINVUL) dmgFlags |= DMG_FOILINVUL;
			if (flash->flags7 & MF7_FOILBUDDHA) dmgFlags |= DMG_FOILBUDDHA;
			dmgType = flash->DamageType;
		}

		int newdam = P_DamageMobj(self->target, self, self->target, actualDamage, dmgType, dmgFlags);
		P_TraceBleed(newdam > 0 ? newdam : actualDamage, self->target, self);
	}

	return 0;
}

//==========================================================================
//
// The ultimate code pointer: Fully customizable missiles!
//
//==========================================================================
enum CM_Flags
{
	CMF_AIMMODE = 3,
	CMF_TRACKOWNER = 4,
	CMF_CHECKTARGETDEAD = 8,

	CMF_ABSOLUTEPITCH = 16,
	CMF_OFFSETPITCH = 32,
	CMF_SAVEPITCH = 64,

	CMF_ABSOLUTEANGLE = 128,
	CMF_BADPITCH = 256
};

DEFINE_ACTION_FUNCTION(AActor, A_SpawnProjectile)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS		(ti, AActor);
	PARAM_FLOAT_DEF	(Spawnheight);
	PARAM_FLOAT_DEF	(Spawnofs_xy);
	PARAM_ANGLE_DEF	(Angle);
	PARAM_INT_DEF	(flags);
	PARAM_ANGLE_DEF	(Pitch);
	PARAM_INT_DEF	(ptr);

	AActor *ref = COPY_AAPTR(self, ptr);

	int aimmode = flags & CMF_AIMMODE;

	AActor * targ;
	AActor * missile = nullptr;

	if (ref != NULL || aimmode == 2)
	{
		if (ti) 
		{
			DAngle angle = self->Angles.Yaw - 90;
			double x = Spawnofs_xy * angle.Cos();
			double y = Spawnofs_xy * angle.Sin();
			double z = Spawnheight + self->GetBobOffset() - 32 + (self->player? self->player->crouchoffset : 0.);

			DVector3 pos = self->Pos();
			switch (aimmode)
			{
			case 0:
			default:
				// same adjustment as above (in all 3 directions this time) - for better aiming!
				self->SetXYZ(self->Vec3Offset(x, y, z));
				missile = P_SpawnMissileXYZ(self->PosPlusZ(32.), self, ref, ti, false);
				self->SetXYZ(pos);
				break;

			case 1:
				missile = P_SpawnMissileXYZ(self->Vec3Offset(x, y, self->GetBobOffset() + Spawnheight), self, ref, ti, false);
				break;

			case 2:
				self->SetXYZ(self->Vec3Offset(x, y, 0.));
				missile = P_SpawnMissileAngleZSpeed(self, self->Z() + self->GetBobOffset() + Spawnheight, ti, self->Angles.Yaw, 0, GetDefaultByType(ti)->Speed, self, false);
				self->SetXYZ(pos);

				flags |= CMF_ABSOLUTEPITCH;

				break;
			}

			if (missile != NULL)
			{
				// Use the actual velocity instead of the missile's Speed property
				// so that this can handle missiles with a high vertical velocity 
				// component properly.

				double missilespeed;

				if ( (CMF_ABSOLUTEPITCH|CMF_OFFSETPITCH) & flags)
				{
					if (CMF_OFFSETPITCH & flags)
					{
						Pitch += missile->Vel.Pitch();
					}
					missilespeed = fabs(Pitch.Cos() * missile->Speed);
					missile->Vel.Z = Pitch.Sin() * missile->Speed;
					if (!(flags & CMF_BADPITCH)) missile->Vel.Z *= -1;
				}
				else
				{
					missilespeed = missile->VelXYToSpeed();
				}

				if (CMF_SAVEPITCH & flags)
				{
					missile->Angles.Pitch = Pitch;
					// In aimmode 0 and 1 without absolutepitch or offsetpitch, the pitch parameter
					// contains the unapplied parameter. In that case, it is set as pitch without
					// otherwise affecting the spawned actor.
				}

				missile->Angles.Yaw = (CMF_ABSOLUTEANGLE & flags) ? Angle : missile->Angles.Yaw + Angle;
				missile->VelFromAngle(missilespeed);
	
				// handle projectile shooting projectiles - track the
				// links back to a real owner
                if (self->isMissile(!!(flags & CMF_TRACKOWNER)))
                {
                	AActor *owner = self ;//->target;
                	while (owner->isMissile(!!(flags & CMF_TRACKOWNER)) && owner->target)
						owner = owner->target;
                	targ = owner;
                	missile->target = owner;
					// automatic handling of seeker missiles
					if (self->flags2 & missile->flags2 & MF2_SEEKERMISSILE)
					{
						missile->tracer = self->tracer;
					}
                }
				else if (missile->flags2 & MF2_SEEKERMISSILE)
				{
					// automatic handling of seeker missiles
					missile->tracer = self->target;
				}
				// we must redo the spectral check here because the owner is set after spawning so the FriendPlayer value may be wrong
				if (missile->flags4 & MF4_SPECTRAL)
				{
					if (missile->target != NULL)
					{
						missile->SetFriendPlayer(missile->target->player);
					}
					else
					{
						missile->FriendPlayer = 0;
					}
				}
				P_CheckMissileSpawn(missile, self->radius);
			}
		}
	}
	else if (flags & CMF_CHECKTARGETDEAD)
	{
		// Target is dead and the attack shall be aborted.
		if (self->SeeState != NULL && (self->health > 0 || !(self->flags3 & MF3_ISMONSTER)))
			self->SetState(self->SeeState);
	}
	ACTION_RETURN_OBJECT(missile);
}

//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
enum CBA_Flags
{
	CBAF_AIMFACING = 1,
	CBAF_NORANDOM = 2,
	CBAF_EXPLICITANGLE = 4,
	CBAF_NOPITCH = 8,
	CBAF_NORANDOMPUFFZ = 16,
	CBAF_PUFFTARGET = 32,
	CBAF_PUFFMASTER = 64,
	CBAF_PUFFTRACER = 128,
};

static void AimBulletMissile(AActor *proj, AActor *puff, int flags, bool temp, bool cba);

DEFINE_ACTION_FUNCTION(AActor, A_CustomBulletAttack)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_ANGLE		(spread_xy);
	PARAM_ANGLE		(spread_z);
	PARAM_INT		(numbullets);
	PARAM_INT		(damageperbullet);
	PARAM_CLASS_DEF	(pufftype, AActor);
	PARAM_FLOAT_DEF	(range);
	PARAM_INT_DEF	(flags);
	PARAM_INT_DEF	(ptr);
	PARAM_CLASS_DEF (missile, AActor);
	PARAM_FLOAT_DEF (Spawnheight);
	PARAM_FLOAT_DEF (Spawnofs_xy);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (range == 0)
		range = MISSILERANGE;

	int i;
	DAngle bangle;
	DAngle bslope = 0.;
	int laflags = (flags & CBAF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

	if (ref != NULL || (flags & CBAF_AIMFACING))
	{
		if (!(flags & CBAF_AIMFACING))
		{
			A_Face(self, ref);
		}
		bangle = self->Angles.Yaw;

		if (!(flags & CBAF_NOPITCH)) bslope = P_AimLineAttack (self, bangle, MISSILERANGE);
		if (pufftype == nullptr) pufftype = PClass::FindActor(NAME_BulletPuff);

		S_Sound (self, CHAN_WEAPON, self->AttackSound, 1, ATTN_NORM);
		for (i = 0; i < numbullets; i++)
		{
			DAngle angle = bangle;
			DAngle slope = bslope;

			if (flags & CBAF_EXPLICITANGLE)
			{
				angle += spread_xy;
				slope += spread_z;
			}
			else
			{
				angle += spread_xy * (pr_cwbullet.Random2() / 255.);
				slope += spread_z * (pr_cwbullet.Random2() / 255.);
			}

			int damage = damageperbullet;

			if (!(flags & CBAF_NORANDOM))
				damage *= ((pr_cabullet()%3)+1);

			AActor *puff = P_LineAttack(self, angle, range, slope, damage, NAME_Hitscan, pufftype, laflags);
			if (missile != nullptr && pufftype != nullptr)
			{
				double x = Spawnofs_xy * angle.Cos();
				double y = Spawnofs_xy * angle.Sin();
				
				DVector3 pos = self->Pos();
				self->SetXYZ(self->Vec3Offset(x, y, 0.));
				AActor *proj = P_SpawnMissileAngleZSpeed(self, self->Z() + self->GetBobOffset() + Spawnheight, missile, self->Angles.Yaw, 0, GetDefaultByType(missile)->Speed, self, false);
				self->SetXYZ(pos);
				
				if (proj)
				{
					bool temp = (puff == nullptr);
					if (!puff)
					{
						puff = P_LineAttack(self, angle, range, slope, 0, NAME_Hitscan, pufftype, laflags | LAF_NOINTERACT);
					}
					if (puff)
					{			
						AimBulletMissile(proj, puff, flags, temp, true);
					}
				}
			}
		}
    }
	return 0;
}

//==========================================================================
//
// A fully customizable melee attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_CustomMeleeAttack)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF	(damage);
	PARAM_SOUND_DEF	(meleesound);
	PARAM_SOUND_DEF	(misssound);
	PARAM_NAME_DEF	(damagetype);
	PARAM_BOOL_DEF	(bleed);

	if (damagetype == NAME_None)
		damagetype = NAME_Melee;	// Melee is the default type

	if (!self->target)
		return 0;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		if (meleesound)
			S_Sound (self, CHAN_WEAPON, meleesound, 1, ATTN_NORM);
		int newdam = P_DamageMobj (self->target, self, self, damage, damagetype);
		if (bleed)
			P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
	else
	{
		if (misssound)
			S_Sound (self, CHAN_WEAPON, misssound, 1, ATTN_NORM);
	}
	return 0;
}

//==========================================================================
//
// A fully customizable combo attack
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_CustomComboAttack)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS		(ti, AActor);
	PARAM_FLOAT		(spawnheight);
	PARAM_INT		(damage);
	PARAM_SOUND_DEF	(meleesound);
	PARAM_NAME_DEF	(damagetype);
	PARAM_BOOL_DEF	(bleed);

	if (!self->target)
		return 0;
				
	A_FaceTarget (self);
	if (self->CheckMeleeRange())
	{
		if (damagetype == NAME_None)
			damagetype = NAME_Melee;	// Melee is the default type
		if (meleesound)
			S_Sound (self, CHAN_WEAPON, meleesound, 1, ATTN_NORM);
		int newdam = P_DamageMobj (self->target, self, self, damage, damagetype);
		if (bleed)
			P_TraceBleed (newdam > 0 ? newdam : damage, self->target, self);
	}
	else if (ti) 
	{
		// This seemingly senseless code is needed for proper aiming.
		double add = spawnheight + self->GetBobOffset() - 32;
		self->AddZ(add);
		AActor *missile = P_SpawnMissileXYZ (self->PosPlusZ(32.), self, self->target, ti, false);
		self->AddZ(-add);

		if (missile)
		{
			// automatic handling of seeker missiles
			if (missile->flags2 & MF2_SEEKERMISSILE)
			{
				missile->tracer = self->target;
			}
			P_CheckMissileSpawn(missile, self->radius);
		}
	}
	return 0;
}

//==========================================================================
//
// State jump function
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AStateProvider, A_JumpIfNoAmmo)
{
	PARAM_ACTION_PROLOGUE(AStateProvider);
	PARAM_STATE_ACTION(jump);

	if (!ACTION_CALL_FROM_PSPRITE() || self->player->ReadyWeapon == nullptr)
	{
		ACTION_RETURN_STATE(NULL);
	}

	if (!self->player->ReadyWeapon->CheckAmmo(self->player->ReadyWeapon->bAltFire, false, true))
	{
		ACTION_RETURN_STATE(jump);
	}
	ACTION_RETURN_STATE(NULL);
}


//==========================================================================
//
// An even more customizable hitscan attack
//
//==========================================================================
enum FB_Flags
{
	FBF_USEAMMO = 1,
	FBF_NORANDOM = 2,
	FBF_EXPLICITANGLE = 4,
	FBF_NOPITCH = 8,
	FBF_NOFLASH = 16,
	FBF_NORANDOMPUFFZ = 32,
	FBF_PUFFTARGET = 64,
	FBF_PUFFMASTER = 128,
	FBF_PUFFTRACER = 256,
};

static void AimBulletMissile(AActor *proj, AActor *puff, int flags, bool temp, bool cba)
{
	if (proj && puff)
	{
		if (proj)
		{
			// FAF_BOTTOM = 1
			// Aim for the base of the puff as that's where blood puffs will spawn... roughly.

			A_Face(proj, puff, 0., 0., 0., 0., 1);
			proj->Vel3DFromAngle(proj->Angles.Pitch, proj->Speed);

			if (!temp)
			{
				if (cba)
				{
					if (flags & CBAF_PUFFTARGET)	proj->target = puff;
					if (flags & CBAF_PUFFMASTER)	proj->master = puff;
					if (flags & CBAF_PUFFTRACER)	proj->tracer = puff;
				}
				else
				{
					if (flags & FBF_PUFFTARGET)	proj->target = puff;
					if (flags & FBF_PUFFMASTER)	proj->master = puff;
					if (flags & FBF_PUFFTRACER)	proj->tracer = puff;
				}
			}
		}
	}
	if (puff && temp)
	{
		puff->Destroy();
	}
}

DEFINE_ACTION_FUNCTION(AStateProvider, A_FireBullets)
{
	PARAM_ACTION_PROLOGUE(AStateProvider);
	PARAM_ANGLE		(spread_xy);
	PARAM_ANGLE		(spread_z);
	PARAM_INT		(numbullets);
	PARAM_INT		(damageperbullet);
	PARAM_CLASS_DEF	(pufftype, AActor);
	PARAM_INT_DEF	(flags);
	PARAM_FLOAT_DEF	(range);
	PARAM_CLASS_DEF (missile, AActor);
	PARAM_FLOAT_DEF (Spawnheight);
	PARAM_FLOAT_DEF (Spawnofs_xy);

	if (!self->player) return 0;

	player_t *player = self->player;
	AWeapon *weapon = player->ReadyWeapon;

	int i;
	DAngle bangle;
	DAngle bslope = 0.;
	int laflags = (flags & FBF_NORANDOMPUFFZ)? LAF_NORANDOMPUFFZ : 0;

	if ((flags & FBF_USEAMMO) && weapon && ACTION_CALL_FROM_PSPRITE())
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return 0;	// out of ammo
	}
	
	if (range == 0)	range = PLAYERMISSILERANGE;

	if (!(flags & FBF_NOFLASH)) static_cast<APlayerPawn *>(self)->PlayAttacking2 ();

	if (!(flags & FBF_NOPITCH)) bslope = P_BulletSlope(self);
	bangle = self->Angles.Yaw;

	if (pufftype == NULL) pufftype = PClass::FindActor(NAME_BulletPuff);

	if (weapon != NULL)
	{
		S_Sound(self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);
	}

	if ((numbullets == 1 && !player->refire) || numbullets == 0)
	{
		int damage = damageperbullet;

		if (!(flags & FBF_NORANDOM))
			damage *= ((pr_cwbullet()%3)+1);

		AActor *puff = P_LineAttack(self, bangle, range, bslope, damage, NAME_Hitscan, pufftype, laflags);

		if (missile != nullptr)
		{
			bool temp = false;
			DAngle ang = self->Angles.Yaw - 90;
			DVector2 ofs = ang.ToVector(Spawnofs_xy);
			AActor *proj = P_SpawnPlayerMissile(self, ofs.X, ofs.Y, Spawnheight, missile, bangle, nullptr, nullptr, false, true);
			if (proj)
			{
				if (!puff)
				{
					temp = true;
					puff = P_LineAttack(self, bangle, range, bslope, 0, NAME_Hitscan, pufftype, laflags | LAF_NOINTERACT);
				}
				AimBulletMissile(proj, puff, flags, temp, false);
			}
		}
	}
	else 
	{
		if (numbullets < 0)
			numbullets = 1;
		for (i = 0; i < numbullets; i++)
		{
			DAngle angle = bangle;
			DAngle slope = bslope;

			if (flags & FBF_EXPLICITANGLE)
			{
				angle += spread_xy;
				slope += spread_z;
			}
			else
			{
				angle += spread_xy * (pr_cwbullet.Random2() / 255.);
				slope += spread_z * (pr_cwbullet.Random2() / 255.);
			}

			int damage = damageperbullet;

			if (!(flags & FBF_NORANDOM))
				damage *= ((pr_cwbullet()%3)+1);

			AActor *puff = P_LineAttack(self, angle, range, slope, damage, NAME_Hitscan, pufftype, laflags);

			if (missile != nullptr)
			{
				bool temp = false;
				DAngle ang = self->Angles.Yaw - 90;
				DVector2 ofs = ang.ToVector(Spawnofs_xy);
				AActor *proj = P_SpawnPlayerMissile(self, ofs.X, ofs.Y, Spawnheight, missile, angle, nullptr, nullptr, false, true);
				if (proj)
				{
					if (!puff)
					{
						temp = true;
						puff = P_LineAttack(self, angle, range, slope, 0, NAME_Hitscan, pufftype, laflags | LAF_NOINTERACT);
					}
					AimBulletMissile(proj, puff, flags, temp, false);
				}
			}
		}
	}
	return 0;
}


//==========================================================================
//
// A_FireProjectile
//
//==========================================================================
enum FP_Flags
{
	FPF_AIMATANGLE = 1,
	FPF_TRANSFERTRANSLATION = 2,
	FPF_NOAUTOAIM = 4,
};
DEFINE_ACTION_FUNCTION(AStateProvider, A_FireProjectile)
{
	PARAM_ACTION_PROLOGUE(AStateProvider);
	PARAM_CLASS		(ti, AActor);
	PARAM_ANGLE_DEF	(angle);
	PARAM_BOOL_DEF	(useammo);
	PARAM_FLOAT_DEF	(spawnofs_xy);
	PARAM_FLOAT_DEF	(spawnheight);
	PARAM_INT_DEF	(flags);
	PARAM_ANGLE_DEF	(pitch);

	if (!self->player)
		ACTION_RETURN_OBJECT(nullptr);

	player_t *player = self->player;
	AWeapon *weapon = player->ReadyWeapon;
	FTranslatedLineTarget t;

		// Only use ammo if called from a weapon
	if (useammo && ACTION_CALL_FROM_PSPRITE() && weapon)
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			ACTION_RETURN_OBJECT(nullptr);	// out of ammo
	}

	if (ti) 
	{
		DAngle ang = self->Angles.Yaw - 90;
		DVector2 ofs = ang.ToVector(spawnofs_xy);
		DAngle shootangle = self->Angles.Yaw;

		if (flags & FPF_AIMATANGLE) shootangle += angle;

		// Temporarily adjusts the pitch
		DAngle saved_player_pitch = self->Angles.Pitch;
		self->Angles.Pitch += pitch;
		AActor * misl=P_SpawnPlayerMissile (self, ofs.X, ofs.Y, spawnheight, ti, shootangle, &t, NULL, false, (flags & FPF_NOAUTOAIM) != 0);
		self->Angles.Pitch = saved_player_pitch;

		// automatic handling of seeker missiles
		if (misl)
		{
			if (flags & FPF_TRANSFERTRANSLATION)
				misl->Translation = self->Translation;
			if (t.linetarget && !t.unlinked && (misl->flags2 & MF2_SEEKERMISSILE))
				misl->tracer = t.linetarget;
			if (!(flags & FPF_AIMATANGLE))
			{
				// This original implementation is to aim straight ahead and then offset
				// the angle from the resulting direction. 
				misl->Angles.Yaw += angle;
				misl->VelFromAngle(misl->VelXYToSpeed());
			}
		}
		ACTION_RETURN_OBJECT(misl);
	}
	ACTION_RETURN_OBJECT(nullptr);
}


//==========================================================================
//
// A_CustomPunch
//
// Berserk is not handled here. That can be done with A_CheckIfInventory
//
//==========================================================================

enum
{
	CPF_USEAMMO = 1,
	CPF_DAGGER = 2,
	CPF_PULLIN = 4,
	CPF_NORANDOMPUFFZ = 8,
	CPF_NOTURN = 16,
	CPF_STEALARMOR = 32,
};

DEFINE_ACTION_FUNCTION(AStateProvider, A_CustomPunch)
{
	PARAM_ACTION_PROLOGUE(AStateProvider);
	PARAM_INT		(damage);
	PARAM_BOOL_DEF	(norandom);
	PARAM_INT_DEF	(flags);
	PARAM_CLASS_DEF	(pufftype, AActor);
	PARAM_FLOAT_DEF	(range);
	PARAM_FLOAT_DEF	(lifesteal);
	PARAM_INT_DEF	(lifestealmax);
	PARAM_CLASS_DEF	(armorbonustype, AActor);
	PARAM_SOUND_DEF	(MeleeSound);
	PARAM_SOUND_DEF	(MissSound);

	if (!self->player)
		return 0;

	player_t *player = self->player;
	AWeapon *weapon = player->ReadyWeapon;


	DAngle angle;
	DAngle pitch;
	FTranslatedLineTarget t;
	int			actualdamage;

	if (!norandom)
		damage *= pr_cwpunch() % 8 + 1;

	angle = self->Angles.Yaw + pr_cwpunch.Random2() * (5.625 / 256);
	if (range == 0) range = DEFMELEERANGE;
	pitch = P_AimLineAttack (self, angle, range, &t, 0., ALF_CHECK3D);

	// only use ammo when actually hitting something!
	if ((flags & CPF_USEAMMO) && t.linetarget && weapon && ACTION_CALL_FROM_PSPRITE())
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return 0;	// out of ammo
	}

	if (pufftype == NULL)
		pufftype = PClass::FindActor(NAME_BulletPuff);
	int puffFlags = LAF_ISMELEEATTACK | ((flags & CPF_NORANDOMPUFFZ) ? LAF_NORANDOMPUFFZ : 0);

	P_LineAttack (self, angle, range, pitch, damage, NAME_Melee, pufftype, puffFlags, &t, &actualdamage);

	if (!t.linetarget)
	{
		if (MissSound) S_Sound(self, CHAN_WEAPON, MissSound, 1, ATTN_NORM);
	}
	else
	{
		if (lifesteal > 0 && !(t.linetarget->flags5 & MF5_DONTDRAIN))
		{
			if (flags & CPF_STEALARMOR)
			{
				if (armorbonustype == NULL)
				{
					armorbonustype = PClass::FindActor("ArmorBonus");
				}
				if (armorbonustype != NULL)
				{
					auto armorbonus = Spawn(armorbonustype);
					armorbonus->IntVar(NAME_SaveAmount) *= int(actualdamage * lifesteal);
					if (lifestealmax > 0) armorbonus->IntVar("MaxSaveAmount") = lifestealmax;
					armorbonus->flags |= MF_DROPPED;
					armorbonus->ClearCounters();

					if (!static_cast<AInventory*>(armorbonus)->CallTryPickup(self))
					{
						armorbonus->Destroy ();
					}
				}
			}
			else
			{
				P_GiveBody (self, int(actualdamage * lifesteal), lifestealmax);
			}
		}
		if (weapon != NULL)
		{
			if (MeleeSound) S_Sound(self, CHAN_WEAPON, MeleeSound, 1, ATTN_NORM);
			else			S_Sound (self, CHAN_WEAPON, weapon->AttackSound, 1, ATTN_NORM);
		}

		if (!(flags & CPF_NOTURN))
		{
			// turn to face target
			self->Angles.Yaw = t.angleFromSource;
		}

		if (flags & CPF_PULLIN) self->flags |= MF_JUSTATTACKED;
		if (flags & CPF_DAGGER) P_DaggerAlert (self, t.linetarget);
	}
	return 0;
}


//==========================================================================
//
// customizable railgun attack function
//
//==========================================================================
DEFINE_ACTION_FUNCTION(AStateProvider, A_RailAttack)
{
	PARAM_ACTION_PROLOGUE(AStateProvider);
	PARAM_INT		(damage);
	PARAM_INT_DEF	(spawnofs_xy);
	PARAM_BOOL_DEF	(useammo);
	PARAM_COLOR_DEF	(color1);
	PARAM_COLOR_DEF	(color2);
	PARAM_INT_DEF	(flags);
	PARAM_FLOAT_DEF	(maxdiff);
	PARAM_CLASS_DEF	(pufftype, AActor);
	PARAM_ANGLE_DEF	(spread_xy);
	PARAM_ANGLE_DEF	(spread_z);
	PARAM_FLOAT_DEF	(range)	;
	PARAM_INT_DEF	(duration);
	PARAM_FLOAT_DEF	(sparsity);
	PARAM_FLOAT_DEF	(driftspeed);
	PARAM_CLASS_DEF	(spawnclass, AActor);
	PARAM_FLOAT_DEF	(spawnofs_z);
	PARAM_INT_DEF	(SpiralOffset);
	PARAM_INT_DEF	(limit);
	
	if (range == 0) range = 8192;
	if (sparsity == 0) sparsity=1.0;

	if (self->player == NULL)
		return 0;

	AWeapon *weapon = self->player->ReadyWeapon;

	// only use ammo when actually hitting something!
	if (useammo && weapon != NULL && ACTION_CALL_FROM_PSPRITE())
	{
		if (!weapon->DepleteAmmo(weapon->bAltFire, true))
			return 0;	// out of ammo
	}

	if (!(flags & RAF_EXPLICITANGLE))
	{
		spread_xy = spread_xy * pr_crailgun.Random2() / 255;
		spread_z = spread_z * pr_crailgun.Random2() / 255;
	}

	FRailParams p;
	p.source = self;
	p.damage = damage;
	p.offset_xy = spawnofs_xy;
	p.offset_z = spawnofs_z;
	p.color1 = color1;
	p.color2 = color2;
	p.maxdiff = maxdiff;
	p.flags = flags;
	p.puff = pufftype;
	p.angleoffset = spread_xy;
	p.pitchoffset = spread_z;
	p.distance = range;
	p.duration = duration;
	p.sparsity = sparsity;
	p.drift = driftspeed;
	p.spawnclass = spawnclass;
	p.SpiralOffset = SpiralOffset;
	p.limit = limit;
	P_RailAttack(&p);
	return 0;
}

//==========================================================================
//
// also for monsters
//
//==========================================================================
enum
{
	CRF_DONTAIM = 0,
	CRF_AIMPARALLEL = 1,
	CRF_AIMDIRECT = 2,
	CRF_EXPLICITANGLE = 4,
};

DEFINE_ACTION_FUNCTION(AActor, A_CustomRailgun)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(damage);
	PARAM_INT_DEF	(spawnofs_xy)		
	PARAM_COLOR_DEF	(color1)			
	PARAM_COLOR_DEF	(color2)			
	PARAM_INT_DEF	(flags)				
	PARAM_INT_DEF	(aim)				
	PARAM_FLOAT_DEF	(maxdiff)			
	PARAM_CLASS_DEF	(pufftype, AActor)	
	PARAM_ANGLE_DEF	(spread_xy)			
	PARAM_ANGLE_DEF	(spread_z)			
	PARAM_FLOAT_DEF	(range)				
	PARAM_INT_DEF	(duration)			
	PARAM_FLOAT_DEF	(sparsity)			
	PARAM_FLOAT_DEF	(driftspeed)		
	PARAM_CLASS_DEF	(spawnclass, AActor)
	PARAM_FLOAT_DEF	(spawnofs_z)		
	PARAM_INT_DEF	(SpiralOffset)		
	PARAM_INT_DEF	(limit)				
	PARAM_FLOAT_DEF	(veleffect)

	if (range == 0) range = 8192.;
	if (sparsity == 0) sparsity = 1;

	FTranslatedLineTarget t;

	DVector3 savedpos = self->Pos();
	DAngle saved_angle = self->Angles.Yaw;
	DAngle saved_pitch = self->Angles.Pitch;

	if (aim && self->target == NULL)
	{
		return 0;
	}
	// [RH] Andy Baker's stealth monsters
	if (self->flags & MF_STEALTH)
	{
		self->visdir = 1;
	}

	self->flags &= ~MF_AMBUSH;


	if (aim)
	{
		self->Angles.Yaw = self->AngleTo(self->target);
	}
	self->Angles.Pitch = P_AimLineAttack (self, self->Angles.Yaw, MISSILERANGE, &t, 60., 0, aim ? self->target : NULL);
	if (t.linetarget == NULL && aim)
	{
		// We probably won't hit the target, but aim at it anyway so we don't look stupid.
		DVector2 xydiff = self->Vec2To(self->target);
		double zdiff = self->target->Center() - self->Center() - self->Floorclip;
		self->Angles.Pitch = -VecToAngle(xydiff.Length(), zdiff);
	}
	// Let the aim trail behind the player
	if (aim)
	{
		saved_angle = self->Angles.Yaw = self->AngleTo(self->target, -self->target->Vel.X * veleffect, -self->target->Vel.Y * veleffect);

		if (aim == CRF_AIMDIRECT)
		{
			// Tricky: We must offset to the angle of the current position
			// but then change the angle again to ensure proper aim.
			self->SetXY(self->Vec2Offset(
				spawnofs_xy * self->Angles.Yaw.Cos(),
				spawnofs_xy * self->Angles.Yaw.Sin()));
			spawnofs_xy = 0;
			self->Angles.Yaw = self->AngleTo(self->target,- self->target->Vel.X * veleffect, -self->target->Vel.Y * veleffect);
		}

		if (self->target->flags & MF_SHADOW)
		{
			DAngle rnd = pr_crailgun.Random2() * (45. / 256.);
			self->Angles.Yaw += rnd;
		}
	}

	if (!(flags & CRF_EXPLICITANGLE))
	{
		spread_xy = spread_xy * pr_crailgun.Random2() / 255;
		spread_z = spread_z * pr_crailgun.Random2() / 255;
	}

	FRailParams p;
	p.source = self;
	p.damage = damage;
	p.offset_xy = spawnofs_xy;
	p.offset_z = spawnofs_z;
	p.color1 = color1;
	p.color2 = color2;
	p.maxdiff = maxdiff;
	p.flags = flags;
	p.puff = pufftype;
	p.angleoffset = spread_xy;
	p.pitchoffset = spread_z;
	p.distance = range;
	p.duration = duration;
	p.sparsity = sparsity;
	p.drift = driftspeed;
	p.spawnclass = spawnclass;
	p.SpiralOffset = SpiralOffset;
	p.limit = 0;
	P_RailAttack(&p);

	self->SetXYZ(savedpos);
	self->Angles.Yaw = saved_angle;
	self->Angles.Pitch = saved_pitch;
	return 0;
}

//===========================================================================
//
// DoGiveInventory
//
//===========================================================================

static bool DoGiveInventory(AActor *receiver, bool orresult, VM_ARGS)
{
	int paramnum = 0;
	PARAM_CLASS		(mi, AInventory);
	PARAM_INT_DEF	(amount)

	if (!orresult)
	{
		PARAM_INT_DEF(setreceiver)
		receiver = COPY_AAPTR(receiver, setreceiver);
	}
	if (receiver == NULL)
	{ // If there's nothing to receive it, it's obviously a fail, right?
		return false;
	}
	// Owned inventory items cannot own anything because their Inventory pointer is repurposed for the owner's linked list.
	if (receiver->IsKindOf(RUNTIME_CLASS(AInventory)) && static_cast<AInventory*>(receiver)->Owner != nullptr)
	{
		return false;
	}

	if (amount <= 0)
	{
		amount = 1;
	}
	if (mi) 
	{
		AInventory *item = static_cast<AInventory *>(Spawn(mi));
		if (item == NULL)
		{
			return false;
		}
		if (item->IsKindOf(NAME_Health))
		{
			item->Amount *= amount;
		}
		else
		{
			item->Amount = amount;
		}
		item->flags |= MF_DROPPED;
		item->ClearCounters();
		if (!item->CallTryPickup(receiver))
		{
			item->Destroy();
			return false;
		}
		else
		{
			return true;
		}
	}
	return false;
}

DEFINE_ACTION_FUNCTION(AActor, A_GiveInventory)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_BOOL(DoGiveInventory(self, false, VM_ARGS_NAMES));
}	

DEFINE_ACTION_FUNCTION(AActor, A_GiveToTarget)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_BOOL(DoGiveInventory(self->target, false, VM_ARGS_NAMES));
}

DEFINE_ACTION_FUNCTION(AActor, A_GiveToChildren)
{
	PARAM_SELF_PROLOGUE(AActor);

	TThinkerIterator<AActor> it;
	AActor *mo;
	int count = 0;

	while ((mo = it.Next()))
	{
		if (mo->master == self)
		{
			count += DoGiveInventory(mo, true, VM_ARGS_NAMES);
		}
	}
	ACTION_RETURN_INT(count);
}

DEFINE_ACTION_FUNCTION(AActor, A_GiveToSiblings)
{
	PARAM_SELF_PROLOGUE(AActor);

	TThinkerIterator<AActor> it;
	AActor *mo;
	int count = 0;

	if (self->master != NULL)
	{
		while ((mo = it.Next()))
		{
			if (mo->master == self->master && mo != self)
			{
				count += DoGiveInventory(mo, true, VM_ARGS_NAMES);
			}
		}
	}
	ACTION_RETURN_INT(count);
}

//===========================================================================
//
// A_SetInventory
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SetInventory)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS(itemtype, AInventory);
	PARAM_INT(amount);
	PARAM_INT_DEF(ptr);
	PARAM_BOOL_DEF(beyondMax);

	bool res = false;

	if (itemtype == nullptr)
	{
		ACTION_RETURN_BOOL(false);
	}

	AActor *mobj = COPY_AAPTR(self, ptr);

	if (mobj == nullptr)
	{
		ACTION_RETURN_BOOL(false);
	}

	// Do not run this function on voodoo dolls because the way they transfer the inventory to the player will not work with the code below.
	if (mobj->player != nullptr)
	{
		mobj = mobj->player->mo;
	}
	ACTION_RETURN_BOOL(mobj->SetInventory(itemtype, amount, beyondMax));
}

//===========================================================================
//
// A_TakeInventory
//
//===========================================================================

enum
{
	TIF_NOTAKEINFINITE = 1,
};

bool DoTakeInventory(AActor *receiver, bool orresult, VM_ARGS)
{
	int paramnum = 0;
	PARAM_CLASS		(itemtype, AInventory);
	PARAM_INT_DEF	(amount);
	PARAM_INT_DEF	(flags);
	
	if (itemtype == NULL)
	{
		return false;
	}
	if (!orresult)
	{
		PARAM_INT_DEF(setreceiver);
		receiver = COPY_AAPTR(receiver, setreceiver);
	}
	if (receiver == NULL)
	{
		return false;
	}

	return receiver->TakeInventory(itemtype, amount, true, (flags & TIF_NOTAKEINFINITE) != 0);
}

DEFINE_ACTION_FUNCTION(AActor, A_TakeInventory)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_BOOL(DoTakeInventory(self, false, VM_ARGS_NAMES));
}	

DEFINE_ACTION_FUNCTION(AActor, A_TakeFromTarget)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_BOOL(DoTakeInventory(self->target, false, VM_ARGS_NAMES));
}	

DEFINE_ACTION_FUNCTION(AActor, A_TakeFromChildren)
{
	PARAM_SELF_PROLOGUE(AActor);
	TThinkerIterator<AActor> it;
	AActor *mo;
	int count = 0;

	while ((mo = it.Next()))
	{
		if (mo->master == self)
		{
			count += DoTakeInventory(mo, true, VM_ARGS_NAMES);
		}
	}
	ACTION_RETURN_INT(count);
}

DEFINE_ACTION_FUNCTION(AActor, A_TakeFromSiblings)
{
	PARAM_SELF_PROLOGUE(AActor);
	TThinkerIterator<AActor> it;
	AActor *mo;
	int count = 0;

	if (self->master != NULL)
	{
		while ((mo = it.Next()))
		{
			if (mo->master == self->master && mo != self)
			{
				count += DoTakeInventory(mo, true, VM_ARGS_NAMES);
			}
		}
	}
	ACTION_RETURN_INT(count);
}

//===========================================================================
//
// Common code for A_SpawnItem and A_SpawnItemEx
//
//===========================================================================

enum SIX_Flags
{
	SIXF_TRANSFERTRANSLATION	= 0x00000001,
	SIXF_ABSOLUTEPOSITION		= 0x00000002,
	SIXF_ABSOLUTEANGLE			= 0x00000004,
	SIXF_ABSOLUTEVELOCITY		= 0x00000008,
	SIXF_SETMASTER				= 0x00000010,
	SIXF_NOCHECKPOSITION		= 0x00000020,
	SIXF_TELEFRAG				= 0x00000040,
	SIXF_CLIENTSIDE				= 0x00000080,	// only used by Skulldronum
	SIXF_TRANSFERAMBUSHFLAG		= 0x00000100,
	SIXF_TRANSFERPITCH			= 0x00000200,
	SIXF_TRANSFERPOINTERS		= 0x00000400,
	SIXF_USEBLOODCOLOR			= 0x00000800,
	SIXF_CLEARCALLERTID			= 0x00001000,
	SIXF_MULTIPLYSPEED			= 0x00002000,
	SIXF_TRANSFERSCALE			= 0x00004000,
	SIXF_TRANSFERSPECIAL		= 0x00008000,
	SIXF_CLEARCALLERSPECIAL		= 0x00010000,
	SIXF_TRANSFERSTENCILCOL		= 0x00020000,
	SIXF_TRANSFERALPHA			= 0x00040000,
	SIXF_TRANSFERRENDERSTYLE	= 0x00080000,
	SIXF_SETTARGET				= 0x00100000,
	SIXF_SETTRACER				= 0x00200000,
	SIXF_NOPOINTERS				= 0x00400000,
	SIXF_ORIGINATOR				= 0x00800000,
	SIXF_TRANSFERSPRITEFRAME	= 0x01000000,
	SIXF_TRANSFERROLL			= 0x02000000,
	SIXF_ISTARGET				= 0x04000000,
	SIXF_ISMASTER				= 0x08000000,
	SIXF_ISTRACER				= 0x10000000,
};

static bool InitSpawnedItem(AActor *self, AActor *mo, int flags)
{
	if (mo == NULL)
	{
		return false;
	}
	AActor *originator = self;

	if (!(mo->flags2 & MF2_DONTTRANSLATE))
	{
		if (flags & SIXF_TRANSFERTRANSLATION)
		{
			mo->Translation = self->Translation;
		}
		else if (flags & SIXF_USEBLOODCOLOR)
		{
			// [XA] Use the spawning actor's BloodColor to translate the newly-spawned object.
			mo->Translation = self->BloodTranslation;
		}
	}
	if (flags & SIXF_TRANSFERPOINTERS)
	{
		mo->target = self->target;
		mo->master = self->master; // This will be overridden later if SIXF_SETMASTER is set
		mo->tracer = self->tracer;
	}

	mo->Angles.Yaw = self->Angles.Yaw;
	if (flags & SIXF_TRANSFERPITCH)
	{
		mo->Angles.Pitch = self->Angles.Pitch;
	}
	if (!(flags & SIXF_ORIGINATOR))
	{
		while (originator && originator->isMissile())
		{
			originator = originator->target;
		}
	}
	if (flags & SIXF_TELEFRAG) 
	{
		P_TeleportMove(mo, mo->Pos(), true);
		// This is needed to ensure consistent behavior.
		// Otherwise it will only spawn if nothing gets telefragged
		flags |= SIXF_NOCHECKPOSITION;	
	}
	if (mo->flags3 & MF3_ISMONSTER)
	{
		if (!(flags & SIXF_NOCHECKPOSITION) && !P_TestMobjLocation(mo))
		{
			// The monster is blocked so don't spawn it at all!
			mo->ClearCounters();
			mo->Destroy();
			return false;
		}
		else if (originator && !(flags & SIXF_NOPOINTERS))
		{
			if (originator->flags3 & MF3_ISMONSTER)
			{
				// If this is a monster transfer all friendliness information
				mo->CopyFriendliness(originator, true);
			}
			else if (originator->player)
			{
				// A player always spawns a monster friendly to him
				mo->flags |= MF_FRIENDLY;
				mo->SetFriendPlayer(originator->player);

				AActor * attacker=originator->player->attacker;
				if (attacker)
				{
					if (!(attacker->flags&MF_FRIENDLY) || 
						(deathmatch && attacker->FriendPlayer!=0 && attacker->FriendPlayer!=mo->FriendPlayer))
					{
						// Target the monster which last attacked the player
						mo->LastHeard = mo->target = attacker;
					}
				}
			}
		}
	}
	else if (!(flags & SIXF_TRANSFERPOINTERS))
	{
		// If this is a missile or something else set the target to the originator
		mo->target = originator ? originator : self;
	}
	if (flags & SIXF_NOPOINTERS)
	{
		//[MC]Intentionally eliminate pointers. Overrides TRANSFERPOINTERS, but is overridden by SETMASTER/TARGET/TRACER.
		mo->LastHeard = NULL; //Sanity check.
		mo->target = NULL;
		mo->master = NULL;
		mo->tracer = NULL;
	}
	if (flags & SIXF_SETMASTER)
	{ // don't let it attack you (optional)!
		mo->master = originator;
	}
	if (flags & SIXF_SETTARGET)
	{
		mo->target = originator;
	}
	if (flags & SIXF_SETTRACER)
	{
		mo->tracer = originator;
	}
	if (flags & SIXF_TRANSFERSCALE)
	{
		mo->Scale = self->Scale;
	}
	if (flags & SIXF_TRANSFERAMBUSHFLAG)
	{
		mo->flags = (mo->flags & ~MF_AMBUSH) | (self->flags & MF_AMBUSH);
	}
	if (flags & SIXF_CLEARCALLERTID)
	{
		self->RemoveFromHash();
		self->tid = 0;
	}
	if (flags & SIXF_TRANSFERSPECIAL)
	{
		mo->special = self->special;
		memcpy(mo->args, self->args, sizeof(self->args));
	}
	if (flags & SIXF_CLEARCALLERSPECIAL)
	{
		self->special = 0;
		memset(self->args, 0, sizeof(self->args));
	}
	if (flags & SIXF_TRANSFERSTENCILCOL)
	{
		mo->fillcolor = self->fillcolor;
	}
	if (flags & SIXF_TRANSFERALPHA)
	{
		mo->Alpha = self->Alpha;
	}
	if (flags & SIXF_TRANSFERRENDERSTYLE)
	{
		mo->RenderStyle = self->RenderStyle;
	}
	
	if (flags & SIXF_TRANSFERSPRITEFRAME)
	{
		mo->sprite = self->sprite;
		mo->frame = self->frame;
	}

	if (flags & SIXF_TRANSFERROLL)
	{
		mo->Angles.Roll = self->Angles.Roll;
	}

	if (flags & SIXF_ISTARGET)
	{
		self->target = mo;
	}
	if (flags & SIXF_ISMASTER)
	{
		self->master = mo;
	}
	if (flags & SIXF_ISTRACER)
	{
		self->tracer = mo;
	}
	return true;
}

//===========================================================================
//
// A_SpawnItem
//
// Spawns an item in front of the caller like Heretic's time bomb
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SpawnItem)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_CLASS_DEF	(missile, AActor)		
	PARAM_FLOAT_DEF	(distance)				
	PARAM_FLOAT_DEF	(zheight)				
	PARAM_BOOL_DEF	(useammo)				
	PARAM_BOOL_DEF	(transfer_translation);
		
	if (numret > 1) ret[1].SetObject(nullptr);

	if (missile == NULL)
	{
		if (numret > 0) ret[0].SetInt(false);
		return MIN(numret, 2);
	}

	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && (GetDefaultByType(missile)->flags3 & MF3_ISMONSTER))
	{
		if (numret > 0) ret[0].SetInt(true);
		return MIN(numret, 2);
	}

	if (ACTION_CALL_FROM_PSPRITE())
	{
		// Used from a weapon, so use some ammo
		AWeapon *weapon = self->player->ReadyWeapon;

		if (weapon == NULL)
		{
			if (numret > 0) ret[0].SetInt(true);
			return MIN(numret, 2);
		}
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire))
		{
			if (numret > 0) ret[0].SetInt(true);
			return MIN(numret, 2);
		}
	}

	AActor *mo = Spawn( missile, self->Vec3Angle(distance, self->Angles.Yaw, -self->Floorclip + self->GetBobOffset() + zheight), ALLOW_REPLACE);

	int flags = (transfer_translation ? SIXF_TRANSFERTRANSLATION : 0) + (useammo ? SIXF_SETMASTER : 0);
	bool res = InitSpawnedItem(self, mo, flags);	// for an inventory item's use state
	if (numret > 0) ret[0].SetInt(res);
	if (numret > 1) ret[1].SetObject(mo);
	return MIN(numret, 2);

}

//===========================================================================
//
// A_SpawnItemEx
//
// Enhanced spawning function
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_SpawnItemEx)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS		(missile, AActor);
	PARAM_FLOAT_DEF	(xofs)		
	PARAM_FLOAT_DEF	(yofs)		
	PARAM_FLOAT_DEF	(zofs)		
	PARAM_FLOAT_DEF	(xvel)		
	PARAM_FLOAT_DEF	(yvel)		
	PARAM_FLOAT_DEF	(zvel)		
	PARAM_ANGLE_DEF	(angle)		
	PARAM_INT_DEF	(flags)		
	PARAM_INT_DEF	(chance)	
	PARAM_INT_DEF	(tid)		

	if (numret > 1) ret[1].SetObject(nullptr);

	if (missile == NULL) 
	{
		if (numret > 0) ret[0].SetInt(false);
		return MIN(numret, 2);
	}
	if (chance > 0 && pr_spawnitemex() < chance)
	{
		if (numret > 0) ret[0].SetInt(true);
		return MIN(numret, 2);
	}
	// Don't spawn monsters if this actor has been massacred
	if (self->DamageType == NAME_Massacre && (GetDefaultByType(missile)->flags3 & MF3_ISMONSTER))
	{
		if (numret > 0) ret[0].SetInt(true);
		return MIN(numret, 2);
	}

	DVector2 pos;

	if (!(flags & SIXF_ABSOLUTEANGLE))
	{
		angle += self->Angles.Yaw;
	}
	double s = angle.Sin();
	double c = angle.Cos();

	if (flags & SIXF_ABSOLUTEPOSITION)
	{
		pos = self->Vec2Offset(xofs, yofs);
	}
	else
	{
		// in relative mode negative y values mean 'left' and positive ones mean 'right'
		// This is the inverse orientation of the absolute mode!
		pos = self->Vec2Offset(xofs * c + yofs * s, xofs * s - yofs*c);
	}

	if (!(flags & SIXF_ABSOLUTEVELOCITY))
	{
		// Same orientation issue here!
		double newxvel = xvel * c + yvel * s;
		yvel = xvel * s - yvel * c;
		xvel = newxvel;
	}

	AActor *mo = Spawn(missile, DVector3(pos, self->Z() - self->Floorclip + self->GetBobOffset() + zofs), ALLOW_REPLACE);
	bool res = InitSpawnedItem(self, mo, flags);
	if (res)
	{
		if (tid != 0)
		{
			assert(mo->tid == 0);
			mo->tid = tid;
			mo->AddToHash();
		}
		mo->Vel = {xvel, yvel, zvel};
		if (flags & SIXF_MULTIPLYSPEED)
		{
			mo->Vel *= mo->Speed;
		}
		mo->Angles.Yaw = angle;
	}
	if (numret > 0) ret[0].SetInt(res);
	if (numret > 1) ret[1].SetObject(mo);
	return MIN(numret, 2);
}

//===========================================================================
//
// A_ThrowGrenade
//
// Throws a grenade (like Hexen's fighter flechette)
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_ThrowGrenade)
{
		PARAM_ACTION_PROLOGUE(AActor);
	PARAM_CLASS		(missile, AActor);
		PARAM_FLOAT_DEF	(zheight)	
		PARAM_FLOAT_DEF	(xyvel)		
		PARAM_FLOAT_DEF	(zvel)		
		PARAM_BOOL_DEF	(useammo)	

	if (numret > 1) ret[1].SetObject(nullptr);

	if (missile == NULL)
	{
		if (numret > 0) ret[0].SetInt(false);
		return MIN(numret, 2);
	}
	if (ACTION_CALL_FROM_PSPRITE())
	{
		// Used from a weapon, so use some ammo
		AWeapon *weapon = self->player->ReadyWeapon;

		if (weapon == NULL)
		{
			if (numret > 0) ret[0].SetInt(true);
			return MIN(numret, 2);
		}
		if (useammo && !weapon->DepleteAmmo(weapon->bAltFire))
		{
			if (numret > 0) ret[0].SetInt(true);
			return MIN(numret, 2);
		}
	}

	AActor *bo;

	bo = Spawn(missile, 
			self->PosPlusZ(-self->Floorclip + self->GetBobOffset() + zheight + 35 + (self->player? self->player->crouchoffset : 0.)),
			ALLOW_REPLACE);
	if (bo)
	{
		P_PlaySpawnSound(bo, self);
		if (xyvel != 0)
			bo->Speed = xyvel;
		bo->Angles.Yaw = self->Angles.Yaw + (((pr_grenade()&7) - 4) * (360./256.));

		DAngle pitch = -self->Angles.Pitch;
		DAngle angle = bo->Angles.Yaw;

		// There are two vectors we are concerned about here: xy and z. We rotate
		// them separately according to the shooter's pitch and then sum them to
		// get the final velocity vector to shoot with.

		double xy_xyscale = bo->Speed * pitch.Cos();
		double xy_velz = bo->Speed * pitch.Sin();
		double xy_velx = xy_xyscale * angle.Cos();
		double xy_vely = xy_xyscale * angle.Sin();

		pitch = self->Angles.Pitch;
		double z_xyscale = zvel * pitch.Sin();
		double z_velz = zvel * pitch.Cos();
		double z_velx = z_xyscale * angle.Cos();
		double z_vely = z_xyscale * angle.Sin();

		bo->Vel.X = xy_velx + z_velx + self->Vel.X / 2;
		bo->Vel.Y = xy_vely + z_vely + self->Vel.Y / 2;
		bo->Vel.Z = xy_velz + z_velz;

		bo->target = self;
		if (!P_CheckMissileSpawn(bo, self->radius)) bo = nullptr;

		if (numret > 0) ret[0].SetInt(true);
		if (numret > 1) ret[1].SetObject(bo);
		return MIN(numret, 2);
	} 
	else
	{
		if (numret > 0) ret[0].SetInt(false);
		return MIN(numret, 2);
	}
}


//===========================================================================
//
// A_Recoil
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_Recoil)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(xyvel);

	self->Thrust(self->Angles.Yaw + 180., xyvel);
	return 0;
}


//===========================================================================
//
// A_SelectWeapon
//
//===========================================================================
enum SW_Flags
{
	SWF_SELECTPRIORITY = 1,
};
DEFINE_ACTION_FUNCTION(AActor, A_SelectWeapon)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS(cls, AWeapon);
	PARAM_INT_DEF(flags);

	bool selectPriority = !!(flags & SWF_SELECTPRIORITY);

	if ((!selectPriority && cls == NULL) || self->player == NULL)
	{
		ACTION_RETURN_BOOL(false);
	}

	AWeapon *weaponitem = static_cast<AWeapon*>(self->FindInventory(cls));

	if (weaponitem != NULL && weaponitem->IsKindOf(NAME_Weapon))
	{
		if (self->player->ReadyWeapon != weaponitem)
		{
			self->player->PendingWeapon = weaponitem;
		}
		ACTION_RETURN_BOOL(true);
	}
	else if (selectPriority)
	{
		// [XA] if the named weapon cannot be found (or is a dummy like 'None'),
		//      select the next highest priority weapon. This is basically
		//      the same as A_CheckReload minus the ammo check. Handy.
		self->player->mo->PickNewWeapon(NULL);
		ACTION_RETURN_BOOL(true);
	}
	else
	{
		ACTION_RETURN_BOOL(false);
	}
}


//===========================================================================
//
// A_Print
//
//===========================================================================
EXTERN_CVAR(Float, con_midtime)

DEFINE_ACTION_FUNCTION(AActor, A_Print)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_STRING	(text);
	PARAM_FLOAT_DEF	(time);
	PARAM_NAME_DEF	(fontname);

	if (text[0] == '$') text = GStrings(&text[1]);
	if (self->CheckLocalView (consoleplayer) ||
		(self->target != NULL && self->target->CheckLocalView (consoleplayer)))
	{
		float saved = con_midtime;
		FFont *font = NULL;
		
		if (fontname != NAME_None)
		{
			font = V_GetFont(fontname);
		}
		if (time > 0)
		{
			con_midtime = float(time);
		}
		FString formatted = strbin1(text);
		C_MidPrint(font != NULL ? font : SmallFont, formatted.GetChars());
		con_midtime = saved;
	}
	return 0;
}

//===========================================================================
//
// A_PrintBold
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_PrintBold)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_STRING	(text);
	PARAM_FLOAT_DEF	(time);
	PARAM_NAME_DEF	(fontname);

	float saved = con_midtime;
	FFont *font = NULL;
	
	if (text[0] == '$') text = GStrings(&text[1]);
	if (fontname != NAME_None)
	{
		font = V_GetFont(fontname);
	}
	if (time > 0)
	{
		con_midtime = float(time);
	}
	FString formatted = strbin1(text);
	C_MidPrintBold(font != NULL ? font : SmallFont, formatted.GetChars());
	con_midtime = saved;
	return 0;
}

//===========================================================================
//
// A_Log
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_Log)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_STRING(text);
	PARAM_BOOL_DEF(local);

	if (local && !self->CheckLocalView(consoleplayer)) return 0;

	if (text[0] == '$') text = GStrings(&text[1]);
	FString formatted = strbin1(text);
	Printf("%s\n", formatted.GetChars());
	return 0;
}

//=========================================================================
//
// A_LogInt
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LogInt)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(num);
	PARAM_BOOL_DEF(local);

	if (local && !self->CheckLocalView(consoleplayer)) return 0;
	Printf("%d\n", num);
	return 0;
}

//=========================================================================
//
// A_LogFloat
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LogFloat)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(num);
	PARAM_BOOL_DEF(local);

	if (local && !self->CheckLocalView(consoleplayer)) return 0;
	IGNORE_FORMAT_PRE
	Printf("%H\n", num);
	IGNORE_FORMAT_POST
	return 0;
}

//===========================================================================
//
// A_SetTranslucent
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_SetTranslucent)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT		(alpha);
	PARAM_INT_DEF	(mode);

	mode = mode == 0 ? STYLE_Translucent : mode == 2 ? STYLE_Fuzzy : STYLE_Add;

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->Alpha = clamp(alpha, 0., 1.);
	self->RenderStyle = ERenderStyle(mode);
	return 0;
}

//===========================================================================
//
// A_SetRenderStyle
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_SetRenderStyle)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(alpha);
	PARAM_INT_DEF(mode);

	self->Alpha = clamp(alpha, 0., 1.);
	self->RenderStyle = ERenderStyle(mode);
	return 0;
}

//===========================================================================
//
// A_FadeIn
//
// Fades the actor in
//
//===========================================================================

enum FadeFlags
{
	FTF_REMOVE =	1 << 0,
	FTF_CLAMP =		1 << 1,
};

DEFINE_ACTION_FUNCTION(AActor, A_FadeIn)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT_DEF(reduce);
	PARAM_INT_DEF(flags);


	if (reduce == 0)
	{
		reduce = 0.1;
	}
	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->Alpha += reduce;

	if (self->Alpha >= 1.)
	{
		if (flags & FTF_CLAMP)
		{
			self->Alpha = 1.;
		}
		if (flags & FTF_REMOVE)
		{
			P_RemoveThing(self);
		}
	}
	return 0;
}

//===========================================================================
//
// A_FadeOut
//
// fades the actor out and destroys it when done
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_FadeOut)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT_DEF(reduce);
	PARAM_INT_DEF(flags);

	if (reduce == 0)
	{
		reduce = 0.1;
	}
	self->RenderStyle.Flags &= ~STYLEF_Alpha1;
	self->Alpha -= reduce;
	if (self->Alpha <= 0)
	{
		if (flags & FTF_CLAMP)
		{
			self->Alpha = 0;
		}
		if (flags & FTF_REMOVE)
		{
			P_RemoveThing(self);
		}
	}
	return 0;
}

//===========================================================================
//
// A_FadeTo
//
// fades the actor to a specified transparency by a specified amount and
// destroys it if so desired
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_FadeTo)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT		(target);
	PARAM_FLOAT_DEF	(amount);
	PARAM_INT_DEF	(flags);

	self->RenderStyle.Flags &= ~STYLEF_Alpha1;

	if (self->Alpha > target)
	{
		self->Alpha -= amount;

		if (self->Alpha < target)
		{
			self->Alpha = target;
		}
	}
	else if (self->Alpha < target)
	{
		self->Alpha += amount;

		if (self->Alpha > target)
		{
			self->Alpha = target;
		}
	}
	if (flags & FTF_CLAMP)
	{
		self->Alpha = clamp(self->Alpha, 0., 1.);
	}
	if (self->Alpha == target && (flags & FTF_REMOVE))
	{
		P_RemoveThing(self);
	}
	return 0;
}

//===========================================================================
//
// A_SpawnDebris
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_SpawnDebris)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS		(debris, AActor);
	PARAM_BOOL_DEF	(transfer_translation)
	PARAM_FLOAT_DEF	(mult_h)				
	PARAM_FLOAT_DEF	(mult_v)				
	int i;
	AActor *mo;

	if (debris == NULL)
		return 0;

	// only positive values make sense here
	if (mult_v <= 0) mult_v = 1;
	if (mult_h <= 0) mult_h = 1;
	
	for (i = 0; i < GetDefaultByType(debris)->health; i++)
	{
		double xo = (pr_spawndebris() - 128) / 16.;
		double yo = (pr_spawndebris() - 128) / 16.;
		double zo = pr_spawndebris()*self->Height / 256 + self->GetBobOffset();
		mo = Spawn(debris, self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);
		if (mo)
		{
			if (transfer_translation)
			{
				mo->Translation = self->Translation;
			}
			if (i < mo->GetInfo()->NumOwnedStates)
			{
				mo->SetState (mo->GetInfo()->OwnedStates + i);
			}
			mo->Vel.X = mult_h * pr_spawndebris.Random2() / 64.;
			mo->Vel.Y = mult_h * pr_spawndebris.Random2() / 64.;
			mo->Vel.Z = mult_v * ((pr_spawndebris() & 7) + 5);
		}
	}
	return 0;
}

//===========================================================================
//
// A_SpawnParticle
//
//===========================================================================
enum SPFflag
{
	SPF_FULLBRIGHT =		1,
	SPF_RELPOS =			1 << 1,
	SPF_RELVEL =			1 << 2,
	SPF_RELACCEL =			1 << 3,
	SPF_RELANG =			1 << 4,
	SPF_NOTIMEFREEZE =		1 << 5,
};

DEFINE_ACTION_FUNCTION(AActor, A_SpawnParticle)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_COLOR		(color);
	PARAM_INT_DEF	(flags)		
	PARAM_INT_DEF	(lifetime)	
	PARAM_FLOAT_DEF	(size)		
	PARAM_ANGLE_DEF	(angle)		
	PARAM_FLOAT_DEF	(xoff)		
	PARAM_FLOAT_DEF	(yoff)		
	PARAM_FLOAT_DEF	(zoff)		
	PARAM_FLOAT_DEF	(xvel)		
	PARAM_FLOAT_DEF	(yvel)		
	PARAM_FLOAT_DEF	(zvel)		
	PARAM_FLOAT_DEF	(accelx)	
	PARAM_FLOAT_DEF	(accely)	
	PARAM_FLOAT_DEF	(accelz)	
	PARAM_FLOAT_DEF	(startalpha)
	PARAM_FLOAT_DEF	(fadestep)	
	PARAM_FLOAT_DEF (sizestep)	

	startalpha = clamp(startalpha, 0., 1.);
	if (fadestep > 0) fadestep = clamp(fadestep, 0., 1.);
	size = fabs(size);
	if (lifetime != 0)
	{
		if (flags & SPF_RELANG) angle += self->Angles.Yaw;
		double s = angle.Sin();
		double c = angle.Cos();
		DVector3 pos(xoff, yoff, zoff);
		DVector3 vel(xvel, yvel, zvel);
		DVector3 acc(accelx, accely, accelz);
		//[MC] Code ripped right out of A_SpawnItemEx.
		if (flags & SPF_RELPOS)
		{
			// in relative mode negative y values mean 'left' and positive ones mean 'right'
			// This is the inverse orientation of the absolute mode!
			pos.X = xoff * c + yoff * s;
			pos.Y = xoff * s - yoff * c;
		}
		if (flags & SPF_RELVEL)
		{
			vel.X = xvel * c + yvel * s;
			vel.Y = xvel * s - yvel * c;
		}
		if (flags & SPF_RELACCEL)
		{
			acc.X = accelx * c + accely * s;
			acc.Y = accelx * s - accely * c;
		}
		P_SpawnParticle(self->Vec3Offset(pos), vel, acc, color, startalpha, lifetime, size, fadestep, sizestep, flags);
	}
	return 0;
}

//===========================================================================
//
// A_CheckSight
// jumps if no player can see this actor
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, CheckIfSeen)
{
	PARAM_SELF_PROLOGUE(AActor);

	for (int i = 0; i < MAXPLAYERS; i++) 
	{
		if (playeringame[i])
		{
			// Always check sight from each player.
			if (P_CheckSight(players[i].mo, self, SF_IGNOREVISIBILITY))
			{
				ACTION_RETURN_BOOL(false);
			}
			// If a player is viewing from a non-player, then check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				P_CheckSight(players[i].camera, self, SF_IGNOREVISIBILITY))
			{
				ACTION_RETURN_BOOL(false);
			}
		}
	}
	ACTION_RETURN_BOOL(true);
}

//===========================================================================
//
// A_CheckSightOrRange
// Jumps if this actor is out of range of all players *and* out of sight.
// Useful for maps with many multi-actor special effects.
//
//===========================================================================
static bool DoCheckSightOrRange(AActor *self, AActor *camera, double range, bool twodi, bool checksight)
{
	if (camera == NULL)
	{
		return false;
	}
	// Check distance first, since it's cheaper than checking sight.
	DVector2 pos = camera->Vec2To(self);
	double dz;
	double eyez = camera->Center();
	if (eyez > self->Top())
	{
		dz = self->Top() - eyez;
	}
	else if (eyez < self->Z())
	{
		dz = self->Z() - eyez;
	}
	else
	{
		dz = 0;
	}
	double distance = DVector3(pos, twodi? 0. : dz).LengthSquared();
	if (distance <= range)
	{
		// Within range
		return true;
	}

	// Now check LOS.
	if (checksight && P_CheckSight(camera, self, SF_IGNOREVISIBILITY))
	{ // Visible
		return true;
	}
	return false;
}

DEFINE_ACTION_FUNCTION(AActor, CheckSightOrRange)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(range);
	PARAM_BOOL_DEF(twodi);

	range *= range;
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			// Always check from each player.
			if (DoCheckSightOrRange(self, players[i].mo, range, twodi, true))
			{
				ACTION_RETURN_BOOL(false);
			}
			// If a player is viewing from a non-player, check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				DoCheckSightOrRange(self, players[i].camera, range, twodi, true))
			{
				ACTION_RETURN_BOOL(false);
			}
		}
	}
	ACTION_RETURN_BOOL(true);
}


DEFINE_ACTION_FUNCTION(AActor, CheckRange)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(range);
	PARAM_BOOL_DEF(twodi);

	range *= range;
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			// Always check from each player.
			if (DoCheckSightOrRange(self, players[i].mo, range, twodi, false))
			{
				ACTION_RETURN_BOOL(false);
			}
			// If a player is viewing from a non-player, check that too.
			if (players[i].camera != NULL && players[i].camera->player == NULL &&
				DoCheckSightOrRange(self, players[i].camera, range, twodi, false))
			{
				ACTION_RETURN_BOOL(false);
			}
		}
	}
	ACTION_RETURN_BOOL(true);
}


//===========================================================================
//
// Inventory drop
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_DropInventory)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS(drop, AInventory);
	PARAM_INT_DEF(amount);

	if (drop)
	{
		AInventory *inv = self->FindInventory(drop);
		if (inv)
		{
			self->DropInventory(inv, amount);
		}
	}
	return 0;
}


//===========================================================================
//
// A_SetBlend
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_SetBlend)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_COLOR		(color);
	PARAM_FLOAT		(alpha);
	PARAM_INT		(tics);
	PARAM_COLOR_DEF	(color2);
	PARAM_FLOAT_DEF	(alpha2);

	if (color == MAKEARGB(255,255,255,255))
		color = 0;
	if (color2 == MAKEARGB(255,255,255,255))
		color2 = 0;
//	if (color2.a == 0)
//		color2 = color;

	Create<DFlashFader>(color.r/255.f, color.g/255.f, color.b/255.f, float(alpha),
					color2.r/255.f, color2.g/255.f, color2.b/255.f, float(alpha2),
					float(tics)/TICRATE, self, true);
	return 0;
}


//===========================================================================
//
// A_CountdownArg
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CountdownArg)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(cnt);
	PARAM_STATE_DEF(state)

	if (cnt<0 || cnt >= 5) return 0;
	if (!self->args[cnt]--)
	{
		if (self->flags&MF_MISSILE)
		{
			P_ExplodeMissile(self, NULL, NULL);
		}
		else if (self->flags&MF_SHOOTABLE)
		{
			P_DamageMobj(self, NULL, NULL, self->health, NAME_None, DMG_FORCED);
		}
		else
		{
			if (state == nullptr) state = self->FindState(NAME_Death);
			self->SetState(state);
		}
	}
	return 0;
}

//============================================================================
//
// A_Burst
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_Burst)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS(chunk, AActor);

	int i, numChunks;
	AActor * mo;

	if (chunk == NULL)
	{
		return 0;
	}

	self->Vel.Zero();
	self->Height = self->GetDefault()->Height;

	// [RH] In Hexen, this creates a random number of shards (range [24,56])
	// with no relation to the size of the self shattering. I think it should
	// base the number of shards on the size of the dead thing, so bigger
	// things break up into more shards than smaller things.
	// An self with radius 20 and height 64 creates ~40 chunks.
	numChunks = MAX<int> (4, int(self->radius * self->Height)/32);
	i = (pr_burst.Random2()) % (numChunks/4);
	for (i = MAX (24, numChunks + i); i >= 0; i--)
	{
		double xo = (pr_burst() - 128) * self->radius / 128;
		double yo = (pr_burst() - 128) * self->radius / 128;
		double zo = (pr_burst() * self->Height / 255);
		mo = Spawn(chunk, self->Vec3Offset(xo, yo, zo), ALLOW_REPLACE);

		if (mo)
		{
			mo->Vel.Z = 4 * (mo->Z() - self->Z()) / self->Height;
			mo->Vel.X = pr_burst.Random2() / 128.;
			mo->Vel.Y = pr_burst.Random2() / 128.;
			mo->RenderStyle = self->RenderStyle;
			mo->Alpha = self->Alpha;
			mo->CopyFriendliness(self, true);
		}
	}

	// [RH] Do some stuff to make this more useful outside Hexen
	if (self->flags4 & MF4_BOSSDEATH)
	{
		A_BossDeath(self);
	}
	A_Unblock(self, true);

	self->Destroy ();
	return 0;
}

//===========================================================================
//
// A_Stop
// resets all velocity of the actor to 0
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_Stop)
{
	PARAM_SELF_PROLOGUE(AActor);
	self->Vel.Zero();
	if (self->player && self->player->mo == self && !(self->player->cheats & CF_PREDICTING))
	{
		self->player->mo->PlayIdle();
		self->player->Vel.Zero();
	}
	return 0;
}

static void CheckStopped(AActor *self)
{
	if (self->player != NULL &&
		self->player->mo == self &&
		!(self->player->cheats & CF_PREDICTING) && !self->Vel.isZero())
	{
		self->player->mo->PlayIdle();
		self->player->Vel.Zero();
	}
}

//===========================================================================
//
// A_Respawn
//
//===========================================================================

enum RS_Flags
{
	RSF_FOG=1,
	RSF_KEEPTARGET=2,
	RSF_TELEFRAG=4,
};

DEFINE_ACTION_FUNCTION(AActor, A_Respawn)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(flags);

	bool oktorespawn = false;
	DVector3 pos = self->Pos();

	self->flags |= MF_SOLID;
	self->Height = self->GetDefault()->Height;
	self->radius = self->GetDefault()->radius;
	self->RestoreSpecialPosition();

	if (flags & RSF_TELEFRAG)
	{
		// [KS] DIE DIE DIE DIE erm *ahem* =)
		oktorespawn = P_TeleportMove(self, self->Pos(), true, false);
	}
	else
	{
		oktorespawn = P_CheckPosition(self, self->Pos(), true);
	}

	if (oktorespawn)
	{
		AActor *defs = self->GetDefault();
		self->health = defs->health;

		// [KS] Don't keep target, because it could be self if the monster committed suicide
		//      ...Actually it's better off an option, so you have better control over monster behavior.
		if (!(flags & RSF_KEEPTARGET))
		{
			self->target = NULL;
			self->LastHeard = NULL;
			self->lastenemy = NULL;
		}
		else
		{
			// Don't attack yourself (Re: "Marine targets itself after suicide")
			if (self->target == self)
				self->target = NULL;
			if (self->lastenemy == self)
				self->lastenemy = NULL;
		}

		self->flags  = (defs->flags & ~MF_FRIENDLY) | (self->flags & MF_FRIENDLY);
		self->flags2 = defs->flags2;
		self->flags3 = (defs->flags3 & ~(MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS)) | (self->flags3 & (MF3_NOSIGHTCHECK | MF3_HUNTPLAYERS));
		self->flags4 = (defs->flags4 & ~MF4_NOHATEPLAYERS) | (self->flags4 & MF4_NOHATEPLAYERS);
		self->flags5 = defs->flags5;
		self->flags6 = defs->flags6;
		self->flags7 = defs->flags7;
		self->flags8 = defs->flags8;
		self->SetState (self->SpawnState);
		self->renderflags &= ~RF_INVISIBLE;

		if (flags & RSF_FOG)
		{
			P_SpawnTeleportFog(self, pos, true, true);
			P_SpawnTeleportFog(self, self->Pos(), false, true);
		}
		if (self->CountsAsKill())
		{
			level.total_monsters++;
		}
	}
	else
	{
		self->flags &= ~MF_SOLID;
	}
	return 0;
}


//==========================================================================
//
// A_PlayerSkinCheck
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, PlayerSkinCheck)
{
	PARAM_SELF_PROLOGUE(AActor);

	ACTION_RETURN_BOOL(self->player != NULL &&
		Skins[self->player->userinfo.GetSkin()].othergame);
}

// [KS] *** Start of my modifications ***

//==========================================================================
//
// A_CheckLOF (state jump, int flags = CRF_AIM_VERT|CRF_AIM_HOR,
//    fixed range = 0, angle angle = 0, angle pitch = 0, 
//    fixed offsetheight = 32, fixed offsetwidth = 0,
//	  int ptr_target = AAPTR_DEFAULT (target) )
//
//==========================================================================

enum CLOF_flags
{
	CLOFF_NOAIM_VERT =			0x00000001,
	CLOFF_NOAIM_HORZ =			0x00000002,

	CLOFF_JUMPENEMY =			0x00000004,
	CLOFF_JUMPFRIEND =			0x00000008,
	CLOFF_JUMPOBJECT =			0x00000010,
	CLOFF_JUMPNONHOSTILE =		0x00000020,

	CLOFF_SKIPENEMY =			0x00000040,
	CLOFF_SKIPFRIEND =			0x00000080,
	CLOFF_SKIPOBJECT =			0x00000100,
	CLOFF_SKIPNONHOSTILE =		0x00000200,

	CLOFF_MUSTBESHOOTABLE =		0x00000400,

	CLOFF_SKIPTARGET =			0x00000800,
	CLOFF_ALLOWNULL =			0x00001000,
	CLOFF_CHECKPARTIAL =		0x00002000,

	CLOFF_MUSTBEGHOST =			0x00004000,
	CLOFF_IGNOREGHOST =			0x00008000,
	
	CLOFF_MUSTBESOLID =			0x00010000,
	CLOFF_BEYONDTARGET =		0x00020000,

	CLOFF_FROMBASE =			0x00040000,
	CLOFF_MUL_HEIGHT =			0x00080000,
	CLOFF_MUL_WIDTH =			0x00100000,

	CLOFF_JUMP_ON_MISS =		0x00200000,
	CLOFF_AIM_VERT_NOOFFSET =	0x00400000,

	CLOFF_SETTARGET =			0x00800000,
	CLOFF_SETMASTER =			0x01000000,
	CLOFF_SETTRACER =			0x02000000,
};

struct LOFData
{
	AActor *Self;
	AActor *Target;
	int Flags;
	bool BadActor;
};

ETraceStatus CheckLOFTraceFunc(FTraceResults &trace, void *userdata)
{
	LOFData *data = (LOFData *)userdata;
	int flags = data->Flags;

	if (trace.HitType != TRACE_HitActor)
	{
		return TRACE_Stop;
	}
	if (trace.Actor == data->Target)
	{
		if (flags & CLOFF_SKIPTARGET)
		{
			if (flags & CLOFF_BEYONDTARGET)
			{
				return TRACE_Skip;
			}
			return TRACE_Abort;
		}
		return TRACE_Stop;
	}
	if (flags & CLOFF_MUSTBESHOOTABLE)
	{ // all shootability checks go here
		if (!(trace.Actor->flags & MF_SHOOTABLE))
		{
			return TRACE_Skip;
		}
		if (trace.Actor->flags2 & MF2_NONSHOOTABLE)
		{
			return TRACE_Skip;
		}
	}
	if ((flags & CLOFF_MUSTBESOLID) && !(trace.Actor->flags & MF_SOLID))
	{
		return TRACE_Skip;
	}
	if (flags & CLOFF_MUSTBEGHOST)
	{
		if (!(trace.Actor->flags3 & MF3_GHOST))
		{
			return TRACE_Skip;
		}
	}
	else if (flags & CLOFF_IGNOREGHOST)
	{
		if (trace.Actor->flags3 & MF3_GHOST)
		{
			return TRACE_Skip;
		}
	}
	if (
			((flags & CLOFF_JUMPENEMY) && data->Self->IsHostile(trace.Actor)) ||
			((flags & CLOFF_JUMPFRIEND) && data->Self->IsFriend(trace.Actor)) ||
			((flags & CLOFF_JUMPOBJECT) && !(trace.Actor->flags3 & MF3_ISMONSTER)) ||
			((flags & CLOFF_JUMPNONHOSTILE) && (trace.Actor->flags3 & MF3_ISMONSTER) && !data->Self->IsHostile(trace.Actor))
		)
	{
		return TRACE_Stop;
	}
	if (
			((flags & CLOFF_SKIPENEMY) && data->Self->IsHostile(trace.Actor)) ||
			((flags & CLOFF_SKIPFRIEND) && data->Self->IsFriend(trace.Actor)) ||
			((flags & CLOFF_SKIPOBJECT) && !(trace.Actor->flags3 & MF3_ISMONSTER)) ||
			((flags & CLOFF_SKIPNONHOSTILE) && (trace.Actor->flags3 & MF3_ISMONSTER) && !data->Self->IsHostile(trace.Actor))
		)
	{
		return TRACE_Skip;
	}
	data->BadActor = true;
	return TRACE_Abort;
}

DEFINE_ACTION_FUNCTION(AActor, CheckLOF)
{
	// Check line of fire

	/*
		Not accounted for / I don't know how it works: FLOORCLIP
	*/

	AActor *target;
	DVector3 pos;
	DVector3 vel;

	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF	(flags)			
	PARAM_FLOAT_DEF	(range)			
	PARAM_FLOAT_DEF	(minrange)		
	PARAM_ANGLE_DEF	(angle)			
	PARAM_ANGLE_DEF	(pitch)			
	PARAM_FLOAT_DEF	(offsetheight)	
	PARAM_FLOAT_DEF	(offsetwidth)	
	PARAM_INT_DEF	(ptr_target)	
	PARAM_FLOAT_DEF	(offsetforward)	

	DAngle ang;

	target = COPY_AAPTR(self, ptr_target == AAPTR_DEFAULT ? AAPTR_TARGET|AAPTR_PLAYER_GETTARGET|AAPTR_NULL : ptr_target); // no player-support by default

	if (flags & CLOFF_MUL_HEIGHT)
	{
		if (self->player != NULL)
		{
			// Synced with hitscan: self->player->mo->height is strangely conscientious about getting the right actor for player
			offsetheight *= self->player->mo->Height * self->player->crouchfactor;
		}
		else
		{
			offsetheight *= self->Height;
		}
	}
	if (flags & CLOFF_MUL_WIDTH)
	{
		offsetforward *= self->radius;
		offsetwidth *= self->radius;
}
		
	pos = self->PosPlusZ(offsetheight - self->Floorclip);

		if (!(flags & CLOFF_FROMBASE))
		{ // default to hitscan origin

			// Synced with hitscan: self->Height is strangely NON-conscientious about getting the right actor for player
			pos.Z += self->Height *0.5;
			if (self->player != NULL)
			{
				pos.Z += self->player->mo->AttackZOffset * self->player->crouchfactor;
			}
			else
			{
				pos.Z += 8;
			}
		}

		if (target)
		{
			if (range > 0 && !(flags & CLOFF_CHECKPARTIAL))
			{
				double distance = self->Distance3D(target);
				if (distance > range)
				{
					ACTION_RETURN_BOOL(false);
				}
			}

			if (flags & CLOFF_NOAIM_HORZ)
			{
				ang = self->Angles.Yaw;
			}
			else ang = self->AngleTo (target);
				
			angle += ang;

			double s = ang.Sin();
			double c = ang.Cos();
				
			DVector2 xy = self->Vec2Offset(offsetforward * c + offsetwidth * s, offsetforward * s - offsetwidth * c);

			pos.X = xy.X;
			pos.Y = xy.Y;

			double xydist = self->Distance2D(target);
			if (flags & CLOFF_NOAIM_VERT)
			{
				pitch += self->Angles.Pitch;
			}
			else if (flags & CLOFF_AIM_VERT_NOOFFSET)
			{
				pitch -= VecToAngle(xydist, target->Center() - pos.Z + offsetheight);
			}
			else
			{
				pitch -= VecToAngle(xydist, target->Center() - pos.Z);
			}
		}
		else if (flags & CLOFF_ALLOWNULL)
		{
			angle += self->Angles.Yaw;
			pitch += self->Angles.Pitch;

			double s = angle.Sin();
			double c = angle.Cos();

			DVector2 xy = self->Vec2Offset(offsetforward * c + offsetwidth * s, offsetforward * s - offsetwidth * c);

			pos.X = xy.X;
			pos.Y = xy.Y;
		}
		else
		{
			ACTION_RETURN_BOOL(false);
		}

		double cp = pitch.Cos();

		vel = { cp * angle.Cos(), cp * angle.Sin(), -pitch.Sin() };

	/* Variable set:

		jump, flags, target
		pos (trace point of origin)
		vel (trace unit vector)
		range
	*/

	sector_t *sec = P_PointInSector(pos);

	if (range == 0)
	{
		range = (self->player != NULL) ? PLAYERMISSILERANGE : MISSILERANGE;
	}

	FTraceResults trace;
	LOFData lof_data;

	lof_data.Self = self;
	lof_data.Target = target;
	lof_data.Flags = flags;
	lof_data.BadActor = false;

	Trace(pos, sec, vel, range, ActorFlags::FromInt(0xFFFFFFFF), ML_BLOCKEVERYTHING, self, trace, TRACE_PortalRestrict,
		CheckLOFTraceFunc, &lof_data);

	if (trace.HitType == TRACE_HitActor ||
		((flags & CLOFF_JUMP_ON_MISS) && !lof_data.BadActor && trace.HitType != TRACE_HitNone))
	{
		if (minrange > 0 && trace.Distance < minrange)
		{
			ACTION_RETURN_BOOL(false);
		}
		if ((trace.HitType == TRACE_HitActor) && (trace.Actor != NULL) && !(lof_data.BadActor))
		{
			if (flags & (CLOFF_SETTARGET))	self->target = trace.Actor;
			if (flags & (CLOFF_SETMASTER))	self->master = trace.Actor;
			if (flags & (CLOFF_SETTRACER))	self->tracer = trace.Actor;
		}
		ACTION_RETURN_BOOL(true);
	}
	ACTION_RETURN_BOOL(false);
}

//==========================================================================
//
// A_JumpIfTargetInLOS (state label, optional fixed fov, optional int flags,
// optional fixed dist_max, optional fixed dist_close)
//
// Jumps if the actor can see its target, or if the player has a linetarget.
// ProjectileTarget affects how projectiles are treated. If set, it will use
// the target of the projectile for seekers, and ignore the target for
// normal projectiles. If not set, it will use the missile's owner instead
// (the default). ProjectileTarget is now flag JLOSF_PROJECTILE. dist_max
// sets the maximum distance that actor can see, 0 means forever. dist_close
// uses special behavior if certain flags are set, 0 means no checks.
//
//==========================================================================

enum JLOS_flags
{
	JLOSF_PROJECTILE =		1 << 0,
	JLOSF_NOSIGHT =			1 << 1,
	JLOSF_CLOSENOFOV = 		1 << 2,
	JLOSF_CLOSENOSIGHT =	1 << 3,
	JLOSF_CLOSENOJUMP =		1 << 4,
	JLOSF_DEADNOJUMP =		1 << 5,
	JLOSF_CHECKMASTER =		1 << 6,
	JLOSF_TARGETLOS =		1 << 7,
	JLOSF_FLIPFOV =			1 << 8,
	JLOSF_ALLYNOJUMP =		1 << 9,
	JLOSF_COMBATANTONLY =	1 << 10,
	JLOSF_NOAUTOAIM =		1 << 11,
	JLOSF_CHECKTRACER =		1 << 12,
};

DEFINE_ACTION_FUNCTION(AActor, CheckIfTargetInLOS)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_ANGLE_DEF	(fov)		
	PARAM_INT_DEF	(flags)		
	PARAM_FLOAT_DEF	(dist_max)	
	PARAM_FLOAT_DEF	(dist_close)

	AActor *target, *viewport;
	FTranslatedLineTarget t;

	bool doCheckSight;

	if (!self->player)
	{
		if (flags & JLOSF_CHECKMASTER)
		{
			target = self->master;
		}
		else if ((self->flags & MF_MISSILE && (flags & JLOSF_PROJECTILE)) || (flags & JLOSF_CHECKTRACER))
		{
			if ((self->flags2 & MF2_SEEKERMISSILE) || (flags & JLOSF_CHECKTRACER))
				target = self->tracer;
			else
				target = NULL;
		}
		else
		{
			target = self->target;
		}

		if (target == NULL)
		{ // [KS] Let's not call P_CheckSight unnecessarily in this case.
			ACTION_RETURN_BOOL(false);
		}
		if ((flags & JLOSF_DEADNOJUMP) && (target->health <= 0))
		{
			ACTION_RETURN_BOOL(false);
		}

		doCheckSight = !(flags & JLOSF_NOSIGHT);
	}
	else
	{
		// Does the player aim at something that can be shot?
		P_AimLineAttack(self, self->Angles.Yaw, MISSILERANGE, &t, (flags & JLOSF_NOAUTOAIM) ? 0.5 : 0., ALF_PORTALRESTRICT);
		
		if (!t.linetarget)
		{
			ACTION_RETURN_BOOL(false);
		}
		target = t.linetarget;

		switch (flags & (JLOSF_TARGETLOS|JLOSF_FLIPFOV))
		{
		case JLOSF_TARGETLOS|JLOSF_FLIPFOV:
			// target makes sight check, player makes fov check; player has verified fov
			fov = 0.;
			// fall-through
		case JLOSF_TARGETLOS:
			doCheckSight = !(flags & JLOSF_NOSIGHT); // The target is responsible for sight check and fov
			break;
		default:
			// player has verified sight and fov
			fov = 0.;
			// fall-through
		case JLOSF_FLIPFOV: // Player has verified sight, but target must verify fov
			doCheckSight = false;
			break;
		}
	}

	// [FDARI] If target is not a combatant, don't jump
	if ( (flags & JLOSF_COMBATANTONLY) && (!target->player) && !(target->flags3 & MF3_ISMONSTER))
	{
		ACTION_RETURN_BOOL(false);
	}
	// [FDARI] If actors share team, don't jump
	if ((flags & JLOSF_ALLYNOJUMP) && self->IsFriend(target))
	{
		ACTION_RETURN_BOOL(false);
	}
	double distance = self->Distance3D(target);

	if (dist_max && (distance > dist_max))
	{
		ACTION_RETURN_BOOL(false);
	}
	if (dist_close && (distance < dist_close))
	{
		if (flags & JLOSF_CLOSENOJUMP)
		{
			ACTION_RETURN_BOOL(false);
		}
		if (flags & JLOSF_CLOSENOFOV)
			fov = 0.;

		if (flags & JLOSF_CLOSENOSIGHT)
			doCheckSight = false;
	}

	if (flags & JLOSF_TARGETLOS) { viewport = target; target = self; }
	else { viewport = self; }

	if (doCheckSight && !P_CheckSight (viewport, target, SF_IGNOREVISIBILITY))
	{
		ACTION_RETURN_BOOL(false);
	}

	if (flags & JLOSF_FLIPFOV)
	{
		if (viewport == self) { viewport = target; target = self; }
		else { target = viewport; viewport = self; }
	}

	fov = MIN<DAngle>(fov, 360.);

	if (fov > 0)
	{
		DAngle an = absangle(viewport->AngleTo(target), viewport->Angles.Yaw);

		if (an > (fov / 2))
		{
			ACTION_RETURN_BOOL(false); // [KS] Outside of FOV - return
		}
	}
	ACTION_RETURN_BOOL(true);
}


//==========================================================================
//
// A_JumpIfInTargetLOS (state label, optional fixed fov, optional int flags
// optional fixed dist_max, optional fixed dist_close)
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, CheckIfInTargetLOS)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_ANGLE_DEF	(fov)		
	PARAM_INT_DEF	(flags)		
	PARAM_FLOAT_DEF	(dist_max)	
	PARAM_FLOAT_DEF	(dist_close)

	AActor *target;

	if (flags & JLOSF_CHECKMASTER)
	{
		target = self->master;
	}
	else if (self->flags & MF_MISSILE && (flags & JLOSF_PROJECTILE))
	{
		if (self->flags2 & MF2_SEEKERMISSILE)
			target = self->tracer;
		else
			target = NULL;
	}
	else
	{
		target = self->target;
	}

	if (target == NULL)
	{ // [KS] Let's not call P_CheckSight unnecessarily in this case.
		ACTION_RETURN_BOOL(false);
	}

	if ((flags & JLOSF_DEADNOJUMP) && (target->health <= 0))
	{
		ACTION_RETURN_BOOL(false);
	}

	double distance = self->Distance3D(target);

	if (dist_max && (distance > dist_max))
	{
		ACTION_RETURN_BOOL(false);
	}

	bool doCheckSight = !(flags & JLOSF_NOSIGHT);

	if (dist_close && (distance < dist_close))
	{
		if (flags & JLOSF_CLOSENOJUMP)
		{
			ACTION_RETURN_BOOL(false);
		}
		if (flags & JLOSF_CLOSENOFOV)
			fov = 0.;

		if (flags & JLOSF_CLOSENOSIGHT)
			doCheckSight = false;
	}

	if (fov > 0 && (fov < 360.))
	{
		DAngle an = absangle(target->AngleTo(self), target->Angles.Yaw);

		if (an > (fov / 2))
		{
			ACTION_RETURN_BOOL(false); // [KS] Outside of FOV - return
		}
	}
	if (doCheckSight && !P_CheckSight (target, self, SF_IGNOREVISIBILITY))
	{
		ACTION_RETURN_BOOL(false);
	}
	ACTION_RETURN_BOOL(true);
}

//===========================================================================
//
// Modified code pointer from Skulltag
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AStateProvider, A_CheckForReload)
{
	PARAM_ACTION_PROLOGUE(AStateProvider);

	if ( self->player == NULL || self->player->ReadyWeapon == NULL )
	{
		ACTION_RETURN_STATE(NULL);
	}
	PARAM_INT		(count);
	PARAM_STATE_ACTION	(jump);
	PARAM_BOOL_DEF	(dontincrement);

	if (numret > 0)
	{
		ret->SetPointer(NULL);
		numret = 1;
	}

	AWeapon *weapon = self->player->ReadyWeapon;

	int ReloadCounter = weapon->ReloadCounter;
	if (!dontincrement || ReloadCounter != 0)
		ReloadCounter = (weapon->ReloadCounter+1) % count;
	else // 0 % 1 = 1?  So how do we check if the weapon was never fired?  We should only do this when we're not incrementing the counter though.
		ReloadCounter = 1;

	// If we have not made our last shot...
	if (ReloadCounter != 0)
	{
		// Go back to the refire frames, instead of continuing on to the reload frames.
		if (numret != 0)
		{
			ret->SetPointer(jump);
		}
	}
	else
	{
		// We need to reload. However, don't reload if we're out of ammo.
		weapon->CheckAmmo(false, false);
	}
	if (!dontincrement)
	{
		weapon->ReloadCounter = ReloadCounter;
	}
	return numret;
}

//===========================================================================
//
// Resets the counter for the above function
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AStateProvider, A_ResetReloadCounter)
{
	PARAM_ACTION_PROLOGUE(AStateProvider);

	if (self->player == NULL || self->player->ReadyWeapon == NULL)
		return 0;

	AWeapon *weapon = self->player->ReadyWeapon;
	weapon->ReloadCounter = 0;
	return 0;
}

//===========================================================================
//
// A_ChangeFlag
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_ChangeFlag)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_STRING	(flagname);
	PARAM_BOOL		(value);

	ModActorFlag(self, flagname, value);
	return 0;
}

//===========================================================================
//
// A_CheckFlag
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, CheckFlag)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_STRING	(flagname);
	PARAM_INT_DEF	(checkpointer);

	AActor *owner = COPY_AAPTR(self, checkpointer);
	ACTION_RETURN_BOOL(owner != nullptr && CheckActorFlag(owner, flagname));
}


DEFINE_ACTION_FUNCTION(AActor, A_ChangeCountFlags)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(kill);
	PARAM_INT_DEF(item);
	PARAM_INT_DEF(secret);

	if (self->CountsAsKill() && self->health > 0) --level.total_monsters;
	if (self->flags & MF_COUNTITEM) --level.total_items;
	if (self->flags5 & MF5_COUNTSECRET) --level.total_secrets;

	if (kill != -1)
	{
		if (kill == 0) self->flags &= ~MF_COUNTKILL;
		else self->flags |= MF_COUNTKILL;
	}

	if (item != -1)
	{
		if (item == 0) self->flags &= ~MF_COUNTITEM;
		else self->flags |= MF_COUNTITEM;
	}

	if (secret != -1)
	{
		if (secret == 0) self->flags5 &= ~MF5_COUNTSECRET;
		else self->flags5 |= MF5_COUNTSECRET;
	}
	if (self->CountsAsKill() && self->health > 0) ++level.total_monsters;
	if (self->flags & MF_COUNTITEM) ++level.total_items;
	if (self->flags5 & MF5_COUNTSECRET) ++level.total_secrets;
	return 0;
}


enum ERaise
{
	RF_TRANSFERFRIENDLINESS = 1,
	RF_NOCHECKPOSITION = 2
};

//===========================================================================
//
// A_RaiseMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseMaster)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(flags);

	bool copy = !!(flags & RF_TRANSFERFRIENDLINESS);
	if (self->master != NULL)
	{
		P_Thing_Raise(self->master, copy ? self : NULL, (flags & RF_NOCHECKPOSITION));
	}
	return 0;
}

//===========================================================================
//
// A_RaiseChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseChildren)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(flags);

	TThinkerIterator<AActor> it;
	AActor *mo;

	bool copy = !!(flags & RF_TRANSFERFRIENDLINESS);
	while ((mo = it.Next()) != NULL)
	{
		if (mo->master == self)
		{
			P_Thing_Raise(mo, copy ? self : NULL, (flags & RF_NOCHECKPOSITION));
		}
	}
	return 0;
}

//===========================================================================
//
// A_RaiseSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseSiblings)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(flags);

	TThinkerIterator<AActor> it;
	AActor *mo;

	bool copy = !!(flags & RF_TRANSFERFRIENDLINESS);
	if (self->master != NULL)
	{
		while ((mo = it.Next()) != NULL)
		{
			if (mo->master == self->master && mo != self)
			{
				P_Thing_Raise(mo, copy ? self : NULL, (flags & RF_NOCHECKPOSITION));
			}
		}
	}
	return 0;
}

//===========================================================================
//
// A_RaiseSelf
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RaiseSelf)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(flags);
	ACTION_RETURN_BOOL(P_Thing_Raise(self, NULL, (flags & RF_NOCHECKPOSITION)));
}

//===========================================================================
//
// CanRaise
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, CanRaise)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_BOOL(P_Thing_CanRaise(self));
}

//===========================================================================
//
// A_MonsterRefire
//
// Keep firing unless target got out of sight
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_MonsterRefire)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT	(prob);
	PARAM_STATE	(jump);

	A_FaceTarget(self);

	if (pr_monsterrefire() < prob)
	{
		ACTION_RETURN_STATE(NULL);
	}
	if (self->target == NULL
		|| P_HitFriend (self)
		|| self->target->health <= 0
		|| !P_CheckSight (self, self->target, SF_SEEPASTBLOCKEVERYTHING|SF_SEEPASTSHOOTABLELINES) )
	{
		ACTION_RETURN_STATE(jump);
	}
	ACTION_RETURN_STATE(NULL);
}

//===========================================================================
//
// A_SetAngle
//
// Set actor's angle (in degrees).
//
//===========================================================================
enum
{
	SPF_FORCECLAMP = 1,	// players always clamp
	SPF_INTERPOLATE = 2,
};


DEFINE_ACTION_FUNCTION(AActor, A_SetAngle)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT_DEF(angle);
	PARAM_INT_DEF(flags);
	PARAM_INT_DEF(ptr);

	AActor *ref = COPY_AAPTR(self, ptr);
	if (ref != NULL)
	{
		ref->SetAngle(angle, !!(flags & SPF_INTERPOLATE));
	}
	return 0;
}

//===========================================================================
//
// A_SetPitch
//
// Set actor's pitch (in degrees).
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SetPitch)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(pitch);
	PARAM_INT_DEF(flags);
	PARAM_INT_DEF(ptr);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (ref != NULL)
	{
		ref->SetPitch(pitch, !!(flags & SPF_INTERPOLATE), !!(flags & SPF_FORCECLAMP));
	}
	return 0;
}

//===========================================================================
//
// [Nash] A_SetRoll
//
// Set actor's roll (in degrees).
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SetRoll)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT		(roll);
	PARAM_INT_DEF	(flags);
	PARAM_INT_DEF	(ptr)	;
	AActor *ref = COPY_AAPTR(self, ptr);

	if (ref != NULL)
	{
		ref->SetRoll(roll, !!(flags & SPF_INTERPOLATE));
	}
	return 0;
}

//===========================================================================
//
// A_ScaleVelocity
//
// Scale actor's velocity.
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ScaleVelocity)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(scale);
	PARAM_INT_DEF(ptr);

	AActor *ref = COPY_AAPTR(self, ptr);

	if (ref == NULL)
	{
		return 0;
	}

	bool was_moving = !ref->Vel.isZero();

	ref->Vel *= scale;

	// If the actor was previously moving but now is not, and is a player,
	// update its player variables. (See A_Stop.)
	if (was_moving)
	{
		CheckStopped(ref);
	}
	return 0;
}

//===========================================================================
//
// A_ChangeVelocity
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ChangeVelocity)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT_DEF	(x)		
	PARAM_FLOAT_DEF	(y)		
	PARAM_FLOAT_DEF	(z)		
	PARAM_INT_DEF	(flags)	
	PARAM_INT_DEF	(ptr)	

	AActor *ref = COPY_AAPTR(self, ptr);

	if (ref == NULL)
	{
		return 0;
	}

	INTBOOL was_moving = !ref->Vel.isZero();

	DVector3 vel(x, y, z);
	double sina = ref->Angles.Yaw.Sin();
	double cosa = ref->Angles.Yaw.Cos();

	if (flags & 1)	// relative axes - make x, y relative to actor's current angle
	{
		vel.X = x*cosa - y*sina;
		vel.Y = x*sina + y*cosa;
	}
	if (flags & 2)	// discard old velocity - replace old velocity with new velocity
	{
		ref->Vel = vel;
	}
	else	// add new velocity to old velocity
	{
		ref->Vel += vel;
	}

	if (was_moving)
	{
		CheckStopped(ref);
	}
	return 0;
}

//===========================================================================
//
// A_SetUserVar
//
//===========================================================================

static PField *GetVar(DObject *self, FName varname)
{
	PField *var = dyn_cast<PField>(self->GetClass()->FindSymbol(varname, true));

	if (var == NULL || (var->Flags & (VARF_Native | VARF_Private | VARF_Protected | VARF_Static)) || !var->Type->isScalar())
	{
		Printf("%s is not a user variable in class %s\n", varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return nullptr;
	}
	return var;
}

DEFINE_ACTION_FUNCTION(AActor, A_SetUserVar)
{
	PARAM_SELF_PROLOGUE(DObject);
	PARAM_NAME	(varname);
	PARAM_INT	(value);

	// Set the value of the specified user variable.
	PField *var = GetVar(self, varname);
	if (var != nullptr)
	{
		var->Type->SetValue(reinterpret_cast<uint8_t *>(self) + var->Offset, value);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SetUserVarFloat)
{
	PARAM_SELF_PROLOGUE(DObject);
	PARAM_NAME	(varname);
	PARAM_FLOAT	(value);

	// Set the value of the specified user variable.
	PField *var = GetVar(self, varname);
	if (var != nullptr)
	{
		var->Type->SetValue(reinterpret_cast<uint8_t *>(self) + var->Offset, value);
	}
	return 0;
}

//===========================================================================
//
// A_SetUserArray
//
//===========================================================================

static PField *GetArrayVar(DObject *self, FName varname, int pos)
{
	PField *var = dyn_cast<PField>(self->GetClass()->FindSymbol(varname, true));

	if (var == NULL || (var->Flags & (VARF_Native | VARF_Private | VARF_Protected | VARF_Static)) ||
		!var->Type->isArray() || !static_cast<PArray *>(var->Type)->ElementType->isScalar())
	{
		Printf("%s is not a user array in class %s\n", varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return nullptr;
	}
	if ((unsigned)pos >= static_cast<PArray *>(var->Type)->ElementCount)
	{
		Printf("%d is out of bounds in array %s in class %s\n", pos, varname.GetChars(),
			self->GetClass()->TypeName.GetChars());
		return nullptr;
	}
	return var;
}

DEFINE_ACTION_FUNCTION(AActor, A_SetUserArray)
{
	PARAM_SELF_PROLOGUE(DObject);
	PARAM_NAME	(varname);
	PARAM_INT	(pos);
	PARAM_INT	(value);

	// Set the value of the specified user array at index pos.
	PField *var = GetArrayVar(self, varname, pos);
	if (var != nullptr)
	{
		PArray *arraytype = static_cast<PArray *>(var->Type);
		arraytype->ElementType->SetValue(reinterpret_cast<uint8_t *>(self) + var->Offset + arraytype->ElementSize * pos, value);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SetUserArrayFloat)
{
	PARAM_SELF_PROLOGUE(DObject);
	PARAM_NAME	(varname);
	PARAM_INT	(pos);
	PARAM_FLOAT	(value);

	// Set the value of the specified user array at index pos.
	PField *var = GetArrayVar(self, varname, pos);
	if (var != nullptr)
	{
		PArray *arraytype = static_cast<PArray *>(var->Type);
		arraytype->ElementType->SetValue(reinterpret_cast<uint8_t *>(self) + var->Offset + arraytype->ElementSize * pos, value);
	}
	return 0;
}

//===========================================================================
//
// A_Teleport([state teleportstate, [class targettype,
// [class fogtype, [int flags, [fixed mindist,
// [fixed maxdist]]]]]])
//
// Attempts to teleport to a targettype at least mindist away and at most
// maxdist away (0 means unlimited). If successful, spawn a fogtype at old
// location and place calling actor in teleportstate. 
//
//===========================================================================
enum T_Flags
{
	TF_TELEFRAG =		0x00000001, // Allow telefrag in order to teleport.
	TF_RANDOMDECIDE =	0x00000002, // Randomly fail based on health. (A_Srcr2Decide)
	TF_FORCED =			0x00000004, // Forget what's in the way. TF_Telefrag takes precedence though.
	TF_KEEPVELOCITY =	0x00000008, // Preserve velocity.
	TF_KEEPANGLE =		0x00000010, // Keep angle.
	TF_USESPOTZ =		0x00000020, // Set the z to the spot's z, instead of the floor.
	TF_NOSRCFOG =		0x00000040, // Don't leave any fog behind when teleporting.
	TF_NODESTFOG =		0x00000080, // Don't spawn any fog at the arrival position.
	TF_USEACTORFOG =	0x00000100, // Use the actor's TeleFogSourceType and TeleFogDestType fogs.
	TF_NOJUMP =			0x00000200, // Don't jump after teleporting.
	TF_OVERRIDE =		0x00000400, // Ignore NOTELEPORT.
	TF_SENSITIVEZ =		0x00000800, // Fail if the actor wouldn't fit in the position (for Z).
};

DEFINE_ACTION_FUNCTION(AActor, A_Teleport)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_STATE_ACTION_DEF	(teleport_state)			
	PARAM_CLASS_DEF		(target_type, ASpecialSpot)	
	PARAM_CLASS_DEF		(fog_type, AActor)			
	PARAM_INT_DEF		(flags)						
	PARAM_FLOAT_DEF		(mindist)					
	PARAM_FLOAT_DEF		(maxdist)					
	PARAM_INT_DEF		(ptr)						

	AActor *ref = COPY_AAPTR(self, ptr);

	// A_Teleport and A_Warp were the only codepointers that can state jump
	// *AND* have a meaningful inventory state chain result. Grrr.
	if (numret > 1)
	{
		ret[1].SetInt(false);
		numret = 2;
	}
	if (numret > 0)
	{
		ret[0].SetPointer(NULL);
	}

	if (!ref)
	{
		return numret;
	}

	if ((ref->flags2 & MF2_NOTELEPORT) && !(flags & TF_OVERRIDE))
	{
		return numret;
	}

	// Randomly choose not to teleport like A_Srcr2Decide.
	if (flags & TF_RANDOMDECIDE)
	{
		static const int chance[] =
		{
			192, 120, 120, 120, 64, 64, 32, 16, 0
		};

		unsigned int chanceindex = ref->health / ((ref->SpawnHealth()/8 == 0) ? 1 : ref->SpawnHealth()/8);

		if (chanceindex >= countof(chance))
		{
			chanceindex = countof(chance) - 1;
		}

		if (pr_teleport() >= chance[chanceindex])
		{
			return numret;
		}
	}

	DSpotState *state = DSpotState::GetSpotState();
	if (state == NULL)
	{
		return numret;
	}

	if (target_type == NULL)
	{
		target_type = PClass::FindActor("BossSpot");
	}

	AActor *spot = state->GetSpotWithMinMaxDistance(target_type, ref->X(), ref->Y(), mindist, maxdist);
	if (spot == NULL)
	{
		return numret;
	}

	// [MC] By default, the function adjusts the actor's Z if it's below the floor or above the ceiling.
	// This can be an issue as actors designed to maintain specific z positions wind up teleporting
	// anyway when they should not, such as a floor rising above or ceiling lowering below the position
	// of the spot.
	if (flags & TF_SENSITIVEZ)
	{
		double posz = (flags & TF_USESPOTZ) ? spot->Z() : spot->floorz;
		if ((posz + ref->Height > spot->ceilingz) || (posz < spot->floorz))
		{
			return numret;
		}
	}
	DVector3 prev = ref->Pos();
	double aboveFloor = spot->Z() - spot->floorz;
	double finalz = spot->floorz + aboveFloor;

	if (spot->Top() > spot->ceilingz)
		finalz = spot->ceilingz - ref->Height;
	else if (spot->Z() < spot->floorz)
		finalz = spot->floorz;

	DVector3 tpos = spot->PosAtZ(finalz);

	//Take precedence and cooperate with telefragging first.
	bool tele_result = P_TeleportMove(ref, tpos, !!(flags & TF_TELEFRAG));

	if (!tele_result && (flags & TF_FORCED))
	{
		//If for some reason the original move didn't work, regardless of telefrag, force it to move.
		ref->SetOrigin(tpos, false);
		tele_result = true;
	}

	AActor *fog1 = NULL, *fog2 = NULL;
	if (tele_result)
	{
		//If a fog type is defined in the parameter, or the user wants to use the actor's predefined fogs,
		//and if there's no desire to be fogless, spawn a fog based upon settings.
		if (fog_type || (flags & TF_USEACTORFOG))
		{ 
			if (!(flags & TF_NOSRCFOG))
			{
				if (flags & TF_USEACTORFOG)
					P_SpawnTeleportFog(ref, prev, true, true);
				else
				{
					fog1 = Spawn(fog_type, prev, ALLOW_REPLACE);
					if (fog1 != NULL)
						fog1->target = ref;
				}
			}
			if (!(flags & TF_NODESTFOG))
			{
				if (flags & TF_USEACTORFOG)
					P_SpawnTeleportFog(ref, ref->Pos(), false, true);
				else
				{
					fog2 = Spawn(fog_type, ref->Pos(), ALLOW_REPLACE);
					if (fog2 != NULL)
						fog2->target = ref;
				}
			}
		}
		
		ref->SetZ((flags & TF_USESPOTZ) ? spot->Z() : ref->floorz, false);

		if (!(flags & TF_KEEPANGLE))
			ref->Angles.Yaw = spot->Angles.Yaw;

		if (!(flags & TF_KEEPVELOCITY)) ref->Vel.Zero();

		if (!(flags & TF_NOJUMP)) //The state jump should only happen with the calling actor.
		{
			if (teleport_state == NULL)
			{
				// Default to Teleport.
				teleport_state = self->FindState("Teleport");
				// If still nothing, then return.
				if (teleport_state == NULL)
				{
					return numret;
				}
			}
			if (numret > 0)
			{
				ret[0].SetPointer(teleport_state);
			}
			return numret;
		}
	}
	if (numret > 1)
	{
		ret[1].SetInt(tele_result);
	}
	return numret;
}

//===========================================================================
//
// A_Quake
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_Quake)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(intensity);
	PARAM_INT		(duration);
	PARAM_INT		(damrad);
	PARAM_INT		(tremrad);
	PARAM_SOUND_DEF	(sound);

	P_StartQuake(self, 0, intensity, duration, damrad, tremrad, sound);
	return 0;
}

//===========================================================================
//
// A_QuakeEx
//
// Extended version of A_Quake. Takes individual axis into account and can
// take flags.
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_QuakeEx)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(intensityX);
	PARAM_INT(intensityY);
	PARAM_INT(intensityZ);
	PARAM_INT(duration);
	PARAM_INT(damrad);
	PARAM_INT(tremrad);
	PARAM_SOUND_DEF(sound);
	PARAM_INT_DEF(flags);
	PARAM_FLOAT_DEF(mulWaveX);
	PARAM_FLOAT_DEF(mulWaveY);
	PARAM_FLOAT_DEF(mulWaveZ);
	PARAM_INT_DEF(falloff);
	PARAM_INT_DEF(highpoint);
	PARAM_FLOAT_DEF(rollIntensity);
	PARAM_FLOAT_DEF(rollWave);
	P_StartQuakeXYZ(self, 0, intensityX, intensityY, intensityZ, duration, damrad, tremrad, sound, flags, mulWaveX, mulWaveY, mulWaveZ, falloff, highpoint, 
		rollIntensity, rollWave);
	return 0;
}

//===========================================================================
//
// A_Weave
//
//===========================================================================

void A_Weave(AActor *self, int xyspeed, int zspeed, double xydist, double zdist)
{
	DVector2 newpos;
	int weaveXY, weaveZ;
	DAngle angle;
	double dist;

	weaveXY = self->WeaveIndexXY & 63;
	weaveZ = self->WeaveIndexZ & 63;
	angle = self->Angles.Yaw + 90;

	if (xydist != 0 && xyspeed != 0)
	{
		dist = BobSin(weaveXY) * xydist;
		newpos = self->Pos().XY() - angle.ToVector(dist);
		weaveXY = (weaveXY + xyspeed) & 63;
		dist = BobSin(weaveXY) * xydist;
		newpos += angle.ToVector(dist);
		if (!(self->flags5 & MF5_NOINTERACTION))
		{
			P_TryMove (self, newpos, true);
		}
		else
		{
			FLinkContext ctx;
			self->UnlinkFromWorld (&ctx);
			self->flags |= MF_NOBLOCKMAP;
			// We need to do portal offsetting here explicitly, because SetXY cannot do that.
			newpos -= self->Pos().XY();
			self->SetXY(self->Vec2Offset(newpos.X, newpos.Y));
			self->LinkToWorld (&ctx);
		}
		self->WeaveIndexXY = weaveXY;
	}
	if (zdist != 0 && zspeed != 0)
	{
		self->AddZ(-BobSin(weaveZ) * zdist);
		weaveZ = (weaveZ + zspeed) & 63;
		self->AddZ(BobSin(weaveZ) * zdist);
		self->WeaveIndexZ = weaveZ;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_Weave)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT	(xspeed);
	PARAM_INT	(yspeed);
	PARAM_FLOAT	(xdist);
	PARAM_FLOAT	(ydist);
	A_Weave(self, xspeed, yspeed, xdist, ydist);
	return 0;
}




//===========================================================================
//
// A_LineEffect
//
// This allows linedef effects to be activated inside deh frames.
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LineEffect)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(special);
	PARAM_INT_DEF(tag);

	line_t junk;
	maplinedef_t oldjunk;
	bool res = false;
	if (!(self->flags6 & MF6_LINEDONE))						// Unless already used up
	{
		if ((oldjunk.special = special))					// Linedef type
		{
			oldjunk.tag = tag;								// Sector tag for linedef
			P_TranslateLineDef(&junk, &oldjunk);			// Turn into native type
			res = !!P_ExecuteSpecial(junk.special, NULL, self, false, junk.args[0], 
				junk.args[1], junk.args[2], junk.args[3], junk.args[4]); 
			if (res && !(junk.flags & ML_REPEAT_SPECIAL))	// If only once,
				self->flags6 |= MF6_LINEDONE;				// no more for this thing
		}
	}
	ACTION_RETURN_BOOL(res);
}

//==========================================================================
//
// A Wolf3D-style attack codepointer
//
//==========================================================================
enum WolfAttackFlags
{
	WAF_NORANDOM	= 1,
	WAF_USEPUFF		= 2,
};

DEFINE_ACTION_FUNCTION(AActor, A_WolfAttack)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF	(flags)				
	PARAM_SOUND_DEF	(sound)				
	PARAM_FLOAT_DEF	(snipe)				
	PARAM_INT_DEF	(maxdamage)			
	PARAM_INT_DEF	(blocksize)			
	PARAM_INT_DEF	(pointblank)		
	PARAM_INT_DEF	(longrange)			
	PARAM_FLOAT_DEF	(runspeed)			
	PARAM_CLASS_DEF	(pufftype, AActor)	

	if (!self->target)
		return 0;

	// Enemy can't see target
	if (!P_CheckSight(self, self->target))
		return 0;

	A_FaceTarget (self);

	// Target can dodge if it can see enemy
	DAngle angle = absangle(self->target->Angles.Yaw, self->target->AngleTo(self));
	bool dodge = (P_CheckSight(self->target, self) && angle < 30. * 256. / 360.);	// 30 byteangles ~ 21°

	// Distance check is simplistic
	DVector2 vec = self->Vec2To(self->target);
	double dx = fabs (vec.X);
	double dy = fabs (vec.Y);
	double dist = dx > dy ? dx : dy;

	// Some enemies are more precise
	dist *= snipe;

	// Convert distance into integer number of blocks
	int idist = int(dist / blocksize);

	// Now for the speed accuracy thingie
	double speed = self->target->Vel.LengthSquared();
	int hitchance = speed < runspeed ? 256 : 160;

	// Distance accuracy (factoring dodge)
	hitchance -= idist * (dodge ? 16 : 8);

	// While we're here, we may as well do something for this:
	if (self->target->flags & MF_SHADOW)
	{
		hitchance >>= 2;
	}

	// The attack itself
	if (pr_cabullet() < hitchance)
	{
		// Compute position for spawning blood/puff
		DAngle angle = self->target->AngleTo(self);
		DVector3 BloodPos = self->target->Vec3Angle(self->target->radius, angle, self->target->Height/2);

		int damage = flags & WAF_NORANDOM ? maxdamage : (1 + (pr_cabullet() % maxdamage));
		if (dist >= pointblank)
			damage >>= 1;
		if (dist >= longrange)
			damage >>= 1;
		FName mod = NAME_None;
		bool spawnblood = !((self->target->flags & MF_NOBLOOD) 
			|| (self->target->flags2 & (MF2_INVULNERABLE|MF2_DORMANT)));
		if (flags & WAF_USEPUFF && pufftype)
		{
			AActor *dpuff = GetDefaultByType(pufftype->GetReplacement());
			mod = dpuff->DamageType;

			if (dpuff->flags2 & MF2_THRUGHOST && self->target->flags3 & MF3_GHOST)
				damage = 0;
			
			if ((0 && dpuff->flags3 & MF3_PUFFONACTORS) || !spawnblood)
			{
				spawnblood = false;
				P_SpawnPuff(self, pufftype, BloodPos, angle, angle, 0);
			}
		}
		else if (self->target->flags3 & MF3_GHOST)
			damage >>= 2;
		if (damage)
		{
			int newdam = P_DamageMobj(self->target, self, self, damage, mod, DMG_THRUSTLESS);
			if (spawnblood)
			{
				P_SpawnBlood(BloodPos, angle, newdam > 0 ? newdam : damage, self->target);
				P_TraceBleed(newdam > 0 ? newdam : damage, self->target, self);
			}
		}
	}

	// And finally, let's play the sound
	S_Sound (self, CHAN_WEAPON, sound, 1, ATTN_NORM);
	return 0;
}


//==========================================================================
//
// A_Warp
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_Warp)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(destination_selector);
	PARAM_FLOAT_DEF(xofs)				
	PARAM_FLOAT_DEF(yofs)				
	PARAM_FLOAT_DEF(zofs)				
	PARAM_ANGLE_DEF(angle)				
	PARAM_INT_DEF(flags)				
	PARAM_STATE_ACTION_DEF(success_state)		
	PARAM_FLOAT_DEF(heightoffset)		
	PARAM_FLOAT_DEF(radiusoffset)		
	PARAM_ANGLE_DEF(pitch)				
	
	AActor *reference;

	// A_Teleport and A_Warp were the only codepointers that can state jump
	// *AND* have a meaningful inventory state chain result. Grrr.
	if (numret > 1)
	{
		ret[1].SetInt(false);
		numret = 2;
	}
	if (numret > 0)
	{
		ret[0].SetPointer(NULL);
	}

	if ((flags & WARPF_USETID))
	{
		reference = SingleActorFromTID(destination_selector, self);
	}
	else
	{
		reference = COPY_AAPTR(self, destination_selector);
	}

	//If there is no actor to warp to, fail.
	if (!reference)
	{
		return numret;
	}

	if (P_Thing_Warp(self, reference, xofs, yofs, zofs, angle, flags, heightoffset, radiusoffset, pitch))
	{
		if (success_state)
		{
			// Jumps should never set the result for inventory state chains!
			// in this case, you have the statejump to help you handle all the success anyway.
			if (numret > 0)
			{
				ret[0].SetPointer(success_state);
			}
		}
		else if (numret > 1)
		{
			ret[1].SetInt(true);
		}
	}
	return numret;
}

static bool DoCheckSpecies(AActor *mo, FName filterSpecies, bool exclude)
{
	FName actorSpecies = mo->GetSpecies();
	if (filterSpecies == NAME_None) return true;
	return exclude ? (actorSpecies != filterSpecies) : (actorSpecies == filterSpecies);
}

static bool DoCheckClass(AActor *mo, PClassActor *filterClass, bool exclude)
{
	const PClass *actorClass = mo->GetClass();
	if (filterClass == NULL) return true;
	return exclude ? (actorClass != filterClass) : (actorClass == filterClass);
}

//==========================================================================
//
// A_RadiusGive(item, distance, flags, amount, filter, species)
//
// Uses code roughly similar to A_Explode (but without all the compatibility
// baggage and damage computation code) to give an item to all eligible mobjs
// in range.
//
//==========================================================================
enum RadiusGiveFlags
{
	RGF_GIVESELF	=   1 << 0,
	RGF_PLAYERS		=   1 << 1,
	RGF_MONSTERS	=   1 << 2,
	RGF_OBJECTS		=   1 << 3,
	RGF_VOODOO		=	1 << 4,
	RGF_CORPSES		=	1 << 5,
	RGF_NOTARGET	=	1 << 6,
	RGF_NOTRACER	=	1 << 7,
	RGF_NOMASTER	=	1 << 8,
	RGF_CUBE		=	1 << 9,
	RGF_NOSIGHT		=	1 << 10,
	RGF_MISSILES	=	1 << 11,
	RGF_INCLUSIVE	=	1 << 12,
	RGF_ITEMS		=	1 << 13,
	RGF_KILLED		=	1 << 14,
	RGF_EXFILTER	=	1 << 15,
	RGF_EXSPECIES	=	1 << 16,
	RGF_EITHER		=	1 << 17,

	RGF_MASK		=	/*2111*/
						RGF_GIVESELF |
						RGF_PLAYERS |
						RGF_MONSTERS |
						RGF_OBJECTS |
						RGF_VOODOO |
						RGF_CORPSES | 
						RGF_KILLED |
						RGF_MISSILES |
						RGF_ITEMS,
};

static bool DoRadiusGive(AActor *self, AActor *thing, PClassActor *item, int amount, double distance, int flags, PClassActor *filter, FName species, double mindist)
{
	
	bool doPass = false;
	// Always allow self to give, no matter what other flags are specified. Otherwise, not at all.
	if (thing == self)
	{
		if (!(flags & RGF_GIVESELF))
			return false;
		doPass = true;
	}
	else if (thing->flags & MF_MISSILE)
	{
		if (!(flags & RGF_MISSILES))
			return false;
		doPass = true;
	}
	else if (((flags & RGF_ITEMS) && thing->IsKindOf(RUNTIME_CLASS(AInventory))) ||
			((flags & RGF_CORPSES) && thing->flags & MF_CORPSE) ||
			((flags & RGF_KILLED) && thing->flags6 & MF6_KILLED))
	{
		doPass = true;
	}
	else if ((flags & (RGF_MONSTERS | RGF_OBJECTS | RGF_PLAYERS | RGF_VOODOO)))
	{
		// Make sure it's alive as we're not looking for corpses or killed here.
		if (!doPass && thing->health > 0)
		{
			if (thing->player != nullptr)
			{
				if (((flags & RGF_PLAYERS) && (thing->player->mo == thing)) ||
					((flags & RGF_VOODOO) && (thing->player->mo != thing)))
				{
					doPass = true;
				}
			}
			else
			{
				if (((flags & RGF_MONSTERS) && (thing->flags3 & MF3_ISMONSTER)) ||
					((flags & RGF_OBJECTS) && (!(thing->flags3 & MF3_ISMONSTER)) &&
					(thing->flags & MF_SHOOTABLE || thing->flags6 & MF6_VULNERABLE)))
				{
					doPass = true;
				}
			}
		}
	}

	// Nothing matched up so don't bother with the rest.
	if (!doPass)
		return false;

	//[MC] Check for a filter, species, and the related exfilter/expecies/either flag(s).
	bool filterpass = DoCheckClass(thing, filter, !!(flags & RGF_EXFILTER)),
		speciespass = DoCheckSpecies(thing, species, !!(flags & RGF_EXSPECIES));

	if ((flags & RGF_EITHER) ? (!(filterpass || speciespass)) : (!(filterpass && speciespass)))
	{
		if (thing != self)      //Don't let filter and species obstruct RGF_GIVESELF.
			return false;
	}

	if ((thing != self) && (flags & (RGF_NOTARGET | RGF_NOMASTER | RGF_NOTRACER)))
	{
		//Check for target, master, and tracer flagging.
		bool targetPass = true;
		bool masterPass = true;
		bool tracerPass = true;
		bool ptrPass = false;

		if ((thing == self->target) && (flags & RGF_NOTARGET))
			targetPass = false;
		if ((thing == self->master) && (flags & RGF_NOMASTER))
			masterPass = false;
		if ((thing == self->tracer) && (flags & RGF_NOTRACER))
			tracerPass = false;

		ptrPass = (flags & RGF_INCLUSIVE) ? (targetPass || masterPass || tracerPass) : (targetPass && masterPass && tracerPass);

		//We should not care about what the actor is here. It's safe to abort this actor.
		if (!ptrPass)
			return false;
	}

	if (doPass)
	{
		DVector3 diff = self->Vec3To(thing);
		diff.Z += thing->Height *0.5;
		if (flags & RGF_CUBE)
		{ // check if inside a cube
			double dx = fabs(diff.X);
			double dy = fabs(diff.Y);
			double dz = fabs(diff.Z);
			if ((dx > distance || dy > distance || dz > distance) || (mindist && (dx < mindist && dy < mindist && dz < mindist)))
			{
				return false;
			}
		}
		else
		{ // check if inside a sphere
			double lengthsquared = diff.LengthSquared();
			if (lengthsquared > distance*distance || (mindist && (lengthsquared < mindist*mindist)))
			{
				return false;
			}
		}

		if ((flags & RGF_NOSIGHT) || P_CheckSight(thing, self, SF_IGNOREVISIBILITY | SF_IGNOREWATERBOUNDARY))
		{ // OK to give; target is in direct path, or the monster doesn't care about it being in line of sight.
			AInventory *gift = static_cast<AInventory *>(Spawn(item));
			if (gift->IsKindOf(NAME_Health))
			{
				gift->Amount *= amount;
			}
			else
			{
				gift->Amount = amount;
			}
			gift->flags |= MF_DROPPED;
			gift->ClearCounters();
			if (!gift->CallTryPickup(thing))
			{
				gift->Destroy();
				return false;
			}
			else
			{
				return true;
			}
		}
	}
	return false;
}

DEFINE_ACTION_FUNCTION(AActor, A_RadiusGive)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS		(item, AInventory);
	PARAM_FLOAT		(distance);
	PARAM_INT		(flags);
	PARAM_INT_DEF	(amount);
	PARAM_CLASS_DEF	(filter, AActor);
	PARAM_NAME_DEF	(species);
	PARAM_FLOAT_DEF	(mindist);
	PARAM_INT_DEF	(limit);

	// We need a valid item, valid targets, and a valid range
	if (item == nullptr || (flags & RGF_MASK) == 0 || !flags || distance <= 0 || mindist >= distance)
	{
		ACTION_RETURN_INT(0);
	}
	bool unlimited = (limit <= 0);
	if (amount == 0)
	{
		amount = 1;
	}
	AActor *thing;
	int given = 0;
	if (flags & RGF_MISSILES)
	{
		TThinkerIterator<AActor> it;
		while ((thing = it.Next()) && ((unlimited) || (given < limit)))
		{
			given += DoRadiusGive(self, thing, item, amount, distance, flags, filter, species, mindist);
		}
	}
	else
	{
		FPortalGroupArray check(FPortalGroupArray::PGA_Full3d);
		double mid = self->Center();
		FMultiBlockThingsIterator it(check, self->X(), self->Y(), mid-distance, mid+distance, distance, false, self->Sector);
		FMultiBlockThingsIterator::CheckResult cres;

		while ((it.Next(&cres)) && ((unlimited) || (given < limit)))
		{
			given += DoRadiusGive(self, cres.thing, item, amount, distance, flags, filter, species, mindist);
		}
	}
	ACTION_RETURN_INT(given);
}

//==========================================================================
//
// A_SetTics
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SetTics)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(tics_to_set);

	if (ACTION_CALL_FROM_PSPRITE())
	{
		DPSprite *pspr = self->player->FindPSprite(stateinfo->mPSPIndex);
		if (pspr != nullptr)
		{
			pspr->Tics = tics_to_set;
			return 0;
		}
	}
	else if (ACTION_CALL_FROM_ACTOR())
	{
		// Just set tics for self.
		self->tics = tics_to_set;
	}
	// for inventory state chains this needs to be ignored.
	return 0;
}

//==========================================================================
//
// A_DropItem
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_DropItem)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS   (spawntype, AActor);
	PARAM_INT_DEF(amount);
	PARAM_INT_DEF(chance);

	P_DropItem(self, spawntype, amount, chance);
	return 0;
}

//===========================================================================
//
// Common A_Damage handler
//
// A_Damage* (int amount, str damagetype, int flags, str filter, str species)
// Damages the specified actor by the specified amount. Negative values heal.
// Flags: See below.
// Filter: Specified actor is the only type allowed to be affected.
// Species: Specified species is the only type allowed to be affected.
//
// Examples: 
// A_Damage(20,"Normal",DMSS_FOILINVUL,0,"DemonicSpecies") <--Only actors 
//	with a species "DemonicSpecies" will be affected. Use 0 to not filter by actor.
//
//===========================================================================

enum DMSS
{
	DMSS_FOILINVUL			= 1,	//Foil invulnerability
	DMSS_AFFECTARMOR		= 2,	//Make it affect armor
	DMSS_KILL				= 4,	//Damages them for their current health
	DMSS_NOFACTOR			= 8,	//Ignore DamageFactors
	DMSS_FOILBUDDHA			= 16,	//Can kill actors with Buddha flag, except the player.
	DMSS_NOPROTECT			= 32,	//Ignores PowerProtection entirely
	DMSS_EXFILTER			= 64,	//Changes filter into a blacklisted class instead of whitelisted.
	DMSS_EXSPECIES			= 128,	// ^ but with species instead.
	DMSS_EITHER				= 256,  //Allow either type or species to be affected.
	DMSS_INFLICTORDMGTYPE	= 512,	//Ignore the passed damagetype and use the inflictor's instead.
};

static void DoDamage(AActor *dmgtarget, AActor *inflictor, AActor *source, int amount, FName DamageType, int flags, PClassActor *filter, FName species)
{
	bool filterpass = DoCheckClass(dmgtarget, filter, !!(flags & DMSS_EXFILTER)),
		speciespass = DoCheckSpecies(dmgtarget, species, !!(flags & DMSS_EXSPECIES));
	if ((flags & DMSS_EITHER) ? (filterpass || speciespass) : (filterpass && speciespass))
	{
		int dmgFlags = 0;
		if (flags & DMSS_FOILINVUL)
			dmgFlags |= DMG_FOILINVUL;
		if (flags & DMSS_FOILBUDDHA)
			dmgFlags |= DMG_FOILBUDDHA;
		if (flags & (DMSS_KILL | DMSS_NOFACTOR)) //Kill implies NoFactor
			dmgFlags |= DMG_NO_FACTOR;
		if (!(flags & DMSS_AFFECTARMOR) || (flags & DMSS_KILL)) //Kill overrides AffectArmor
			dmgFlags |= DMG_NO_ARMOR;
		if (flags & DMSS_KILL) //Kill adds the value of the damage done to it. Allows for more controlled extreme death types.
			amount += dmgtarget->health;
		if (flags & DMSS_NOPROTECT) //Ignore PowerProtection.
			dmgFlags |= DMG_NO_PROTECT;
	
		if (amount > 0)
		{ //Should wind up passing them through just fine.
			if (flags & DMSS_INFLICTORDMGTYPE)
				DamageType = inflictor->DamageType;

			P_DamageMobj(dmgtarget, inflictor, source, amount, DamageType, dmgFlags);
		}
		else if (amount < 0)
		{
			amount = -amount;
			P_GiveBody(dmgtarget, amount);
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_DamageSelf)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(amount);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src);
	AActor *inflictor = COPY_AAPTR(self, inflict);

	DoDamage(self, inflictor, source, amount, damagetype, flags, filter, species);
	return 0;
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_DamageTarget)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(amount);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src);
	AActor *inflictor = COPY_AAPTR(self, inflict);

	if (self->target != NULL)
		DoDamage(self->target, inflictor, source, amount, damagetype, flags, filter, species);
	return 0;
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_DamageTracer)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(amount);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src);
	AActor *inflictor = COPY_AAPTR(self, inflict);

	if (self->tracer != NULL)
		DoDamage(self->tracer, inflictor, source, amount, damagetype, flags, filter, species);
	return 0;
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_DamageMaster)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(amount);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src);
	AActor *inflictor = COPY_AAPTR(self, inflict);

	if (self->master != NULL)
		DoDamage(self->master, inflictor, source, amount, damagetype, flags, filter, species);
	return 0;
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_DamageChildren)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(amount);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src);
	AActor *inflictor = COPY_AAPTR(self, inflict);

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self)
			DoDamage(mo, inflictor, source, amount, damagetype, flags, filter, species);
	}
	return 0;
}

//===========================================================================
//
//
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_DamageSiblings)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(amount);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src);
	AActor *inflictor = COPY_AAPTR(self, inflict);

	TThinkerIterator<AActor> it;
	AActor *mo;

	if (self->master != NULL)
	{
		while ((mo = it.Next()))
		{
			if (mo->master == self->master && mo != self)
				DoDamage(mo, inflictor, source, amount, damagetype, flags, filter, species);
		}
	}
	return 0;
}


//===========================================================================
//
// A_Kill*(damagetype, int flags)
//
//===========================================================================
enum KILS
{
	KILS_FOILINVUL		= 1 << 0,
	KILS_KILLMISSILES	= 1 << 1,
	KILS_NOMONSTERS		= 1 << 2,
	KILS_FOILBUDDHA		= 1 << 3,
	KILS_EXFILTER		= 1 << 4,
	KILS_EXSPECIES		= 1 << 5,
	KILS_EITHER			= 1 << 6,
};

static void DoKill(AActor *killtarget, AActor *inflictor, AActor *source, FName damagetype, int flags, PClassActor *filter, FName species)
{
	bool filterpass = DoCheckClass(killtarget, filter, !!(flags & KILS_EXFILTER)),
		speciespass = DoCheckSpecies(killtarget, species, !!(flags & KILS_EXSPECIES));
	if ((flags & KILS_EITHER) ? (filterpass || speciespass) : (filterpass && speciespass)) //Check this first. I think it'll save the engine a lot more time this way.
	{
		int dmgFlags = DMG_NO_ARMOR | DMG_NO_FACTOR;

		if (flags & KILS_FOILINVUL)
			dmgFlags |= DMG_FOILINVUL;
		if (flags & KILS_FOILBUDDHA)
			dmgFlags |= DMG_FOILBUDDHA;
	
		if ((killtarget->flags & MF_MISSILE) && (flags & KILS_KILLMISSILES))
		{
			//[MC] Now that missiles can set masters, lets put in a check to properly destroy projectiles. BUT FIRST! New feature~!
			//Check to see if it's invulnerable. Disregarded if foilinvul is on, but never works on a missile with NODAMAGE
			//since that's the whole point of it.
			if ((!(killtarget->flags2 & MF2_INVULNERABLE) || (flags & KILS_FOILINVUL)) &&
				(!(killtarget->flags7 & MF7_BUDDHA) || (flags & KILS_FOILBUDDHA)) && 
				!(killtarget->flags5 & MF5_NODAMAGE))
			{
				P_ExplodeMissile(killtarget, NULL, NULL);
			}
		}
		if (!(flags & KILS_NOMONSTERS))
		{
			P_DamageMobj(killtarget, inflictor, source, killtarget->health, damagetype, dmgFlags);
		}
	}
}


//===========================================================================
//
// A_KillTarget(damagetype, int flags)
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillTarget)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src);
	AActor *inflictor = COPY_AAPTR(self, inflict);

	if (self->target != NULL)
		DoKill(self->target, inflictor, source, damagetype, flags, filter, species);
	return 0;
}

//===========================================================================
//
// A_KillTracer(damagetype, int flags)
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillTracer)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src);
	AActor *inflictor = COPY_AAPTR(self, inflict);

	if (self->tracer != NULL)
		DoKill(self->tracer, inflictor, source, damagetype, flags, filter, species);
	return 0;
}

//===========================================================================
//
// A_KillMaster(damagetype, int flags)
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillMaster)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src); 
	AActor *inflictor = COPY_AAPTR(self, inflict);

	if (self->master != NULL)
		DoKill(self->master, inflictor, source, damagetype, flags, filter, species);
	return 0;
}

//===========================================================================
//
// A_KillChildren(damagetype, int flags)
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillChildren)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src);
	AActor *inflictor = COPY_AAPTR(self, inflict);

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ( (mo = it.Next()) )
	{
		if (mo->master == self) 
		{
			DoKill(mo, inflictor, source, damagetype, flags, filter, species);
		}
	}
	return 0;
}

//===========================================================================
//
// A_KillSiblings(damagetype, int flags)
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_KillSiblings)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME_DEF	(damagetype)	
	PARAM_INT_DEF	(flags)			
	PARAM_CLASS_DEF	(filter, AActor)
	PARAM_NAME_DEF	(species)		
	PARAM_INT_DEF	(src)			
	PARAM_INT_DEF	(inflict)		

	AActor *source = COPY_AAPTR(self, src);
	AActor *inflictor = COPY_AAPTR(self, inflict);

	TThinkerIterator<AActor> it;
	AActor *mo;

	if (self->master != NULL)
	{
		while ( (mo = it.Next()) )
		{
			if (mo->master == self->master && mo != self)
			{ 
				DoKill(mo, inflictor, source, damagetype, flags, filter, species);
			}
		}
	}
	return 0;
}

//===========================================================================
//
// DoRemove
//
//===========================================================================

enum RMVF_flags
{
	RMVF_MISSILES		= 1 << 0,
	RMVF_NOMONSTERS		= 1 << 1,
	RMVF_MISC			= 1 << 2,
	RMVF_EVERYTHING		= 1 << 3,
	RMVF_EXFILTER		= 1 << 4,
	RMVF_EXSPECIES		= 1 << 5,
	RMVF_EITHER			= 1 << 6,
};

static void DoRemove(AActor *removetarget, int flags, PClassActor *filter, FName species)
{
	bool filterpass = DoCheckClass(removetarget, filter, !!(flags & RMVF_EXFILTER)),
		speciespass = DoCheckSpecies(removetarget, species, !!(flags & RMVF_EXSPECIES));
	if ((flags & RMVF_EITHER) ? (filterpass || speciespass) : (filterpass && speciespass))
	{
		if ((flags & RMVF_EVERYTHING))
		{
			P_RemoveThing(removetarget);
		}
		if ((flags & RMVF_MISC) && !((removetarget->flags3 & MF3_ISMONSTER) && (removetarget->flags & MF_MISSILE)))
		{
			P_RemoveThing(removetarget);
		}
		if ((removetarget->flags3 & MF3_ISMONSTER) && !(flags & RMVF_NOMONSTERS))
		{
			P_RemoveThing(removetarget);
		}
		if ((removetarget->flags & MF_MISSILE) && (flags & RMVF_MISSILES))
		{
			P_RemoveThing(removetarget);
		}
	}
}

//===========================================================================
//
// A_RemoveTarget
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RemoveTarget)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(flags);
	PARAM_CLASS_DEF(filter, AActor);
	PARAM_NAME_DEF(species);

	if (self->target != NULL)
	{
		DoRemove(self->target, flags, filter, species);
	}
	return 0;
}

//===========================================================================
//
// A_RemoveTracer
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RemoveTracer)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(flags);
	PARAM_CLASS_DEF(filter, AActor);
	PARAM_NAME_DEF(species);

	if (self->tracer != NULL)
	{
		DoRemove(self->tracer, flags, filter, species);
	}
	return 0;
}

//===========================================================================
//
// A_RemoveMaster
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RemoveMaster)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(flags);
	PARAM_CLASS_DEF(filter, AActor);
	PARAM_NAME_DEF(species);

	if (self->master != NULL)
	{
		DoRemove(self->master, flags, filter, species);
	}
	return 0;
}

//===========================================================================
//
// A_RemoveChildren
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RemoveChildren)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_BOOL_DEF(removeall);
	PARAM_INT_DEF(flags);
	PARAM_CLASS_DEF(filter, AActor);
	PARAM_NAME_DEF(species);

	TThinkerIterator<AActor> it;
	AActor *mo;

	while ((mo = it.Next()) != NULL)
	{
		if (mo->master == self && (mo->health <= 0 || removeall))
		{
			DoRemove(mo, flags, filter, species);
		}
	}
	return 0;
}

//===========================================================================
//
// A_RemoveSiblings
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_RemoveSiblings)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_BOOL_DEF(removeall);
	PARAM_INT_DEF(flags);
	PARAM_CLASS_DEF(filter, AActor);
	PARAM_NAME_DEF(species);

	TThinkerIterator<AActor> it;
	AActor *mo;

	if (self->master != NULL)
	{
		while ((mo = it.Next()) != NULL)
		{
			if (mo->master == self->master && mo != self && (mo->health <= 0 || removeall))
			{
				DoRemove(mo, flags, filter, species);
			}
		}
	}
	return 0;
}

//===========================================================================
//
// A_Remove
//
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_Remove)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(removee);
	PARAM_INT_DEF(flags);
	PARAM_CLASS_DEF(filter, AActor);
	PARAM_NAME_DEF(species);

	AActor *reference = COPY_AAPTR(self, removee);
	if (reference != NULL)
	{
		DoRemove(reference, flags, filter, species);
	}
	return 0;
}

//===========================================================================
//
// A_SetTeleFog
//
// Sets the teleport fog(s) for the calling actor.
// Takes a name of the classes for the source and destination.
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SetTeleFog)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS(oldpos, AActor);
	PARAM_CLASS(newpos, AActor);

	self->TeleFogSourceType = oldpos;
	self->TeleFogDestType = newpos;
	return 0;
}

//===========================================================================
//
// A_SwapTeleFog
//
// Switches the source and dest telefogs around. 
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SwapTeleFog)
{
	PARAM_SELF_PROLOGUE(AActor);
	if ((self->TeleFogSourceType != self->TeleFogDestType)) //Does nothing if they're the same.
	{
		PClassActor *temp = self->TeleFogSourceType;
		self->TeleFogSourceType = self->TeleFogDestType;
		self->TeleFogDestType = temp;
	}
	return 0;
}

//===========================================================================
//
// A_SetFloatBobPhase
//
// Changes the FloatBobPhase of the actor.
//===========================================================================


//===========================================================================
// A_SetHealth
//
// Changes the health of the actor.
// Takes a pointer as well.
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SetHealth)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT		(health);
	PARAM_INT_DEF	(ptr);

	AActor *mobj = COPY_AAPTR(self, ptr);

	if (!mobj)
	{
		return 0;
	}

	player_t *player = mobj->player;
	if (player)
	{
		if (health <= 0)
			player->mo->health = mobj->health = player->health = 1; //Copied from the buddha cheat.
		else
			player->mo->health = mobj->health = player->health = health;
	}
	else if (mobj)
	{
		if (health <= 0)
			mobj->health = 1;
		else
			mobj->health = health;
	}
	return 0;
}

//===========================================================================
// A_ResetHealth
//
// Resets the health of the actor to default, except if their dead.
// Takes a pointer.
//===========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ResetHealth)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(ptr);

	AActor *mobj = COPY_AAPTR(self, ptr);

	if (!mobj)
	{
		return 0;
	}

	player_t *player = mobj->player;
	if (player && (player->mo->health > 0))
	{
		player->health = player->mo->health = player->mo->GetDefault()->health; //Copied from the resurrect cheat.
	}
	else if (mobj && (mobj->health > 0))
	{
		mobj->health = mobj->SpawnHealth();
	}
	return 0;
}

//===========================================================================
// A_SetSpecies(str species, ptr)
//
// Sets the species of the calling actor('s pointer).
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_SetSpecies)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME(species);
	PARAM_INT_DEF(ptr);

	AActor *mobj = COPY_AAPTR(self, ptr);
	if (!mobj)
	{
		return 0;
	}

	mobj->Species = species;
	return 0;
}

//===========================================================================
//
// A_SetChaseThreshold(int threshold, bool def, int ptr)
//
// Sets the current chase threshold of the actor (pointer). If def is true,
// changes the default threshold which the actor resets to once it switches
// targets and doesn't have the +QUICKTORETALIATE flag.
//===========================================================================
DEFINE_ACTION_FUNCTION(AActor, A_SetChaseThreshold)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(threshold);
	PARAM_BOOL_DEF(def);
	PARAM_INT_DEF(ptr);


	AActor *mobj = COPY_AAPTR(self, ptr);
	if (!mobj)
	{
		return 0;
	}
	if (def)
		mobj->DefThreshold = (threshold >= 0) ? threshold : 0;
	else
		mobj->threshold = (threshold >= 0) ? threshold : 0;
	return 0;
}

//==========================================================================
//
// A_CheckProximity(jump, classname, distance, count, flags, ptr)
//
// Checks to see if a certain actor class is close to the 
// actor/pointer within distance, in numbers.
//==========================================================================
DEFINE_ACTION_FUNCTION(AActor, CheckProximity)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_CLASS(classname, AActor);
	PARAM_FLOAT(distance);
	PARAM_INT_DEF(count);
	PARAM_INT_DEF(flags);
	PARAM_INT_DEF(ptr);

	ACTION_RETURN_BOOL(!!P_Thing_CheckProximity(self, classname, distance, count, flags, ptr));
}

/*===========================================================================
A_CheckBlock
(state block, int flags, int ptr)

Checks if something is blocking the actor('s pointer) 'ptr'.

The SET pointer flags only affect the caller, not the pointer.
===========================================================================*/
enum CBF
{
	CBF_NOLINES			= 1 << 0,	//Don't check lines.
	CBF_SETTARGET		= 1 << 1,	//Sets the caller/pointer's target to the actor blocking it. Actors only.
	CBF_SETMASTER		= 1 << 2,	//^ but with master.
	CBF_SETTRACER		= 1 << 3,	//^ but with tracer.
	CBF_SETONPTR		= 1 << 4,	//Sets the pointer change on the actor doing the checking instead of self.
	CBF_DROPOFF			= 1 << 5,	//Check for dropoffs.
	CBF_NOACTORS		= 1 << 6,	//Don't check actors.
	CBF_ABSOLUTEPOS		= 1 << 7,	//Absolute position for offsets.
	CBF_ABSOLUTEANGLE	= 1 << 8,	//Absolute angle for offsets.
};

DEFINE_ACTION_FUNCTION(AActor, CheckBlock)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT_DEF(flags)	
	PARAM_INT_DEF(ptr)		
	PARAM_FLOAT_DEF(xofs)	
	PARAM_FLOAT_DEF(yofs)	
	PARAM_FLOAT_DEF(zofs)	
	PARAM_ANGLE_DEF(angle)	

	AActor *mobj = COPY_AAPTR(self, ptr);

	//Needs at least one state jump to work. 
	if (!mobj)
	{
		ACTION_RETURN_BOOL(false);
	}

	if (!(flags & CBF_ABSOLUTEANGLE))
	{
		angle += self->Angles.Yaw;
	}

	DVector3 oldpos = mobj->Pos();
	DVector3 pos;

	if (flags & CBF_ABSOLUTEPOS)
	{
		pos = { xofs, yofs, zofs };
	}
	else
	{
		double s = angle.Sin();
		double c = angle.Cos();
		pos = mobj->Vec3Offset(xofs * c + yofs * s, xofs * s - yofs * c, zofs);
	}
	
	// Next, try checking the position based on the sensitivity desired.
	// If checking for dropoffs, set the z so we can have maximum flexibility.
	// Otherwise, set origin and set it back after testing.

	bool checker = false;
	if (flags & CBF_DROPOFF)
	{
		// Unfortunately, whenever P_CheckMove returned false, that means it could
		// ignore a variety of flags mainly because of P_CheckPosition. This
		// results in picking up false positives due to actors or lines being in the way
		// when they clearly should not be.

		int fpass = PCM_DROPOFF;
		if (flags & CBF_NOACTORS)	fpass |= PCM_NOACTORS;
		if (flags & CBF_NOLINES)	fpass |= PCM_NOLINES;
		mobj->SetZ(pos.Z);
		checker = P_CheckMove(mobj, pos, fpass);
		mobj->SetZ(oldpos.Z);
	}
	else
	{
		mobj->SetOrigin(pos, true);
		checker = P_TestMobjLocation(mobj);
		mobj->SetOrigin(oldpos, true);
	}
	
	if (checker)
	{
		ACTION_RETURN_BOOL(false);
	}

	if (mobj->BlockingMobj)
	{
		AActor *setter = (flags & CBF_SETONPTR) ? mobj : self;
		if (setter)
		{
			if (flags & CBF_SETTARGET)	setter->target = mobj->BlockingMobj;
			if (flags & CBF_SETMASTER)	setter->master = mobj->BlockingMobj;
			if (flags & CBF_SETTRACER)	setter->tracer = mobj->BlockingMobj;
		}
	}

	//[MC] I don't know why I let myself be persuaded not to include a flag.
	//If an actor is loaded with pointers, they don't really have any options to spare.
	//Also, fail if a dropoff or a step is too great to pass over when checking for dropoffs.
	
	ACTION_RETURN_BOOL((!(flags & CBF_NOACTORS) && (mobj->BlockingMobj)) || (!(flags & CBF_NOLINES) && mobj->BlockingLine != NULL) ||
		((flags & CBF_DROPOFF) && !checker));
}

//===========================================================================
//
// A_FaceMovementDirection(angle offset, bool pitch, ptr)
//
// Sets the actor('s pointer) to face the direction of travel.
//===========================================================================
enum FMDFlags
{
	FMDF_NOPITCH =			1 << 0,
	FMDF_INTERPOLATE =		1 << 1,
	FMDF_NOANGLE =			1 << 2,
};
DEFINE_ACTION_FUNCTION(AActor, A_FaceMovementDirection)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_ANGLE_DEF(offset)		
	PARAM_ANGLE_DEF(anglelimit)	
	PARAM_ANGLE_DEF(pitchlimit)	
	PARAM_INT_DEF(flags)		
	PARAM_INT_DEF(ptr)			

	AActor *mobj = COPY_AAPTR(self, ptr);

	//Need an actor.
	if (!mobj || ((flags & FMDF_NOPITCH) && (flags & FMDF_NOANGLE)))
	{
		ACTION_RETURN_BOOL(false);
	}

	//Don't bother calculating this if we don't have any horizontal movement.
	if (!(flags & FMDF_NOANGLE) && (mobj->Vel.X != 0 || mobj->Vel.Y != 0))
	{
		DAngle current = mobj->Angles.Yaw;
		DAngle angle = mobj->Vel.Angle();
		//Done because using anglelimit directly causes a signed/unsigned mismatch.

		//Code borrowed from A_Face*.
		if (anglelimit > 0)
		{
			DAngle delta = -deltaangle(current, angle);
			if (fabs(delta) > anglelimit)
			{
				if (delta < 0)
				{
					current += anglelimit + offset;
				}
				else if (delta > 0)
				{
					current -= anglelimit + offset;
				}
				mobj->SetAngle(current, !!(flags & FMDF_INTERPOLATE));
			}
			else
				mobj->SetAngle(angle + offset, !!(flags & FMDF_INTERPOLATE));
		}
		else
			mobj->SetAngle(angle + offset, !!(flags & FMDF_INTERPOLATE));
	}

	if (!(flags & FMDF_NOPITCH))
	{
		DAngle current = mobj->Angles.Pitch;
		const DVector2 velocity = mobj->Vel.XY();
		DAngle pitch = -VecToAngle(velocity.Length(), mobj->Vel.Z);
		if (pitchlimit > 0)
		{
			DAngle pdelta = deltaangle(current, pitch);

			if (fabs(pdelta) > pitchlimit)
			{
				if (pdelta > 0)
				{
					current -= MIN(pitchlimit, pdelta);
				}
				else //if (pdelta < 0)
				{
					current += MIN(pitchlimit, -pdelta);
				}
				mobj->SetPitch(current, !!(flags & FMDF_INTERPOLATE));
			}
			else
			{
				mobj->SetPitch(pitch, !!(flags & FMDF_INTERPOLATE));
			}

		}
		else
		{
			mobj->SetPitch(pitch, !!(flags & FMDF_INTERPOLATE));
		}
	}
	ACTION_RETURN_BOOL(true);
}

//==========================================================================
//
// A_CopySpriteFrame(from, to, flags)
//
// Copies the sprite and/or frame from one pointer to another.
//==========================================================================
enum CPSFFlags
{
	CPSF_NOSPRITE =			1,
	CPSF_NOFRAME =			1 << 1,
};

DEFINE_ACTION_FUNCTION(AActor, A_CopySpriteFrame)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(from);
	PARAM_INT(to);
	PARAM_INT_DEF(flags);

	AActor *copyfrom = COPY_AAPTR(self, from);
	AActor *copyto = COPY_AAPTR(self, to);

	if (copyfrom == copyto || copyfrom == nullptr || copyto == nullptr || ((flags & CPSF_NOSPRITE) && (flags & CPSF_NOFRAME)))
	{
		ACTION_RETURN_BOOL(false);
	}
	
	if (!(flags & CPSF_NOSPRITE))	copyto->sprite = copyfrom->sprite;
	if (!(flags & CPSF_NOFRAME))	copyto->frame = copyfrom->frame;
	ACTION_RETURN_BOOL(true);
}

//==========================================================================
//
// A_SetMaskRotation(anglestart, angleend, pitchstart, pitchend, flags, ptr)
//
// Specifies how much to fake a sprite rotation.
//==========================================================================

enum VRFFlags
{
	VRF_NOANGLESTART =		1,
	VRF_NOANGLEEND =		1 << 1,
	VRF_NOPITCHSTART =		1 << 2,
	VRF_NOPITCHEND =		1 << 3,
};

DEFINE_ACTION_FUNCTION(AActor, A_SetVisibleRotation)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_ANGLE_DEF(anglestart)
	PARAM_ANGLE_DEF(angleend)	
	PARAM_ANGLE_DEF(pitchstart)
	PARAM_ANGLE_DEF(pitchend)	
	PARAM_INT_DEF(flags)		
	PARAM_INT_DEF(ptr)			

	AActor *mobj = COPY_AAPTR(self, ptr);

	if (mobj == nullptr)
	{
		ACTION_RETURN_BOOL(false);
	}
		
	if (!(flags & VRF_NOANGLESTART))
	{
		mobj->VisibleStartAngle = anglestart;
	}
	if (!(flags & VRF_NOANGLEEND))
	{
		mobj->VisibleEndAngle = angleend;
	}
	if (!(flags & VRF_NOPITCHSTART))
	{
		mobj->VisibleStartPitch = pitchstart;
	}
	if (!(flags & VRF_NOPITCHEND))
	{
		mobj->VisibleEndPitch = pitchend;
	}

	ACTION_RETURN_BOOL(true);
}

//==========================================================================
//
//
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SetTranslation)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME(trname);

	self->SetTranslation(trname);
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_CheckTerrain)
{
	PARAM_SELF_PROLOGUE(AActor);

	sector_t *sec = self->Sector;

	if (self->Z() == sec->floorplane.ZatPoint(self) && sec->PortalBlocksMovement(sector_t::floor))
	{
		if (sec->special == Damage_InstantDeath)
		{
			P_DamageMobj(self, NULL, NULL, 999, NAME_InstantDeath);
		}
		else if (sec->special == Scroll_StrifeCurrent)
		{
			int anglespeed = tagManager.GetFirstSectorTag(sec) - 100;
			double speed = (anglespeed % 10) / 16.;
			DAngle an = (anglespeed / 10) * (360 / 8.);
			self->Thrust(an, speed);
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SetSize)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT_DEF(newradius);
	PARAM_FLOAT_DEF(newheight);
	PARAM_BOOL_DEF(testpos);

	if (newradius < 0.)		newradius = self->radius;
	if (newheight < 0.)		newheight = self->Height;

	double oldradius = self->radius;
	double oldheight = self->Height;

	FLinkContext ctx;
	self->UnlinkFromWorld(&ctx);
	self->radius = newradius;
	self->Height = newheight;
	self->LinkToWorld(&ctx);

	if (testpos && !P_TestMobjLocation(self))
	{
		self->UnlinkFromWorld(&ctx);
		self->radius = oldradius;
		self->Height = oldheight;
		self->LinkToWorld(&ctx);
		ACTION_RETURN_BOOL(false);
	}
	if (self->player && self->player->mo == self)
	{
		self->player->mo->FullHeight = newheight;
	}

	ACTION_RETURN_BOOL(true);
}

DEFINE_ACTION_FUNCTION(AActor, SetCamera)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_OBJECT(cam, AActor);
	PARAM_BOOL_DEF(revert);

	if (self->player == nullptr || self->player->mo != self) return 0;

	if (cam == nullptr)
	{
		cam = self;
		revert = false;
	}
	AActor *oldcamera = self->player->camera;
	self->player->camera = cam;
	if (revert) self->player->cheats |= CF_REVERTPLEASE;

	if (oldcamera != cam)
	{
		R_ClearPastViewer(cam);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SprayDecal)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_STRING(name);
	PARAM_FLOAT_DEF(dist);
	SprayDecal(self, name, dist);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SetMugshotState)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_STRING(name);
	if (self->CheckLocalView(consoleplayer))
		StatusBar->SetMugShotState(name);
	return 0;
}

// This needs to account for the fact that internally renderstyles are stored as a series of operations, 
// but the script side only cares about symbolic constants.
DEFINE_ACTION_FUNCTION(AActor, GetRenderStyle)
{
	PARAM_SELF_PROLOGUE(AActor);
	for(unsigned i=0;i<STYLE_Count;i++)
	{
		if (self->RenderStyle == LegacyRenderStyles[i]) ACTION_RETURN_INT(i);
	}
	ACTION_RETURN_INT(-1);	// no symbolic constant exists to handle this style.
}
