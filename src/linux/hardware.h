#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#include "doomtype.h"
#include "v_video.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "d_main.h"
#include "i_video.h"

class IVideo
{
 public:
	virtual ~IVideo () {}

	virtual EDisplayType GetDisplayType () = 0;
	virtual bool FullscreenChanged (bool fs) = 0;
	virtual void SetWindowedScale (float scale) = 0;
	virtual bool CanBlit () = 0;

	virtual bool SetMode (int width, int height, int bits, bool fs) = 0;
	virtual void SetPalette (DWORD *palette) = 0;
	virtual void UpdateScreen (DCanvas *canvas) = 0;
	virtual void ReadScreen (byte *block) = 0;

	virtual int GetModeCount () = 0;
	virtual void StartModeIterator (int bits) = 0;
	virtual bool NextMode (int *width, int *height) = 0;

	virtual bool AllocateSurface (DCanvas *scrn, int width, int height, int bits, bool primary = false) = 0;
	virtual void ReleaseSurface (DCanvas *scrn) = 0;
	virtual void LockSurface (DCanvas *scrn) = 0;
	virtual void UnlockSurface (DCanvas *scrn) = 0;
	virtual bool Blit (DCanvas *src, int sx, int sy, int sw, int sh,
					   DCanvas *dst, int dx, int dy, int dw, int dh) = 0;
};

class IInputDevice
{
 public:
	virtual ~IInputDevice () {}
	virtual void ProcessInput (bool parm) = 0;
};

class IKeyboard : public IInputDevice
{
 public:
	virtual void ProcessInput (bool consoleOpen) = 0;
	virtual void SetKeypadRemapping (bool remap) = 0;
	virtual void GetKeyRepeats (int &delay, int &rate)
		{
			delay = (250*TICRATE)/1000;
			rate = TICRATE / 15;
		}
};

class IMouse : public IInputDevice
{
 public:
	virtual void SetGrabbed (bool grabbed) = 0;
	virtual void ProcessInput (bool active) = 0;
};

class IJoystick : public IInputDevice
{
 public:
	enum EJoyProp
	{
		JOYPROP_SpeedMultiplier,
		JOYPROP_XSensitivity,
		JOYPROP_YSensitivity,
		JOYPROP_XThreshold,
		JOYPROP_YThreshold
	};
	virtual void SetProperty (EJoyProp prop, float val) = 0;
};

enum EInterfaceType
{
	INTERFACE_Video,
	INTERFACE_Keyboard,
	INTERFACE_Mouse,
	INTERFACE_Joystick
};

struct IEngineGlobals
{
	void (*Error) (const char *error, ...);
	void (*FatalError) (const char *error, ...);
	int (*Printf) (int printlevel, const char *fmt, ...);
	
	int (*CurrentTic) ();
	QWORD (*CurrentTime) ();

	bool (*MouseShouldBeGrabbed) ();
	void (*PostEvent) (const event_t *ev);

	void (*RegisterCommand) (DConsoleCommand *cmd, const char *name);
	void (*UnregisterCommand) (DConsoleCommand *cmd);
	void (*RegisterCvar) (cvar_t *cvar, const char *name, const char *def,
						  DWORD flags, void (*callback)(cvar_t &));
	void (*UnregisterCvar) (cvar_t *cvar);
};

typedef void *(*queryinterface_t) (EInterfaceType, int);
typedef queryinterface_t (*preplib_t) (IEngineGlobals *);

#ifdef HARDWARELIB

static IEngineGlobals *ge;

#undef BEGIN_COMMAND
#undef BEGIN_CUSTOM_CVAR
#undef EXTERN_CVAR
#undef CVAR

#else

void I_InitHardware ();

extern int KeyRepeatRate, KeyRepeatDelay;

#endif	// HARDWARELIB

#endif	// __HARDWARE_H__
