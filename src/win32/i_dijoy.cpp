// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x800
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <dinput.h>
#ifndef __GNUC__
#include <wbemidl.h>
#endif
#include <oleauto.h>
#include <dbt.h>
#include <malloc.h>

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
#include "m_menu.h"
#include "templates.h"
#include "gameconfigfile.h"
#include "cmdlib.h"
#include "v_text.h"
#include "m_argv.h"

// WBEMIDL BITS -- because w32api doesn't have this, either -----------------

#ifdef __GNUC__
struct IWbemClassObject : public IUnknown
{
public:
	virtual HRESULT __stdcall GetQualifierSet() = 0;
	virtual HRESULT __stdcall Get(LPCWSTR wszName, long lFlags, VARIANT *pVal, long *pType, long *plFlavor) = 0;
	virtual HRESULT __stdcall Put() = 0;
	virtual HRESULT __stdcall Delete() = 0;
	virtual HRESULT __stdcall GetNames() = 0;
	virtual HRESULT __stdcall BeginEnumeration() = 0;
	virtual HRESULT __stdcall Next() = 0;
	virtual HRESULT __stdcall EndEnumeration() = 0;
	virtual HRESULT __stdcall GetPropertyQualifierSet() = 0;
	virtual HRESULT __stdcall Clone() = 0;
	virtual HRESULT __stdcall GetObjectText() = 0;
	virtual HRESULT __stdcall SpawnDerivedClass() = 0;
	virtual HRESULT __stdcall SpawnInstance() = 0;
	virtual HRESULT __stdcall CompareTo() = 0;
	virtual HRESULT __stdcall GetPropertyOrigin() = 0;
	virtual HRESULT __stdcall InheritsFrom() = 0;
	virtual HRESULT __stdcall GetMethod() = 0;
	virtual HRESULT __stdcall PutMethod() = 0;
	virtual HRESULT __stdcall DeleteMethod() = 0;
	virtual HRESULT __stdcall BeginMethodEnumeration() = 0;
	virtual HRESULT __stdcall NextMethod() = 0;
	virtual HRESULT __stdcall EndMethodEnumeration() = 0;
	virtual HRESULT __stdcall GetMethodQualifierSet() = 0;
	virtual HRESULT __stdcall GetMethodOrigin() = 0;
};

struct IEnumWbemClassObject : public IUnknown
{
public:
	virtual HRESULT __stdcall Reset() = 0;
	virtual HRESULT __stdcall Next(long lTimeout, ULONG uCount,
		IWbemClassObject **apObjects, ULONG *puReturned) = 0;
	virtual HRESULT __stdcall NextAsync() = 0;
	virtual HRESULT __stdcall Clone() = 0;
	virtual HRESULT __stdcall Skip(long lTimeout, ULONG nCount) = 0;
};

struct IWbemServices : public IUnknown
{
public:
	virtual HRESULT __stdcall OpenNamespace() = 0;
	virtual HRESULT __stdcall CancelAsyncCall() = 0;
	virtual HRESULT __stdcall QueryObjectSink() = 0;
	virtual HRESULT __stdcall GetObject() = 0;
	virtual HRESULT __stdcall GetObjectAsync() = 0;
	virtual HRESULT __stdcall PutClass() = 0;
	virtual HRESULT __stdcall PutClassAsync() = 0;
	virtual HRESULT __stdcall DeleteClass() = 0;
	virtual HRESULT __stdcall DeleteClassAsync() = 0;
	virtual HRESULT __stdcall CreateClassEnum() = 0;
	virtual HRESULT __stdcall CreateClassEnumAsync() = 0;
	virtual HRESULT __stdcall PutInstance() = 0;
	virtual HRESULT __stdcall PutInstanceAsync() = 0;
	virtual HRESULT __stdcall DeleteInstance() = 0;
	virtual HRESULT __stdcall DeleteInstanceAsync() = 0;
	virtual HRESULT __stdcall CreateInstanceEnum(
		const BSTR strFilter, long lFlags, void *pCtx, IEnumWbemClassObject **ppEnum) = 0;
	virtual HRESULT __stdcall CreateInstanceEnumAsync() = 0;
	virtual HRESULT __stdcall ExecQuery() = 0;
	virtual HRESULT __stdcall ExecQueryAsync() = 0;
	virtual HRESULT __stdcall ExecNotificationQuery() = 0;
	virtual HRESULT __stdcall ExecNotificationQueryAsync() = 0;
	virtual HRESULT __stdcall ExecMethod() = 0;
	virtual HRESULT __stdcall ExecMethodAsync() = 0;
};

struct IWbemLocator : public IUnknown
{
public:
	virtual HRESULT __stdcall ConnectServer(
		const BSTR strNetworkResource,
		const BSTR strUser,
		const BSTR strPassword,
		const BSTR strLocale,
		long lSecurityFlags,
		const BSTR strAuthority,
		void *pCtx,
		IWbemServices **ppNamespace) = 0;
};
#endif

