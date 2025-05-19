#ifndef M_JOY_H
#define M_JOY_H

#include "basics.h"
#include "tarray.h"
#include "c_cvars.h"

#include <cstdint>

enum EJoyAxis
{
	JOYAXIS_None = -1,
	JOYAXIS_Yaw,
	JOYAXIS_Pitch,
	JOYAXIS_Forward,
	JOYAXIS_Side,
	JOYAXIS_Up,
//	JOYAXIS_Roll,		// Ha ha. No roll for you.
	NUM_JOYAXIS,
};

// Generic configuration interface for a controller.
struct IJoystickConfig
{
	virtual ~IJoystickConfig() = 0;

	virtual FString GetName() = 0;
	virtual float GetSensitivity() = 0;
	virtual void SetSensitivity(float scale) = 0;

	virtual int GetNumAxes() = 0;
	virtual float GetAxisDeadZone(int axis) = 0;
	virtual EJoyAxis GetAxisMap(int axis) = 0;
	virtual const char *GetAxisName(int axis) = 0;
	virtual float GetAxisScale(int axis) = 0;

	virtual void SetAxisDeadZone(int axis, float zone) = 0;
	virtual void SetAxisMap(int axis, EJoyAxis gameaxis) = 0;
	virtual void SetAxisScale(int axis, float scale) = 0;

	virtual bool GetEnabled() = 0;
	virtual void SetEnabled(bool enabled) = 0;

	virtual bool AllowsEnabledInBackground() = 0;
	virtual bool GetEnabledInBackground() = 0;
	virtual void SetEnabledInBackground(bool enabled) = 0;

	// Used by the saver to not save properties that are at their defaults.
	virtual bool IsSensitivityDefault() = 0;
	virtual bool IsAxisDeadZoneDefault(int axis) = 0;
	virtual bool IsAxisMapDefault(int axis) = 0;
	virtual bool IsAxisScaleDefault(int axis) = 0;

	virtual void SetDefaultConfig() = 0;
	virtual FString GetIdentifier() = 0;
};

EXTERN_CVAR(Bool, use_joystick);

bool M_LoadJoystickConfig(IJoystickConfig *joy);
void M_SaveJoystickConfig(IJoystickConfig *joy);

void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, int base);
void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, const int *keys);
int Joy_XYAxesToButtons(double x, double y);
double Joy_RemoveDeadZone(double axisval, double deadzone, uint8_t *buttons);

// These ought to be provided by a system-specific i_input.cpp.
void I_GetAxes(float axes[NUM_JOYAXIS]);
void I_GetJoysticks(TArray<IJoystickConfig *> &sticks);
void I_RumbleRaw(uint32_t duration_ms, uint16_t high_freq, uint16_t low_freq, uint16_t left_trig, uint16_t right_trig);
void I_Rumble(uint duration_ms, double high_freq, double low_freq, double left_trig, double right_trig);
void I_Rumble(const FString& identifier);
IJoystickConfig *I_UpdateDeviceList();
extern void UpdateJoystickMenu(IJoystickConfig *);

#endif
