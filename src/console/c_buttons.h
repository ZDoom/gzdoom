#pragma once

// Actions
struct FButtonStatus
{
	enum { MAX_KEYS = 6 };	// Maximum number of keys that can press this button

	uint16_t Keys[MAX_KEYS];
	uint8_t bDown;				// Button is down right now
	uint8_t bWentDown;			// Button went down this tic
	uint8_t bWentUp;			// Button went up this tic
	uint8_t padTo16Bytes;

	bool PressKey (int keynum);		// Returns true if this key caused the button to be pressed.
	bool ReleaseKey (int keynum);	// Returns true if this key is no longer pressed.
	void ResetTriggers () { bWentDown = bWentUp = false; }
	void Reset () { bDown = bWentDown = bWentUp = false; }
};

struct FActionMap
{
	FButtonStatus* Button;
	unsigned int	Key;	// value from passing Name to MakeKey()
	char			Name[12];
};


extern FButtonStatus Button_Mlook, Button_Klook, Button_Use, Button_AltAttack,
	Button_Attack, Button_Speed, Button_MoveRight, Button_MoveLeft,
	Button_Strafe, Button_LookDown, Button_LookUp, Button_Back,
	Button_Forward, Button_Right, Button_Left, Button_MoveDown,
	Button_MoveUp, Button_Jump, Button_ShowScores, Button_Crouch,
	Button_Zoom, Button_Reload,
	Button_User1, Button_User2, Button_User3, Button_User4,
	Button_AM_PanLeft, Button_AM_PanRight, Button_AM_PanDown, Button_AM_PanUp,
	Button_AM_ZoomIn, Button_AM_ZoomOut;
extern bool ParsingKeyConf, UnsafeExecutionContext;

void ResetButtonTriggers ();	// Call ResetTriggers for all buttons
void ResetButtonStates ();		// Same as above, but also clear bDown
FButtonStatus *FindButton (unsigned int key);
void AddButtonTabCommands();
extern FActionMap ActionMaps[];