// MACROS ------------------------------------------------------------------

#define DEFAULT_DEADZONE			0.25f

// TYPES -------------------------------------------------------------------

class FDInputJoystick : public FInputDevice, IJoystickConfig
{
public:
	FDInputJoystick(const GUID *instance, FString &name);
	~FDInputJoystick();

	bool GetDevice();
	void ProcessInput();
	void AddAxes(float axes[NUM_JOYAXIS]);
	void SaveConfig();
	bool LoadConfig();
	FString GetIdentifier();
	void SetDefaultConfig();

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

protected:
	struct AxisInfo
	{
		FString Name;
		GUID Guid;
		DWORD Type;
		DWORD Ofs;
		LONG Min, Max;
		float Value;
		float DeadZone;
		float Multiplier;
		EJoyAxis GameAxis;
		BYTE ButtonValue;
	};
	struct ButtonInfo
	{
		FString Name;
		GUID Guid;
		DWORD Type;
		DWORD Ofs;
		BYTE Value;
	};

	LPDIRECTINPUTDEVICE8 Device;
	GUID Instance;
	FString Name;
	bool Marked;
	float Multiplier;
	int Warmup;
	TArray<AxisInfo> Axes;
	TArray<ButtonInfo> Buttons;
	TArray<ButtonInfo> POVs;

	DIOBJECTDATAFORMAT *Objects;
	DIDATAFORMAT DataFormat;

	static BOOL CALLBACK EnumObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef);
	void OrderAxes();
	bool ReorderAxisPair(const GUID &x, const GUID &y, int pos);
	HRESULT SetDataFormat();
	bool SetConfigSection(bool create);

	friend class FDInputJoystickManager;
};

class FDInputJoystickManager : public FJoystickCollection
{
public:
	FDInputJoystickManager();
	~FDInputJoystickManager();

	bool GetDevice();
	void ProcessInput();
	bool WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result);
	void AddAxes(float axes[NUM_JOYAXIS]);
	void GetDevices(TArray<IJoystickConfig *> &sticks);

protected:
	struct Enumerator
	{
		GUID Instance;
		FString Name;
	};
	TArray<FDInputJoystick *> Devices;

	FDInputJoystick *EnumDevices();

	static BOOL CALLBACK EnumCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);
	static int STACK_ARGS NameSort(const void *a, const void *b);
	static int STACK_ARGS GUIDSort(const void *a, const void *b);
	static bool IsXInputDevice(const GUID *guid);
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void UpdateJoystickMenu();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void MapAxis(FIntCVar &var, int num);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern menu_t JoystickMenu;
extern LPDIRECTINPUT8 g_pdi;
extern HWND Window;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Bool,  use_joystick,			false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
FJoystickCollection *JoyDevices[NUM_JOYDEVICES];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const BYTE POVButtons[9] = { 0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09, 0x00 };

//("dc12a687-737f-11cf-884d-00aa004b2e24")
static const IID IID_IWbemLocator =		{ 0xdc12a687, 0x737f, 0x11cf,
	{ 0x88, 0x4d, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24 } };

//("4590f811-1d3a-11d0-891f-00aa004b2e24")
static const CLSID CLSID_WbemLocator =	{ 0x4590f811, 0x1d3a, 0x11d0,
	{ 0x89, 0x1f, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24 } };

// CODE --------------------------------------------------------------------

//===========================================================================
//
// FDInputJoystick - Constructor
//
//===========================================================================

FDInputJoystick::FDInputJoystick(const GUID *instance, FString &name)
{
	Device = NULL;
	DataFormat.rgodf = NULL;
	Instance = *instance;
	Name = name;
	Marked = false;
}

//===========================================================================
//
// FDInputJoystick - Destructor
//
//===========================================================================

FDInputJoystick::~FDInputJoystick()
{
	SAFE_RELEASE(Device);
	if (DataFormat.rgodf != NULL)
	{
		delete[] DataFormat.rgodf;
	}
}

//===========================================================================
//
// FDInputJoystick :: GetDevice
//
//===========================================================================

bool FDInputJoystick::GetDevice()
{
	HRESULT hr;

	if (g_pdi == NULL)
	{
		return false;
	}
	hr = g_pdi->CreateDevice(Instance, &Device, NULL);
	if (FAILED(hr) || Device == NULL)
	{
		return false;
	}
	hr = Device->EnumObjects(EnumObjectsCallback, this, DIDFT_ABSAXIS | DIDFT_BUTTON | DIDFT_POV);
	OrderAxes();
	hr = SetDataFormat();
	if (FAILED(hr))
	{
		Printf(TEXTCOLOR_ORANGE "Setting data format for %s failed.\n", Name.GetChars());
		return false;
	}
	hr = Device->SetCooperativeLevel(Window, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(hr))
	{
		Printf(TEXTCOLOR_ORANGE "Setting cooperative level for %s failed.\n", Name.GetChars());
		return false;
	}
	Device->Acquire();
	LoadConfig();
	Warmup = 4;
	return true;
}

