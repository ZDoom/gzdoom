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
	struct {
		double base = 1.0; // [0,1] -> number <1 turns down rumble strength
		double high_frequency = 1.0; // [0,inf)
		double low_frequency = 1.0; // [0,inf)
		double left_trigger = 1.0; // [0,inf)
		double right_trigger = 1.0; // [0,inf)
	} strength;
	struct Haptics current = {0,0,0,0,0}; // current state of the controller
	TMap<FName, struct Haptics> channels; // active rumbles (that will be mixed)
} Haptics;

TMap<FName, struct Haptics> RumbleDefinition = {};
TMap<FName, FName> RumbleMapping = {};
TMap<FName, FName> RumbleAlias = {};
TArray<FName> RumbleMissed = {};

// fallback names. these exist in base sndinfo
const FName HapticIntense = "INTENSE",
			HapticHeavy = "HEAVY",
			HapticMedium = "MEDIUM",
			HapticLight = "LIGHT",
			HapticSubtle = "SUBTLE";

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// for fine-control, if user wants/needs
// added because trigger haptics are much stronger on xbone controller than expected
CUSTOM_CVARD(Float, haptics_strength_lf, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "low frequency motor fine-control")
{
	if (self < 0) self = 0;
	Haptics.strength.low_frequency = self * Haptics.strength.base;
};
CUSTOM_CVARD(Float, haptics_strength_hf, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "high frequency motor fine-control")
{
	if (self < 0) self = 0;
	Haptics.strength.high_frequency = self * Haptics.strength.base;
};
CUSTOM_CVARD(Float, haptics_strength_lt, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "left trigger motor fine-control")
{
	if (self < 0) self = 0;
	Haptics.strength.left_trigger = self * Haptics.strength.base;
};
CUSTOM_CVARD(Float, haptics_strength_rt, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "right trigger motor fine-control")
{
	if (self < 0) self = 0;
	Haptics.strength.right_trigger = self * Haptics.strength.base;
};

CUSTOM_CVARD(Int, haptics_strength, 10, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "Translate linear haptics to audio taper")
{
	double l1 = self / 10.0;
	double l2 = l1 * l1;
	double l3 = l2 * l1;
	double m3 = 2; // cubed portion
	double m2 = -2; // squared portion
	double m1 = 1 - m2 - m3; // linear portion

	Haptics.enabled = self > 0;
	Haptics.strength.base = l1*m1 + l2*m2 + l3*m3;
	Haptics.strength.high_frequency = haptics_strength_hf * Haptics.strength.base;
	Haptics.strength.low_frequency = haptics_strength_lf * Haptics.strength.base;
	Haptics.strength.left_trigger = haptics_strength_lt * Haptics.strength.base;
	Haptics.strength.right_trigger = haptics_strength_rt * Haptics.strength.base;

	if (!Haptics.enabled) I_Rumble(0, 0, 0, 0);
}

CVARD(Bool, haptics_debug, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "print diagnostics for haptic feedback");

CUSTOM_CVARD(Int, haptics_compat, HAPTCOMPAT_MATCH, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "haptic feedback compatibility level")
{
	if (self < 0) self = 0;
	if (self >= NUM_HAPTCOMPAT) self = NUM_HAPTCOMPAT-1;
}

CVARD(Bool, haptics_do_menus,  true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "allow haptic feedback for menus");
CVARD(Bool, haptics_do_world,  true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "allow haptic feedback for things acting on player");
CVARD(Bool, haptics_do_damage, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "allow haptic feedback for things hurting player");
CVARD(Bool, haptics_do_action, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "allow haptic feedback for player doing things");

// CODE --------------------------------------------------------------------

//==========================================================================
//
// Joy_GuessMapping
//
// Takes a sound name, and tried to figure out a similar mapping.
// These are cached, so performance is probably not a huge deal
// This can absolutely be expanded/improved, and probably should.
//
//==========================================================================

const FName * Joy_GuessMapping(const FName identifier)
{
	FString text = identifier.GetChars();

	text.ToLower();
	// I would like to slugify here
	// Maybe one day

	auto search = [&text](TArray<FString> &strs) {
		for (auto str: strs)
		{
			if (text.IndexOf(str) != -1) return true;
		}
		return false;
	};

	static TArray<FString> intense = { "quake", "death", "gibbed" };
	static TArray<FString> heavy = { "teleport", "activate", "secret" };
	static TArray<FString> medium = { "success", "grunt", "land", "pain", "pkup", "pickup", "fist", "weapon", "fire"};
	static TArray<FString> light = { "push", "menu", "use", "fail", "open", "close", "eject" };

	if (search(intense)) return &HapticIntense;
	if (search(heavy)) return &HapticHeavy;
	if (search(medium)) return &HapticMedium;
	if (search(light)) return &HapticLight;

	return nullptr;
}

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
	FName actual = identifier;

	// try to grab an aliased sound
	// todo: mapping candidate for aliases
	if (!mapping)
	{
		auto id = soundEngine->FindSoundTentative(identifier.GetChars());

		// loop a couple layers deep, trying to find a candidate
		for (auto i = 0; i < MAX_TRY_DEPTH; i++) {
			if (!id.isvalid()) break;

			actual = soundEngine->GetSoundName(id);
			mapping = RumbleMapping.CheckKey(actual);

			if (mapping) break;

			// writable to be able to get random sound
			auto sfx = soundEngine->GetWritableSfx(id);
			// is there a way to get the actual random sound? Selecting the first is probably fine
			id = sfx->bRandomHeader
				? soundEngine->ResolveRandomSound(sfx)->Choices[0]
				: sfx->link;
		}
	}

	// convert unknown skinned sound to sound
	// is there a better way to do this?
	if (!mapping)
	{
		FString idString = identifier.GetChars();

		auto skindex = idString.IndexOf("*");

		if (skindex >= 0)
		{
			idString.Remove(0, skindex);
		}

		actual = FName(idString);

		mapping = RumbleMapping.CheckKey(actual);
	}

	bool replaced = actual != identifier && actual.IsValidName();

	auto warn = [&identifier]() {
		RumbleMissed.Push(identifier);
		Printf(DMSG_WARNING, "Unknown rumble mapping '%s'\n", identifier.GetChars());
	};

	if (!mapping && identifier != "")
	{
		switch (haptics_compat)
		{
		case HAPTCOMPAT_NONE: // sometimes warn for devs
			if (!RumbleMissed.Contains(identifier)) warn();
			break;
		case HAPTCOMPAT_WARN: // always warn for authors
			Printf(TEXTCOLOR_ORANGE "Unknown rumble mapping '%s'\n", identifier.GetChars());
			break;
		case HAPTCOMPAT_MATCH: // try our best to choose correct
			if ((mapping = Joy_GuessMapping(identifier))) replaced = true;
			else warn();
			break;
		case HAPTCOMPAT_ALL: // simple fallback
			mapping = &HapticMedium;
			replaced = true;
			break;
		}
	}

	if (mapping && replaced)
	{
		// cache replacement
		RumbleMapping.Insert(identifier, *mapping);
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

	if (!rumble && !RumbleMissed.Contains(identifier)) {
		RumbleMissed.Push(identifier);
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
	RumbleMissed.Clear();
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
		data.high_frequency * Haptics.strength.high_frequency * strength,
		data.low_frequency * Haptics.strength.low_frequency * strength,
		data.left_trigger * Haptics.strength.left_trigger * strength,
		data.right_trigger * Haptics.strength.right_trigger * strength,
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
