// HEADER FILES ------------------------------------------------------------

#include "m_joy.h"
#include "gameconfigfile.h"
#include "d_event.h"

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

CUSTOM_CVAR(Bool, use_joystick, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
#ifdef _WIN32
	joy_ps2raw.Callback();
	joy_dinput.Callback();
	joy_xinput.Callback();
#endif
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Bits 0 is X+, 1 is X-, 2 is Y+, and 3 is Y-.
static BYTE JoyAngleButtons[8] = { 1, 1+4, 4, 2+4, 2, 2+8, 8, 1+8 };

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

static bool M_SetJoystickConfigSection(IJoystickConfig *joy, bool create)
{
	FString id = "Joy:";
	id += joy->GetIdentifier();
	return GameConfig->SetSection(id, create);
}

//==========================================================================
//
// M_LoadJoystickConfig
//
//==========================================================================

bool M_LoadJoystickConfig(IJoystickConfig *joy)
{
	char key[32];
	const char *value;
	int axislen;
	int numaxes;

	joy->SetDefaultConfig();
	if (!M_SetJoystickConfigSection(joy, false))
	{
		return false;
	}
	value = GameConfig->GetValueForKey("Sensitivity");
	if (value != NULL)
	{
		joy->SetSensitivity((float)atof(value));
	}
	numaxes = joy->GetNumAxes();
	for (int i = 0; i < numaxes; ++i)
	{
		axislen = mysnprintf(key, countof(key), "Axis%u", i);

		mysnprintf(key + axislen, countof(key) - axislen, "deadzone");
		value = GameConfig->GetValueForKey(key);
		if (value != NULL)
		{
			joy->SetAxisDeadZone(i, (float)atof(value));
		}

		mysnprintf(key + axislen, countof(key) - axislen, "scale");
		value = GameConfig->GetValueForKey(key);
		if (value != NULL)
		{
			joy->SetAxisScale(i, (float)atof(value));
		}

		mysnprintf(key + axislen, countof(key) - axislen, "map");
		value = GameConfig->GetValueForKey(key);
		if (value != NULL)
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
	char key[32], value[32];
	int axislen, numaxes;

	if (M_SetJoystickConfigSection(joy, true))
	{
		GameConfig->ClearCurrentSection();
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

double Joy_RemoveDeadZone(double axisval, double deadzone, BYTE *buttons)
{
	BYTE butt;

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
		rad += 2*M_PI;
	}
	// The circle is divided into eight segments for corresponding
	// button combinations. Each segment is pi/4 radians wide. We offset
	// by half this so that the segments are centered around the ideal lines
	// their buttons represent instead of being right on the lines.
	rad += M_PI/8;		// Offset
	rad *= 4/M_PI;		// Convert range from [0,2pi) to [0,8)
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