//===========================================================================
//
// FDInputJoystick :: ProcessInput
//
// Send button events and record axes for later.
//
//===========================================================================

void FDInputJoystick::ProcessInput()
{
	HRESULT hr;
	BYTE *state;
	unsigned i;
	event_t ev;

	if (Device == NULL)
	{
		return;
	}

	hr = Device->Poll();
	if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
	{
		hr = Device->Acquire();
	}
	if (FAILED(hr))
	{
		return;
	}

	state = (BYTE *)alloca(DataFormat.dwDataSize);
	hr = Device->GetDeviceState(DataFormat.dwDataSize, state);
	if (FAILED(hr))
		return;

	// Some controllers send false values when they are first
	// initialized, so give some time for them to get past that
	// before we pay any attention to their state.
	if (Warmup > 0)
	{
		Warmup--;
		return;
	}

	// Convert axis values to floating point and save them for a later call
	// to AddAxes(). Axes that are past their dead zone will also be translated
	// into button presses.
	for (i = 0; i < Axes.Size(); ++i)
	{
		AxisInfo *info = &Axes[i];
		LONG value = *(LONG *)(state + info->Ofs);
		double axisval;
		BYTE buttonstate;

		// Scale to [-1.0, 1.0]
		axisval = (value - info->Min) * 2.0 / (info->Max - info->Min) - 1.0;
		// Cancel out dead zone
		axisval = Joy_RemoveDeadZone(axisval, info->DeadZone, &buttonstate);
		info->Value = float(axisval);
		if (i < NUM_JOYAXISBUTTONS)
		{
			Joy_GenerateButtonEvents(info->ButtonValue, buttonstate, 2, KEY_JOYAXIS1PLUS + i*2);
		}
		info->ButtonValue = buttonstate;
	}

	// Compare button states and generate events for buttons that have changed.
	memset(&ev, 0, sizeof(ev));
	for (i = 0; i < Buttons.Size(); ++i)
	{
		ButtonInfo *info = &Buttons[i];
		BYTE newstate = *(BYTE *)(state + info->Ofs) & 0x80;
		if (newstate != info->Value)
		{
			info->Value = newstate;
			ev.data1 = KEY_FIRSTJOYBUTTON + i;
			ev.type = (newstate != 0) ? EV_KeyDown : EV_KeyUp;
			D_PostEvent(&ev);
		}
	}

	// POV hats are treated as a set of four buttons, because if it's a
	// D-pad, that's exactly what it is.
	for (i = 0; i < POVs.Size(); ++i)
	{
		ButtonInfo *info = &POVs[i];
		DWORD povangle = *(DWORD *)(state + info->Ofs);
		int pov;

		// Smoosh POV angles down into octants. 8 is centered.
		pov = (LOWORD(povangle) == 0xFFFF) ? 8 : ((povangle + 2250) % 36000) / 4500;

		// Convert octant to one or two buttons needed to represent it.
		pov = POVButtons[pov];

		// Send events for POV "buttons" that have changed.
		Joy_GenerateButtonEvents(info->Value, pov, 4, KEY_JOYPOV1_UP + i*4);
		info->Value = pov;
	}
}

//===========================================================================
//
// FDInputJoystick :: AddAxes
//
// Add the values of each axis to the game axes.
//
//===========================================================================

void FDInputJoystick::AddAxes(float axes[NUM_JOYAXIS])
{
	float mul = Multiplier;
	if (Button_Speed.bDown)
	{
		mul *= 0.5f;
	}

	for (unsigned i = 0; i < Axes.Size(); ++i)
	{
		// Add to the game axis.
		axes[Axes[i].GameAxis] -= float(Axes[i].Value * mul * Axes[i].Multiplier);
	}
}

//===========================================================================
//
// FDInputJoystick :: EnumObjectsCallback							STATIC
//
// Finds all axes, buttons, and hats on the controller.
//
//===========================================================================

