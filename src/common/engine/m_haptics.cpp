/*
** m_haptics.cpp
**
** Haptic feedback implementation
**
**---------------------------------------------------------------------------
**
** Copyright 2025 Marcus Minhorst
** Copyright 2025 ZDoom + GZDoom teams, and contributors
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/
**
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_haptics.h"
#include "name.h"
#include "printf.h"
#include "s_soundinternal.h"
#include "tarray.h"
#include "vm.h"
#include "zstring.h"

// MACROS ------------------------------------------------------------------

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

//==========================================================================
//
// rumble
//
// test command
//
//==========================================================================

CCMD (rumble)
{
	int count = argv.argc()-1;
	int ticks;
	double high_freq, low_freq, left_trig, right_trig;

	switch (count) {
		case 0: {
			TArray<FString> unused, used;

			{
				TMapIterator<FName, struct Haptics> it(RumbleDefinition);
				TMap<FName, struct Haptics>::Pair* pair;
				while (it.NextPair(pair)) unused.AddUnique(pair->Key.GetChars());
			}
			{
				TMapIterator<FName, FName> it(RumbleAlias);
				TMap<FName, FName>::Pair* pair;
				while (it.NextPair(pair)) unused.AddUnique(pair->Key.GetChars());
			}
			{
				TMapIterator<FName, FName> it(RumbleAlias);
				TMap<FName, FName>::Pair* pair;
				while (it.NextPair(pair))
				{
					if (unused.Contains(pair->Value.GetChars()))
						unused.Delete(unused.Find(pair->Value.GetChars()));
				}
			}
			{
				TMapIterator<FName, FName> it(RumbleMapping);
				TMap<FName, FName>::Pair* pair;
				Printf("Mappings:\n");
				while (it.NextPair(pair)) {
					used.AddUnique(pair->Value.GetChars());
					if (unused.Contains(pair->Value.GetChars()))
						unused.Delete(unused.Find(pair->Value.GetChars()));
					auto mapping = Joy_GetRumble(pair->Value);
					FString key = pair->Key.GetChars();
					FString val = pair->Value.GetChars();
					key.ToLower();
					val.ToUpper();
					FString a = FStringf("'%s'\t->\t'%s'", key.GetChars(), val.GetChars());
					FString b = mapping
						? FStringf(
							"{ %d %g %g %g %g }",
							mapping->ticks,
							mapping->high_frequency,
							mapping->low_frequency,
							mapping->left_trigger,
							mapping->right_trigger
						) : "[undefined]";
					Printf("\t%s\t->\t%s\n", a.GetChars(), b.GetChars());
				}
			}

			if (unused.Size() > 0)
			{
				Printf("Unused:\n");
				for (auto i:unused)
				{
					FString s = i.GetChars();
					s.ToUpper();
					Printf("\t'%s'\n", s.GetChars());
				}
			}

			Printf("Testing rumble for 5s\n");
			Joy_Rumble("", {5 * TICRATE, 1.0, 1.0, 1.0, 1.0});
		}
		break;
		case 1:
			Printf("Testing rumble for action '%s'\n", argv[1]);
			Joy_Rumble(argv[1]);
			break;
		case 5:
			try {
				ticks = std::stoi(argv[1], nullptr, 10);
				high_freq = static_cast <double> (std::stof(argv[2], nullptr));
				low_freq = static_cast <double> (std::stof(argv[3], nullptr));
				left_trig = static_cast <double> (std::stof(argv[4], nullptr));
				right_trig = static_cast <double> (std::stof(argv[5], nullptr));
			} catch (...) {
				Printf("Failed to parse args\n");
				return;
			}
			Printf("testing rumble with params (%d, %f, %f, %f, %f)\n", ticks, high_freq, low_freq, left_trig, right_trig);
			Joy_Rumble("", {ticks, high_freq, low_freq, left_trig, right_trig});
			break;
		default:
			Printf(
				"usage:\n  %s\n  %s\n  %s\n",
				"rumble",
				"rumble string_id",
				"rumble int_duration float_high_freq float_low_freq float_left_trig float_right_trigger"
			);
			break;
	}
}

//==========================================================================
//
// _Rumble
//
// VM wrapper for Joy_Rumble
//
//==========================================================================

void _Rumble(const int identifier) {
	Joy_Rumble(ENamedName(identifier));
}

//==========================================================================
//
// Rumble
//
// VM function for named Joy_Rumble
//
//==========================================================================

DEFINE_ACTION_FUNCTION_NATIVE(DHaptics, Rumble, _Rumble)
{
	PARAM_PROLOGUE;
	PARAM_INT(identifier);
	_Rumble(ENamedName(identifier));
	return 0;
}

//==========================================================================
//
// _RumbleDirect
//
// VM wrapper for Joy_Rumble
//
//==========================================================================

void _RumbleDirect(int source, int tic_count, double high_frequency, double low_frequency, double left_trigger, double right_trigger) {
	Joy_Rumble(ENamedName(source), {tic_count, high_frequency, low_frequency, left_trigger, right_trigger});
}

//==========================================================================
//
// Rumble
//
// VM function for direct Joy_Rumble
//
//==========================================================================

DEFINE_ACTION_FUNCTION_NATIVE(DHaptics, RumbleDirect, _RumbleDirect)
{
	PARAM_PROLOGUE;
	PARAM_INT(source);
	PARAM_INT(tic_count);
	PARAM_FLOAT(high_frequency);
	PARAM_FLOAT(low_frequency);
	PARAM_FLOAT(left_trigger);
	PARAM_FLOAT(right_trigger);
	_RumbleDirect(source, tic_count, high_frequency, low_frequency, left_trigger, right_trigger);
	return 0;
}
