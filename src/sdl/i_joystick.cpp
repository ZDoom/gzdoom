#include <SDL_joystick.h>

#include "m_joy.h"

class SDLInputJoystick: public IJoystickConfig
{
public:
	SDLInputJoystick(int DeviceIndex) : DeviceIndex(DeviceIndex), Multiplier(1.0f)
	{
		Device = SDL_JoystickOpen(DeviceIndex);
		if(Device != NULL)
			SetDefaultConfig();
	}
	~SDLInputJoystick()
	{
		if(Device != NULL)
			M_SaveJoystickConfig(this);
		SDL_JoystickClose(Device);
	}

	bool IsValid() const
	{
		return Device != NULL;
	}

	FString GetName()
	{
		return SDL_JoystickName(DeviceIndex);
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
		return SDL_JoystickNumAxes(Device);
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
		Axes[axis].DeadZone = zone;
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
		return Axes[axis].DeadZone == 0.0f;
	}
	bool IsAxisMapDefault(int axis)
	{
		if(axis >= 5)
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
			info.Name.Format("Axis %d", i);
			info.DeadZone = 0.0f;
			info.Multiplier = 1.0f;
			if(i >= 5)
				info.GameAxis = JOYAXIS_None;
			else
				info.GameAxis = DefaultAxes[i];
			Axes.Push(info);
		}
	}
	FString GetIdentifier()
	{
		char id[16];
		mysnprintf(id, countof(id), "JS:%d", DeviceIndex);
		return id;
	}

	void AddAxes(float axes[NUM_JOYAXIS])
	{
		// Add to game axes.
		for (int i = 0; i < GetNumAxes(); ++i)
		{
			if(Axes[i].GameAxis != JOYAXIS_None)
				axes[Axes[i].GameAxis] -= float(((double)SDL_JoystickGetAxis(Device, i)/32768.0) * Multiplier * Axes[i].Multiplier);
		}
	}

protected:
	struct AxisInfo
	{
		FString Name;
		float DeadZone;
		float Multiplier;
		EJoyAxis GameAxis;
	};
	static const EJoyAxis DefaultAxes[5];

	int					DeviceIndex;
	SDL_Joystick		*Device;

	float				Multiplier;
	TArray<AxisInfo>	Axes;
};
const EJoyAxis SDLInputJoystick::DefaultAxes[5] = {JOYAXIS_Side, JOYAXIS_Forward, JOYAXIS_Pitch, JOYAXIS_Yaw, JOYAXIS_Up};

class SDLInputJoystickManager
{
public:
	SDLInputJoystickManager()
	{
		for(int i = 0;i < SDL_NumJoysticks();i++)
		{
			SDLInputJoystick *device = new SDLInputJoystick(i);
			if(device->IsValid())
				Joysticks.Push(device);
			else
				delete device;
		}
	}
	~SDLInputJoystickManager()
	{
		for(unsigned int i = 0;i < Joysticks.Size();i++)
			delete Joysticks[i];
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
protected:
	TArray<SDLInputJoystick *> Joysticks;
};
static SDLInputJoystickManager *JoystickManager;

void I_StartupJoysticks()
{
	JoystickManager = new SDLInputJoystickManager();
}
void I_ShutdownJoysticks()
{
	delete JoystickManager;
}

void I_GetJoysticks(TArray<IJoystickConfig *> &sticks)
{
	sticks.Clear();

	JoystickManager->GetDevices(sticks);
}

void I_GetAxes(float axes[NUM_JOYAXIS])
{
	for (int i = 0; i < NUM_JOYAXIS; ++i)
	{
		axes[i] = 0;
	}
	if (use_joystick)
	{
		JoystickManager->AddAxes(axes);
	}
}

IJoystickConfig *I_UpdateDeviceList()
{
	return NULL;
}