BOOL CALLBACK FDInputJoystick::EnumObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
	FDInputJoystick *joy = (FDInputJoystick *)pvRef;

	if (lpddoi->guidType == GUID_Button)
	{
		ButtonInfo info;
		info.Name = lpddoi->tszName;
		info.Guid = lpddoi->guidType;
		info.Type = lpddoi->dwType;
		info.Ofs = 0;
		info.Value = 0;
		// We don't have the key labels necessary to support more than 128
		// joystick buttons. This is what DIJOYSTATE2 offers, so we
		// probably don't need to worry about any devices with more than
		// that.
		if (joy->Buttons.Size() < 128)
		{
			joy->Buttons.Push(info);
		}
	}
	else if (lpddoi->guidType == GUID_POV)
	{
		ButtonInfo info;
		info.Name = lpddoi->tszName;
		info.Guid = lpddoi->guidType;
		info.Type = lpddoi->dwType;
		info.Ofs = 0;
		info.Value = 0;
		// We don't have the key labels necessary to support more than 4
		// hats. I don't know any devices with more than 1, and the
		// standard DirectInput DIJOYSTATE does not support more than 4
		// hats either, so this is probably a non-issue.
		if (joy->POVs.Size() < 4)
		{
			joy->POVs.Push(info);
		}
	}
	else
	if (lpddoi->guidType == GUID_XAxis ||
		lpddoi->guidType == GUID_YAxis ||
		lpddoi->guidType == GUID_ZAxis ||
		lpddoi->guidType == GUID_RxAxis ||
		lpddoi->guidType == GUID_RyAxis ||
		lpddoi->guidType == GUID_RzAxis ||
		lpddoi->guidType == GUID_Slider)
	{
		DIPROPRANGE diprg;
		AxisInfo info;

		diprg.diph.dwSize = sizeof(DIPROPRANGE);
		diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		diprg.diph.dwObj = lpddoi->dwType;
		diprg.diph.dwHow = DIPH_BYID;
		diprg.lMin = 0;
		diprg.lMax = 0;
		joy->Device->GetProperty(DIPROP_RANGE, &diprg.diph);

		info.Name = lpddoi->tszName;
		info.Guid = lpddoi->guidType;
		info.Type = lpddoi->dwType;
		info.Ofs = 0;
		info.Min = diprg.lMin;
		info.Max = diprg.lMax;
		info.GameAxis = JOYAXIS_None;
		info.Value = 0;
		info.ButtonValue = 0;
		joy->Axes.Push(info);
	}
	return DIENUM_CONTINUE;
}

//===========================================================================
//
// FDInputJoystick :: OrderAxes
//
// Try to put the axes in some sort of sane order. X and Y axes are pretty
// much standard. Unfortunately, the rest are entirely up to the
// manufacturers to decide how they want to assign them.
//
//===========================================================================

void FDInputJoystick::OrderAxes()
{
	// Make X,Y the first pair.
	if (!ReorderAxisPair(GUID_XAxis, GUID_YAxis, 0))
	{
		return;
	}
	// The second pair is either Rx,Ry or Rz,Z, depending on what we have
	if (!ReorderAxisPair(GUID_RxAxis, GUID_RyAxis, 2))
	{
		ReorderAxisPair(GUID_RzAxis, GUID_ZAxis, 2);
	}
}

bool FDInputJoystick::ReorderAxisPair(const GUID &xid, const GUID &yid, int pos)
{
	unsigned i;
	int x, y;

	// Find each axis.
	x = -1;
	y = -1;
	for (i = 0; i < Axes.Size(); ++i)
	{
		if (x < 0 && Axes[i].Guid == xid)
		{
			x = i;
		}
		else if (y < 0 && Axes[i].Guid == yid)
		{
			y = i;
		}
	}
	// If we don't have both X and Y axes, do nothing.
	if (x < 0 || y < 0)
	{
		return false;
	}
	if (x == pos + 1 && y == pos)
	{ // Xbox 360 Controllers return them in this order.
		swap(Axes[pos], Axes[pos + 1]);
	}
	else if (x != pos || y != pos + 1)
	{
		AxisInfo xinfo = Axes[x], yinfo = Axes[y];
		Axes.Delete(x);
		if (x < y)
		{
			y--;
		}
		Axes.Delete(y);
		Axes.Insert(pos, xinfo);
		Axes.Insert(pos + 1, yinfo);
	}
	return true;
}

//===========================================================================
//
// FDInputJoystick :: SetDataFormat
//
// Using the objects we previously enumerated, construct a data format
// structure for DirectInput to use for this device. We could use the
// predefined c_dfDIJoystick2, except that we would have no way of knowing
// which axis mapped to a certain point in the structure if there is more
// than one of a particular type of axis. The dwOfs member of
// DIDEVICEOBJECTINSTANCE is practically useless, because it describes the
// offset of the object in the device's native data format, and not
// the offset in something we can actually use.
//
//===========================================================================

HRESULT FDInputJoystick::SetDataFormat()
{
	DIOBJECTDATAFORMAT *objects;
	DWORD numobjs;
	DWORD nextofs;
	unsigned i;

	objects = new DIOBJECTDATAFORMAT[Axes.Size() + POVs.Size() + Buttons.Size()];
	numobjs = nextofs = 0;

	// Add all axes
	for (i = 0; i < Axes.Size(); ++i)
	{
		objects[i].pguid = &Axes[i].Guid;
		objects[i].dwOfs = Axes[i].Ofs = nextofs;
		objects[i].dwType = Axes[i].Type;
		objects[i].dwFlags = 0;
		nextofs += sizeof(LONG);
	}
	numobjs = i;
	// Add all POVs
	for (i = 0; i < POVs.Size(); ++i)
	{
		objects[numobjs + i].pguid = &POVs[i].Guid;
		objects[numobjs + i].dwOfs = POVs[i].Ofs = nextofs;
		objects[numobjs + i].dwType = POVs[i].Type;
		objects[numobjs + i].dwFlags = 0;
		nextofs += sizeof(DWORD);
	}
	numobjs += i;
	// Add all buttons
	for (i = 0; i < Buttons.Size(); ++i)
	{
		objects[numobjs + i].pguid = &Buttons[i].Guid;
		objects[numobjs + i].dwOfs = Buttons[i].Ofs = nextofs;
		objects[numobjs + i].dwType = Buttons[i].Type;
		objects[numobjs + i].dwFlags = 0;
		nextofs += sizeof(BYTE);
	}
	numobjs += i;

	// Set format
	DataFormat.dwSize = sizeof(DIDATAFORMAT);
	DataFormat.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
	DataFormat.dwFlags = DIDF_ABSAXIS;
	DataFormat.dwDataSize = (nextofs + 3) & ~3;		// Round to the nearest multiple of 4.
	DataFormat.dwNumObjs = numobjs;
	DataFormat.rgodf = objects;
	return Device->SetDataFormat(&DataFormat);
}

