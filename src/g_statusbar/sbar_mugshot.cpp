/*
** sbar_mugshot.cpp
**
** Draws customizable mugshots for the status bar.
**
**---------------------------------------------------------------------------
** Copyright 2008 Braden Obrzut
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

#include "r_defs.h"
#include "m_random.h"
#include "d_player.h"
#include "sbar.h"
#include "r_utility.h"
#include "actorinlines.h"

#define ST_RAMPAGEDELAY 		(2*TICRATE)
#define ST_MUCHPAIN 			20

TArray<FMugShotState> MugShotStates;


//===========================================================================
//
// FMugShotFrame constructor
//
//===========================================================================

FMugShotFrame::FMugShotFrame()
{
}

//===========================================================================
//
// FMugShotFrame destructor
//
//===========================================================================

FMugShotFrame::~FMugShotFrame()
{
}

//===========================================================================
//
// FMugShotFrame :: GetTexture
//
// Assemble a graphic name with the specified prefix and return the FTexture.
//
//===========================================================================

FTexture *FMugShotFrame::GetTexture(const char *default_face, const char *skin_face, int random, int level,
	int direction, bool uses_levels, bool health2, bool healthspecial, bool directional)
{
	int index = !directional ? random % Graphic.Size() : direction;
	if ((unsigned int)index > Graphic.Size() - 1)
	{
		index = Graphic.Size() - 1;
	}
	FString sprite(skin_face != NULL && skin_face[0] != 0 ? skin_face : default_face, 3);
	sprite += Graphic[index];
	if (uses_levels) //change the last character to the level
	{
		if (!health2 && (!healthspecial || index == 1))
		{
			sprite.LockBuffer()[2 + Graphic[index].Len()] += level;
		}
		else
		{
			sprite.LockBuffer()[1 + Graphic[index].Len()] += level;
		}
		sprite.UnlockBuffer();
	}
	return TexMan.GetTexture(TexMan.CheckForTexture(sprite, ETextureType::Any, FTextureManager::TEXMAN_TryAny|FTextureManager::TEXMAN_AllowSkins));
}

//===========================================================================
//
// MugShotState default constructor
//
//===========================================================================

FMugShotState::FMugShotState()
{
}

//===========================================================================
//
// MugShotState named constructor
//
//===========================================================================

FMugShotState::FMugShotState(FName name)
{
	State = name;
	bUsesLevels = false;
	bHealth2 = false;
	bHealthSpecial = false;
	bDirectional = false;
	bFinished = true;
	Random = M_Random();
}

//===========================================================================
//
// FMugShotState :: Tick
//
//===========================================================================

void FMugShotState::Tick()
{
	if (Time == -1)
	{ //When the delay is negative 1, stay on this frame indefinitely.
		return;
	}
	if (Time != 0)
	{
		Time--;
	}
	else if (Position < Frames.Size() - 1)
	{
		Position++;
		Time = Frames[Position].Delay;
		Random = M_Random();
	}
	else
	{
		bFinished = true;
	}
}

//===========================================================================
//
// FMugShotState :: Reset
//
//===========================================================================

void FMugShotState::Reset()
{
	Time = Frames[0].Delay;
	Position = 0;
	bFinished = false;
	Random = M_Random();
}

//===========================================================================
//
// FindMugShotState
//
//===========================================================================

FMugShotState *FindMugShotState(FName state)
{
	for (unsigned int i = 0; i < MugShotStates.Size(); i++)
	{
		if (MugShotStates[i].State == state)
			return &MugShotStates[i];
	}
	return NULL;
}

//===========================================================================
//
// FindMugShotStateIndex
//
// Used to allow replacements of states
//
//===========================================================================

int FindMugShotStateIndex(FName state)
{
	for (unsigned int i = 0; i < MugShotStates.Size(); i++)
	{
		if (MugShotStates[i].State == state)
			return i;
	}
	return -1;
}

//===========================================================================
//
// FMugShot constructor
//
//===========================================================================

FMugShot::FMugShot()
{
	Reset();
}

//===========================================================================
//
// FMugShot :: Reset
//
//===========================================================================

void FMugShot::Reset()
{
	FaceHealth = -1;
	bEvilGrin = false;
	bNormal = true;
	bDamageFaceActive = false;
	bOuchActive = false;
	CurrentState = NULL;
	RampageTimer = 0;
	LastDamageAngle = 1;
}

//===========================================================================
//
// FMugShot :: Tick
//
// Do some stuff related to the mug shot that has to be done at 35fps
//
//===========================================================================

void FMugShot::Tick(player_t *player)
{
	if (CurrentState != NULL)
	{
		CurrentState->Tick();
		if (CurrentState->bFinished)
		{
			bNormal = true;
			bOuchActive = false;
			CurrentState = NULL;
		}
	}
	if (player->attackdown && !(player->cheats & (CF_FROZEN | CF_TOTALLYFROZEN)) && player->ReadyWeapon)
	{
		if (RampageTimer != ST_RAMPAGEDELAY)
		{
			RampageTimer++;
		}
	}
	else
	{
		RampageTimer = 0;
	}
	FaceHealth = player->health;
}

//===========================================================================
//
// FMugShot :: SetState
//
// Sets the mug shot state and resets it if it is not the state we are
// already on. Wait_till_done is basically a priority variable; when set to
// true the state won't change unless the previous state is finished.  Reset
// overrides the behavior of only switching when the state is not the one we
// are already on.
// Returns true if the requested state was switched to or is already playing,
// and false if the requested state could not be set.
//
//===========================================================================

bool FMugShot::SetState(const char *state_name, bool wait_till_done, bool reset)
{
	// Search for full name.
	FMugShotState *state = FindMugShotState(FName(state_name, true));
	if (state == NULL)
	{
		// Search for initial name, if the full one contains a dot.
		const char *dot = strchr(state_name, '.');
		if (dot != NULL)
		{
			state = FindMugShotState(FName(state_name, dot - state_name, true));
		}
		if (state == NULL)
		{
			// Requested state does not exist, so do nothing.
			return false;
		}
	}
	bNormal = false; //Assume we are not setting god or normal for now.
	bOuchActive = false;
	if (state != CurrentState)
	{
		if (!wait_till_done || CurrentState == NULL || CurrentState->bFinished)
		{
			CurrentState = state;
			state->Reset();
			return true;
		}
		return false;
	}
	else if(reset)
	{
		state->Reset();
		return true;
	}
	return true;
}

//===========================================================================
//
// FMugShot :: UpdateState
//
//===========================================================================

CVAR(Bool,st_oldouch,false,CVAR_ARCHIVE)
int FMugShot::UpdateState(player_t *player, StateFlags stateflags)
{
	FString		full_state_name;

	if (player->health > 0)
	{
		if (bEvilGrin && !(stateflags & DISABLEGRIN))
		{
			if (player->bonuscount)
			{
				SetState("grin", false);
				return 0;
			}
		}
		bEvilGrin = false;

		bool ouch = (!st_oldouch && FaceHealth - player->health > ST_MUCHPAIN) || (st_oldouch && player->health - FaceHealth > ST_MUCHPAIN);
		if (player->damagecount && 
			// Now go in if pain is disabled but we think ouch will be shown (and ouch is not disabled!)
			(!(stateflags & DISABLEPAIN) || (((FaceHealth != -1 && ouch) || bOuchActive) && !(stateflags & DISABLEOUCH))))
		{
			int damage_angle = 1;
			if (player->attacker && player->attacker != player->mo)
			{
				if (player->mo != NULL)
				{
					// The next 12 lines are from the Doom statusbar code.
					DAngle badguyangle = player->mo->AngleTo(player->attacker);
					DAngle diffang = deltaangle(player->mo->Angles.Yaw, badguyangle);
					if (diffang > 45.)
					{ // turn face right
						damage_angle = 2;
					}
					else if (diffang < -45.)
					{ // turn face left
						damage_angle = 0;
					}
				}
			}
			bool use_ouch = false;
			if (((FaceHealth != -1 && ouch) || bOuchActive) && !(stateflags & DISABLEOUCH))
			{
				use_ouch = true;
				full_state_name = "ouch.";
			}
			else
			{
				full_state_name = "pain.";
			}
			full_state_name += player->LastDamageType;
			if (SetState(full_state_name, false, true))
			{
				bDamageFaceActive = (CurrentState != NULL);
				LastDamageAngle = damage_angle;
				bOuchActive = use_ouch;
			}
			return damage_angle;
		}
		if (bDamageFaceActive)
		{
			if (CurrentState == NULL)
			{
				bDamageFaceActive = false;
			}
			else
			{
				bool use_ouch = false;
				if (((FaceHealth != -1 && ouch) || bOuchActive) && !(stateflags & DISABLEOUCH))
				{
					use_ouch = true;
					full_state_name = "ouch.";
				}
				else
				{
					full_state_name = "pain.";
				}
				full_state_name += player->LastDamageType;
				if (SetState(full_state_name))
				{
					bOuchActive = use_ouch;
				}
				return LastDamageAngle;
			}
		}

		if (RampageTimer == ST_RAMPAGEDELAY && !(stateflags & DISABLERAMPAGE))
		{
			SetState("rampage", !bNormal); //If we have nothing better to show, use the rampage face.
			return 0;
		}

		if (bNormal)
		{
			bool good;
			if ((player->cheats & CF_GODMODE) || (player->cheats & CF_GODMODE2) || (player->mo != NULL && player->mo->flags2 & MF2_INVULNERABLE))
			{
				good = SetState((stateflags & ANIMATEDGODMODE) ? "godanimated" : "god");
			}
			else
			{
				good = SetState("normal");
			}
			if (good)
			{
				bNormal = true; //SetState sets bNormal to false.
			}
		}
	}
	else
	{
		if (!(stateflags & XDEATHFACE) || !(player->cheats & CF_EXTREMELYDEAD))
		{
			full_state_name = "death.";
		}
		else
		{
			full_state_name = "xdeath.";
		}
		full_state_name += player->LastDamageType;
		SetState(full_state_name);
		bNormal = true; //Allow the face to return to alive states when the player respawns.
	}
	return 0;
}

//===========================================================================
//
// FMugShot :: GetFace
//
// Updates the status of the mug shot and returns the current face texture.
//
//===========================================================================

FTexture *FMugShot::GetFace(player_t *player, const char *default_face, int accuracy, StateFlags stateflags)
{
	int angle = UpdateState(player, stateflags);
	int level = 0;
	int max = player->mo->IntVar(NAME_MugShotMaxHealth);
	if (max < 0)
	{
		max = player->mo->GetMaxHealth();
	}
	else if (max == 0)
	{
		max = 100;
	}
	while (player->health < (accuracy - 1 - level) * (max / accuracy))
	{
		level++;
	}
	if (CurrentState != NULL)
	{
		int skin = player->userinfo.GetSkin();
		const char *skin_face = (stateflags & FMugShot::CUSTOM) ? nullptr : (player->morphTics ? (GetDefaultByType(player->MorphedPlayerClass))->NameVar(NAME_Face).GetChars() : Skins[skin].Face.GetChars());
		return CurrentState->GetCurrentFrameTexture(default_face, skin_face, level, angle);
	}
	return NULL;
}
