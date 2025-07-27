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
#include "c_dispatch.h"
#include "vectors.h"
#include "m_joy.h"
#include "configfile.h"
#include "i_interface.h"
#include "d_eventbase.h"
#include "cmdlib.h"
#include "printf.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR(Bool, joy_ps2raw)
EXTERN_CVAR(Bool, joy_dinput)
EXTERN_CVAR(Bool, joy_xinput)

extern const float JOYDEADZONE_DEFAULT = 0.1; // reduced from 0.25

extern const float JOYSENSITIVITY_DEFAULT = 1.0;

extern const float JOYTHRESH_DEFAULT = 0.05;
extern const float JOYTHRESH_TRIGGER = 0.05;
extern const float JOYTHRESH_STICK_X = 0.65;
extern const float JOYTHRESH_STICK_Y = 0.35;

extern const CubicBezier JOYCURVE[NUM_JOYCURVE] = {
	{{0.3, 0.0, 0.7, 0.4}}, // DEFAULT -> QUADRATIC

	{{0.0, 0.0, 1.0, 1.0}}, // LINEAR
	{{0.3, 0.0, 0.7, 0.4}}, // QUADRATIC
	{{0.5, 0.0, 0.7, 0.2}}, // CUBIC
};

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVARD(Bool, use_joystick, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL, "enables input from the joystick if it is present")
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

		mysnprintf(key + axislen, countof(key) - axislen, "threshold");
		value = GameConfig->GetValueForKey(key);
		if (value)
		{
			joy->SetAxisDigitalThreshold(i, (float)atof(value));
		}

		mysnprintf(key + axislen, countof(key) - axislen, "curve");
		value = GameConfig->GetValueForKey(key);
		if (value)
		{
			joy->SetAxisResponseCurve(i, (EJoyCurve)clamp(atoi(value), (int)JOYCURVE_CUSTOM, (int)NUM_JOYCURVE-1));
		}

		mysnprintf(key + axislen, countof(key) - axislen, "curve-x1");
		value = GameConfig->GetValueForKey(key);
		if (value)
		{
			joy->SetAxisResponseCurvePoint(i, 0, (float)atof(value));
		}

		mysnprintf(key + axislen, countof(key) - axislen, "curve-y1");
		value = GameConfig->GetValueForKey(key);
		if (value)
		{
			joy->SetAxisResponseCurvePoint(i, 1, (float)atof(value));
		}

		mysnprintf(key + axislen, countof(key) - axislen, "curve-x2");
		value = GameConfig->GetValueForKey(key);
		if (value)
		{
			joy->SetAxisResponseCurvePoint(i, 2, (float)atof(value));
		}

		mysnprintf(key + axislen, countof(key) - axislen, "curve-y2");
		value = GameConfig->GetValueForKey(key);
		if (value)
		{
			joy->SetAxisResponseCurvePoint(i, 3, (float)atof(value));
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
			if (!joy->IsAxisDigitalThresholdDefault(i))
			{
				mysnprintf(key + axislen, countof(key) - axislen, "threshold");
				mysnprintf(value, countof(value), "%g", joy->GetAxisDigitalThreshold(i));
				GameConfig->SetValueForKey(key, value);
			}
			if (!joy->IsAxisResponseCurveDefault(i))
			{
				mysnprintf(key + axislen, countof(key) - axislen, "curve");
				mysnprintf(value, countof(value), "%d", joy->GetAxisResponseCurve(i));
				GameConfig->SetValueForKey(key, value);
			}
			if (joy->GetAxisResponseCurve(i) == JOYCURVE_CUSTOM)
			{
				mysnprintf(key + axislen, countof(key) - axislen, "curve-x1");
				mysnprintf(value, countof(value), "%g", joy->GetAxisResponseCurvePoint(i, 0));
				GameConfig->SetValueForKey(key, value);
				mysnprintf(key + axislen, countof(key) - axislen, "curve-y1");
				mysnprintf(value, countof(value), "%g", joy->GetAxisResponseCurvePoint(i, 1));
				GameConfig->SetValueForKey(key, value);
				mysnprintf(key + axislen, countof(key) - axislen, "curve-x2");
				mysnprintf(value, countof(value), "%g", joy->GetAxisResponseCurvePoint(i, 2));
				GameConfig->SetValueForKey(key, value);
				mysnprintf(key + axislen, countof(key) - axislen, "curve-y2");
				mysnprintf(value, countof(value), "%g", joy->GetAxisResponseCurvePoint(i, 3));
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

CCMD (gamepad)
{
	int COMMAND = 1, IDENTIFIER = 2, VALUE = 3;
	int argc = argv.argc()-1;

	TArray<IJoystickConfig *> sticks;
	I_GetJoysticks(sticks);

	auto usage = []()
	{
		Printf(
			"usage:"
			"\n\tgamepad list"
			"\n\tgamepad reset       pad"
			"\n\tgamepad enabled     pad      [0|1]"
			"\n\tgamepad background  pad      [0|1]"
			"\n\tgamepad sensitivity pad      [float]"
			"\n\tgamepad deadzone    pad.axis [float]"
			"\n\tgamepad scale       pad.axis [float]"
			"\n\tgamepad threshold   pad.axis [float]"
			"\n\tgamepad curve       pad.axis [-1|0|1|2|3]"
			"\n\tgamepad curve-x1    pad.axis [float]"
			"\n\tgamepad curve-y1    pad.axis [float]"
			"\n\tgamepad curve-x2    pad.axis [float]"
			"\n\tgamepad curve-y2    pad.axis [float]"
			"\n"
		);
	};

	if (argc < COMMAND)
	{
		return usage();
	};

	FName command = argv[COMMAND];

	if (argc < IDENTIFIER)
	{
		if (command == "list")
		{
			for (int i = 0; i < sticks.SSize(); i++)
			{
				Printf("%d: '%s'\n", i, sticks[i]->GetName().GetChars());
				for (int j = 0; j < sticks[i]->GetNumAxes(); j++)
				{
					Printf("  %d.%d: '%s'\n", i, j, sticks[i]->GetAxisName(j));
				}
			}
			return;
		}
		return usage();
	}

	const char * id = argv[IDENTIFIER];
	const char * hasAxis = strchr(id, '.');

	int pad, axis;

	try {
		pad = (int)std::stod(id);

		if (pad < 0 || pad >= sticks.SSize())
		{
			return (void) Printf("Pad # out of range\n");
		}
	} catch (...) {
		return (void) Printf("Failed to parse pad #\n");
	}

	if (hasAxis)
	{
		try {
			axis = (int)std::stod(hasAxis+1);

			if (axis < 0 || axis >= sticks[pad]->GetNumAxes())
			{
				return (void) Printf("Axis # out of range\n");
			}
		} catch (...) {
			return (void) Printf("Failed to parse axis #\n");
		}
	}

	float value = 0;
	bool set = argc >= VALUE;

	if (set)
	{
		try {
			value = std::stod(argv[VALUE]);
		} catch (...) {
			return (void) Printf("Failed to parse args\n");
		}
	}

	if (command == "reset")
	{
		if (set) return usage();
		sticks[pad]->Reset();
		return;
	}
	if (command == "enabled")
	{
		if (set) sticks[pad]->SetEnabled((int)value);
		return (void) Printf("%d\n", sticks[pad]->GetEnabled());
	}
	if (command == "background")
	{
		if (set) sticks[pad]->SetEnabledInBackground((int)value);
		return (void) Printf("%d\n", sticks[pad]->GetEnabledInBackground());
	}
	if (command == "sensitivity")
	{
		if (set) sticks[pad]->SetSensitivity(value);
		return (void) Printf("%g\n", sticks[pad]->GetSensitivity());
	}
	if (command == "deadzone")
	{
		if (set) sticks[pad]->SetAxisDeadZone(axis, value);
		return (void) Printf("%g\n", sticks[pad]->GetAxisDeadZone(axis));
	}
	if (command == "scale")
	{
		if (set) sticks[pad]->SetAxisScale(axis, value);
		return (void) Printf("%g\n", sticks[pad]->GetAxisScale(axis));
	}
	if (command == "threshold")
	{
		if (set) sticks[pad]->SetAxisDigitalThreshold(axis, value);
		return (void) Printf("%g\n", sticks[pad]->GetAxisDigitalThreshold(axis));
	}
	if (command == "curve")
	{
		if (set) sticks[pad]->SetAxisResponseCurve(axis, (EJoyCurve)value);
		return (void) Printf("%d\n", sticks[pad]->GetAxisResponseCurve(axis));
	}
	if (command == "curve-x1")
	{
		if (set) sticks[pad]->SetAxisResponseCurvePoint(axis, 0, value);
		return (void) Printf("%g\n", sticks[pad]->GetAxisResponseCurvePoint(axis, 0));
	}
	if (command == "curve-y1")
	{
		if (set) sticks[pad]->SetAxisResponseCurvePoint(axis, 1, value);
		return (void) Printf("%g\n", sticks[pad]->GetAxisResponseCurvePoint(axis, 1));
	}
	if (command == "curve-x2")
	{
		if (set) sticks[pad]->SetAxisResponseCurvePoint(axis, 2, value);
		return (void) Printf("%g\n", sticks[pad]->GetAxisResponseCurvePoint(axis, 2));
	}
	if (command == "curve-y2")
	{
		if (set) sticks[pad]->SetAxisResponseCurvePoint(axis, 3, value);
		return (void) Printf("%g\n", sticks[pad]->GetAxisResponseCurvePoint(axis, 3));
	}

	return usage();
}

//===========================================================================
//
// Joy_ApplyResponseCurveBezier
//
// Applies cubic bezier easing function
// Curve is defined by control points [(0,0) (x1,y1) (x2,y2) (1,1)]
// https://developer.mozilla.org/en-US/docs/Web/CSS/easing-function/cubic-bezier
//
//===========================================================================

double Joy_ApplyResponseCurveBezier(const CubicBezier &curve, double input)
{
	// clamp + trivial cases
	if (input == 0) return 0;
	double sign = (input >= 0)? 1.0: -1.0;
	input = abs(input);
	input = (input > 1.0)? 1.0: input;
	if (input == 1.0) return sign*input;

	double t = input, T;
	float x1 = curve.x1, y1 = curve.y1, x2 = curve.x2, y2 = curve.y2;

	const int max_iter = 4;
	for (auto i = 0; i < max_iter; i++)
	{
		T = 1-t;

		double x = 3*T*T*t*x1 + 3*T*t*t*x2 + t*t*t;
		double dx = 3*T*T*x1 + 6*T*t*(x2-x1) + 3*t*t*(1-x2);

		// no div by 0
		if (abs(dx) < 0.00001) break;

		t = clamp(t - (x-input)/dx, 0.0, 1.0);
	}

	T = 1-t;
	t = 3*T*T*t*y1 + 3*T*t*t*y2 + t*t*t;

	return sign*t;
}

//===========================================================================
//
// Joy_ManageSingleAxis
//
//===========================================================================

double Joy_ManageSingleAxis(double axisval, double deadzone, double threshold, const CubicBezier &curve, uint8_t *buttons)
{
	uint8_t butt;

	// Cancel out deadzone.
	if (fabs(axisval) < deadzone)
	{
		axisval = 0;
		butt = 0;
	}
	else
	{
		// Make the dead zone the new 0.
		if (axisval < 0)
		{
			axisval = (axisval + deadzone) / (1.0 - deadzone);
			butt = 2;	// button minus
		}
		else
		{
			axisval = (axisval - deadzone) / (1.0 - deadzone);
			butt = 1;	// button plus
		}

		// Apply input response curve
		axisval = Joy_ApplyResponseCurveBezier(curve, axisval);

		if (abs(axisval) < threshold)
		{
			// Needs to meet digital threshold to send button
			butt = 0;
		}
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
// Joy_ManageThumbstick
//
// Given two axes and their settings, handle applying deadzone, digital
// threshold, input response curve, and setting button state. This is
// necessary, because doing the two axes individually creates awkward
// behavior (such as cross shaped deadzones, sluggish input response when
// pressing diagonally, so on).
//
//===========================================================================

double Joy_ManageThumbstick(
	double *axis_x, double *axis_y,
	double deadzone_x, double deadzone_y,
	double threshold_x, double threshold_y,
	const CubicBezier &curve_x, const CubicBezier &curve_y,
	uint8_t *buttons)
{
	double ret_x = *axis_x;
	double ret_y = *axis_y;

	double x_abs = abs(*axis_x);
	double y_abs = abs(*axis_y);
	double magnitude = sqrt((x_abs * x_abs) + (y_abs * y_abs));

	double ret_dist = 0;
	uint8_t ret_butt = 0;

	double xy_lerp = 0.5;
	if (magnitude > 0)
	{
		// Later it would be nice to have a single deadzone / threshold / curve setting
		// for the whole thumbstick instead of awkwardly trying to combine them, but
		// that requires re-considering how axes are exposed to the rest of the engine.
		// This will do for now.
		xy_lerp = abs(cos(atan2(ret_y, ret_x)));
	}

	double deadzone = (xy_lerp * deadzone_x) + ((1.0 - xy_lerp) * deadzone_y);
	if (magnitude < deadzone)
	{
		ret_x = 0;
		ret_y = 0;
	}
	else
	{
		// Make the dead zone the new 0.
		ret_dist = (magnitude - deadzone) / (1.0 - deadzone);

		const CubicBezier curve = {
			(float)((xy_lerp * curve_x.x1) + ((1.0 - xy_lerp) * curve_y.x1)),
			(float)((xy_lerp * curve_x.y1) + ((1.0 - xy_lerp) * curve_y.y1)),
			(float)((xy_lerp * curve_x.x2) + ((1.0 - xy_lerp) * curve_y.x2)),
			(float)((xy_lerp * curve_x.y2) + ((1.0 - xy_lerp) * curve_y.y2))
		};

		ret_dist = Joy_ApplyResponseCurveBezier(curve, ret_dist);

		ret_x = (ret_x / magnitude) * ret_dist;
		ret_y = (ret_y / magnitude) * ret_dist;

		double threshold = (xy_lerp * threshold_x) + ((1.0 - xy_lerp) * threshold_y);
		if (ret_dist >= threshold)
		{
			ret_butt = Joy_XYAxesToButtons(ret_x, ret_y);
		}
	}

	*axis_x = ret_x;
	*axis_y = ret_y;

	if (buttons != NULL)
	{
		*buttons = ret_butt;
	}

	return ret_dist;
}

//===========================================================================
//
// Joy_GenerateButtonEvent
//
// Send either a button up or button down event for supplied key code
//
//===========================================================================

void Joy_GenerateButtonEvent(bool down, EKeyCodes which)
{
	event_t event = { 0,0,0,0,0,0,0 };
	event.type = down ? EV_KeyDown : EV_KeyUp;
	event.data1 = which;
	D_PostEvent(&event);
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
		int mask = 1;
		for (int j = 0; j < numbuttons; mask <<= 1, ++j)
		{
			if (changed & mask)
			{
				Joy_GenerateButtonEvent(newbuttons & mask, static_cast<EKeyCodes>(base + j));
			}
		}
	}
}

void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, const int *keys)
{
	int changed = oldbuttons ^ newbuttons;
	if (changed != 0)
	{
		int mask = 1;
		for (int j = 0; j < numbuttons; mask <<= 1, ++j)
		{
			if (changed & mask)
			{
				Joy_GenerateButtonEvent(newbuttons & mask, static_cast<EKeyCodes>(keys[j]));
			}
		}
	}
}