//===========================================================================
//
// FDInputJoystick :: GetIdentifier
//
//===========================================================================

FString FDInputJoystick::GetIdentifier()
{
	char id[48];

	id[0] = 'D'; id[1] = 'I'; id[2] = ':';
	FormatGUID(id + 3, countof(id) - 3, Instance);
	return id;
}

//===========================================================================
//
// FDInputJoystick :: SetConfigSection
//
// Sets up the config for reading or writing this controller's axis config. 
//
//===========================================================================

bool FDInputJoystick::SetConfigSection(bool create)
{
	FString id = GetIdentifier();
	id += ".Axes";
	return GameConfig->SetSection(id, create);
	DIDEVICEINSTANCE inst = { sizeof(DIDEVICEINSTANCE), };
}

//===========================================================================
//
// FDInputJoystick :: LoadConfig
//
//===========================================================================

bool FDInputJoystick::LoadConfig()
{
	char key[32];
	const char *value;
	int axislen;

	SetDefaultConfig();
	if (!SetConfigSection(false))
	{
		return false;
	}
	value = GameConfig->GetValueForKey("Multiplier");
	if (value != NULL)
	{
		Multiplier = (float)atof(value);
	}
	for (unsigned i = 0; i < Axes.Size(); ++i)
	{
		axislen = mysnprintf(key, countof(key), "Axis%u", i);

		mysnprintf(key + axislen, countof(key) - axislen, "deadzone");
		value = GameConfig->GetValueForKey(key);
		if (value != NULL)
		{
			Axes[i].DeadZone = (float)atof(value);
		}
		mysnprintf(key + axislen, countof(key) - axislen, "multiplier");
		value = GameConfig->GetValueForKey(key);
		if (value != NULL)
		{
			Axes[i].Multiplier = (float)atof(value);
		}
		mysnprintf(key + axislen, countof(key) - axislen, "gameaxis");
		value = GameConfig->GetValueForKey(key);
		if (value != NULL)
		{
			Axes[i].GameAxis = (EJoyAxis)atoi(value);
			if (Axes[i].GameAxis < JOYAXIS_None || Axes[i].GameAxis >= NUM_JOYAXIS)
			{
				Axes[i].GameAxis = JOYAXIS_None;
			}
		}
	}
	return true;
}

//===========================================================================
//
// FDInputJoystick :: SaveConfig
//
//===========================================================================

void FDInputJoystick::SaveConfig()
{
	char key[32], value[32];
	int axislen;

	if (SetConfigSection(true))
	{
		GameConfig->ClearCurrentSection();
		mysnprintf(value, countof(value), "%g", Multiplier);
		GameConfig->SetValueForKey("Multiplier", value);
		for (unsigned i = 0; i < Axes.Size(); ++i)
		{
			axislen = mysnprintf(key, countof(key), "Axis%u", i);

			mysnprintf(key + axislen, countof(key) - axislen, "deadzone");
			mysnprintf(value, countof(value), "%g", Axes[i].DeadZone);
			GameConfig->SetValueForKey(key, value);
			mysnprintf(key + axislen, countof(key) - axislen, "multiplier");
			mysnprintf(value, countof(value), "%g", Axes[i].Multiplier);
			GameConfig->SetValueForKey(key, value);
			mysnprintf(key + axislen, countof(key) - axislen, "gameaxis");
			mysnprintf(value, countof(value), "%d", Axes[i].GameAxis);
			GameConfig->SetValueForKey(key, value);
		}
	}
}

//===========================================================================
//
// FDInputJoystick :: SetDefaultConfig
//
// Try for a reasonable default axis configuration.
//
//===========================================================================

