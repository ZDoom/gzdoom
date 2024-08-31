/*
** p_things.cpp
** ACS-accessible thing utilities
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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

#include "doomtype.h"
#include "p_local.h"
#include "info.h"
#include "doomstat.h"
#include "c_dispatch.h"
#include "a_sharedglobal.h"
#include "v_text.h"
#include "d_player.h"
#include "r_utility.h"
#include "p_spec.h"
#include "actorptrselect.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "vm.h"

static FRandom pr_leadtarget ("LeadTarget");

bool FLevelLocals::EV_Thing_Spawn (int tid, AActor *source, int type, DAngle angle, bool fog, int newtid)
{
	int rtn = 0;
	PClassActor *kind;
	AActor *spot, *mobj;
	auto iterator = GetActorIterator(tid);

	kind = P_GetSpawnableType(type);

	if (kind == NULL)
		return false;

	// Handle decorate replacements.
	kind = kind->GetReplacement(this);

	if ((GetDefaultByType(kind)->flags3 & MF3_ISMONSTER) && 
		((dmflags & DF_NO_MONSTERS) || (flags2 & LEVEL2_NOMONSTERS)))
		return false;

	if (tid == 0)
	{
		spot = source;
	}
	else
	{
		spot = iterator.Next();
	}
	while (spot != NULL)
	{
		mobj = Spawn (spot->Level, kind, spot->Pos(), ALLOW_REPLACE);

		if (mobj != NULL)
		{
			ActorFlags2 oldFlags2 = mobj->flags2;
			mobj->flags2 |= MF2_PASSMOBJ;
			if (P_TestMobjLocation (mobj))
			{
				rtn++;
				mobj->Angles.Yaw = (angle != DAngle::fromDeg(1000000.) ? angle : spot->Angles.Yaw);
				if (fog)
				{
					P_SpawnTeleportFog(mobj, spot->Pos(), false, true);
				}
				if (mobj->flags & MF_SPECIAL)
					mobj->flags |= MF_DROPPED;	// Don't respawn
				mobj->SetTID(newtid);
				mobj->flags2 = oldFlags2;
			}
			else
			{
				// If this is a monster, subtract it from the total monster
				// count, because it already added to it during spawning.
				mobj->ClearCounters();
				mobj->Destroy ();
			}
		}
		spot = iterator.Next();
	}

	return rtn != 0;
}

// [BC] Added
// [RH] Fixed

bool P_MoveThing(AActor *source, const DVector3 &pos, bool fog)
{
	DVector3 old = source->Pos();

	source->SetOrigin (pos, true);
	if (P_TestMobjLocation (source))
	{
		if (fog)
		{
			P_SpawnTeleportFog(source, pos, false, true);
			P_SpawnTeleportFog(source, old, true, true);
		}
		source->ClearInterpolation();
		source->renderflags |= RF_NOINTERPOLATEVIEW;
		return true;
	}
	else
	{
		source->SetOrigin (old, true);
		return false;
	}
}

bool FLevelLocals::EV_Thing_Move (int tid, AActor *source, int mapspot, bool fog)
{
	AActor *target;

	if (tid != 0)
	{
		auto iterator1 = GetActorIterator(tid);
		source = iterator1.Next();
	}
	auto iterator2 = GetActorIterator(mapspot);
	target = iterator2.Next ();

	if (source != NULL && target != NULL)
	{
		return P_MoveThing(source, target->Pos(), fog);
	}
	return false;
}

//==========================================================================
//
// VelIntercept
//
//==========================================================================

void InterceptDefaultAim(AActor *mobj, AActor *targ, DVector3 aim, double speed)
{
	if (mobj == nullptr || targ == nullptr)	return;
	mobj->Angles.Yaw = mobj->AngleTo(targ);
	mobj->Vel = aim.Resized(speed);
}

// [MC] Was part of P_Thing_Projectile, now its own function for use in ZScript.
// Aims mobj at targ based on speed and targ's velocity.
static void VelIntercept(AActor *targ, AActor *mobj, double speed, bool aimpitch = false, bool oldvel = false, bool resetvel = false, bool leadtarget = true)
{
	if (targ == nullptr || mobj == nullptr)	return;

	DVector3 prevel = mobj->Vel;
	DVector3 aim = mobj->Vec3To(targ);
	aim.Z += targ->Height / 2;

	if (leadtarget && speed > 0 && !targ->Vel.isZero())
	{
		// Aiming at the target's position some time in the future
		// is basically just an application of the law of sines:
		//     a/sin(A) = b/sin(B)
		// Thanks to all those on the notgod phorum for helping me
		// with the math. I don't think I would have thought of using
		// trig alone had I been left to solve it by myself.

		DVector3 tvel = targ->Vel;
		if (!(targ->flags & MF_NOGRAVITY) && targ->waterlevel < 3)
		{ // If the target is subject to gravity and not underwater,
		  // assume that it isn't moving vertically. Thanks to gravity,
		  // even if we did consider the vertical component of the target's
		  // velocity, we would still miss more often than not.
			tvel.Z = 0.0;

			if (targ->Vel.X == 0 && targ->Vel.Y == 0)
			{
				InterceptDefaultAim(mobj, targ, aim, speed);
				if (resetvel)
				{
					mobj->Vel = prevel;
				}
				return;
			}
		}
		double dist = aim.Length();
		double targspeed = tvel.Length();
		double ydotx = -aim | tvel;
		double a = g_acos(clamp(ydotx / targspeed / dist, -1.0, 1.0));
		double multiplier = double(pr_leadtarget.Random2())*0.1 / 255 + 1.1;
		double sinb = -clamp(targspeed*multiplier * g_sin(a) / speed, -1.0, 1.0);
		// Use the cross product of two of the triangle's sides to get a
		// rotation vector.
		DVector3 rv(tvel ^ aim);
		// The vector must be normalized.
		rv.MakeUnit();
		// Now combine the rotation vector with angle b to get a rotation matrix.
		DMatrix3x3 rm(rv, g_cos(g_asin(sinb)), sinb);
		// And multiply the original aim vector with the matrix to get a
		// new aim vector that leads the target.
		DVector3 aimvec = rm * aim;
		// And make the projectile follow that vector at the desired speed.
		mobj->Vel = aimvec * (speed / dist);
		mobj->AngleFromVel();
		if (oldvel)
		{
			mobj->Vel = prevel;
		}
		if (aimpitch) // [MC] Ripped right out of A_FaceMovementDirection
		{
			const DVector2 velocity = mobj->Vel.XY();
			mobj->Angles.Pitch = -VecToAngle(velocity.Length(), mobj->Vel.Z);
		}
	}
	else
	{
		InterceptDefaultAim(mobj, targ, aim, speed);
	}
	if (resetvel)
	{
		mobj->Vel = prevel;
	}
}

DEFINE_ACTION_FUNCTION(AActor, VelIntercept)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(targ, AActor);
	PARAM_FLOAT(speed);
	PARAM_BOOL(aimpitch);
	PARAM_BOOL(oldvel);
	PARAM_BOOL(resetvel);
	if (speed < 0)	speed = self->Speed;
	VelIntercept(targ, self, speed, aimpitch, oldvel, resetvel);
	return 0;
}

bool FLevelLocals::EV_Thing_Projectile (int tid, AActor *source, int type, const char *type_name, DAngle angle,
	double speed, double vspeed, int dest, AActor *forcedest, int gravity, int newtid,
	bool leadTarget)
{
	int rtn = 0;
	PClassActor *kind;
	AActor *spot, *mobj, *targ = forcedest;
	auto iterator = GetActorIterator(tid);
	int defflags3;

	if (type_name == NULL)
	{
		kind = P_GetSpawnableType(type);
	}
	else
	{
		kind = PClass::FindActor(type_name);
	}
	if (kind == NULL)
	{
		return false;
	}

	// Handle decorate replacements.
	kind = kind->GetReplacement(this);

	defflags3 = GetDefaultByType(kind)->flags3;
	if ((defflags3 & MF3_ISMONSTER) && 
		((dmflags & DF_NO_MONSTERS) || (flags2 & LEVEL2_NOMONSTERS)))
		return false;

	if (tid == 0)
	{
		spot = source;
	}
	else
	{
		spot = iterator.Next();
	}
	while (spot != NULL)
	{
		auto tit = GetActorIterator(dest);

		if (dest == 0 || (targ = tit.Next()))
		{
			do
			{
				double z = spot->Z();
				if (defflags3 & MF3_FLOORHUGGER)
				{
					z = ONFLOORZ;
				}
				else if (defflags3 & MF3_CEILINGHUGGER)
				{
					z = ONCEILINGZ;
				}
				else if (z != ONFLOORZ)
				{
					z -= spot->Floorclip;
				}
				mobj = Spawn (spot->Level, kind, spot->PosAtZ(z), ALLOW_REPLACE);

				if (mobj)
				{
					mobj->SetTID(newtid);
					P_PlaySpawnSound(mobj, spot);
					if (gravity)
					{
						mobj->flags &= ~MF_NOGRAVITY;
						if (!(mobj->flags3 & MF3_ISMONSTER) && gravity == 1)
						{
							mobj->Gravity = 1./8;
						}
					}
					else
					{
						mobj->flags |= MF_NOGRAVITY;
					}
					mobj->target = spot;

					if (targ != nullptr)
					{
						VelIntercept(targ, mobj, speed, false, false, false, leadTarget);

						if (mobj->flags2 & MF2_SEEKERMISSILE)
						{
							mobj->tracer = targ;
						}
					}
					else
					{
						mobj->Angles.Yaw = angle;
						mobj->VelFromAngle(speed);
						mobj->Vel.Z = vspeed;
					}
					// Set the missile's speed to reflect the speed it was spawned at.
					if (mobj->flags & MF_MISSILE)
					{
						mobj->Speed = mobj->VelToSpeed();
					}
					// Hugger missiles don't have any vertical velocity
					if (mobj->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER))
					{
						mobj->Vel.Z = 0;
					}
					if (mobj->flags & MF_SPECIAL)
					{
						mobj->flags |= MF_DROPPED;
					}
					if (mobj->flags & MF_MISSILE)
					{
						if (P_CheckMissileSpawn (mobj, spot->radius))
						{
							rtn = true;
						}
					}
					else if (!P_TestMobjLocation (mobj))
					{
						// If this is a monster, subtract it from the total monster
						// count, because it already added to it during spawning.
						mobj->ClearCounters();
						mobj->Destroy ();
					}
					else
					{
						// It spawned fine.
						rtn = 1;
					}
				}
			} while (dest != 0 && (targ = tit.Next()));
		}
		spot = iterator.Next();
	}

	return rtn != 0;
}

int FLevelLocals::EV_Thing_Damage (int tid, AActor *whofor0, int amount, FName type)
{
	auto iterator = GetActorIterator(tid);
	int count = 0;
	AActor *actor;

	actor = (tid == 0 ? whofor0 : iterator.Next());
	while (actor)
	{
		AActor *next = tid == 0 ? NULL : iterator.Next ();
		if (actor->flags & MF_SHOOTABLE)
		{
			if (amount > 0)
			{
				P_DamageMobj (actor, NULL, whofor0, amount, type);
			}
			else if (actor->health < actor->SpawnHealth())
			{
				actor->health -= amount;
				if (actor->health > actor->SpawnHealth())
				{
					actor->health = actor->SpawnHealth();
				}
				if (actor->player != NULL)
				{
					actor->player->health = actor->health;
				}
			}
			count++;
		}
		actor = next;
	}
	return count;
}

void P_RemoveThing(AActor * actor)
{
	// Don't remove live players.
	if (actor->player == NULL || actor != actor->player->mo)
	{
		// Don't also remove owned inventory items
		if (!actor->IsMapActor()) return;

		// be friendly to the level statistics. ;)
		actor->ClearCounters();
		actor->Destroy ();
	}

}

bool P_Thing_Raise(AActor *thing, AActor *raiser, int flags)
{
	if (!thing)	
		return false;

	FState * RaiseState = thing->GetRaiseState();
	if (RaiseState == NULL)
	{
		return false;	// monster doesn't have a raise state
	}
	
	AActor *info = thing->GetDefault ();

	thing->Vel.X = thing->Vel.Y = 0;

	// [RH] Check against real height and radius
	double oldheight = thing->Height;
	double oldradius = thing->radius;
	ActorFlags oldflags = thing->flags;

	thing->flags |= MF_SOLID;
	thing->Height = info->Height;	// [RH] Use real height
	thing->radius = info->radius;	// [RH] Use real radius
	if (!(flags & RF_NOCHECKPOSITION) && !P_CheckPosition (thing, thing->Pos().XY()))
	{
		thing->flags = oldflags;
		thing->radius = oldradius;
		thing->Height = oldheight;
		return false;
	}

	if (!P_CanResurrect(raiser, thing))
		return false;

	S_Sound (thing, CHAN_BODY, 0, "vile/raise", 1, ATTN_IDLE);

	thing->Revive();

	if ((flags & RF_TRANSFERFRIENDLINESS) && raiser != nullptr)
	{
		// Let's copy the friendliness of the one who raised it.
		thing->CopyFriendliness(raiser, false);
	}

	thing->SetState (RaiseState);
	return true;
}

bool P_Thing_CanRaise(AActor *thing)
{
	FState * RaiseState = thing->GetRaiseState();
	if (RaiseState == NULL)
	{
		return false;
	}
	
	AActor *info = thing->GetDefault();

	// Check against real height and radius
	ActorFlags oldflags = thing->flags;
	double oldheight = thing->Height;
	double oldradius = thing->radius;

	thing->flags |= MF_SOLID;
	thing->Height = info->Height;
	thing->radius = info->radius;

	bool check = P_CheckPosition (thing, thing->Pos().XY());

	// Restore checked properties
	thing->flags = oldflags;
	thing->radius = oldradius;
	thing->Height = oldheight;

	if (!check)
	{
		return false;
	}

	return true;
}

void P_Thing_SetVelocity(AActor *actor, const DVector3 &vec, bool add, bool setbob)
{
	if (actor != NULL)
	{
		if (!add)
		{
			actor->Vel.Zero();
			if (actor->player != NULL) actor->player->Vel.Zero();
		}
		actor->Vel += vec;
		if (setbob && actor->player != NULL)
		{
			actor->player->Vel += vec.XY();
		}
	}
}

int P_Thing_CheckInputNum(player_t *p, int inputnum)
{
	int renum = 0;
	if (p)
	{
		switch (inputnum)
		{
		case INPUT_OLDBUTTONS:		renum = p->original_oldbuttons;			break;
		case INPUT_BUTTONS:			renum = p->original_cmd.buttons;		break;
		case INPUT_PITCH:			renum = p->original_cmd.pitch;			break;
		case INPUT_YAW:				renum = p->original_cmd.yaw;			break;
		case INPUT_ROLL:			renum = p->original_cmd.roll;			break;
		case INPUT_FORWARDMOVE:		renum = p->original_cmd.forwardmove;	break;
		case INPUT_SIDEMOVE:		renum = p->original_cmd.sidemove;		break;
		case INPUT_UPMOVE:			renum = p->original_cmd.upmove;			break;

		case MODINPUT_OLDBUTTONS:	renum = p->oldbuttons;					break;
		case MODINPUT_BUTTONS:		renum = p->cmd.ucmd.buttons;			break;
		case MODINPUT_PITCH:		renum = p->cmd.ucmd.pitch;				break;
		case MODINPUT_YAW:			renum = p->cmd.ucmd.yaw;				break;
		case MODINPUT_ROLL:			renum = p->cmd.ucmd.roll;				break;
		case MODINPUT_FORWARDMOVE:	renum = p->cmd.ucmd.forwardmove;		break;
		case MODINPUT_SIDEMOVE:		renum = p->cmd.ucmd.sidemove;			break;
		case MODINPUT_UPMOVE:		renum = p->cmd.ucmd.upmove;				break;

		default:					renum = 0;								break;
		}
	}
	return renum;
}
int P_Thing_CheckProximity(FLevelLocals *Level, AActor *self, PClass *classname, double distance, int count, int flags, int ptr, bool counting)
{
	AActor *ref = COPY_AAPTREX(Level, self, ptr);

	// We need these to check out.
	if (!ref || !classname || distance <= 0)
		return 0;
	
	int counter = 0;
	int result = 0;
	double closer = distance, farther = 0, current = distance;
	const bool ptrWillChange = !!(flags & (CPXF_SETTARGET | CPXF_SETMASTER | CPXF_SETTRACER));
	const bool ptrDistPref = !!(flags & (CPXF_CLOSEST | CPXF_FARTHEST));

	auto it = self->Level->GetThinkerIterator<AActor>();
	AActor *mo, *dist = nullptr;

	// [MC] Process of elimination, I think, will get through this as quickly and 
	// efficiently as possible. 
	while ((mo = it.Next()))
	{
		if (mo == ref) //Don't count self.
			continue;

		// no unmorphed versions of currently morphed players.
		if (mo->flags & MF_UNMORPHED)
			continue;

		// Check inheritance for the classname. Taken partly from CheckClass DECORATE function.
		if (flags & CPXF_ANCESTOR)
		{
			if (!(mo->IsKindOf(classname)))
				continue;
		}
		// Otherwise, just check for the regular class name.
		else if (classname != mo->GetClass())
			continue;

		if (!mo->IsMapActor())
		{
			// Skip owned item because its position could remain unchanged since attachment to owner
			// Most likely it is the last location of this item in the world before pick up
			continue;
		}

		// [MC]Make sure it's in range and respect the desire for Z or not. The function forces it to use
		// Z later for ensuring CLOSEST and FARTHEST flags are respected perfectly.
		// Ripped from sphere checking in A_RadiusGive (along with a number of things).
		if ((ref->Distance2D(mo) < distance &&
			((flags & CPXF_NOZ) ||
			((ref->Z() > mo->Z() && ref->Z() - mo->Top() < distance) ||
			(ref->Z() <= mo->Z() && mo->Z() - ref->Top() < distance)))))
		{
			if ((flags & CPXF_CHECKSIGHT) && !(P_CheckSight(mo, ref, SF_IGNOREVISIBILITY | SF_IGNOREWATERBOUNDARY)))
				continue;

			if (mo->flags6 & MF6_KILLED)
			{
				if (!(flags & (CPXF_COUNTDEAD | CPXF_DEADONLY)))
					continue;
			}
			else
			{
				if (flags & CPXF_DEADONLY)
					continue;
			}
			if (ptrWillChange)
			{
				current = ref->Distance2D(mo);

				if ((flags & CPXF_CLOSEST) && (current < closer))
				{
					dist = mo;
					closer = current; // This actor's closer. Set the new standard.
				}
				else if ((flags & CPXF_FARTHEST) && (current > farther))
				{
					dist = mo;
					farther = current;
				}
				else if (!dist)
					dist = mo; // Just get the first one and call it quits if there's nothing selected.
			}
			counter++;

			// Abort if the number of matching classes nearby is greater, we have obviously succeeded in our goal.
			// Don't abort if calling the counting version CheckProximity non-action function.
			if (!counting && counter > count)
			{					
				result = (flags & (CPXF_LESSOREQUAL | CPXF_EXACT)) ? 0 : 1;

				// However, if we have one SET* flag and either the closest or farthest flags, keep the function going.
				if (ptrWillChange && ptrDistPref)
					continue;
				else
					break;
			}
		}
	}

	if (ptrWillChange && dist != 0)
	{
		if (flags & CPXF_SETONPTR)
		{
			if (flags & CPXF_SETTARGET)		ref->target = dist;
			if (flags & CPXF_SETMASTER)		ref->master = dist;
			if (flags & CPXF_SETTRACER)		ref->tracer = dist;
		}
		else
		{
			if (flags & CPXF_SETTARGET)		self->target = dist;
			if (flags & CPXF_SETMASTER)		self->master = dist;
			if (flags & CPXF_SETTRACER)		self->tracer = dist;
		}
	}

	if (!counting)
	{
		if (counter == count)
			result = 1;
		else if (counter < count)
			result = !!((flags & CPXF_LESSOREQUAL) && !(flags & CPXF_EXACT)) ? 1 : 0;
	}
	return counting ? counter : result;
}

int P_Thing_Warp(AActor *caller, AActor *reference, double xofs, double yofs, double zofs, DAngle angle, int flags, double heightoffset, double radiusoffset, DAngle pitch)
{
	if (flags & WARPF_MOVEPTR)
	{
		AActor *temp = reference;
		reference = caller;
		caller = temp;
	}

	DVector3 old = caller->Pos();
	auto Level = caller->Level;
	int oldpgroup = caller->Sector->PortalGroup;

	zofs += reference->Height * heightoffset;
	

	if (!(flags & WARPF_ABSOLUTEANGLE))
	{
		angle += (flags & WARPF_USECALLERANGLE) ? caller->Angles.Yaw: reference->Angles.Yaw;
	}

	const double rad = radiusoffset * reference->radius;
	const double s = angle.Sin();
	const double c = angle.Cos();

	if (!(flags & WARPF_ABSOLUTEPOSITION))
	{
		if (!(flags & WARPF_ABSOLUTEOFFSET))
		{
			double xofs1 = xofs;

			// (borrowed from A_SpawnItemEx, assumed workable)
			// in relative mode negative y values mean 'left' and positive ones mean 'right'
			// This is the inverse orientation of the absolute mode!
			
			xofs = xofs1 * c + yofs * s;
			yofs = xofs1 * s - yofs * c;
		}

		if (flags & WARPF_TOFLOOR)
		{
			// set correct xy
			// now the caller's floorz should be appropriate for the assigned xy-position
			// assigning position again with.
			// extra unlink, link and environment calculation
			caller->SetOrigin(reference->Vec3Offset(xofs + rad * c, yofs + rad * s, 0.), true);
			// The two-step process is important.
			caller->SetZ(caller->floorz + zofs);
		}
		else
		{
			caller->SetOrigin(reference->Vec3Offset(xofs + rad * c, yofs + rad * s, zofs), true);
		}
	}
	else // [MC] The idea behind "absolute" is meant to be "absolute". Override everything, just like A_SpawnItemEx's.
	{
		caller->SetOrigin(xofs + rad * c, yofs + rad * s, zofs, true);
		if (flags & WARPF_TOFLOOR)
		{
			caller->SetZ(caller->floorz + zofs);
		}
	}

	if ((flags & WARPF_NOCHECKPOSITION) || P_TestMobjLocation(caller))
	{
		if (flags & WARPF_TESTONLY)
		{
			caller->SetOrigin(old, true);
		}
		else
		{
			caller->Angles.Yaw = angle;

			if (flags & WARPF_COPYPITCH)
				caller->SetPitch(reference->Angles.Pitch, false);
			
			if (pitch != nullAngle)
				caller->SetPitch(caller->Angles.Pitch + pitch, false);
			
			if (flags & WARPF_COPYVELOCITY)
			{
				caller->Vel = reference->Vel;
			}
			if (flags & WARPF_STOP)
			{
				caller->Vel.Zero();
			}

			// this is no fun with line portals 
			if (flags & WARPF_WARPINTERPOLATION)
			{
				// This just translates the movement but doesn't change the vector
				DVector3 displacedold  = old + Level->Displacements.getOffset(oldpgroup, caller->Sector->PortalGroup);
				caller->Prev += caller->Pos() - displacedold;
				caller->PrevPortalGroup = caller->Sector->PortalGroup;
			}
			else if (flags & WARPF_COPYINTERPOLATION)
			{
				// Map both positions of the reference actor to the current portal group
				DVector3 displacedold = old + Level->Displacements.getOffset(reference->PrevPortalGroup, caller->Sector->PortalGroup);
				DVector3 displacedref = old + Level->Displacements.getOffset(reference->Sector->PortalGroup, caller->Sector->PortalGroup);
				caller->Prev = caller->Pos() + displacedold - displacedref;
				caller->PrevPortalGroup = caller->Sector->PortalGroup;
			}
			else if (!(flags & WARPF_INTERPOLATE))
			{
				caller->ClearInterpolation();
			}

			if ((flags & WARPF_BOB) && (reference->flags2 & MF2_FLOATBOB))
			{
				caller->AddZ(reference->GetBobOffset());
			}
			P_TryMove(caller, caller->Pos().XY(), false);
		}
		return true;
	}
	caller->SetOrigin(old, true);
	return false;
}

//==========================================================================
//
// A_Warp
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, Warp)
{
	PARAM_SELF_PROLOGUE(AActor)
	PARAM_OBJECT(destination, AActor)
	PARAM_FLOAT(xofs)				
	PARAM_FLOAT(yofs)				
	PARAM_FLOAT(zofs)				
	PARAM_ANGLE(angle)				
	PARAM_INT(flags)				
	PARAM_FLOAT(heightoffset)		
	PARAM_FLOAT(radiusoffset)		
	PARAM_ANGLE(pitch)				

	const int result = destination == nullptr ? 0 :
		P_Thing_Warp(self, destination, xofs, yofs, zofs, angle, flags, heightoffset, radiusoffset, pitch);
	ACTION_RETURN_INT(result);
}
