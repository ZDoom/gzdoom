#ifndef M_JOY_H
#define M_JOY_H

#include "basics.h"
#include "keydef.h"
#include "tarray.h"
#include "c_cvars.h"

union CubicBezier {
	struct {
		float x1;
		float y1;
		float x2;
		float y2;
	};
	float pts[4];
};

enum EJoyCurve {
	JOYCURVE_CUSTOM = -1,
	JOYCURVE_DEFAULT,
	JOYCURVE_LINEAR,
	JOYCURVE_QUADRATIC,
	JOYCURVE_CUBIC,

	NUM_JOYCURVE
};

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

extern const float JOYDEADZONE_DEFAULT;

extern const float JOYSENSITIVITY_DEFAULT;

extern const float JOYTHRESH_DEFAULT;

extern const float JOYTHRESH_TRIGGER;
extern const float JOYTHRESH_STICK_X;
extern const float JOYTHRESH_STICK_Y;

extern const CubicBezier JOYCURVE[NUM_JOYCURVE];

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
	virtual EJoyCurve GetAxisResponseCurve(int axis) = 0;
	virtual float GetAxisResponseCurvePoint(int axis, int point) = 0;

	virtual void SetAxisDeadZone(int axis, float zone) = 0;
	virtual void SetAxisMap(int axis, EJoyAxis gameaxis) = 0;
	virtual void SetAxisScale(int axis, float scale) = 0;
	virtual void SetAxisDigitalThreshold(int axis, float threshold) = 0;
	virtual void SetAxisResponseCurve(int axis, EJoyCurve preset) = 0;
	virtual void SetAxisResponseCurvePoint(int axis, int point, float value) = 0;

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
double Joy_ApplyResponseCurveBezier(const CubicBezier &curve, double input);

// These ought to be provided by a system-specific i_input.cpp.
void I_GetAxes(float axes[NUM_JOYAXIS]);
void I_GetJoysticks(TArray<IJoystickConfig *> &sticks);
IJoystickConfig *I_UpdateDeviceList();
extern void UpdateJoystickMenu(IJoystickConfig *);

#endif