void FDInputJoystick::SetDefaultConfig()
{
	Multiplier = 1;
	for (unsigned i = 0; i < Axes.Size(); ++i)
	{
		Axes[i].DeadZone = DEFAULT_DEADZONE;
		Axes[i].Multiplier = 1;
		Axes[i].GameAxis = JOYAXIS_None;
	}
	// Triggers on a 360 controller have a much smaller deadzone.
	if (Axes.Size() == 5 && Axes[4].Guid == GUID_ZAxis)
	{
		Axes[4].DeadZone = 30 / 256.f;
	}
	// Two axes? Horizontal is yaw and vertical is forward.
	if (Axes.Size() == 2)
	{
		Axes[0].GameAxis = JOYAXIS_Yaw;
		Axes[1].GameAxis = JOYAXIS_Forward;
	}
	// Three axes? First two are movement, third is yaw.
	else if (Axes.Size() >= 3)
	{
		Axes[0].GameAxis = JOYAXIS_Side;
		Axes[1].GameAxis = JOYAXIS_Forward;
		Axes[2].GameAxis = JOYAXIS_Yaw;
		// Four axes? First two are movement, last two are looking around.
		if (Axes.Size() >= 4)
		{
			Axes[3].GameAxis = JOYAXIS_Pitch;	Axes[3].Multiplier = 0.75f;
			// Five axes? Use the fifth one for moving up and down.
			if (Axes.Size() >= 5)
			{
				Axes[4].GameAxis = JOYAXIS_Up;
			}
		}
	}
	// If there is only one axis, then we make no assumptions about how
	// the user might want to use it.
}

//===========================================================================
//
// FDInputJoystick :: GetName
//
//===========================================================================

FString FDInputJoystick::GetName()
{
	return Name;
}

//===========================================================================
//
// FDInputJoystick :: GetSensitivity
//
//===========================================================================

float FDInputJoystick::GetSensitivity()
{
	return Multiplier;
}

//===========================================================================
//
// FDInputJoystick :: SetSensitivity
//
//===========================================================================

void FDInputJoystick::SetSensitivity(float scale)
{
	Multiplier = scale;
}

//===========================================================================
//
// FDInputJoystick :: GetNumAxes
//
//===========================================================================

int FDInputJoystick::GetNumAxes()
{
	return (int)Axes.Size();
}

//===========================================================================
//
// FDInputJoystick :: GetAxisDeadZone
//
//===========================================================================

float FDInputJoystick::GetAxisDeadZone(int axis)
{
	if (unsigned(axis) >= Axes.Size())
	{
		return 0;
	}
	return Axes[axis].DeadZone;
}

//===========================================================================
//
// FDInputJoystick :: GetAxisMap
//
//===========================================================================

EJoyAxis FDInputJoystick::GetAxisMap(int axis)
{
	if (unsigned(axis) >= Axes.Size())
	{
		return JOYAXIS_None;
	}
	return Axes[axis].GameAxis;
}

//===========================================================================
//
// FDInputJoystick :: GetAxisName
//
//===========================================================================

const char *FDInputJoystick::GetAxisName(int axis)
{
	if (unsigned(axis) < Axes.Size())
	{
		return Axes[axis].Name;
	}
	return "Invalid";
}

//===========================================================================
//
// FDInputJoystick :: GetAxisScale
//
//===========================================================================

float FDInputJoystick::GetAxisScale(int axis)
{
	if (unsigned(axis) >= Axes.Size())
	{
		return 0;
	}
	return Axes[axis].Multiplier;
}

//===========================================================================
//
// FDInputJoystick :: SetAxisDeadZone
//
//===========================================================================

void FDInputJoystick::SetAxisDeadZone(int axis, float deadzone)
{
	if (unsigned(axis) < Axes.Size())
	{
		Axes[axis].DeadZone = clamp(deadzone, 0.f, 1.f);
	}
}

//===========================================================================
//
// FDInputJoystick :: SetAxisMap
//
//===========================================================================

void FDInputJoystick::SetAxisMap(int axis, EJoyAxis gameaxis)
{
	if (unsigned(axis) < Axes.Size())
	{
		Axes[axis].GameAxis = (unsigned(gameaxis) < NUM_JOYAXIS) ? gameaxis : JOYAXIS_None;
	}
}

//===========================================================================
//
// FDInputJoystick :: SetAxisScale
//
//===========================================================================

void FDInputJoystick::SetAxisScale(int axis, float scale)
{
	if (unsigned(axis) < Axes.Size())
	{
		Axes[axis].Multiplier = scale;
	}
}

//===========================================================================
//
// FDInputJoystickManager - Constructor
//
//===========================================================================

FDInputJoystickManager::FDInputJoystickManager()
{
}

//===========================================================================
//
// FDInputJoystickManager - Destructor
//
//===========================================================================

FDInputJoystickManager::~FDInputJoystickManager()
{
	for (unsigned i = 0; i < Devices.Size(); ++i)
	{
		delete Devices[i];
	}
}

//===========================================================================
//
// FDInputJoystickManager :: GetDevice
//
//===========================================================================

bool FDInputJoystickManager::GetDevice()
{
	if (g_pdi == NULL || !use_joystick || Args->CheckParm("-nojoy"))
	{
		return false;
	}
	EnumDevices();
	return true;
}

//===========================================================================
//
// FDInputJoystickManager :: ProcessInput
//
// Process input for every attached device.
//
//===========================================================================

