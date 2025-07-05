/*
** i_joystick.cpp
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
#include <SDL.h>
#include <SDL_gamecontroller.h>
#include <cstdlib>

#include "basics.h"
#include "cmdlib.h"

#include "d_eventbase.h"
#include "i_input.h"
#include "m_joy.h"

class SDLInputJoystick: public IJoystickConfig
{
public:
	SDLInputJoystick(int DeviceIndex) :
	DeviceIndex(DeviceIndex),
	InstanceID(SDL_JoystickGetDeviceInstanceID(DeviceIndex)),
	Multiplier(JOYSENSITIVITY_DEFAULT),
	Enabled(true),
	SettingsChanged(false)
	{
		if (SDL_IsGameController(DeviceIndex))
		{
			Mapping = SDL_GameControllerOpen(DeviceIndex);
			Device = NULL;

			DefaultAxes = DefaultControllerAxes;
			DefaultAxesCount = sizeof(DefaultControllerAxes) / sizeof(DefaultAxisConfig);

			if(Mapping != NULL)
			{
				NumAxes = SDL_CONTROLLER_AXIS_MAX;
				NumHats = 0;

				SetDefaultConfig();
			}
		}
		else
		{
			Device = SDL_JoystickOpen(DeviceIndex);
			Mapping = NULL;

			DefaultAxes = DefaultJoystickAxes;
			DefaultAxesCount = sizeof(DefaultJoystickAxes) / sizeof(DefaultAxisConfig);

			if(Device != NULL)
			{
				NumAxes = SDL_JoystickNumAxes(Device);
				NumHats = SDL_JoystickNumHats(Device);

				SetDefaultConfig();
			}
		}
		M_LoadJoystickConfig(this);
	}
	~SDLInputJoystick()
	{
		if(IsValid() && SettingsChanged)
			M_SaveJoystickConfig(this);
		if (Mapping)
			SDL_GameControllerClose(Mapping);
		if (Device)
			SDL_JoystickClose(Device);
	}

	bool IsValid() const
	{
		return Device != NULL || Mapping != NULL;
	}

	FString GetName()
	{
		return (Mapping)
			? SDL_GameControllerName(Mapping)
			: SDL_JoystickName(Device);
	}
	float GetSensitivity()
	{
		return Multiplier;
	}
	void SetSensitivity(float scale)
	{
		SettingsChanged = true;
		Multiplier = scale;
	}

	int GetNumAxes()
	{
		return NumAxes + NumHats*2;
	}
	float GetAxisDeadZone(int axis)
	{
		return Axes[axis].DeadZone;
	}
	EJoyAxis GetAxisMap(int axis)
	{
		return Axes[axis].GameAxis;
	}
	const char *GetAxisName(int axis)
	{
		return Axes[axis].Name.GetChars();
	}
	float GetAxisScale(int axis)
	{
		return Axes[axis].Multiplier;
	}
	float GetAxisDigitalThreshold(int axis)
	{
		return Axes[axis].DigitalThreshold;
	}
	EJoyCurve GetAxisResponseCurve(int axis)
	{
		return Axes[axis].ResponseCurvePreset;
	}
	float GetAxisResponseCurvePoint(int axis, int point)
	{
		return unsigned(point) < 4
			? Axes[axis].ResponseCurve.pts[point]
			: 0;
	};

	void SetAxisDeadZone(int axis, float zone)
	{
		SettingsChanged = true;
		Axes[axis].DeadZone = clamp(zone, 0.f, 1.f);
	}
	void SetAxisMap(int axis, EJoyAxis gameaxis)
	{
		SettingsChanged = true;
		Axes[axis].GameAxis = gameaxis;
	}
	void SetAxisScale(int axis, float scale)
	{
		SettingsChanged = true;
		Axes[axis].Multiplier = scale;
	}
	void SetAxisDigitalThreshold(int axis, float threshold)
	{
		SettingsChanged = true;
		Axes[axis].DigitalThreshold = threshold;
	}
	void SetAxisResponseCurve(int axis, EJoyCurve preset)
	{
		if (preset >= NUM_JOYCURVE || preset < JOYCURVE_CUSTOM) return;
		SettingsChanged = true;
		Axes[axis].ResponseCurvePreset = preset;
		if (preset == JOYCURVE_CUSTOM) return;
		Axes[axis].ResponseCurve = JOYCURVE[preset];
	}
	void SetAxisResponseCurvePoint(int axis, int point, float value)
	{
		if (unsigned(point) < 4)
		{
			SettingsChanged = true;
			Axes[axis].ResponseCurvePreset = JOYCURVE_CUSTOM;
			Axes[axis].ResponseCurve.pts[point] = value;
		}
	}

	// Used by the saver to not save properties that are at their defaults.
	bool IsSensitivityDefault()
	{
		return Multiplier == JOYSENSITIVITY_DEFAULT;
	}
	bool IsAxisDeadZoneDefault(int axis)
	{
		if(axis >= DefaultAxesCount)
			return Axes[axis].DeadZone == JOYDEADZONE_DEFAULT;
		return Axes[axis].DeadZone == DefaultAxes[axis].DeadZone;
	}
	bool IsAxisMapDefault(int axis)
	{
		if(axis >= DefaultAxesCount)
			return Axes[axis].GameAxis == JOYAXIS_None;
		return Axes[axis].GameAxis == DefaultAxes[axis].GameAxis;
	}
	bool IsAxisScaleDefault(int axis)
	{
		if(axis >= DefaultAxesCount)
			return Axes[axis].Multiplier == JOYSENSITIVITY_DEFAULT;
		return Axes[axis].Multiplier == DefaultAxes[axis].Multiplier;
	}
	bool IsAxisDigitalThresholdDefault(int axis)
	{
		if(axis >= DefaultAxesCount)
			return Axes[axis].DigitalThreshold == JOYTHRESH_DEFAULT;
		return Axes[axis].DigitalThreshold == DefaultAxes[axis].DigitalThreshold;
	}
	bool IsAxisResponseCurveDefault(int axis)
	{
		if(axis >= DefaultAxesCount)
			return Axes[axis].ResponseCurvePreset == JOYCURVE_DEFAULT;
		return Axes[axis].ResponseCurvePreset == DefaultAxes[axis].ResponseCurvePreset;
	}

	void SetDefaultConfig()
	{
		if (Axes.size() == 0)
		{
			for(int i = 0;i < GetNumAxes();i++)
			{
				Axes.Push({});
			}
		}

		for(int i = 0;i < GetNumAxes();i++)
		{
			if (Mapping) {
				switch(i) {
					case SDL_CONTROLLER_AXIS_LEFTX: Axes[i].Name = "Left Stick X"; break;
					case SDL_CONTROLLER_AXIS_LEFTY: Axes[i].Name = "Left Stick Y"; break;
					case SDL_CONTROLLER_AXIS_RIGHTX: Axes[i].Name = "Right Stick X"; break;
					case SDL_CONTROLLER_AXIS_RIGHTY: Axes[i].Name = "Right Stick Y"; break;
					case SDL_CONTROLLER_AXIS_TRIGGERLEFT: Axes[i].Name = "Left Trigger"; break;
					case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: Axes[i].Name = "Right Trigger"; break;
					default: Axes[i].Name.Format("Axis %d", i+1); break;
				}
			} else {
				if(i < NumAxes)
					Axes[i].Name.Format("Axis %d", i+1);
				else
					Axes[i].Name.Format("Hat %d (%c)", (i-NumAxes)/2 + 1, (i-NumAxes)%2 == 0 ? 'x' : 'y');
			}

			Axes[i].Value = 0.0;
			Axes[i].ButtonValue = 0;

			if (i < DefaultAxesCount)
			{
				Axes[i].GameAxis = DefaultAxes[i].GameAxis;
				Axes[i].DeadZone = DefaultAxes[i].DeadZone;
				Axes[i].Multiplier = DefaultAxes[i].Multiplier;
				Axes[i].DigitalThreshold = DefaultAxes[i].DigitalThreshold;
				Axes[i].ResponseCurvePreset = DefaultAxes[i].ResponseCurvePreset;
				Axes[i].ResponseCurve = JOYCURVE[DefaultAxes[i].ResponseCurvePreset];
			}
			else
			{
				Axes[i].GameAxis = JOYAXIS_None;
				Axes[i].DeadZone = JOYDEADZONE_DEFAULT;
				Axes[i].Multiplier = JOYSENSITIVITY_DEFAULT;
				Axes[i].DigitalThreshold = JOYTHRESH_DEFAULT;
				Axes[i].ResponseCurvePreset = JOYCURVE_DEFAULT;
				Axes[i].ResponseCurve = JOYCURVE[JOYCURVE_DEFAULT];
			}
		}
	}

	bool GetEnabled()
	{
		return Enabled;
	}

	void SetEnabled(bool enabled)
	{
		SettingsChanged = true;
		Enabled = enabled;
	}

	bool AllowsEnabledInBackground() { return false; }
	bool GetEnabledInBackground() { return false; }
	void SetEnabledInBackground(bool enabled) {}

	FString GetIdentifier()
	{
		char id[16];
		snprintf(id, countof(id), "JS:%d", DeviceIndex);
		return id;
	}

	void AddAxes(float axes[NUM_JOYAXIS])
	{
		// Add to game axes.
		for (int i = 0; i < GetNumAxes(); ++i)
		{
			if(Axes[i].GameAxis != JOYAXIS_None)
				axes[Axes[i].GameAxis] -= float(Axes[i].Value * Multiplier * Axes[i].Multiplier);
		}
	}

	void ProcessInput() {
		uint8_t buttonstate;

		if (Mapping)
		{
			// GameController API available

			auto lastTriggerL = Axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].Value > Axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].DigitalThreshold;
			auto lastTriggerR = Axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].Value > Axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].DigitalThreshold;

			for (auto i = 0; i < SDL_CONTROLLER_AXIS_MAX && i < NumAxes; ++i)
			{
				buttonstate = 0;

				Axes[i].Value = SDL_GameControllerGetAxis(Mapping, static_cast<SDL_GameControllerAxis>(i))/32767.0;
				Axes[i].Value = Joy_RemoveDeadZone(Axes[i].Value, Axes[i].DeadZone, &buttonstate);
				Axes[i].Value = Joy_ApplyResponseCurveBezier(Axes[i].ResponseCurve, Axes[i].Value);
			}

			auto currTriggerL = Axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].Value > Axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].DigitalThreshold;
			auto currTriggerR = Axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].Value > Axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].DigitalThreshold;

			if (lastTriggerL != currTriggerL) Joy_GenerateButtonEvent(currTriggerL, KEY_PAD_LTRIGGER);
			if (lastTriggerR != currTriggerR) Joy_GenerateButtonEvent(currTriggerR, KEY_PAD_RTRIGGER);

			// todo: right stick
			buttonstate = Joy_XYAxesToButtons(
				abs(Axes[0].Value) < Axes[0].DigitalThreshold ? 0 : Axes[0].Value,
				abs(Axes[1].Value) < Axes[1].DigitalThreshold ? 0 : Axes[1].Value
			);
			Joy_GenerateButtonEvents(Axes[0].ButtonValue, buttonstate, 4, KEY_JOYAXIS1PLUS);
			Axes[0].ButtonValue = buttonstate;
		}
		else
		{
			// Joystick API fallback

			for (int i = 0; i < NumAxes; ++i)
			{
				buttonstate = 0;

				Axes[i].Value = SDL_JoystickGetAxis(Device, i)/32767.0;
				Axes[i].Value = Joy_RemoveDeadZone(Axes[i].Value, Axes[i].DeadZone, &buttonstate);
				Axes[i].Value = Joy_ApplyResponseCurveBezier(Axes[i].ResponseCurve, Axes[i].Value);

				// Map button to axis
				// X and Y are handled differently so if we have 2 or more axes then we'll use that code instead.
				if (NumAxes == 1 || (i >= 2 && i < NUM_JOYAXISBUTTONS))
				{
					Joy_GenerateButtonEvents(Axes[i].ButtonValue, buttonstate, 2, KEY_JOYAXIS1PLUS + i*2);
					Axes[i].ButtonValue = buttonstate;
				}
			}

			if(NumAxes > 1)
			{
				buttonstate = Joy_XYAxesToButtons(
					abs(Axes[0].Value) < Axes[0].DigitalThreshold ? 0 : Axes[0].Value,
					abs(Axes[1].Value) < Axes[1].DigitalThreshold ? 0 : Axes[1].Value
				);
				Joy_GenerateButtonEvents(Axes[0].ButtonValue, buttonstate, 4, KEY_JOYAXIS1PLUS);
				Axes[0].ButtonValue = buttonstate;
			}

			// Map POV hats to buttons and axes.  Why axes?  Well apparently I have
			// a gamepad where the left control stick is a POV hat (instead of the
			// d-pad like you would expect, no that's pressure sensitive).  Also
			// KDE's joystick dialog maps them to axes as well.
			for (int i = 0; i < NumHats; ++i)
			{
				AxisInfo &x = Axes[NumAxes + i*2];
				AxisInfo &y = Axes[NumAxes + i*2 + 1];

				buttonstate = SDL_JoystickGetHat(Device, i);

				// If we're going to assume that we can pass SDL's value into
				// Joy_GenerateButtonEvents then we might as well assume the format here.
				if(buttonstate & 0x1) // Up
					y.Value = -1.0;
				else if(buttonstate & 0x4) // Down
					y.Value = 1.0;
				else
					y.Value = 0.0;
				if(buttonstate & 0x2) // Left
					x.Value = 1.0;
				else if(buttonstate & 0x8) // Right
					x.Value = -1.0;
				else
					x.Value = 0.0;

				if(i < 4)
				{
					Joy_GenerateButtonEvents(x.ButtonValue, buttonstate, 4, KEY_JOYPOV1_UP + i*4);
					x.ButtonValue = buttonstate;
				}
			}
		}
	}

protected:
	struct AxisInfo
	{
		FString Name;
		float DeadZone;
		float Multiplier;
		float DigitalThreshold;
		EJoyCurve ResponseCurvePreset;
		CubicBezier ResponseCurve;
		EJoyAxis GameAxis;
		double Value;
		uint8_t ButtonValue;
	};
	struct DefaultAxisConfig
	{
		float DeadZone;
		EJoyAxis GameAxis;
		float Multiplier;
		float DigitalThreshold;
		EJoyCurve ResponseCurvePreset;
	};
	static const DefaultAxisConfig DefaultJoystickAxes[5];
	static const DefaultAxisConfig DefaultControllerAxes[6];
	const DefaultAxisConfig * DefaultAxes;
	int DefaultAxesCount;

	int					DeviceIndex;
	int					InstanceID;
	SDL_Joystick		*Device;
	SDL_GameController	*Mapping;

	float				Multiplier;
	bool				Enabled;
	TArray<AxisInfo>	Axes;
	int					NumAxes;
	int					NumHats;
	bool 				SettingsChanged;

	friend class SDLInputJoystickManager;
};

// [Nash 4 Feb 2024] seems like on Linux, the third axis is actually the Left Trigger, resulting in the player uncontrollably looking upwards.
const SDLInputJoystick::DefaultAxisConfig SDLInputJoystick::DefaultJoystickAxes[5] = {
	{JOYDEADZONE_DEFAULT, JOYAXIS_Side,    JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_X, JOYCURVE_DEFAULT},
	{JOYDEADZONE_DEFAULT, JOYAXIS_Forward, JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_Y, JOYCURVE_DEFAULT},
	{JOYDEADZONE_DEFAULT, JOYAXIS_None,    JOYSENSITIVITY_DEFAULT, JOYTHRESH_DEFAULT, JOYCURVE_DEFAULT},
	{JOYDEADZONE_DEFAULT, JOYAXIS_Yaw,     JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_X, JOYCURVE_DEFAULT},
	{JOYDEADZONE_DEFAULT, JOYAXIS_Pitch,   JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_Y, JOYCURVE_DEFAULT}
};

// Defaults if we have access to the GameController API for this device
const SDLInputJoystick::DefaultAxisConfig SDLInputJoystick::DefaultControllerAxes[6] = {
	{JOYDEADZONE_DEFAULT, JOYAXIS_Side,    JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_X, JOYCURVE_DEFAULT},
	{JOYDEADZONE_DEFAULT, JOYAXIS_Forward, JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_Y, JOYCURVE_DEFAULT},
	{JOYDEADZONE_DEFAULT, JOYAXIS_Yaw,     JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_X, JOYCURVE_DEFAULT},
	{JOYDEADZONE_DEFAULT, JOYAXIS_Pitch,   JOYSENSITIVITY_DEFAULT, JOYTHRESH_STICK_Y, JOYCURVE_DEFAULT},
	{JOYDEADZONE_DEFAULT, JOYAXIS_None,    JOYSENSITIVITY_DEFAULT, JOYTHRESH_TRIGGER, JOYCURVE_DEFAULT},
	{JOYDEADZONE_DEFAULT, JOYAXIS_None,    JOYSENSITIVITY_DEFAULT, JOYTHRESH_TRIGGER, JOYCURVE_DEFAULT},
};

class SDLInputJoystickManager
{
public:
	SDLInputJoystickManager()
	{
		UpdateDeviceList();
	}

	void UpdateDeviceList()
	{
		Joysticks.DeleteAndClear();
		for(int i = 0; i < SDL_NumJoysticks(); i++)
		{
			SDLInputJoystick *device = new SDLInputJoystick(i);
			if(device->IsValid())
				Joysticks.Push(device);
			else
				delete device;
		}
	}

	void AddAxes(float axes[NUM_JOYAXIS])
	{
		for(unsigned int i = 0;i < Joysticks.Size();i++)
			Joysticks[i]->AddAxes(axes);
	}

	void GetDevices(TArray<IJoystickConfig *> &sticks)
	{
		for(unsigned int i = 0;i < Joysticks.Size();i++)
		{
			M_LoadJoystickConfig(Joysticks[i]);
			sticks.Push(Joysticks[i]);
		}
	}

	void ProcessInput() const
	{
		for(unsigned int i = 0;i < Joysticks.Size();++i)
			if(Joysticks[i]->Enabled) Joysticks[i]->ProcessInput();
	}

	bool IsJoystickEnabled(int instanceID)
	{
		for(unsigned int i = 0; i < Joysticks.Size(); i++)
		{
			if (Joysticks[i]->InstanceID != instanceID)
			{
				continue;
			}
			return Joysticks[i]->Enabled;
		}
		return false;
	}

protected:
	TDeletingArray<SDLInputJoystick *> Joysticks;
};
static SDLInputJoystickManager *JoystickManager;

void I_StartupJoysticks()
{
#ifndef NO_SDL_JOYSTICK
	if(SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) >= 0)
		JoystickManager = new SDLInputJoystickManager();
#endif
}
void I_ShutdownInput()
{
	if(JoystickManager)
	{
		delete JoystickManager;
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
	}
}

void I_GetJoysticks(TArray<IJoystickConfig *> &sticks)
{
	sticks.Clear();

	if (JoystickManager)
		JoystickManager->GetDevices(sticks);
}

void I_GetAxes(float axes[NUM_JOYAXIS])
{
	for (int i = 0; i < NUM_JOYAXIS; ++i)
	{
		axes[i] = 0;
	}
	if (use_joystick && JoystickManager)
	{
		JoystickManager->AddAxes(axes);
	}
}

void I_ProcessJoysticks()
{
	if (use_joystick && JoystickManager)
		JoystickManager->ProcessInput();
}

void I_JoyConsumeEvent(int instanceID, event_t * event)
{
	if (event->type == EV_KeyDown)
	{
		bool okay = use_joystick && JoystickManager && JoystickManager->IsJoystickEnabled(instanceID);
		if (!okay) return;
	}
	D_PostEvent(event);
}

IJoystickConfig *I_UpdateDeviceList()
{
	JoystickManager->UpdateDeviceList();
	return NULL;
}
