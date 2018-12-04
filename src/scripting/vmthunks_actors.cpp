//-----------------------------------------------------------------------------
//
// Copyright 2016-2018 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// VM thunks for internal functions of actor classes
//
// Important note about this file: Since everything in here is supposed to be called
// from JIT-compiled VM code it needs to be very careful about calling conventions.
// As a result none of the integer sized struct types may be used as function
// arguments, because current C++ calling conventions require them to be passed
// by reference. The JIT code, however will pass them by value so any direct native function
// taking such an argument needs to receive it as a naked int.
//
//-----------------------------------------------------------------------------

#include "vm.h"
#include "r_defs.h"
#include "g_levellocals.h"
#include "s_sound.h"
#include "p_local.h"
#include "v_font.h"
#include "gstrings.h"
#include "a_keys.h"
#include "sbar.h"
#include "doomstat.h"
#include "p_acs.h"
#include "a_pickups.h"
#include "a_specialspot.h"
#include "actorptrselect.h"
#include "a_weapons.h"
#include "d_player.h"
#include "p_setup.h"
#include "i_music.h"
#include "p_terrain.h"

DVector2 AM_GetPosition();
int Net_GetLatency(int *ld, int *ad);
void PrintPickupMessage(bool localview, const FString &str);


DEFINE_ACTION_FUNCTION_NATIVE(DObject, SetMusicVolume, I_SetMusicVolume)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(vol);
	I_SetMusicVolume(vol);
	return 0;
}

//=====================================================================================
//
// AActor exports (this will be expanded)
//
//=====================================================================================

DEFINE_ACTION_FUNCTION_NATIVE(AActor, GetPointer, COPY_AAPTR)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(ptr);
	ACTION_RETURN_OBJECT(COPY_AAPTR(self, ptr));
}


DEFINE_ACTION_FUNCTION_NATIVE(AActor, A_PlaySound, A_PlaySound)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_SOUND(soundid);
	PARAM_INT(channel);
	PARAM_FLOAT(volume);
	PARAM_BOOL(looping);
	PARAM_FLOAT(attenuation);
	PARAM_BOOL(local);
	A_PlaySound(self, soundid, channel, volume, looping, attenuation, local);
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(AActor, CheckKeys, P_CheckKeys)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(locknum);
	PARAM_BOOL(remote);
	PARAM_BOOL(quiet);
	ACTION_RETURN_BOOL(P_CheckKeys(self, locknum, remote, quiet));
}

DEFINE_ACTION_FUNCTION(AActor, deltaangle)	// should this be global?
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(a1);
	PARAM_FLOAT(a2);
	ACTION_RETURN_FLOAT(deltaangle(DAngle(a1), DAngle(a2)).Degrees);
}

DEFINE_ACTION_FUNCTION(AActor, absangle)	// should this be global?
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(a1);
	PARAM_FLOAT(a2);
	ACTION_RETURN_FLOAT(absangle(DAngle(a1), DAngle(a2)).Degrees);
}

DEFINE_ACTION_FUNCTION(AActor, Distance2DSquared)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(other, AActor);
	ACTION_RETURN_FLOAT(self->Distance2DSquared(other));
}

DEFINE_ACTION_FUNCTION(AActor, Distance3DSquared)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(other, AActor);
	ACTION_RETURN_FLOAT(self->Distance3DSquared(other));
}

DEFINE_ACTION_FUNCTION(AActor, Distance2D)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(other, AActor);
	ACTION_RETURN_FLOAT(self->Distance2D(other));
}

DEFINE_ACTION_FUNCTION(AActor, Distance3D)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(other, AActor);
	ACTION_RETURN_FLOAT(self->Distance3D(other));
}

DEFINE_ACTION_FUNCTION(AActor, AddZ)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(addz);
	PARAM_BOOL(moving);
	self->AddZ(addz, moving);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, SetZ)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(z);
	self->SetZ(z);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, SetDamage)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(dmg);
	self->SetDamage(dmg);
	return 0;
}

// This combines all 3 variations of the internal function
DEFINE_ACTION_FUNCTION(AActor, VelFromAngle)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(speed);
	PARAM_ANGLE(angle);

	if (speed == 1e37)
	{
		self->VelFromAngle();
	}
	else
	{
		if (angle == 1e37)

		{
			self->VelFromAngle(speed);
		}
		else
		{
			self->VelFromAngle(speed, angle);
		}
	}
	return 0;
}

// This combines all 3 variations of the internal function
DEFINE_ACTION_FUNCTION(AActor, Vel3DFromAngle)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(speed);
	PARAM_ANGLE(angle);
	PARAM_ANGLE(pitch);
	self->Vel3DFromAngle(angle, pitch, speed);
	return 0;
}

