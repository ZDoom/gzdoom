#ifndef M_JOY_H
#define M_JOY_H

#include "basics.h"
#include "keydef.h"
#include "tarray.h"
#include "c_cvars.h"

static const float DEFAULT_DEADZONE = 0.1; // reduced from 0.25

static const float DEFAULT_SENSITIVITY = 1.0;

static const float THRESH_DEFAULT = 0.001;
static const float THRESH_TRIGGER = 0.001;
static const float THRESH_STICK_X = 0.667;
static const float THRESH_STICK_Y = 0.333;

static const float CURVE_EASE_A = 0.1;
static const float CURVE_EASE_B = 1.0;
static const float CURVE_LINE_A = 0.0;
static const float CURVE_LINE_B = 1.0;
static const float CURVE_QUAD_A = 0.1;
static const float CURVE_QUAD_B = 0.5;
static const float CURVE_CUBE_A = 0.1;
static const float CURVE_CUBE_B = 0.2;

static const float CURVE_DEFAULT_A = CURVE_EASE_A;
static const float CURVE_DEFAULT_B = CURVE_EASE_A;
static const float CURVE_STICK_A   = CURVE_DEFAULT_A;
static const float CURVE_STICK_B   = CURVE_DEFAULT_B;
static const float CURVE_TRIGGER_A = CURVE_DEFAULT_A;
static const float CURVE_TRIGGER_B = CURVE_DEFAULT_B;

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
	virtual float GetAxisDigitalThreshold(int axis) = 0;
	virtual float GetAxisResponseCurveA(int axis) = 0;
	virtual float GetAxisResponseCurveB(int axis) = 0;

	virtual void SetAxisDeadZone(int axis, float zone) = 0;
	virtual void SetAxisMap(int axis, EJoyAxis gameaxis) = 0;
	virtual void SetAxisScale(int axis, float scale) = 0;
	virtual void SetAxisDigitalThreshold(int axis, float threshold) = 0;
	virtual void SetAxisResponseCurveA(int axis, float point) = 0;
	virtual void SetAxisResponseCurveB(int axis, float point) = 0;

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
	virtual bool IsAxisDigitalThresholdDefault(int axis) = 0;
	virtual bool IsAxisResponseCurveDefault(int axis) = 0;

	virtual void SetDefaultConfig() = 0;
	virtual FString GetIdentifier() = 0;
};

EXTERN_CVAR(Bool, use_joystick);

bool M_LoadJoystickConfig(IJoystickConfig *joy);
void M_SaveJoystickConfig(IJoystickConfig *joy);

void Joy_GenerateButtonEvent(bool down, EKeyCodes which);
void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, int base);
void Joy_GenerateButtonEvents(int oldbuttons, int newbuttons, int numbuttons, const int *keys);
int Joy_XYAxesToButtons(double x, double y);
double Joy_RemoveDeadZone(double axisval, double deadzone, uint8_t *buttons);
double Joy_ApplyResponseCurveBezier(float a, float b, double t);

// These ought to be provided by a system-specific i_input.cpp.
void I_GetAxes(float axes[NUM_JOYAXIS]);
void I_GetJoysticks(TArray<IJoystickConfig *> &sticks);
IJoystickConfig *I_UpdateDeviceList();
extern void UpdateJoystickMenu(IJoystickConfig *);

#endif
