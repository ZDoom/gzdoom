/*
** i_xinput.cpp
**
** Handles direct input gamepads
**
**---------------------------------------------------------------------------
**
** Copyright 2005-2016 Randy Heit
** Copyright 2017-2025 GZDoom Maintainers and Contributors
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
**
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

#include "m_joy.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <limits.h>

#include "i_input.h"
#include "d_eventbase.h"

#include "gameconfigfile.h"
#include "m_argv.h"
#include "cmdlib.h"
#include "keydef.h"

// MACROS ------------------------------------------------------------------

// This macro is defined by newer versions of xinput.h. In case we are
// compiling with an older version, define it here.
#ifndef XUSER_MAX_COUNT
#define XUSER_MAX_COUNT                 4
#endif

// MinGW
#ifndef XINPUT_DLL
#define XINPUT_DLL_A  "xinput1_3.dll"
#define XINPUT_DLL_W L"xinput1_3.dll"
#ifdef UNICODE
    #define XINPUT_DLL XINPUT_DLL_W
#else
    #define XINPUT_DLL XINPUT_DLL_A
#endif
#endif

extern bool AppActive;

EXTERN_CVAR(Bool, use_joystick)

// TYPES -------------------------------------------------------------------

typedef DWORD (WINAPI *XInputGetStateType)(DWORD index, XINPUT_STATE *state);
typedef DWORD (WINAPI *XInputSetStateType)(DWORD index, XINPUT_STATE *state);
typedef DWORD (WINAPI *XInputGetCapabilitiesType)(DWORD index, DWORD flags, XINPUT_CAPABILITIES *caps);
typedef void  (WINAPI *XInputEnableType)(BOOL enable);

class FXInputController : public IJoystickConfig
{
public:
	FXInputController(int index);
	~FXInputController();

	void ProcessInput();
	void AddAxes(float axes[NUM_AXIS_CODES]);
	bool IsConnected() { return Connected; }

	// IJoystickConfig interface
	FString GetName();
	float GetSensitivity();
	virtual void SetSensitivity(float scale);

	bool HasHaptics();
	float GetHapticsStrength();
	void SetHapticsStrength(float strength);

	int GetNumAxes();
	float GetAxisDeadZone(int axis);
	const char *GetAxisName(int axis);
	float GetAxisScale(int axis);
	float GetAxisDigitalThreshold(int axis);
	EJoyCurve GetAxisResponseCurve(int axis);
	float GetAxisResponseCurvePoint(int axis, int point);

	void SetAxisDeadZone(int axis, float deadzone);
	void SetAxisScale(int axis, float scale);
	void SetAxisDigitalThreshold(int axis, float threshold);
	void SetAxisResponseCurve(int axis, EJoyCurve preset);
	void SetAxisResponseCurvePoint(int axis, int point, float value);

	bool IsSensitivityDefault();
	bool IsHapticsStrengthDefault();
	bool IsAxisDeadZoneDefault(int axis);
	bool IsAxisScaleDefault(int axis);
	bool IsAxisDigitalThresholdDefault(int axis);
	bool IsAxisResponseCurveDefault(int axis);

	bool GetEnabled();
	void SetEnabled(bool enabled);

	void Rumble(float low_freq, float high_freq);

	bool AllowsEnabledInBackground() { return true; }
	bool GetEnabledInBackground() { return EnabledInBackground; }
	void SetEnabledInBackground(bool enabled) { EnabledInBackground = enabled; }

	void SetDefaultConfig();
	FString GetIdentifier();

protected:
	struct AxisInfo
	{
		float Value;
		float DeadZone;
		float Multiplier;
		uint8_t ButtonValue;
		float DigitalThreshold;
		EJoyCurve ResponseCurvePreset;
		CubicBezier ResponseCurve;
	};
	struct DefaultAxisConfig
	{
		float DeadZone;
		float Multiplier;
		float DigitalThreshold;
		EJoyCurve ResponseCurvePreset;
	};
	XINPUT_VIBRATION Vibration;
	enum
	{
		AXIS_ThumbLX,
		AXIS_ThumbLY,
		AXIS_ThumbRX,
		AXIS_ThumbRY,
		AXIS_LeftTrigger,
		AXIS_RightTrigger,
		NUM_AXES
	};

	int Index;
	float Multiplier;
	AxisInfo Axes[NUM_AXES];
	static DefaultAxisConfig DefaultAxes[NUM_AXES];
	DWORD LastPacketNumber;
	int LastButtons;
	bool Connected;
	bool Enabled;
	bool EnabledInBackground;
	bool Haptics;
	float HapticStrength;

	void Attached();
	void Detached();

	static void ProcessThumbstick(int value1, AxisInfo *axis1, int value2, AxisInfo *axis2, int base);
	static void ProcessTrigger(int value, AxisInfo *axis, int base);
};

class FXInputManager : public FJoystickCollection
{
public:
	FXInputManager();
	~FXInputManager();

	bool GetDevice();
	void ProcessInput();
	bool WndProcHook(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam, LRESULT *result);
	void AddAxes(float axes[NUM_AXIS_CODES]);
	void GetDevices(TArray<IJoystickConfig *> &sticks);
	IJoystickConfig *Rescan();

	void Rumble(float low_freq, float high_freq);

protected:
	HMODULE XInputDLL;
	FXInputController *Devices[XUSER_MAX_COUNT];
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Bool, joy_xinput, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	I_StartupXInput();
	event_t ev = { EV_DeviceChange };
	D_PostEvent(&ev);
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static XInputGetStateType			InputGetState;
static XInputSetStateType			InputSetState;
static XInputGetCapabilitiesType	InputGetCapabilities;
static XInputEnableType				InputEnable;

static const char *AxisNames[] =
{
	"Left Thumb X Axis",
	"Left Thumb Y Axis",
	"Right Thumb X Axis",
	"Right Thumb Y Axis",
	"Left Trigger",
	"Right Trigger"
};

static const EAxisCodes AxisCodes[][2] =
{
	{ AXIS_CODE_PAD_LTHUMB_RIGHT, AXIS_CODE_PAD_LTHUMB_LEFT },
	{ AXIS_CODE_PAD_LTHUMB_DOWN, AXIS_CODE_PAD_LTHUMB_UP },
	{ AXIS_CODE_PAD_RTHUMB_RIGHT, AXIS_CODE_PAD_RTHUMB_LEFT },
	{ AXIS_CODE_PAD_RTHUMB_DOWN, AXIS_CODE_PAD_RTHUMB_UP },
	{ AXIS_CODE_PAD_LTRIGGER, AXIS_CODE_NULL },
	{ AXIS_CODE_PAD_RTRIGGER, AXIS_CODE_NULL }
};

FXInputController::DefaultAxisConfig FXInputController::DefaultAxes[NUM_AXES] =
{
	// Dead zone, multiplier, digitalthreshold, curveA, curveB
	{ XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32768.f,  JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_X, JOYCURVE_DEFAULT }, // ThumbLX
	{ XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32768.f,  JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_Y, JOYCURVE_DEFAULT }, // ThumbLY
	{ XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32768.f, JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_X, JOYCURVE_DEFAULT }, // ThumbRX
	{ XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32768.f, JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_Y, JOYCURVE_DEFAULT }, // ThumbRY
	{ XINPUT_GAMEPAD_TRIGGER_THRESHOLD / 256.f,      JOYSENSITIVITY_DEFAULT, JOYTHRESH_TRIGGER, JOYCURVE_DEFAULT }, // LeftTrigger
	{ XINPUT_GAMEPAD_TRIGGER_THRESHOLD / 256.f,      JOYSENSITIVITY_DEFAULT, JOYTHRESH_TRIGGER, JOYCURVE_DEFAULT }  // RightTrigger
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FXInputController - Constructor
//
//==========================================================================

FXInputController::FXInputController(int index)
{
	Index = index;
	Connected = false;
	Enabled = true;
	M_LoadJoystickConfig(this);
}

//==========================================================================
//
// FXInputController - Destructor
//
//==========================================================================

FXInputController::~FXInputController()
{
	// Send button up events before destroying this.
	ProcessThumbstick(0, &Axes[AXIS_ThumbLX], 0, &Axes[AXIS_ThumbLY], KEY_PAD_LTHUMB_RIGHT);
	ProcessThumbstick(0, &Axes[AXIS_ThumbRX], 0, &Axes[AXIS_ThumbRY], KEY_PAD_RTHUMB_RIGHT);
	ProcessTrigger(0, &Axes[AXIS_LeftTrigger], KEY_PAD_LTRIGGER);
	ProcessTrigger(0, &Axes[AXIS_RightTrigger], KEY_PAD_RTRIGGER);
	Joy_GenerateButtonEvents(LastButtons, 0, 16, KEY_PAD_DPAD_UP);
	M_SaveJoystickConfig(this);
}

//==========================================================================
//
// FXInputController :: ProcessInput
//
//==========================================================================

void FXInputController::ProcessInput()
{
	DWORD res;
	XINPUT_STATE state;

	res = InputGetState(Index, &state);
	if (res == ERROR_DEVICE_NOT_CONNECTED)
	{
		if (Connected)
		{
			Detached();
		}
		return;
	}
	if (res != ERROR_SUCCESS)
	{
		return;
	}
	if (!Connected)
	{
		Attached();
	}
	if (state.dwPacketNumber == LastPacketNumber || !Enabled)
	{ // Nothing has changed since last time.
		return;
	}

	// There is a hole in the wButtons bitmask where two buttons could fit.
	// As per the XInput documentation, "bits that are set but not defined ... are reserved,
	// and their state is undefined," so we clear them to make sure they're not set.
	// Our keymapping uses these two slots for the triggers as buttons.
	state.Gamepad.wButtons &= 0xF3FF;

	// Convert axes to floating point and cancel out deadzones.
	// XInput's Y axes are reversed compared to DirectInput.
	ProcessThumbstick(state.Gamepad.sThumbLX, &Axes[AXIS_ThumbLX],
					 -state.Gamepad.sThumbLY, &Axes[AXIS_ThumbLY], KEY_PAD_LTHUMB_RIGHT);
	ProcessThumbstick(state.Gamepad.sThumbRX, &Axes[AXIS_ThumbRX],
					 -state.Gamepad.sThumbRY, &Axes[AXIS_ThumbRY], KEY_PAD_RTHUMB_RIGHT);
	ProcessTrigger(state.Gamepad.bLeftTrigger, &Axes[AXIS_LeftTrigger], KEY_PAD_LTRIGGER);
	ProcessTrigger(state.Gamepad.bRightTrigger, &Axes[AXIS_RightTrigger], KEY_PAD_RTRIGGER);

	// Generate events for buttons that have changed.
	Joy_GenerateButtonEvents(LastButtons, state.Gamepad.wButtons, 16, KEY_PAD_DPAD_UP);

	LastPacketNumber = state.dwPacketNumber;
	LastButtons = state.Gamepad.wButtons;
}

//==========================================================================
//
// FXInputController :: ProcessThumbstick							STATIC
//
// Converts both axes of a thumb stick to floating point, cancels out the
// deadzone, and generates button up/down events for them.
//
//==========================================================================

void FXInputController::ProcessThumbstick(int value1, AxisInfo *axis1,
	int value2, AxisInfo *axis2, int base)
{
	uint8_t buttonstate;
	double axisval1, axisval2;

	axisval1 = (value1 - SHRT_MIN) * 2.0 / 65536 - 1.0;
	axisval2 = (value2 - SHRT_MIN) * 2.0 / 65536 - 1.0;

	Joy_ManageThumbstick(
		&axisval1, &axisval2,
		axis1->DeadZone, axis2->DeadZone,
		axis1->DigitalThreshold, axis2->DigitalThreshold,
		axis1->ResponseCurve, axis2->ResponseCurve,
		&buttonstate
	);

	axis1->Value = float(axisval1);
	axis2->Value = float(axisval2);

	// We store all four buttons in the first axis and ignore the second.
	Joy_GenerateButtonEvents(axis1->ButtonValue, buttonstate, 4, base);
	axis1->ButtonValue = buttonstate;
}

//==========================================================================
//
// FXInputController :: ProcessTrigger								STATIC
//
// Much like ProcessThumbstick, except triggers only go in the positive
// direction and have less precision.
//
//==========================================================================

void FXInputController::ProcessTrigger(int value, AxisInfo *axis, int base)
{
	uint8_t buttonstate;
	double axisval;

	axisval = Joy_ManageSingleAxis(
		value / 256.0,
		axis->DeadZone, axis->DigitalThreshold, axis->ResponseCurve,
		&buttonstate
	);

	axis->Value = float(axisval);
	Joy_GenerateButtonEvents(axis->ButtonValue, buttonstate, 1, base);
	axis->ButtonValue = buttonstate;
}

//==========================================================================
//
// FXInputController :: Attached
//
// This controller was just attached. Set all buttons and axes to 0.
//
//==========================================================================

void FXInputController::Attached()
{
	int i;

	Connected = true;
	LastPacketNumber = ~0;
	LastButtons = 0;
	for (i = 0; i < NUM_AXES; ++i)
	{
		Axes[i].Value = 0;
		Axes[i].ButtonValue = 0;
	}
	XINPUT_CAPABILITIES capabilities;
	if (XInputGetCapabilities(Index, XINPUT_FLAG_GAMEPAD, &capabilities) == ERROR_SUCCESS)
	{
		Haptics = capabilities.Vibration.wLeftMotorSpeed != 0 || capabilities.Vibration.wRightMotorSpeed != 0;
	}
	UpdateJoystickMenu(this);
}

//==========================================================================
//
// FXInputController :: Rumble
//
//==========================================================================

void FXInputController::Rumble(float low_freq, float high_freq)
{
	Vibration.wLeftMotorSpeed  = static_cast<unsigned short>(USHRT_MAX*clamp(high_freq*HapticStrength, 0.f, 1.f));
	Vibration.wRightMotorSpeed = static_cast<unsigned short>(USHRT_MAX*clamp( low_freq*HapticStrength, 0.f, 1.f));
	XInputSetState(Index, &Vibration);
}

//==========================================================================
//
// FXInputController :: Detached
//
// This controller was just detached. Send button ups for buttons that
// were pressed the last time we got input from it.
//
//==========================================================================

void FXInputController::Detached()
{
	int i;

	Connected = false;
	for (i = 0; i < 4; i += 2)
	{
		ProcessThumbstick(0, &Axes[i], 0, &Axes[i+1], KEY_PAD_LTHUMB_RIGHT + i*2);
	}
	for (i = 0; i < 2; ++i)
	{
		ProcessTrigger(0, &Axes[4+i], KEY_PAD_LTRIGGER + i);
	}
	Joy_GenerateButtonEvents(LastButtons, 0, 16, KEY_PAD_DPAD_UP);
	LastButtons = 0;
	UpdateJoystickMenu(NULL);
}

//==========================================================================
//
// FXInputController :: AddAxes
//
// Add the values of each axis to the game axes.
//
//==========================================================================

void FXInputController::AddAxes(float axes[NUM_AXIS_CODES])
{
	for (int i = 0; i < NUM_AXES; ++i)
	{
		// Add to the game axis.
		float axis_value = float(Axes[i].Value * Multiplier * Axes[i].Multiplier);
		int code = AXIS_CODE_NULL;

		if (axis_value > 0.0f)
		{
			code = AxisCodes[i][0];
		}
		else if (axis_value < 0.0f)
		{
			code = AxisCodes[i][1];
		}

		if (code != AXIS_CODE_NULL)
		{
			axes[code] += fabs(axis_value);
		}
	}
}

//==========================================================================
//
// FXInputController :: SetDefaultConfig
//
//==========================================================================

void FXInputController::SetDefaultConfig()
{
	Multiplier = JOYSENSITIVITY_DEFAULT;
	HapticStrength = JOYHAPSTRENGTH_DEFAULT;
	for (int i = 0; i < NUM_AXES; ++i)
	{
		Axes[i].DeadZone = DefaultAxes[i].DeadZone;
		Axes[i].Multiplier = DefaultAxes[i].Multiplier;
		Axes[i].DigitalThreshold = DefaultAxes[i].DigitalThreshold;
		Axes[i].ResponseCurve = JOYCURVE[DefaultAxes[i].ResponseCurvePreset];
		Axes[i].ResponseCurvePreset = DefaultAxes[i].ResponseCurvePreset;
	}
}

//==========================================================================
//
// FXInputController :: GetIdentifier
//
//==========================================================================

FString FXInputController::GetIdentifier()
{
	return FStringf("XI:%d", Index);
}

//==========================================================================
//
// FXInputController :: GetName
//
//==========================================================================

FString FXInputController::GetName()
{
	FString res;
	res.Format("XInput Controller #%d", Index + 1);
	return res;
}

//==========================================================================
//
// FXInputController :: GetSensitivity
//
//==========================================================================

float FXInputController::GetSensitivity()
{
	return Multiplier;
}

//==========================================================================
//
// FXInputController :: SetSensitivity
//
//==========================================================================

void FXInputController::SetSensitivity(float scale)
{
	Multiplier = scale;
}

//==========================================================================
//
// FXInputController :: IsSensitivityDefault
//
//==========================================================================

bool FXInputController::IsSensitivityDefault()
{
	return Multiplier == JOYSENSITIVITY_DEFAULT;
}

//==========================================================================
//
// FXInputController :: HasHaptics
//
//==========================================================================

bool FXInputController::HasHaptics()
{
	return Haptics;
}

//==========================================================================
//
// FXInputController :: GetHapticsStrength
//
//==========================================================================

float FXInputController::GetHapticsStrength()
{
	return HapticStrength;
}

//==========================================================================
//
// FXInputController :: SetHapticsStrength
//
//==========================================================================

void FXInputController::SetHapticsStrength(float strength)
{
	if (Haptics) HapticStrength = clamp(strength, 0.f, 2.f);
}

//==========================================================================
//
// FXInputController :: IsHapticsStrengthDefault
//
//==========================================================================

bool FXInputController::IsHapticsStrengthDefault()
{
	return true;
}

//==========================================================================
//
// FXInputController :: GetNumAxes
//
//==========================================================================

int FXInputController::GetNumAxes()
{
	return NUM_AXES;
}

//==========================================================================
//
// FXInputController :: GetAxisDeadZone
//
//==========================================================================

float FXInputController::GetAxisDeadZone(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return Axes[axis].DeadZone;
	}
	return 0;
}

//==========================================================================
//
// FXInputController :: GetAxisName
//
//==========================================================================

const char *FXInputController::GetAxisName(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return AxisNames[axis];
	}
	return "Invalid";
}

//==========================================================================
//
// FXInputController :: GetAxisScale
//
//==========================================================================

float FXInputController::GetAxisScale(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return Axes[axis].Multiplier;
	}
	return JOYSENSITIVITY_DEFAULT;
}

//==========================================================================
//
// FXInputController :: GetAxisDigitalThreshold
//
//==========================================================================

float FXInputController::GetAxisDigitalThreshold(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return Axes[axis].DigitalThreshold;
	}
	return JOYTHRESH_DEFAULT;
}

//==========================================================================
//
// FXInputController :: GetAxisResponseCurve
//
//==========================================================================

EJoyCurve FXInputController::GetAxisResponseCurve(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return Axes[axis].ResponseCurvePreset;
	}
	return JOYCURVE_DEFAULT;
}

//==========================================================================
//
// FXInputController :: GetAxisResponseCurvePoint
//
//==========================================================================

float FXInputController::GetAxisResponseCurvePoint(int axis, int point)
{
	if (unsigned(axis) < NUM_AXES && unsigned(point) < 4)
	{
		return Axes[axis].ResponseCurve.pts[point];
	}
	return 0;
}

//==========================================================================
//
// FXInputController :: SetAxisDeadZone
//
//==========================================================================

void FXInputController::SetAxisDeadZone(int axis, float deadzone)
{
	if (unsigned(axis) < NUM_AXES)
	{
		Axes[axis].DeadZone = clamp(deadzone, 0.f, 1.f);
	}
}

//==========================================================================
//
// FXInputController :: SetAxisScale
//
//==========================================================================

void FXInputController::SetAxisScale(int axis, float scale)
{
	if (unsigned(axis) < NUM_AXES)
	{
		Axes[axis].Multiplier = scale;
	}
}

//==========================================================================
//
// FXInputController :: SetAxisDigitalThreshold
//
//==========================================================================

void FXInputController::SetAxisDigitalThreshold(int axis, float threshold)
{
	if (unsigned(axis) < NUM_AXES)
	{
		Axes[axis].DigitalThreshold = threshold;
	}
}

//==========================================================================
//
// FXInputController :: SetAxisResponseCurve
//
//==========================================================================

void FXInputController::SetAxisResponseCurve(int axis, EJoyCurve preset)
{
	if (unsigned(axis) < NUM_AXES)
	{
		if (preset >= NUM_JOYCURVE || preset < JOYCURVE_CUSTOM) return;
		Axes[axis].ResponseCurvePreset = preset;
		if (preset == JOYCURVE_CUSTOM) return;
		Axes[axis].ResponseCurve = JOYCURVE[preset];
	}
}

//==========================================================================
//
// FXInputController :: SetAxisResponseCurvePoint
//
//==========================================================================

void FXInputController::SetAxisResponseCurvePoint(int axis, int point, float value)
{
	if (unsigned(axis) < NUM_AXES && unsigned(point) < 4)
	{
		Axes[axis].ResponseCurvePreset = JOYCURVE_CUSTOM;
		Axes[axis].ResponseCurve.pts[point] = value;
	}
}

//===========================================================================
//
// FXInputController :: IsAxisDeadZoneDefault
//
//===========================================================================

bool FXInputController::IsAxisDeadZoneDefault(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return Axes[axis].DeadZone == DefaultAxes[axis].DeadZone;
	}
	return true;
}

//===========================================================================
//
// FXInputController :: IsAxisScaleDefault
//
//===========================================================================

bool FXInputController::IsAxisScaleDefault(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return Axes[axis].Multiplier == DefaultAxes[axis].Multiplier;
	}
	return true;
}

//===========================================================================
//
// FXInputController :: IsAxisDigitalThresholdDefault
//
//===========================================================================

bool FXInputController::IsAxisDigitalThresholdDefault(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return Axes[axis].DigitalThreshold == DefaultAxes[axis].DigitalThreshold;
	}
	return true;
}

//===========================================================================
//
// FXInputController :: IsAxisResponseCurveDefault
//
//===========================================================================

bool FXInputController::IsAxisResponseCurveDefault(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return Axes[axis].ResponseCurvePreset == DefaultAxes[axis].ResponseCurvePreset;
	}
	return true;
}

//===========================================================================
//
// FXInputController :: GetEnabled
//
//===========================================================================

bool FXInputController::GetEnabled()
{
	return Enabled;
}

//===========================================================================
//
// FXInputController :: SetEnabled
//
//===========================================================================

void FXInputController::SetEnabled(bool enabled)
{
	Enabled = enabled;
}

//==========================================================================
//
// FXInputManager - Constructor
//
//==========================================================================

FXInputManager::FXInputManager()
{
	XInputDLL = LoadLibrary(XINPUT_DLL);
	if (XInputDLL != NULL)
	{
		InputGetState = (XInputGetStateType)GetProcAddress(XInputDLL, "XInputGetState");
		InputSetState = (XInputSetStateType)GetProcAddress(XInputDLL, "XInputSetState");
		InputGetCapabilities = (XInputGetCapabilitiesType)GetProcAddress(XInputDLL, "XInputGetCapabilities");
		InputEnable = (XInputEnableType)GetProcAddress(XInputDLL, "XInputEnable");
		// Treat XInputEnable() function as optional
		// It is not available in xinput9_1_0.dll which is XINPUT_DLL in modern SDKs
		// See https://msdn.microsoft.com/en-us/library/windows/desktop/hh405051(v=vs.85).aspx
		if (InputGetState == NULL || InputSetState == NULL || InputGetCapabilities == NULL)
		{
			FreeLibrary(XInputDLL);
			XInputDLL = NULL;
		}
	}
	for (int i = 0; i < XUSER_MAX_COUNT; ++i)
	{
		Devices[i] = (XInputDLL != NULL) ? new FXInputController(i) : NULL;
	}
}

//==========================================================================
//
// FXInputManager - Destructor
//
//==========================================================================

FXInputManager::~FXInputManager()
{
	for (int i = 0; i < XUSER_MAX_COUNT; ++i)
	{
		if (Devices[i] != NULL)
		{
			delete Devices[i];
		}
	}
	if (XInputDLL != NULL)
	{
		FreeLibrary(XInputDLL);
	}
}

//==========================================================================
//
// FXInputManager :: GetDevice
//
//==========================================================================

bool FXInputManager::GetDevice()
{
	return (XInputDLL != NULL);
}

//==========================================================================
//
// FXInputManager :: ProcessInput
//
// Process input for every attached device.
//
//==========================================================================

void FXInputManager::ProcessInput()
{
	for (int i = 0; i < XUSER_MAX_COUNT; ++i)
	{
		if(AppActive || Devices[i]->GetEnabledInBackground())
		{
			Devices[i]->ProcessInput();
		}
	}
}

//===========================================================================
//
// FXInputManager :: AddAxes
//
// Adds the state of all attached device axes to the passed array.
//
//===========================================================================

void FXInputManager::AddAxes(float axes[NUM_AXIS_CODES])
{
	for (int i = 0; i < XUSER_MAX_COUNT; ++i)
	{
		if (Devices[i]->IsConnected())
		{
			Devices[i]->AddAxes(axes);
		}
	}
}

//===========================================================================
//
// FXInputManager :: GetJoysticks
//
// Adds the IJoystick interfaces for each device we created to the sticks
// array, if they are detected as connected.
//
//===========================================================================

void FXInputManager::GetDevices(TArray<IJoystickConfig *> &sticks)
{
	for (int i = 0; i < XUSER_MAX_COUNT; ++i)
	{
		if (Devices[i]->IsConnected())
		{
			sticks.Push(Devices[i]);
		}
	}
}

//===========================================================================
//
// FXInputManager :: WndProcHook
//
// Enable and disable XInput as our window is (de)activated.
//
//===========================================================================

bool FXInputManager::WndProcHook(HWND hWnd, uint32_t message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	if (nullptr != InputEnable && message == WM_ACTIVATE)
	{
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			InputEnable(FALSE);
		}
		else
		{
			InputEnable(TRUE);
		}
	}
	return false;
}

//===========================================================================
//
// FXInputManager :: Rumble
//
//===========================================================================

void FXInputManager::Rumble(float low_freq, float high_freq) {
	for (int i = 0; i < XUSER_MAX_COUNT; ++i)
	{
		if (Devices[i] && Devices[i]->IsConnected() && Devices[i]->GetEnabled())
		{
			Devices[i]->Rumble(low_freq, high_freq);
		}
	}
}

//===========================================================================
//
// FXInputManager :: Rescan
//
//===========================================================================

IJoystickConfig *FXInputManager::Rescan()
{
	return NULL;
}

//===========================================================================
//
// I_StartupXInput
//
//===========================================================================

void I_StartupXInput()
{
	if (!joy_xinput || !use_joystick || Args->CheckParm("-nojoy"))
	{
		if (JoyDevices[INPUT_XInput] != NULL)
		{
			delete JoyDevices[INPUT_XInput];
			JoyDevices[INPUT_XInput] = NULL;
			UpdateJoystickMenu(NULL);
		}
	}
	else
	{
		if (JoyDevices[INPUT_XInput] == NULL)
		{
			FJoystickCollection *joys = new FXInputManager;
			if (joys->GetDevice())
			{
				JoyDevices[INPUT_XInput] = joys;
			}
			else
			{
				delete joys;
			}
		}
	}
}

void I_Rumble(double high_freq, double low_freq, double _left_trig, double _right_trig) {
	if (!use_joystick) return;

	FXInputManager* XInputManager = & static_cast<FXInputManager&> (*JoyDevices[INPUT_XInput]);
	if (XInputManager != NULL)
	{
		XInputManager->Rumble(high_freq, low_freq);
	}
}