// This combines all 3 variations of the internal function
DEFINE_ACTION_FUNCTION(AActor, Thrust)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(speed);
	PARAM_ANGLE(angle);

	if (speed == 1e37)
	{
		self->Thrust();
	}
	else
	{
		if (angle == 1e37)
		{
			self->Thrust(speed);
		}
		else
		{
			self->Thrust(angle, speed);
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, AngleTo)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(targ, AActor);
	PARAM_BOOL(absolute);
	ACTION_RETURN_FLOAT(self->AngleTo(targ, absolute).Degrees);
}

DEFINE_ACTION_FUNCTION(AActor, AngleToVector)
{
	PARAM_PROLOGUE;
	PARAM_ANGLE(angle);
	PARAM_FLOAT(length);
	ACTION_RETURN_VEC2(angle.ToVector(length));
}

DEFINE_ACTION_FUNCTION(AActor, RotateVector)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_ANGLE(angle);
	ACTION_RETURN_VEC2(DVector2(x, y).Rotated(angle));
}

DEFINE_ACTION_FUNCTION(AActor, Normalize180)
{
	PARAM_PROLOGUE;
	PARAM_ANGLE(angle);
	ACTION_RETURN_FLOAT(angle.Normalized180().Degrees);
}

DEFINE_ACTION_FUNCTION(AActor, DistanceBySpeed)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(targ, AActor);
	PARAM_FLOAT(speed);
	ACTION_RETURN_FLOAT(self->DistanceBySpeed(targ, speed));
}

DEFINE_ACTION_FUNCTION(AActor, SetXYZ)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	self->SetXYZ(x, y, z);
	return 0; 
}

DEFINE_ACTION_FUNCTION(AActor, Vec2Angle)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(length);
	PARAM_ANGLE(angle);
	PARAM_BOOL(absolute);
	ACTION_RETURN_VEC2(self->Vec2Angle(length, angle, absolute));
}


DEFINE_ACTION_FUNCTION(AActor, Vec3To)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(t, AActor)
	ACTION_RETURN_VEC3(self->Vec3To(t));
}

DEFINE_ACTION_FUNCTION(AActor, Vec2To)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(t, AActor)
	ACTION_RETURN_VEC2(self->Vec2To(t));
}

DEFINE_ACTION_FUNCTION(AActor, Vec3Angle)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(length)
	PARAM_ANGLE(angle);
	PARAM_FLOAT(z);
	PARAM_BOOL(absolute);
	ACTION_RETURN_VEC3(self->Vec3Angle(length, angle, z, absolute));
}

DEFINE_ACTION_FUNCTION(AActor, Vec2OffsetZ)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_BOOL(absolute);
	ACTION_RETURN_VEC3(self->Vec2OffsetZ(x, y, z, absolute));
}

DEFINE_ACTION_FUNCTION(AActor, Vec2Offset)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_BOOL(absolute);
	ACTION_RETURN_VEC2(self->Vec2Offset(x, y, absolute));
}

DEFINE_ACTION_FUNCTION(AActor, Vec3Offset)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_BOOL(absolute);
	ACTION_RETURN_VEC3(self->Vec3Offset(x, y, z, absolute));
}

DEFINE_ACTION_FUNCTION(AActor, PosRelative)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_POINTER(sec, sector_t);
	ACTION_RETURN_VEC3(self->PosRelative(sec));
}

DEFINE_ACTION_FUNCTION(AActor, RestoreDamage)
{
	PARAM_SELF_PROLOGUE(AActor);
	self->RestoreDamage();
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, PlayerNumber)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_INT(self->player ? int(self->player - players) : 0);
}

DEFINE_ACTION_FUNCTION(AActor, SetFriendPlayer)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_POINTER(player, player_t);
	self->SetFriendPlayer(player);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, ClearBounce)
{
	PARAM_SELF_PROLOGUE(AActor);
	self->BounceFlags = 0;
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, AccuracyFactor)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_FLOAT(self->AccuracyFactor());
}

DEFINE_ACTION_FUNCTION(AActor, CountsAsKill)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_BOOL(self->CountsAsKill());
}

DEFINE_ACTION_FUNCTION(AActor, IsZeroDamage)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_BOOL(self->IsZeroDamage());
}

