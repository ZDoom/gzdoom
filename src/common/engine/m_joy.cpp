/*
**
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
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
#include "doomdef.h"
#include "doomstat.h"
#include "vectors.h"
#include "m_joy.h"
#include "configfile.h"
#include "i_interface.h"
#include "d_eventbase.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "printf.h"
#include "vm.h"
#include "zstring.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR(Bool, joy_ps2raw)
EXTERN_CVAR(Bool, joy_dinput)
EXTERN_CVAR(Bool, joy_xinput)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVARD(Bool, use_joystick, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL, "enables input from the joystick if it is present") 
{
#ifdef _WIN32
	joy_ps2raw->Callback();
	joy_dinput->Callback();
	joy_xinput->Callback();
#endif
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Bits 0 is X+, 1 is X-, 2 is Y+, and 3 is Y-.
static uint8_t JoyAngleButtons[8] = { 1, 1+4, 4, 2+4, 2, 2+8, 8, 1+8 };

// CODE --------------------------------------------------------------------

//==========================================================================
//
// IJoystickConfig - Virtual Destructor
//
//==========================================================================

IJoystickConfig::~IJoystickConfig()
{
}

//==========================================================================
//
// M_SetJoystickConfigSection
//
// Sets up the config for reading or writing this controller's axis config. 
//
//==========================================================================

static bool M_SetJoystickConfigSection(IJoystickConfig *joy, bool create, FConfigFile* GameConfig)
{
	FString id = "Joy:";
	id += joy->GetIdentifier();
	if (!GameConfig) return false;
	return GameConfig->SetSection(id.GetChars(), create);
}

//==========================================================================
//
// M_LoadJoystickConfig
//
//==========================================================================

bool M_LoadJoystickConfig(IJoystickConfig *joy)
{
	FConfigFile* GameConfig = sysCallbacks.GetConfig ? sysCallbacks.GetConfig() : nullptr;
	char key[32];
	const char *value;
	int axislen;
	int numaxes;

	joy->SetDefaultConfig();
	if (!M_SetJoystickConfigSection(joy, false, GameConfig))
	{
		return false;
	}

	assert(GameConfig);

	value = GameConfig->GetValueForKey("Enabled");
	if (value)
	{
		joy->SetEnabled((bool)atoi(value));
	}

	if(joy->AllowsEnabledInBackground())
	{
		value = GameConfig->GetValueForKey("EnabledInBackground");
		if (value)
		{
			joy->SetEnabledInBackground((bool)atoi(value));
		}
	}

	value = GameConfig->GetValueForKey("Sensitivity");
	if (value)
	{
		joy->SetSensitivity((float)atof(value));
	}

	numaxes = joy->GetNumAxes();
	for (int i = 0; i < numaxes; ++i)
	{
		axislen = mysnprintf(key, countof(key), "Axis%u", i);

		mysnprintf(key + axislen, countof(key) - axislen, "deadzone");
		value = GameConfig->GetValueForKey(key);
		if (value)
		{
			joy->SetAxisDeadZone(i, (float)atof(value));
		}

		mysnprintf(key + axislen, countof(key) - axislen, "scale");
		value = GameConfig->GetValueForKey(key);
		if (value)
		{
			joy->SetAxisScale(i, (float)atof(value));
		}

		mysnprintf(key + axislen, countof(key) - axislen, "map");
		value = GameConfig->GetValueForKey(key);
		if (value)
		{
			EJoyAxis gameaxis = (EJoyAxis)atoi(value);
			if (gameaxis < JOYAXIS_None || gameaxis >= NUM_JOYAXIS)
			{
				gameaxis = JOYAXIS_None;
			}
			joy->SetAxisMap(i, gameaxis);
		}
	}
	return true;
}

//==========================================================================
//
// M_SaveJoystickConfig
//
// Only saves settings that are not at their defaults.
//
//==========================================================================

void M_SaveJoystickConfig(IJoystickConfig *joy)
{
	FConfigFile* GameConfig = sysCallbacks.GetConfig ? sysCallbacks.GetConfig() : nullptr;
	char key[32], value[32];
	int axislen, numaxes;

	if (GameConfig != NULL && M_SetJoystickConfigSection(joy, true, GameConfig))
	{
		GameConfig->ClearCurrentSection();
		if (!joy->GetEnabled())
		{
			GameConfig->SetValueForKey("Enabled", "0");
		}

		if (!joy->AllowsEnabledInBackground() && joy->GetEnabledInBackground())
		{
			GameConfig->SetValueForKey("EnabledInBackground", "1");
		}
		
		if (!joy->IsSensitivityDefault())
		{
			mysnprintf(value, countof(value), "%g", joy->GetSensitivity());
			GameConfig->SetValueForKey("Sensitivity", value);
		}
		numaxes = joy->GetNumAxes();
		for (int i = 0; i < numaxes; ++i)
		{
			axislen = mysnprintf(key, countof(key), "Axis%u", i);

			if (!joy->IsAxisDeadZoneDefault(i))
			{
				mysnprintf(key + axislen, countof(key) - axislen, "deadzone");
				mysnprintf(value, countof(value), "%g", joy->GetAxisDeadZone(i));
				GameConfig->SetValueForKey(key, value);
			}
			if (!joy->IsAxisScaleDefault(i))
			{
				mysnprintf(key + axislen, countof(key) - axislen, "scale");
				mysnprintf(value, countof(value), "%g", joy->GetAxisScale(i));
				GameConfig->SetValueForKey(key, value);
			}
			if (!joy->IsAxisMapDefault(i))
			{
				mysnprintf(key + axislen, countof(key) - axislen, "map");
				mysnprintf(value, countof(value), "%d", joy->GetAxisMap(i));
				GameConfig->SetValueForKey(key, value);
			}
		}
		// If the joystick is entirely at its defaults, delete this section
		// so that we don't write out a lone section header.
		if (GameConfig->SectionIsEmpty())
		{
			GameConfig->DeleteCurrentSection();
		}
	}
}


//===========================================================================
//
// Joy_RemoveDeadZone
//
//===========================================================================

double Joy_RemoveDeadZone(double axisval, double deadzone, uint8_t *buttons)
{
	uint8_t butt;

	// Cancel out deadzone.
	if (fabs(axisval) < deadzone)
	{
		axisval = 0;
		butt = 0;
	}
	// Make the dead zone the new 0.
	else if (axisval < 0)
	{
		axisval = (axisval + deadzone) / (1.0 - deadzone);
		butt = 2;	// button minus
	}
	else
	{
		axisval = (axisval - deadzone) / (1.0 - deadzone);
		butt = 1;	// button plus
	}
	if (buttons != NULL)
	{
		*buttons = butt;
	}
	return axisval;
}

//===========================================================================
//
// Joy_XYAxesToButtons
//
// Given two axes, returns a button set for them. They should have already
// been sent through Joy_RemoveDeadZone. For axes that share the same
// physical stick, the angle the stick forms should determine whether or
// not the four component buttons are present. Going by deadzone alone gives
// you huge areas where you have to buttons pressed and thin strips where
// you only have one button pressed. For DirectInput gamepads, there is
// not much standard for how the right stick is presented, so we can only
// do this for the left stick for those, since X and Y axes are pretty
// standard. For XInput and Raw PS2 controllers, both sticks are processed
// through here.
//
//===========================================================================

int Joy_XYAxesToButtons(double x, double y)
{
	if (x == 0 && y == 0)
	{
		return 0;
	}
	double rad = atan2(y, x);
	if (rad < 0)
	{
		rad += 2*pi::pi();
	}
	// The circle is divided into eight segments for corresponding
	// button combinations. Each segment is pi/4 radians wide. We offset
	// by half this so that the segments are centered around the ideal lines
	// their buttons represent instead of being right on the lines.
	rad += pi::pi()/8;		// Offset
	rad *= 4/pi::pi();		// Convert range from [0,2pi) to [0,8)
	return JoyAngleButtons[int(rad) & 7];
}

//===========================================================================
//
// Joy_GenerateButtonEvents
//
// Provided two bitmasks for a set of buttons, generates events to reflect
// any changes from the old to new set, where base is the key for bit 0,
// base+1 is the key for bit 1, etc.
//
//===========================================================================

void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, int base)
{
	int changed = oldbuttons ^ newbuttons;
	if (changed != 0)
	{
		event_t ev = { 0, 0, 0, 0, 0, 0, 0 };
		int mask = 1;
		for (int j = 0; j < numbuttons; mask <<= 1, ++j)
		{
			if (changed & mask)
			{
				ev.data1 = base + j;
				ev.type = (newbuttons & mask) ? EV_KeyDown : EV_KeyUp;
				D_PostEvent(&ev);
			}
		}
	}
}

void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, const int *keys)
{
	int changed = oldbuttons ^ newbuttons;
	if (changed != 0)
	{
		event_t ev = { 0, 0, 0, 0, 0, 0, 0 };
		int mask = 1;
		for (int j = 0; j < numbuttons; mask <<= 1, ++j)
		{
			if (changed & mask)
			{
				ev.data1 = keys[j];
				ev.type = (newbuttons & mask) ? EV_KeyDown : EV_KeyUp;
				D_PostEvent(&ev);
			}
		}
	}
}


//===========================================================================
//
// Haptic feedback begins here
//
//===========================================================================

struct {
	int tic = gametic;
	bool dirty = false;
	struct Haptics current = {0,0,0,0,0};
	struct Haptics channel[1] {{0,0,0,0,0}};
	// todo: add multiple channels
} Haptics;

typedef std::function<void(void)> Lambda;

TMap<FString, Lambda> RumbleDefinition = {};
TMap<FString, FString> RumbleMapping = {};

const FString * Joy_GetMapping(const FString & idenifier, const FString & fallback)
{
	auto mapping = RumbleMapping.CheckKey(idenifier);

	if (!mapping)
	{
		if (!fallback.IsEmpty())
		{
			mapping = RumbleMapping.CheckKey(fallback);
		}

		if (!mapping)
		{
			Printf(DMSG_WARNING, "unknown rumble mapping '%s'\n", idenifier.GetChars());
		}
	}

	return mapping;
}

Lambda * Joy_GetRumble(const FString & idenifier)
{
	auto rumble = RumbleDefinition.CheckKey(idenifier);

	if (!rumble) {
		Printf(DMSG_ERROR, "rumble mapping not found! '%s'\n", idenifier.GetChars());
		return nullptr;
	}

	return rumble;
}

void Joy_AddRumbleType(const FString & idenifier, const struct Haptics data)
{
	RumbleDefinition.Insert(idenifier, [data]() { Joy_Rumble(data); });
}

void Joy_MapRumbleType(const FString & sound, const FString & idenifier)
{
	if (!Joy_GetRumble(idenifier)) return;

	RumbleMapping.Insert(sound, idenifier);
}

void Joy_RumbleTick() {
	if (Haptics.tic >= gametic) return;
	Haptics.tic = gametic;

	if (Haptics.current.ticks != 0 && Haptics.current.ticks < Haptics.tic)
	{
		Haptics.current.ticks = 0;
		Haptics.current.high_frequency = 0;
		Haptics.current.low_frequency = 0;
		Haptics.current.left_trigger = 0;
		Haptics.current.right_trigger = 0;
		Haptics.dirty = true;
	}

	if (!Haptics.dirty) return;

	if (Haptics.channel[0].ticks >= Haptics.tic)
	{
		Haptics.current.ticks = Haptics.channel[0].ticks;
		Haptics.current.high_frequency = Haptics.channel[0].high_frequency;
		Haptics.current.low_frequency = Haptics.channel[0].low_frequency;
		Haptics.current.left_trigger = Haptics.channel[0].left_trigger;
		Haptics.current.right_trigger = Haptics.channel[0].right_trigger;
	}

	I_Rumble(
		Haptics.current.high_frequency,
		Haptics.current.low_frequency,
		Haptics.current.left_trigger,
		Haptics.current.right_trigger
	);

	Haptics.dirty = false;
}

void Joy_Rumble(const struct Haptics data)
{
	if (data.ticks <= 0) return;

	Haptics.channel[0].ticks = Haptics.tic + data.ticks + 1;
	Haptics.channel[0].high_frequency = data.high_frequency;
	Haptics.channel[0].low_frequency = data.low_frequency;
	Haptics.channel[0].left_trigger = data.left_trigger;
	Haptics.channel[0].right_trigger = data.right_trigger;

	Haptics.dirty = true;
}

void Joy_Rumble(const FString & identifier, const FString & fallback)
{
	auto mapping = Joy_GetMapping(identifier, fallback);

	if (mapping == nullptr) return;

	auto rumble = Joy_GetRumble(*mapping);

	if (rumble == nullptr) return;

	(*rumble)();
}

CCMD (rumble)
{
	int count = argv.argc()-1;
	int ticks;
	double high_freq, low_freq, left_trig, right_trig;

	switch (count) {
		case 0:
			Printf("testing rumble for 5s\n");
			Joy_Rumble({5 * TICRATE, 1.0, 1.0, 1.0, 1.0});
			break;
		case 1:
			Printf("testing rumble for action '%s'\n", argv[1]);
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
			Joy_Rumble({ticks, high_freq, low_freq, left_trig, right_trig});
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

void _Rumble(const FString& identifier) {
	Joy_Rumble(identifier);
}

DEFINE_ACTION_FUNCTION_NATIVE(DHaptics, Rumble, _Rumble)
{
	PARAM_PROLOGUE;
	PARAM_STRING(identifier);
	_Rumble(identifier);
	return 0;
}

void _RumbleOr(const FString& identifier, const FString& fallback) {
	Joy_Rumble(identifier, fallback);
}

DEFINE_ACTION_FUNCTION_NATIVE(DHaptics, RumbleOr, _RumbleOr)
{
	PARAM_PROLOGUE;
	PARAM_STRING(identifier);
	PARAM_STRING(fallback);
	_RumbleOr(identifier, fallback);
	return 0;
}

void _RumbleDirect(int tic_count, double high_frequency, double low_frequency, double left_trigger, double right_trigger) {
	Joy_Rumble({tic_count, high_frequency, low_frequency, left_trigger, right_trigger});
}

DEFINE_ACTION_FUNCTION_NATIVE(DHaptics, RumbleDirect, _RumbleDirect)
{
	PARAM_PROLOGUE;
	PARAM_INT(tic_count);
	PARAM_FLOAT(high_frequency);
	PARAM_FLOAT(low_frequency);
	PARAM_FLOAT(left_trigger);
	PARAM_FLOAT(right_trigger);
	_RumbleDirect(tic_count, high_frequency, low_frequency, left_trigger, right_trigger);
	return 0;
}

//===========================================================================
//
// Haptic feedback ends here
//
//===========================================================================
