#pragma once

#include <stdint.h>
#include "tarray.h"
#include "name.h"
#include "keydef.h"

// Actions
struct FButtonStatus
{
	enum { MAX_KEYS = 6 };	// Maximum number of keys that can press this button

	uint16_t Keys[MAX_KEYS];
	bool bDown;				// Button is down right now
	bool bWentDown;			// Button went down this tic
	bool bWentUp;			// Button went up this tic
	bool bReleaseLock;		// Lock ReleaseKey call in ResetButtonStates

	bool bIsAxis;	// Whenever or not this button is being controlled by any axis.
	float Axis;		// How far the button has been pressed. Updated by I_GetAxes.

	void (*PressHandler)();		// for optional game-side customization
	void (*ReleaseHandler)();

	bool PressKey (int keynum);		// Returns true if this key caused the button to be pressed.
	bool ReleaseKey (int keynum);	// Returns true if this key is no longer pressed.
	void AddAxes (FString &btn_name, float joyaxes[NUM_AXIS_CODES]); // Update joystick axis information.
	void ResetTriggers () { bWentDown = bWentUp = false; }
	void Reset () { bDown = bWentDown = bWentUp = bIsAxis = false; Axis = 0.0f; }
};

class ButtonMap
{

	TArray<FButtonStatus> Buttons;
	TArray<FString> NumToName;		// The internal name of the button
	TMap<FName, int> NameToNum;

public:
	void SetButtons(const char** names, int count);

	int NumButtons() const
	{
		return Buttons.Size();
	}

	int FindButtonIndex(const char* func, int funclen = -1) const;

	FButtonStatus* FindButton(const char* func, int funclen = -1)
	{
		int index = FindButtonIndex(func, funclen);
		return index > -1 ? &Buttons[index] : nullptr;
	}

	FButtonStatus* GetButton(int index)
	{
		return &Buttons[index];
	}

	void ResetButtonTriggers();	// Call ResetTriggers for all buttons
	void ResetButtonStates();		// Same as above, but also clear bDown
	void GetAxes();				// Call AddAxes for all buttons
	int ListActionCommands(const char* pattern);
	void AddButtonTabCommands();


	bool ButtonDown(int x) const
	{
		return Buttons[x].bDown;
	}

	bool ButtonDownDigital(int x) const
	{
		// Like ButtonDown, but only for digital buttons.
		// This is necessary, because of how the game runs different
		// code for both digital and analog, and they can't be rolled
		// together without introducing some subtle gameplay differences.
		return (Buttons[x].bIsAxis == false && Buttons[x].bDown == true);
	}

	float ButtonAnalog(int x) const
	{
		// Get the analog value of a button.
		// This is 0.0 (not 1.0) for digital presses, because again,
		// of how the game logic is split between the two.
		if (Buttons[x].bIsAxis == true)
		{
			return Buttons[x].Axis;
		}

		return 0.0f;
	}

	bool ButtonPressed(int x) const
	{
		return Buttons[x].bWentDown;
	}

	bool ButtonReleased(int x) const
	{
		return Buttons[x].bWentUp;
	}

	void ButtonSet(int x) const
	{
		Buttons[x].bDown = Buttons[x].bWentDown = true;
		Buttons[x].bWentUp = false;
	}

	void ClearButton(int x)
	{
		Buttons[x].Reset();
	}
};

extern ButtonMap buttonMap;
