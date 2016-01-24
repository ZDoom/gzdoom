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
#include "s_sound.h"
#include "tables.h"
#include "doomstat.h"
#include "m_random.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "a_sharedglobal.h"
#include "gi.h"
#include "templates.h"
#include "g_level.h"
#include "v_text.h"
#include "i_system.h"

// Set of spawnable things for the Thing_Spawn and Thing_Projectile specials.
FClassMap SpawnableThings;

static FRandom pr_leadtarget ("LeadTarget");

bool P_Thing_Spawn (int tid, AActor *source, int type, angle_t angle, bool fog, int newtid)
{
	int rtn = 0;
	const PClass *kind;
	AActor *spot, *mobj;
	FActorIterator iterator (tid);

	kind = P_GetSpawnableType(type);

	if (kind == NULL)
		return false;

	// Handle decorate replacements.
	kind = kind->GetReplacement();

	if ((GetDefaultByType (kind)->flags3 & MF3_ISMONSTER) && 
		((dmflags & DF_NO_MONSTERS) || (level.flags2 & LEVEL2_NOMONSTERS)))
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
		mobj = Spawn (kind, spot->Pos(), ALLOW_REPLACE);

		if (mobj != NULL)
		{
			ActorFlags2 oldFlags2 = mobj->flags2;
			mobj->flags2 |= MF2_PASSMOBJ;
			if (P_TestMobjLocation (mobj))
			{
				rtn++;
				mobj->angle = (angle != ANGLE_MAX ? angle : spot->angle);
				if (fog)
				{
					P_SpawnTeleportFog(mobj, spot->X(), spot->Y(), spot->Z() + TELEFOGHEIGHT, false, true);
				}
				if (mobj->flags & MF_SPECIAL)
					mobj->flags |= MF_DROPPED;	// Don't respawn
				mobj->tid = newtid;
				mobj->AddToHash ();
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

bool P_MoveThing(AActor *source, fixed_t x, fixed_t y, fixed_t z, bool fog)
{
	fixed_t oldx, oldy, oldz;

	oldx = source->X();
	oldy = source->Y();
	oldz = source->Z();

	source->SetOrigin (x, y, z, false);
	if (P_TestMobjLocation (source))
	{
		if (fog)
		{
			P_SpawnTeleportFog(source, x, y, z, false, true);
			P_SpawnTeleportFog(source, oldx, oldy, oldz, true, true);
		}
		source->PrevX = x;
		source->PrevY = y;
		source->PrevZ = z;
		if (source == players[consoleplayer].camera)
		{
			R_ResetViewInterpolation();
		}
		return true;
	}
	else
	{
		source->SetOrigin (oldx, oldy, oldz, false);
		return false;
	}
}

bool P_Thing_Move (int tid, AActor *source, int mapspot, bool fog)
{
	AActor *target;

	if (tid != 0)
	{
		FActorIterator iterator1(tid);
		source = iterator1.Next();
	}
	FActorIterator iterator2 (mapspot);
	target = iterator2.Next ();

	if (source != NULL && target != NULL)
	{
		return P_MoveThing(source, target->X(), target->Y(), target->Z(), fog);
	}
	return false;
}

bool P_Thing_Projectile (int tid, AActor *source, int type, const char *type_name, angle_t angle,
	fixed_t speed, fixed_t vspeed, int dest, AActor *forcedest, int gravity, int newtid,
	bool leadTarget)
{
	int rtn = 0;
	const PClass *kind;
	AActor *spot, *mobj, *targ = forcedest;
	FActorIterator iterator (tid);
	double fspeed = speed;
	int defflags3;

	if (type_name == NULL)
	{
		kind = P_GetSpawnableType(type);
	}
	else
	{
		kind = PClass::FindClass(type_name);
	}
	if (kind == NULL || kind->ActorInfo == NULL)
	{
		return false;
	}

	// Handle decorate replacements.
	kind = kind->GetReplacement();

	defflags3 = GetDefaultByType (kind)->flags3;
	if ((defflags3 & MF3_ISMONSTER) && 
		((dmflags & DF_NO_MONSTERS) || (level.flags2 & LEVEL2_NOMONSTERS)))
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
		FActorIterator tit (dest);

		if (dest == 0 || (targ = tit.Next()))
		{
			do
			{
				fixed_t z = spot->Z();
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
					z -= spot->floorclip;
				}
				mobj = Spawn (kind, spot->X(), spot->Y(), z, ALLOW_REPLACE);

				if (mobj)
				{
					mobj->tid = newtid;
					mobj->AddToHash ();
					P_PlaySpawnSound(mobj, spot);
					if (gravity)
					{
						mobj->flags &= ~MF_NOGRAVITY;
						if (!(mobj->flags3 & MF3_ISMONSTER) && gravity == 1)
						{
							mobj->gravity = FRACUNIT/8;
						}
					}
					else
					{
						mobj->flags |= MF_NOGRAVITY;
					}
					mobj->target = spot;

					if (targ != NULL)
					{
						fixedvec3 vect = mobj->Vec3To(targ);
						vect.z += targ->height / 2;
						TVector3<double> aim(vect.x, vect.y, vect.z);

						if (leadTarget && speed > 0 && (targ->velx | targ->vely | targ->velz))
						{
							// Aiming at the target's position some time in the future
							// is basically just an application of the law of sines:
							//     a/sin(A) = b/sin(B)
							// Thanks to all those on the notgod phorum for helping me
							// with the math. I don't think I would have thought of using
							// trig alone had I been left to solve it by myself.

							TVector3<double> tvel(targ->velx, targ->vely, targ->velz);
							if (!(targ->flags & MF_NOGRAVITY) && targ->waterlevel < 3)
							{ // If the target is subject to gravity and not underwater,
							  // assume that it isn't moving vertically. Thanks to gravity,
							  // even if we did consider the vertical component of the target's
							  // velocity, we would still miss more often than not.
								tvel.Z = 0.0;
								if ((targ->velx | targ->vely) == 0)
								{
									goto nolead;
								}
							}
							double dist = aim.Length();
							double targspeed = tvel.Length();
							double ydotx = -aim | tvel;
							double a = acos (clamp (ydotx / targspeed / dist, -1.0, 1.0));
							double multiplier = double(pr_leadtarget.Random2())*0.1/255+1.1;
							double sinb = -clamp (targspeed*multiplier * sin(a) / fspeed, -1.0, 1.0);

							// Use the cross product of two of the triangle's sides to get a
							// rotation vector.
							TVector3<double> rv(tvel ^ aim);
							// The vector must be normalized.
							rv.MakeUnit();
							// Now combine the rotation vector with angle b to get a rotation matrix.
							TMatrix3x3<double> rm(rv, cos(asin(sinb)), sinb);
							// And multiply the original aim vector with the matrix to get a
							// new aim vector that leads the target.
							TVector3<double> aimvec = rm * aim;
							// And make the projectile follow that vector at the desired speed.
							double aimscale = fspeed / dist;
							mobj->velx = fixed_t (aimvec[0] * aimscale);
							mobj->vely = fixed_t (aimvec[1] * aimscale);
							mobj->velz = fixed_t (aimvec[2] * aimscale);
							mobj->angle = R_PointToAngle2 (0, 0, mobj->velx, mobj->vely);
						}
						else
						{
nolead:
							mobj->angle = mobj->AngleTo(targ);
							aim.Resize (fspeed);
							mobj->velx = fixed_t(aim[0]);
							mobj->vely = fixed_t(aim[1]);
							mobj->velz = fixed_t(aim[2]);
						}
						if (mobj->flags2 & MF2_SEEKERMISSILE)
						{
							mobj->tracer = targ;
						}
					}
					else
					{
						mobj->angle = angle;
						mobj->velx = FixedMul (speed, finecosine[angle>>ANGLETOFINESHIFT]);
						mobj->vely = FixedMul (speed, finesine[angle>>ANGLETOFINESHIFT]);
						mobj->velz = vspeed;
					}
					// Set the missile's speed to reflect the speed it was spawned at.
					if (mobj->flags & MF_MISSILE)
					{
						mobj->Speed = fixed_t (sqrt (double(mobj->velx)*mobj->velx + double(mobj->vely)*mobj->vely + double(mobj->velz)*mobj->velz));
					}
					// Hugger missiles don't have any vertical velocity
					if (mobj->flags3 & (MF3_FLOORHUGGER|MF3_CEILINGHUGGER))
					{
						mobj->velz = 0;
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

int P_Thing_Damage (int tid, AActor *whofor0, int amount, FName type)
{
	FActorIterator iterator (tid);
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
		if (actor->IsKindOf(RUNTIME_CLASS(AInventory)) && static_cast<AInventory*>(actor)->Owner != NULL) return;

		// be friendly to the level statistics. ;)
		actor->ClearCounters();
		actor->Destroy ();
	}

}

bool P_Thing_Raise(AActor *thing, AActor *raiser)
{
	FState * RaiseState = thing->GetRaiseState();
	if (RaiseState == NULL)
	{
		return true;	// monster doesn't have a raise state
	}
	
	AActor *info = thing->GetDefault ();

	thing->velx = thing->vely = 0;

	// [RH] Check against real height and radius
	fixed_t oldheight = thing->height;
	fixed_t oldradius = thing->radius;
	ActorFlags oldflags = thing->flags;

	thing->flags |= MF_SOLID;
	thing->height = info->height;	// [RH] Use real height
	thing->radius = info->radius;	// [RH] Use real radius
	if (!P_CheckPosition (thing, thing->Pos()))
	{
		thing->flags = oldflags;
		thing->radius = oldradius;
		thing->height = oldheight;
		return false;
	}


	S_Sound (thing, CHAN_BODY, "vile/raise", 1, ATTN_IDLE);

	thing->Revive();

	if (raiser != NULL)
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
	fixed_t oldheight = thing->height;
	fixed_t oldradius = thing->radius;

	thing->flags |= MF_SOLID;
	thing->height = info->height;
	thing->radius = info->radius;

	bool check = P_CheckPosition (thing, thing->Pos());

	// Restore checked properties
	thing->flags = oldflags;
	thing->radius = oldradius;
	thing->height = oldheight;

	if (!check)
	{
		return false;
	}

	return true;
}

void P_Thing_SetVelocity(AActor *actor, fixed_t vx, fixed_t vy, fixed_t vz, bool add, bool setbob)
{
	if (actor != NULL)
	{
		if (!add)
		{
			actor->velx = actor->vely = actor->velz = 0;
			if (actor->player != NULL) actor->player->velx = actor->player->vely = 0;
		}
		actor->velx += vx;
		actor->vely += vy;
		actor->velz += vz;
		if (setbob && actor->player != NULL)
		{
			actor->player->velx += vx;
			actor->player->vely += vy;
		}
	}
}

const PClass *P_GetSpawnableType(int spawnnum)
{
	if (spawnnum < 0)
	{ // A named arg from a UDMF map
		FName spawnname = FName(ENamedName(-spawnnum));
		if (spawnname.IsValidName())
		{
			return PClass::FindClass(spawnname);
		}
	}
	else
	{ // A numbered arg from a Hexen or UDMF map
		const PClass **type = SpawnableThings.CheckKey(spawnnum);
		if (type != NULL)
		{
			return *type;
		}
	}
	return NULL;
}

struct MapinfoSpawnItem
{
	FName classname;	// DECORATE is read after MAPINFO so we do not have the actual classes available here yet.
	// These are for error reporting. We must store the file information because it's no longer available when these items get resolved.
	FString filename;
	int linenum;
};

typedef TMap<int, MapinfoSpawnItem> SpawnMap;
static SpawnMap SpawnablesFromMapinfo;
static SpawnMap ConversationIDsFromMapinfo;

static int STACK_ARGS SpawnableSort(const void *a, const void *b)
{
	return (*((FClassMap::Pair **)a))->Key - (*((FClassMap::Pair **)b))->Key;
}

static void DumpClassMap(FClassMap &themap)
{
	FClassMap::Iterator it(themap);
	FClassMap::Pair *pair, **allpairs;
	int i = 0;

	// Sort into numerical order, since their arrangement in the map can
	// be in an unspecified order.
	allpairs = new FClassMap::Pair *[themap.CountUsed()];
	while (it.NextPair(pair))
	{
		allpairs[i++] = pair;
	}
	qsort(allpairs, i, sizeof(*allpairs), SpawnableSort);
	for (int j = 0; j < i; ++j)
	{
		pair = allpairs[j];
		Printf ("%d %s\n", pair->Key, pair->Value->TypeName.GetChars());
	}
	delete[] allpairs;
}

CCMD(dumpspawnables)
{
	DumpClassMap(SpawnableThings);
}

CCMD (dumpconversationids)
{
	DumpClassMap(StrifeTypes);
}


static void ParseSpawnMap(FScanner &sc, SpawnMap & themap, const char *descript)
{
	TMap<int, bool> defined;
	int error = 0;

	MapinfoSpawnItem editem;

	editem.filename = sc.ScriptName;

	while (true)
	{
		if (sc.CheckString("}")) return;
		else if (sc.CheckNumber())
		{
			int ednum = sc.Number;
			sc.MustGetStringName("=");
			sc.MustGetString();

			bool *def = defined.CheckKey(ednum);
			if (def != NULL)
			{
				sc.ScriptMessage("%s %d defined more than once", descript, ednum);
				error++;
			}
			else if (ednum < 0)
			{
				sc.ScriptMessage("%s must be positive, got %d", descript, ednum);
				error++;
			}
			defined[ednum] = true;
			editem.classname = sc.String;

			themap.Insert(ednum, editem);
		}
		else
		{
			sc.ScriptError("Number expected");
		}
	}
	if (error > 0)
	{
		sc.ScriptError("%d errors encountered in %s definition", error, descript);
	}
}

void FMapInfoParser::ParseSpawnNums()
{
	ParseOpenBrace();
	ParseSpawnMap(sc, SpawnablesFromMapinfo, "Spawn number");
}

void FMapInfoParser::ParseConversationIDs()
{
	ParseOpenBrace();
	ParseSpawnMap(sc, ConversationIDsFromMapinfo, "Conversation ID");
}


void InitClassMap(FClassMap &themap, SpawnMap &thedata)
{
	themap.Clear();
	SpawnMap::Iterator it(thedata);
	SpawnMap::Pair *pair;
	int error = 0;

	while (it.NextPair(pair))
	{
		const PClass *cls = NULL;
		if (pair->Value.classname != NAME_None)
		{
			cls = PClass::FindClass(pair->Value.classname);
			if (cls == NULL)
			{
				Printf(TEXTCOLOR_RED "Script error, \"%s\" line %d:\nUnknown actor class %s\n",
					pair->Value.filename.GetChars(), pair->Value.linenum, pair->Value.classname.GetChars());
				error++;
			}
			themap.Insert(pair->Key, cls);
		}
		else
		{
			themap.Remove(pair->Key);
		}
	}
	if (error > 0)
	{
		I_Error("%d unknown actor classes found", error);
	}
	thedata.Clear();	// we do not need this any longer
}

void InitSpawnablesFromMapinfo()
{
	InitClassMap(SpawnableThings, SpawnablesFromMapinfo);
	InitClassMap(StrifeTypes, ConversationIDsFromMapinfo);
}


int P_Thing_Warp(AActor *caller, AActor *reference, fixed_t xofs, fixed_t yofs, fixed_t zofs, angle_t angle, int flags, fixed_t heightoffset, fixed_t radiusoffset, angle_t pitch)
{
	if (flags & WARPF_MOVEPTR)
	{
		AActor *temp = reference;
		reference = caller;
		caller = temp;
	}

	fixedvec3 old = caller->Pos();
	zofs += FixedMul(reference->height, heightoffset);
	

	if (!(flags & WARPF_ABSOLUTEANGLE))
	{
		angle += (flags & WARPF_USECALLERANGLE) ? caller->angle : reference->angle;
	}

	const fixed_t rad = FixedMul(radiusoffset, reference->radius);
	const angle_t fineangle = angle >> ANGLETOFINESHIFT;

	if (!(flags & WARPF_ABSOLUTEPOSITION))
	{
		if (!(flags & WARPF_ABSOLUTEOFFSET))
		{
			fixed_t xofs1 = xofs;

			// (borrowed from A_SpawnItemEx, assumed workable)
			// in relative mode negative y values mean 'left' and positive ones mean 'right'
			// This is the inverse orientation of the absolute mode!
			
			xofs = FixedMul(xofs1, finecosine[fineangle]) + FixedMul(yofs, finesine[fineangle]);
			yofs = FixedMul(xofs1, finesine[fineangle]) - FixedMul(yofs, finecosine[fineangle]);
		}

		if (flags & WARPF_TOFLOOR)
		{
			// set correct xy
			// now the caller's floorz should be appropriate for the assigned xy-position
			// assigning position again with.
			// extra unlink, link and environment calculation
			caller->SetOrigin(reference->Vec3Offset(
				xofs + FixedMul(rad, finecosine[fineangle]),
				yofs + FixedMul(rad, finesine[fineangle]),
				0), true);
			caller->SetZ(caller->floorz + zofs);
		}
		else
		{
			caller->SetOrigin(reference->Vec3Offset(
				 xofs + FixedMul(rad, finecosine[fineangle]),
				 yofs + FixedMul(rad, finesine[fineangle]),
				 zofs), true);
		}
	}
	else // [MC] The idea behind "absolute" is meant to be "absolute". Override everything, just like A_SpawnItemEx's.
	{
		if (flags & WARPF_TOFLOOR)
		{
			caller->SetOrigin(xofs + FixedMul(rad, finecosine[fineangle]), yofs + FixedMul(rad, finesine[fineangle]), zofs);
			caller->SetZ(caller->floorz + zofs);
		}
		else
		{
			caller->SetOrigin(xofs + FixedMul(rad, finecosine[fineangle]), yofs + FixedMul(rad, finesine[fineangle]), zofs);
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
			caller->angle = angle;

			if (flags & WARPF_COPYPITCH)
				caller->SetPitch(reference->pitch, false);
			
			if (pitch)
				caller->SetPitch(caller->pitch + pitch, false);
			
			if (flags & WARPF_COPYVELOCITY)
			{
				caller->velx = reference->velx;
				caller->vely = reference->vely;
				caller->velz = reference->velz;
			}
			if (flags & WARPF_STOP)
			{
				caller->velx = 0;
				caller->vely = 0;
				caller->velz = 0;
			}

			if (flags & WARPF_WARPINTERPOLATION)
			{
				caller->PrevX += caller->X() - old.x;
				caller->PrevY += caller->Y() - old.y;
				caller->PrevZ += caller->Z() - old.z;
			}
			else if (flags & WARPF_COPYINTERPOLATION)
			{
				caller->PrevX = caller->X() + reference->PrevX - reference->X();
				caller->PrevY = caller->Y() + reference->PrevY - reference->Y();
				caller->PrevZ = caller->Z() + reference->PrevZ - reference->Z();
			}
			else if (!(flags & WARPF_INTERPOLATE))
			{
				caller->PrevX = caller->X();
				caller->PrevY = caller->Y();
				caller->PrevZ = caller->Z();
			}
			if ((flags & WARPF_BOB) && (reference->flags2 & MF2_FLOATBOB))
			{
				caller->AddZ(reference->GetBobOffset());
			}
		}
		return true;
	}
	caller->SetOrigin(old, true);
	return false;
}