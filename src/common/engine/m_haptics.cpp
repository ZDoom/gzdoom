/*
** m_haptics.cpp
** Haptic feedback implementation
**
**---------------------------------------------------------------------------
** Copyright 1998-2025 ZDoom + GZDoom teams, and contributors
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

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "c_cvars.h"
#include "doomstat.h"
#include "m_haptics.h"
#include "name.h"
#include "printf.h"
#include "s_soundinternal.h"
#include "tarray.h"
#include "zstring.h"

// MACROS ------------------------------------------------------------------

#ifndef MAX_TRY_DEPTH
#define MAX_TRY_DEPTH 8
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern bool AppActive;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

struct {
	int tic = gametic; // last tic processed
	bool dirty = false; // do we need to do something next tick ?
	bool enabled = true; // do we need to do anything ever ?
	bool active = true; // is the game currently not paused ?
	struct Haptics current = {0,0,0,0,0}; // current state of the controller
	TMap<FName, struct Haptics> channels; // active rumbles (that will be mixed)
} Haptics;

TMap<FName, struct Haptics> RumbleDefinition = {};
TMap<FName, FName> RumbleMapping = {};
TMap<FName, FName> RumbleAlias = {};

// fallback names. these exist in base sndinfo
const FName HapticIntense = "INTENSE",
			HapticHeavy = "HEAVY",
			HapticMedium = "MEDIUM",
			HapticLight = "LIGHT",
			HapticSubtle = "SUBTLE";

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVARD(Bool, haptics_debug, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "print diagnostics for haptic feedback");

// CODE --------------------------------------------------------------------

//==========================================================================
//
// Joy_GetMapping
//
// Takes a sound name, and find its corresponding rumble identifier.
//
//==========================================================================

const FName * Joy_GetMapping(const FName identifier)
{
	const FName * mapping = RumbleMapping.CheckKey(identifier);

	if (!mapping && identifier != "")
	{
		Printf(DMSG_WARNING, "Unknown rumble mapping '%s'\n", identifier.GetChars());
	}

	return mapping;
}

//==========================================================================
//
// Joy_GetRumble
//
// Takes a rumble identifier, and returns its haptics definition.
//
//==========================================================================

const struct Haptics * Joy_GetRumble(FName identifier)
{
	struct Haptics * rumble = RumbleDefinition.CheckKey(identifier);

	if (!rumble) {
		Printf(DMSG_ERROR, TEXTCOLOR_RED "Rumble mapping not found! '%s'\n", identifier.GetChars());
		return nullptr;
	}

	return rumble;
}

//==========================================================================
//
// Joy_AddRumbleType
//
// Ties a rumble identifier to a haptics definition.
//
//==========================================================================

void Joy_AddRumbleType(const FName identifier, const struct Haptics data)
{
	RumbleDefinition.Insert(identifier, data);
}

//==========================================================================
//
// Joy_AddRumbleAlias
//
// Adds an alternative rumble identifier for a haptics definition.
// Joy_ReadyRumbleMapping must be called after adding any aliases.
//
//==========================================================================

void Joy_AddRumbleAlias(const FName alias, const FName actual)
{
	RumbleAlias.Insert(alias, actual);
}

//==========================================================================
//
// Joy_MapRumbleType
//
// Ties a sound to a rumble identifier.
//
//==========================================================================

void Joy_MapRumbleType(const FName sound, const FName identifier)
{
	RumbleMapping.Insert(sound, identifier);
}

//==========================================================================
//
// Joy_ResetRumbleMapping
//
// Resets entire rumble state
//
//==========================================================================

void Joy_ResetRumbleMapping()
{
	RumbleMapping.Clear();
	RumbleAlias.Clear();
	RumbleDefinition.Clear();
	Haptics.channels.Clear();
	Haptics.current.high_frequency = Haptics.current.low_frequency =
	Haptics.current.left_trigger = Haptics.current.right_trigger =
	Haptics.current.ticks = 0;
	I_Rumble(0, 0, 0, 0);
}

//==========================================================================
//
// Joy_ReadyRumbleMapping
//
// Resets entire rumble state
//
//==========================================================================

void Joy_ReadyRumbleMapping()
{
	TArray<FName> found;
	TMapIterator<FName, FName> it(RumbleAlias);
	TMap<FName, FName>::Pair* pair;

	while (RumbleAlias.CountUsed())
	{
		while (it.NextPair(pair))
		{
			auto predefined = RumbleDefinition.CheckKey(pair->Key);
			if (predefined)
			{
				Printf(DMSG_ERROR, TEXTCOLOR_RED "Rumble alias trying to redefine mapping! '%s'\n", pair->Key.GetChars());
				continue;
			}

			auto mapping = RumbleDefinition.CheckKey(pair->Value);
			if (!mapping) continue;

			found.AddUnique(pair->Key);
			RumbleDefinition.Insert(pair->Key, *mapping);
		}
		it.Reset();

		if (found.Size() == 0)
		{
			FString list = "[";
			while (it.NextPair(pair))
				list.AppendFormat(" '%s'->'%s'", pair->Key.GetChars(), pair->Value.GetChars());
			Printf(DMSG_ERROR, TEXTCOLOR_RED "Circular rumble alias found! (%d) %s ]\n", RumbleAlias.CountUsed(), list.GetChars());
			break;
		}

		while (found.size() > 0) {
			RumbleAlias.Remove(found.Last());
			found.Pop();
		}
	}
}

//==========================================================================
//
// Joy_RumbleTick
//
// Called once per gametic. Manages rumble state.
//
//==========================================================================

void Joy_RumbleTick() {
	if (!Haptics.enabled) return;

	// pause detection
	if (AppActive != Haptics.active)
	{
		Haptics.active = AppActive;

		if (Haptics.active && Haptics.current.ticks != 0)
		{
			I_Rumble(
				Haptics.current.high_frequency,
				Haptics.current.low_frequency,
				Haptics.current.left_trigger,
				Haptics.current.right_trigger
			);
		}
		else
		{
			I_Rumble(0, 0, 0, 0);
		}
	}

	if (Haptics.tic >= gametic) return;

	Haptics.tic = gametic;

	// new value OR time elapsed
	Haptics.dirty |= Haptics.current.ticks != 0 && Haptics.current.ticks < Haptics.tic;

	if (!Haptics.dirty) return;

	// init
	Haptics.current.ticks = 0;
	Haptics.current.high_frequency = 0;
	Haptics.current.low_frequency = 0;
	Haptics.current.left_trigger = 0;
	Haptics.current.right_trigger = 0;

	TMapIterator<FName, struct Haptics> it(Haptics.channels);
	TMap<FName, struct Haptics>::Pair* pair;
	TArray<FName> stale;

	// remove old rumble data
	while (it.NextPair(pair))
	{
		if (pair->Value.ticks < Haptics.tic)
		{
			stale.Push(pair->Key);
		}
	}
	for (auto key: stale)
	{
		Haptics.channels.Remove(key);
	}

	it.Reset();
	while (it.NextPair(pair))
	{
		// grab upcoming event time
		Haptics.current.ticks = Haptics.current.ticks == 0
			? pair->Value.ticks
			: std::min(Haptics.current.ticks, pair->Value.ticks);

		// simple intensity mixing
		Haptics.current.high_frequency += pair->Value.high_frequency;
		Haptics.current.low_frequency += pair->Value.low_frequency;
		Haptics.current.left_trigger += pair->Value.left_trigger;
		Haptics.current.right_trigger += pair->Value.right_trigger;
	}

	// should this be clamped to [0,1]? Maybe a controller api will support "hdr" haptics lol
	// Haptics.current.high_frequency = std::min(std::max(0.0, Haptics.current.high_frequency), 1.0);
	// Haptics.current.low_frequency = std::min(std::max(0.0, Haptics.current.low_frequency), 1.0);
	// Haptics.current.left_trigger = std::min(std::max(0.0, Haptics.current.left_trigger), 1.0);
	// Haptics.current.right_trigger = std::min(std::max(0.0, Haptics.current.right_trigger), 1.0);

	I_Rumble(
		Haptics.current.high_frequency,
		Haptics.current.low_frequency,
		Haptics.current.left_trigger,
		Haptics.current.right_trigger
	);

	Haptics.dirty = false;
}

//==========================================================================
//
// Joy_Rumble
//
// Requests haptic feedback. Attenuation of >=1 results in no-op.
//
//==========================================================================

void Joy_Rumble(const FName source, const struct Haptics data, double attenuation)
{
	if (!Haptics.enabled) return;
	if (data.ticks <= 0) return;
	if (attenuation >= 1) return;

	float strength = 1 - (attenuation < 0? 0: attenuation);

	if (haptics_debug)
		Printf("r %s * %g\n", source.GetChars(), strength);

	// this will overwrite stuff from same source mapping (weapons/pistol not W_BULLET)
	Haptics.channels.Insert(source, {
		Haptics.tic + data.ticks + 1,
		data.high_frequency * strength,
		data.low_frequency * strength,
		data.left_trigger * strength,
		data.right_trigger * strength,
	});

	Haptics.dirty = true;
}

//==========================================================================
//
// Joy_Rumble
//
// Requests haptic feedback. Attenuation of >=1 results in no-op.
//
//==========================================================================

void Joy_Rumble(const FName identifier, double attenuation)
{
	const FName * mapping = Joy_GetMapping(identifier);

	if (mapping == nullptr) return;

	const struct Haptics * rumble = Joy_GetRumble(*mapping);

	if (rumble == nullptr) return;

	Joy_Rumble(identifier, * rumble, attenuation);
}
