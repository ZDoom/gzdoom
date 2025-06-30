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

#include "basics.h"
#include "cmdlib.h"

#include "d_eventbase.h"
#include "m_joy.h"
#include "keydef.h"

#define DEFAULT_DEADZONE 0.25f;

// Very small deadzone so that floating point magic doesn't happen
#define MIN_DEADZONE 0.000001f

class SDLInputJoystick: public IJoystickConfig
{
public:
	SDLInputJoystick(int DeviceIndex) : DeviceIndex(DeviceIndex), Multiplier(1.0f) , Enabled(true)
	{
		if (SDL_IsGameController(DeviceIndex))
		{
			Mapping = SDL_GameControllerOpen(DeviceIndex);

			DefaultAxes = DefaultControllerAxes;
			DefaultAxesCount = sizeof(DefaultControllerAxes) / sizeof(EJoyAxis);

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
			DefaultAxes = DefaultJoystickAxes;
			DefaultAxesCount = sizeof(DefaultJoystickAxes) / sizeof(EJoyAxis);

			if(Device != NULL)
			{
				NumAxes = SDL_JoystickNumAxes(Device);
				NumHats = SDL_JoystickNumHats(Device);

				SetDefaultConfig();
			}
		}
	}
	~SDLInputJoystick()
	{
		if(Device != NULL || Mapping != NULL)
			M_SaveJoystickConfig(this);
		SDL_GameControllerClose(Mapping);
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

	void SetAxisDeadZone(int axis, float zone)
	{
		Axes[axis].DeadZone = clamp(zone, MIN_DEADZONE, 1.f);
	}
	void SetAxisMap(int axis, EJoyAxis gameaxis)
	{
		Axes[axis].GameAxis = gameaxis;
	}
	void SetAxisScale(int axis, float scale)
	{
		Axes[axis].Multiplier = scale;
	}

	// Used by the saver to not save properties that are at their defaults.
	bool IsSensitivityDefault()
	{
		return Multiplier == 1.0f;
	}
	bool IsAxisDeadZoneDefault(int axis)
	{
		return Axes[axis].DeadZone <= MIN_DEADZONE;
	}
	bool IsAxisMapDefault(int axis)
	{
		if(axis >= DefaultAxesCount)
			return Axes[axis].GameAxis == JOYAXIS_None;
		return Axes[axis].GameAxis == DefaultAxes[axis];
	}
	bool IsAxisScaleDefault(int axis)
	{
		return Axes[axis].Multiplier == 1.0f;
	}

	void SetDefaultConfig()
	{
		for(int i = 0;i < GetNumAxes();i++)
		{
			AxisInfo info;

			if (Mapping) {
				switch(i) {
					case SDL_CONTROLLER_AXIS_LEFTX: info.Name = "Left Stick X"; break;
					case SDL_CONTROLLER_AXIS_LEFTY: info.Name = "Left Stick Y"; break;
					case SDL_CONTROLLER_AXIS_RIGHTX: info.Name = "Right Stick X"; break;
					case SDL_CONTROLLER_AXIS_RIGHTY: info.Name = "Right Stick Y"; break;
					case SDL_CONTROLLER_AXIS_TRIGGERLEFT: info.Name = "Left Trigger"; break;
					case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: info.Name = "Right Trigger"; break;
					default: info.Name.Format("Axis %d", i+1); break;
				}
			} else {
				if(i < NumAxes)
					info.Name.Format("Axis %d", i+1);
				else
					info.Name.Format("Hat %d (%c)", (i-NumAxes)/2 + 1, (i-NumAxes)%2 == 0 ? 'x' : 'y');
			}

			info.DeadZone = DEFAULT_DEADZONE;
			info.Multiplier = 1.0f;
			info.Value = 0.0;
			info.ButtonValue = 0;

			info.GameAxis = (i < DefaultAxesCount)? DefaultAxes[i]:  JOYAXIS_None;

			Axes.Push(info);
		}
	}

	bool GetEnabled()
	{
		return Enabled;
	}

	void SetEnabled(bool enabled)
	{
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

	void ProcessInput();

protected:
	struct AxisInfo
	{
		FString Name;
		float DeadZone;
		float Multiplier;
		EJoyAxis GameAxis;
		double Value;
		uint8_t ButtonValue;
	};
	static const EJoyAxis DefaultJoystickAxes[5];
	static const EJoyAxis DefaultControllerAxes[6];
	const EJoyAxis * DefaultAxes;
	int DefaultAxesCount;

	int					DeviceIndex;
	SDL_Joystick		*Device;
	SDL_GameController	*Mapping;

	float				Multiplier;
	bool				Enabled;
	TArray<AxisInfo>	Axes;
	int					NumAxes;
	int					NumHats;

	friend class SDLInputJoystickManager;
};

// [Nash 4 Feb 2024] seems like on Linux, the third axis is actually the Left Trigger, resulting in the player uncontrollably looking upwards.
const EJoyAxis SDLInputJoystick::DefaultJoystickAxes[5] = {JOYAXIS_Side, JOYAXIS_Forward, JOYAXIS_None, JOYAXIS_Yaw, JOYAXIS_Pitch};

// Defaults if we have access to the Gamepad API for this device
const EJoyAxis SDLInputJoystick::DefaultControllerAxes[6] = {JOYAXIS_Side, JOYAXIS_Forward, JOYAXIS_Yaw, JOYAXIS_Pitch, JOYAXIS_None, JOYAXIS_None};



class SDLInputJoystickManager
{
public:
	SDLInputJoystickManager()
	{
		this->UpdateDeviceList();
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

void PostKeyEvent(bool down, EKeyCodes which)
{
	event_t event = { 0,0,0,0,0,0,0 };
	event.type = down ? EV_KeyDown : EV_KeyUp;
	event.data1 = which;
	D_PostEvent(&event);
}

void SDLInputJoystick::ProcessInput() {
	uint8_t buttonstate;

	if (Mapping)
	{
		// GameController API available

		auto lastTriggerL = Axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].Value > MIN_DEADZONE;
		auto lastTriggerR = Axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].Value > MIN_DEADZONE;

		for (auto i = 0; i < SDL_CONTROLLER_AXIS_MAX && i < NumAxes; ++i)
		{
			buttonstate = 0;

			Axes[i].Value = SDL_GameControllerGetAxis(Mapping, static_cast<SDL_GameControllerAxis>(i))/32767.0;
			Axes[i].Value = Joy_RemoveDeadZone(Axes[i].Value, Axes[i].DeadZone, &buttonstate);
		}

		auto currTriggerL = Axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].Value > MIN_DEADZONE;
		auto currTriggerR = Axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].Value > MIN_DEADZONE;

		if (lastTriggerL != currTriggerL) PostKeyEvent(currTriggerL, KEY_PAD_LTRIGGER);
		if (lastTriggerR != currTriggerR) PostKeyEvent(currTriggerR, KEY_PAD_RTRIGGER);

		buttonstate = Joy_XYAxesToButtons(Axes[0].Value, Axes[1].Value);
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
			buttonstate = Joy_XYAxesToButtons(Axes[0].Value, Axes[1].Value);
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

IJoystickConfig *I_UpdateDeviceList()
{
	JoystickManager->UpdateDeviceList();
	return NULL;
}