void FDInputJoystickManager::ProcessInput()
{
	for (unsigned i = 0; i < Devices.Size(); ++i)
	{
		if (Devices[i] != NULL)
		{
			Devices[i]->ProcessInput();
		}
	}
}

//===========================================================================
//
// FDInputJoystickManager :: AddAxes
//
// Adds the state of all attached device axes to the passed array.
//
//===========================================================================

void FDInputJoystickManager :: AddAxes(float axes[NUM_JOYAXIS])
{
	for (unsigned i = 0; i < Devices.Size(); ++i)
	{
		Devices[i]->AddAxes(axes);
	}
}

//===========================================================================
//
// FDInputJoystickManager :: GetDevices
//
// Adds the IJoystick interfaces for each device we created to the sticks
// array.
//
//===========================================================================

void FDInputJoystickManager::GetDevices(TArray<IJoystickConfig *> &sticks)
{
	for (unsigned i = 0; i < Devices.Size(); ++i)
	{
		sticks.Push(Devices[i]);
	}
}

//===========================================================================
//
// FDInputJoystickManager :: WndProcHook
//
// Listen for device change broadcasts and rescan the attached devices
// when they are received.
//
//===========================================================================

bool FDInputJoystickManager::WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	if (message != WM_DEVICECHANGE)
	{
		return false;
	}
#ifdef _DEBUG
	char out[64];
	mysnprintf(out, countof(out), "WM_DEVICECHANGE wParam=%d\n", wParam);
	OutputDebugString(out);
#endif
	if ((wParam != DBT_DEVNODES_CHANGED &&
		 wParam != DBT_DEVICEARRIVAL &&
		 wParam != DBT_CONFIGCHANGED))
	{
		return false;
	}
	UpdateJoystickMenu(EnumDevices());
	// Return false so that other devices can handle this too if they want.
	return false;
}

//===========================================================================
//
// FDInputJoystickManager :: EnumCallback							STATIC
//
// Adds each DirectInput game controller to a TArray.
//
//===========================================================================

BOOL CALLBACK FDInputJoystickManager::EnumCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
	// Do not add XInput devices if XInput was initialized.
	if (JoyDevices[INPUT_XInput] == NULL || !IsXInputDevice(&lpddi->guidProduct))
	{
		TArray<Enumerator> *all = (TArray<Enumerator> *)pvRef;
		Enumerator thisone;

		thisone.Instance = lpddi->guidInstance;
		thisone.Name = lpddi->tszInstanceName;
		all->Push(thisone);
	}
	return DIENUM_CONTINUE;
}

//===========================================================================
//
// FDInputJoystickManager :: IsXInputDevice							STATIC
//
// Pretty much copied straight from the article "XInput and DirectInput".
//
// Enum each PNP device using WMI and check each device ID to see if it
// contains "IG_" (ex. "VID_045E&PID_028E&IG_00"). If it does, then it's an
// XInput device. Unfortunately this information can not be found by just
// using DirectInput.
//
//===========================================================================

bool FDInputJoystickManager::IsXInputDevice(const GUID *guid)
{
	IWbemLocator *wbemlocator = NULL;
	IEnumWbemClassObject *enumdevices = NULL;
	IWbemClassObject *devices[20] = { 0 };
	IWbemServices *wbemservices = NULL;
	BSTR namespce = NULL;
	BSTR deviceid = NULL;
	BSTR classname = NULL;
	DWORD returned = 0;
	bool isxinput = false;
	UINT device = 0;
	VARIANT var;
	HRESULT hr;

	// Create WMI
	hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&wbemlocator);
	if (FAILED(hr) || wbemlocator == NULL)
		goto cleanup;

	if (NULL == (namespce = SysAllocString(OLESTR("\\\\.\\root\\cimv2")))) goto cleanup;
	if (NULL == (classname = SysAllocString(OLESTR("Win32_PNPEntity")))) goto cleanup;
	if (NULL == (deviceid = SysAllocString(OLESTR("DeviceID")))) goto cleanup;

	// Connect to WMI
	hr = wbemlocator->ConnectServer(namespce, NULL, NULL, 0, 0, NULL, NULL, &wbemservices);
	if (FAILED(hr) || wbemservices == NULL)
		goto cleanup;

	// Switch security level to IMPERSONATE.
	CoSetProxyBlanket(wbemservices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
		RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

	hr = wbemservices->CreateInstanceEnum(classname, 0, NULL, &enumdevices);
	if (FAILED(hr) || enumdevices == NULL)
		goto cleanup;

	// Loop over all devices
	for (;;)
	{
		// Get 20 at a time.
		hr = enumdevices->Next(10000, countof(devices), devices, &returned);
		if (FAILED(hr))
			goto cleanup;
		if (returned == 0)
			break;

		for (device = 0; device < returned; device++)
		{
			// For each device, get its device ID.
			hr = devices[device]->Get(deviceid, 0L, &var, NULL, NULL);
			if (SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal != NULL)
			{
				// Check if the device ID contains "IG_". If it does, then it's an XInput device.
				// This information cannot be found from DirectInput.
				if (wcsstr(var.bstrVal, L"IG_"))
				{
					// If it does, then get the VID/PID from var.bstrVal.
					DWORD pid = 0, vid = 0;
					WCHAR *strvid = wcsstr(var.bstrVal, L"VID_");
					if (strvid && swscanf(strvid, L"VID_%4X", &vid) != 1)
						vid = 0;
					WCHAR *strpid = wcsstr(var.bstrVal, L"PID_");
					if (strpid && swscanf(strpid, L"PID_%4X", &pid) != 1)
						pid = 0;

					// Compare the VID/PID to the DInput device.
					DWORD vidpid = MAKELONG(vid, pid);
					if (vidpid == guid->Data1)
					{
						isxinput = true;
						goto cleanup;
					}
				}
			}
			SAFE_RELEASE(devices[device]);
		}
	}

cleanup:
	if (namespce)	SysFreeString(namespce);
	if (deviceid)	SysFreeString(deviceid);
	if (classname)	SysFreeString(classname);
	for (device = 0; device < countof(devices); ++device)
		SAFE_RELEASE(devices[device]);
	SAFE_RELEASE(enumdevices);
	SAFE_RELEASE(wbemlocator);
	SAFE_RELEASE(wbemservices);
	return isxinput;
}