DEFINE_ACTION_FUNCTION(AActor, ClearInterpolation)
{
	PARAM_SELF_PROLOGUE(AActor);
	self->ClearInterpolation();
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, ApplyDamageFactors)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(itemcls, AActor);
	PARAM_NAME(damagetype);
	PARAM_INT(damage);
	PARAM_INT(defdamage);

	DmgFactors &df = itemcls->ActorInfo()->DamageFactors;
	if (df.Size() != 0)
	{
		ACTION_RETURN_INT(df.Apply(damagetype, damage));
	}
	else
	{
		ACTION_RETURN_INT(defdamage);
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_RestoreSpecialPosition)
{
	PARAM_SELF_PROLOGUE(AActor);
	self->RestoreSpecialPosition();
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, GetBobOffset)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(frac);
	ACTION_RETURN_FLOAT(self->GetBobOffset(frac));
}

DEFINE_ACTION_FUNCTION(AActor, SetIdle)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_BOOL(nofunction);
	self->SetIdle(nofunction);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, SpawnHealth)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_INT(self->SpawnHealth());
}

DEFINE_ACTION_FUNCTION(AActor, Revive)
{
	PARAM_SELF_PROLOGUE(AActor);
	self->Revive();
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, GetCameraHeight)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_FLOAT(self->GetCameraHeight());
}

DEFINE_ACTION_FUNCTION(AActor, GetDropItems)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_POINTER(self->GetDropItems());
}

DEFINE_ACTION_FUNCTION(AActor, GetGravity)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_FLOAT(self->GetGravity());
}


DEFINE_ACTION_FUNCTION(AActor, GetTag)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_STRING(def);
	ACTION_RETURN_STRING(self->GetTag(def.Len() == 0 ? nullptr : def.GetChars()));
}


DEFINE_ACTION_FUNCTION(AActor, SetTag)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_STRING(def);
	if (def.IsEmpty()) self->Tag = nullptr;
	else self->Tag = self->mStringPropertyData.Alloc(def);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, ClearCounters)
{
	PARAM_SELF_PROLOGUE(AActor);
	self->ClearCounters();
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, GetModifiedDamage)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME(type);
	PARAM_INT(damage);
	PARAM_BOOL(passive);
	ACTION_RETURN_INT(self->GetModifiedDamage(type, damage, passive));
}
DEFINE_ACTION_FUNCTION(AActor, ApplyDamageFactor)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME(type);
	PARAM_INT(damage);
	ACTION_RETURN_INT(self->ApplyDamageFactor(type, damage));
}

double GetDefaultSpeed(PClassActor *type);

DEFINE_ACTION_FUNCTION(AActor, GetDefaultSpeed)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(type, AActor);
	ACTION_RETURN_FLOAT(GetDefaultSpeed(type));
}

DEFINE_ACTION_FUNCTION(AActor, isTeammate)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(other, AActor);
	ACTION_RETURN_BOOL(self->IsTeammate(other));
}

DEFINE_ACTION_FUNCTION(AActor, GetSpecies)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_INT(self->GetSpecies());
}

DEFINE_ACTION_FUNCTION(AActor, isFriend)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(other, AActor);
	ACTION_RETURN_BOOL(self->IsFriend(other));
}

DEFINE_ACTION_FUNCTION(AActor, isHostile)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(other, AActor);
	ACTION_RETURN_BOOL(self->IsHostile(other));
}

DEFINE_ACTION_FUNCTION(AActor, GetFloorTerrain)
{
	PARAM_SELF_PROLOGUE(AActor);
	ACTION_RETURN_POINTER(&Terrains[P_GetThingFloorType(self)]);
}

DEFINE_ACTION_FUNCTION(AActor, FindUniqueTid)
{
	PARAM_PROLOGUE;
	PARAM_INT(start);
	PARAM_INT(limit);
	ACTION_RETURN_INT(P_FindUniqueTID(start, limit));
}

DEFINE_ACTION_FUNCTION(AActor, RemoveFromHash)
{
	PARAM_SELF_PROLOGUE(AActor);
	self->RemoveFromHash();
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, ChangeTid)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(tid);
	self->RemoveFromHash();
	self->tid = tid;
	self->AddToHash();
	return 0;
}




//=====================================================================================
//
// Inventory exports
//
//=====================================================================================

DEFINE_ACTION_FUNCTION_NATIVE(AInventory, PrintPickupMessage, PrintPickupMessage)
{
	PARAM_PROLOGUE;
	PARAM_BOOL(localview);
	PARAM_STRING(str);
	PrintPickupMessage(localview, str);
	return 0;
}

//=====================================================================================
//
// Key exports
//
//=====================================================================================

DEFINE_ACTION_FUNCTION_NATIVE(AKey, GetKeyTypeCount, P_GetKeyTypeCount)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(P_GetKeyTypeCount());
}

DEFINE_ACTION_FUNCTION_NATIVE(AKey, GetKeyType, P_GetKeyType)
{
	PARAM_PROLOGUE;
	PARAM_INT(num);
	ACTION_RETURN_POINTER(P_GetKeyType(num));
}

