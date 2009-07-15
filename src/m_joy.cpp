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

// CODE --------------------------------------------------------------------

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
	// Cancel out deadzone.
	if (fabs(axisval) < deadzone)
	{
		axisval = 0;
		*buttons = 0;
	}
	// Make the dead zone the new 0.
	else if (axisval < 0)
	{
		axisval = (axisval + deadzone) / (1.0 - deadzone);
		*buttons = 2;	// button minus
	}
	else
	{
		axisval = (axisval - deadzone) / (1.0 - deadzone);
		*buttons = 1;	// button plus
	}
	return axisval;
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
		event_t ev = { 0 };
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
		event_t ev = { 0 };
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
