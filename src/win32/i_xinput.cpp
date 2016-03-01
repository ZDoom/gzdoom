#ifndef NO_XINPUT

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <xinput.h>
#include <malloc.h>
#include <limits.h>

#define USE_WINDOWS_DWORD
#include "i_input.h"
#include "i_system.h"
#include "d_event.h"
#include "d_gui.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomdef.h"
#include "doomstat.h"
#include "win32iface.h"
#include "templates.h"
#include "gameconfigfile.h"
#include "cmdlib.h"
#include "v_text.h"
#include "m_argv.h"

// MACROS ------------------------------------------------------------------

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
	void AddAxes(float axes[NUM_JOYAXIS]);
	bool IsConnected() { return Connected; }

	// IJoystickConfig interface
	FString GetName();
	float GetSensitivity();
	virtual void SetSensitivity(float scale);

	int GetNumAxes();
	float GetAxisDeadZone(int axis);
	EJoyAxis GetAxisMap(int axis);
	const char *GetAxisName(int axis);
	float GetAxisScale(int axis);

	void SetAxisDeadZone(int axis, float deadzone);
	void SetAxisMap(int axis, EJoyAxis gameaxis);
	void SetAxisScale(int axis, float scale);

	bool IsSensitivityDefault();
	bool IsAxisDeadZoneDefault(int axis);
	bool IsAxisMapDefault(int axis);
	bool IsAxisScaleDefault(int axis);

	void SetDefaultConfig();
	FString GetIdentifier();

protected:
	struct AxisInfo
	{
		float Value;
		float DeadZone;
		float Multiplier;
		EJoyAxis GameAxis;
		BYTE ButtonValue;
	};
	struct DefaultAxisConfig
	{
		float DeadZone;
		EJoyAxis GameAxis;
		float Multiplier;
	};
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
	WORD LastButtons;
	bool Connected;

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
	bool WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result);
	void AddAxes(float axes[NUM_JOYAXIS]);
	void GetDevices(TArray<IJoystickConfig *> &sticks);
	IJoystickConfig *Rescan();

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

FXInputController::DefaultAxisConfig FXInputController::DefaultAxes[NUM_AXES] =
{
	// Dead zone, game axis, multiplier
	{ XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32768.f, JOYAXIS_Side, 1 },		// ThumbLX
	{ XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32768.f, JOYAXIS_Forward, 1 },	// ThumbLY
	{ XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32768.f, JOYAXIS_Yaw, 1 },		// ThumbRX
	{ XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32768.f, JOYAXIS_Pitch, 0.75 },	// ThumbRY
	{ XINPUT_GAMEPAD_TRIGGER_THRESHOLD / 256.f, JOYAXIS_None, 0 },			// LeftTrigger
	{ XINPUT_GAMEPAD_TRIGGER_THRESHOLD / 256.f, JOYAXIS_None, 0 }			// RightTrigger
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
	if (state.dwPacketNumber == LastPacketNumber)
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
	BYTE buttonstate;
	double axisval1, axisval2;
	
	axisval1 = (value1 - SHRT_MIN) * 2.0 / 65536 - 1.0;
	axisval2 = (value2 - SHRT_MIN) * 2.0 / 65536 - 1.0;
	axisval1 = Joy_RemoveDeadZone(axisval1, axis1->DeadZone, NULL);
	axisval2 = Joy_RemoveDeadZone(axisval2, axis2->DeadZone, NULL);
	axis1->Value = float(axisval1);
	axis2->Value = float(axisval2);

	// We store all four buttons in the first axis and ignore the second.
	buttonstate = Joy_XYAxesToButtons(axisval1, axisval2);
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
	BYTE buttonstate;
	double axisval;
	
	axisval = Joy_RemoveDeadZone(value / 256.0, axis->DeadZone, &buttonstate);
	Joy_GenerateButtonEvents(axis->ButtonValue, buttonstate, 1, base);
	axis->ButtonValue = buttonstate;
	axis->Value = float(axisval);
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
	UpdateJoystickMenu(this);
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

void FXInputController::AddAxes(float axes[NUM_JOYAXIS])
{
	// Add to game axes.
	for (int i = 0; i < NUM_AXES; ++i)
	{
		axes[Axes[i].GameAxis] -= float(Axes[i].Value * Multiplier * Axes[i].Multiplier);
	}
}

//==========================================================================
//
// FXInputController :: SetDefaultConfig
//
//==========================================================================

void FXInputController::SetDefaultConfig()
{
	Multiplier = 1;
	for (int i = 0; i < NUM_AXES; ++i)
	{
		Axes[i].DeadZone = DefaultAxes[i].DeadZone;
		Axes[i].GameAxis = DefaultAxes[i].GameAxis;
		Axes[i].Multiplier = DefaultAxes[i].Multiplier;
	}
}

//==========================================================================
//
// FXInputController :: GetIdentifier
//
//==========================================================================

FString FXInputController::GetIdentifier()
{
	char id[16];
	mysnprintf(id, countof(id), "XI:%d", Index);
	return id;
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
	return Multiplier == 1;
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
// FXInputController :: GetAxisMap
//
//==========================================================================

EJoyAxis FXInputController::GetAxisMap(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return Axes[axis].GameAxis;
	}
	return JOYAXIS_None;
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
// FXInputController :: SetAxisMap
//
//==========================================================================

void FXInputController::SetAxisMap(int axis, EJoyAxis gameaxis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		Axes[axis].GameAxis = (unsigned(gameaxis) < NUM_JOYAXIS) ? gameaxis : JOYAXIS_None;
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
// FXInputController :: IsAxisMapDefault
//
//===========================================================================

bool FXInputController::IsAxisMapDefault(int axis)
{
	if (unsigned(axis) < NUM_AXES)
	{
		return Axes[axis].GameAxis == DefaultAxes[axis].GameAxis;
	}
	return true;
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
		if (InputGetState == NULL || InputSetState == NULL || InputGetCapabilities == NULL ||
			InputEnable == NULL)
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
		Devices[i]->ProcessInput();
	}
}

//===========================================================================
//
// FXInputManager :: AddAxes
//
// Adds the state of all attached device axes to the passed array.
//
//===========================================================================

void FXInputManager::AddAxes(float axes[NUM_JOYAXIS])
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

bool FXInputManager::WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	if (message == WM_ACTIVATE)
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

#else	// NO_XINPUT

#include "i_input.h"

void I_StartupXInput()
{
	JoyDevices[INPUT_XInput] = NULL;
}

#endif