//===========================================================================
//
// FDInputJoystickManager :: NameSort								STATIC
//
//===========================================================================

int FDInputJoystickManager::NameSort(const void *a, const void *b)
{
	const Enumerator *ea = (const Enumerator *)a;
	const Enumerator *eb = (const Enumerator *)b;
	int lex = ea->Name.Compare(eb->Name);
	if (lex == 0)
	{
		return memcmp(&ea->Instance, &eb->Instance, sizeof(GUID));
	}
	return lex;
}

//===========================================================================
//
// FDInputJoystickManager :: EnumDevices
//
// Find out what DirectInput game controllers are on the system and create
// FDInputJoystick objects for them. May return a pointer to the first new
// device found.
//
//===========================================================================

FDInputJoystick *FDInputJoystickManager::EnumDevices()
{
	FDInputJoystick *newone = NULL;
	TArray<Enumerator> controllers;
	unsigned i, j, k;

	g_pdi->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumCallback, &controllers, DIEDFL_ALLDEVICES);

	// Sort by name so that devices with duplicate names can have numbers appended.
	qsort(&controllers[0], controllers.Size(), sizeof(Enumerator), NameSort);
	for (i = 1; i < controllers.Size(); ++i)
	{
		// Does this device have the same name as the previous one? If so, how
		// many more have the same name?
		for (j = i; j < controllers.Size(); ++j)
		{
			if (controllers[j-1].Name.Compare(controllers[j].Name) != 0)
				break;
		}
		// j is one past the last duplicate name.
		if (j > i)
		{
			// Append numbers.
			for (k = i - 1; k < j; ++k)
			{
				controllers[k].Name.AppendFormat(" #%d", k - i + 2);
			}
		}
	}

	// Compare the new list of devices with the one we previously instantiated.
	// Every device we currently hold is marked 0. Then scan through the new
	// list and try to find it there, if it's found, it is marked 1. At the end
	// of this, devices marked 1 existed before and are left alone. Devices
	// marked 0 are no longer present and should be destroyed. If a device is
	// present in the new list that we have not yet instantiated, we 
	// instantiate it now.
	for (j = 0; j < Devices.Size(); ++j)
	{
		Devices[j]->Marked = false;
	}
	for (i = 0; i < controllers.Size(); ++i)
	{
		GUID *lookfor = &controllers[i].Instance;
		for (j = 0; j < Devices.Size(); ++j)
		{
			if (Devices[j]->Instance == *lookfor)
			{
				Devices[j]->Marked = true;
				break;
			}
		}
		if (j == Devices.Size())
		{ // Not found. Add it.
			FDInputJoystick *device = new FDInputJoystick(lookfor, controllers[i].Name);
			if (!device->GetDevice())
			{
				delete device;
			}
			else
			{
				device->Marked = true;
				Devices.Push(device);
				if (newone == NULL)
				{
					newone = device;
				}
			}
		}
	}
	// Remove detached devices and avoid holes in the list.
	for (i = j = 0; j < Devices.Size(); ++j)
	{
		if (!Devices[j]->Marked)
		{
			delete Devices[j];
		}
		else
		{
			if (i != j)
			{
				Devices[i] = Devices[j];
			}
			++i;
		}
	}
	Devices.Resize(i);
	return newone;
}

//===========================================================================
//
// I_StartupDirectInputJoystick
//
//===========================================================================

void I_StartupDirectInputJoystick()
{
	FJoystickCollection *joys = new FDInputJoystickManager;
	if (joys->GetDevice())
	{
		JoyDevices[INPUT_DIJoy] = joys;
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
