#include <stdlib.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>

#include "m_argv.h"
#include "i_input.h"
#include "i_system.h"
#include "v_video.h"
#include "d_main.h"
#include "c_console.h"
#include "c_cvars.h"
#include "i_video.h"

// from allegro.h
#define END_OF_FUNCTION(x)    void x##_end() { }
#define LOCK_VARIABLE(x)      _go32_dpmi_lock_data((void *)&x, sizeof(x))
#define LOCK_FUNCTION(x)      _go32_dpmi_lock_code(x, (long)x##_end - (long)x)

static void I_StartupKeyboard (void);
static void I_ShutdownKeyboard (void);
static void I_StartupMouse (void);
static void I_ShutdownMouse (void);
#if 0
static void I_StartupJoystick (void);
static void I_JoystickEvents (void);
#endif

#define KEYBOARDINT		9

#define _outbyte(x,y) (outp(x,y))
#define _outhword(x,y) (outpw(x,y))

#define _inbyte(x) (inp(x))
#define _inhword(x) (inpw(x))

#define DIK_LSHIFT		0x2A
#define DIK_RSHIFT		0x36
#define DIK_MULTIPLY	0x37	/* * on numeric keypad */
#define DIK_1			0x02
#define DIK_2			0x03
#define DIK_3			0x04
#define DIK_4			0x05
#define DIK_5			0x06
#define DIK_6			0x07
#define DIK_7			0x08
#define DIK_8			0x09
#define DIK_9			0x0A
#define DIK_0			0x0B
#define DIK_NUMPAD7 	0x47
#define DIK_NUMPAD8 	0x48
#define DIK_NUMPAD9 	0x49
#define DIK_SUBTRACT	0x4A	/* - on numeric keypad */
#define DIK_NUMPAD4 	0x4B
#define DIK_NUMPAD5 	0x4C
#define DIK_NUMPAD6 	0x4D
#define DIK_ADD 		0x4E	/* + on numeric keypad */
#define DIK_NUMPAD1 	0x4F
#define DIK_NUMPAD2 	0x50
#define DIK_NUMPAD3 	0x51
#define DIK_NUMPAD0 	0x52
#define DIK_DECIMAL 	0x53	/* . on numeric keypad */
#define DIK_DIVIDE		0xB5    /* / on numeric keypad */
#define DIK_HOME		0xC7	/* Home on arrow keypad */
#define DIK_UP			0xC8	/* UpArrow on arrow keypad */
#define DIK_PRIOR		0xC9	/* PgUp on arrow keypad */
#define DIK_LEFT		0xCB	/* LeftArrow on arrow keypad */
#define DIK_RIGHT		0xCD	/* RightArrow on arrow keypad */
#define DIK_END 		0xCF	/* End on arrow keypad */
#define DIK_DOWN		0xD0	/* DownArrow on arrow keypad */
#define DIK_NEXT		0xD1	/* PgDn on arrow keypad */
#define DIK_INSERT		0xD2	/* Insert on arrow keypad */
#define DIK_DELETE		0xD3	/* Delete on arrow keypad */

#define KBDQUESIZE 32
static byte keyboardque[KBDQUESIZE];
static unsigned int kbdtail, kbdhead;
static BOOL shiftdown;

// Used by the console for making keys repeat
int KeyRepeatDelay;
int KeyRepeatRate;

extern constate_e ConsoleState;
extern BOOL menuactive;

cvar_t *i_remapkeypad;
cvar_t *usejoystick;
cvar_t *usemouse;

BOOL I_InitInput (void)
{
	atexit (I_ShutdownInput);

	Printf (PRINT_HIGH, "I_StartupMouse\n");
	I_StartupMouse ();
//	Printf (PRINT_HIGH, "I_StartupJoystick\n");
//	I_StartupJoystick ();
	Printf (PRINT_HIGH, "I_StartupKeyboard\n");
	I_StartupKeyboard ();

	KeyRepeatDelay = (250 * TICRATE) / 1000;
	KeyRepeatRate = TICRATE / 15;

	return true;
}


// Free all input resources
void I_ShutdownInput (void)
{
	I_ShutdownMouse ();
	I_ShutdownKeyboard ();
}


/****** Keyboard Routines ******/

static const byte Convert [256] =
{
  //  0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
	  0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',   8,   9, // 0
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',  13,   0, 'a', 's', // 1
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  39, '`',   0,'\\', 'z', 'x', 'c', 'v', // 2
	'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' ',   0,   0,   0,   0,   0,   0, // 3
	  0,   0,   0,   0,   0,   0,   0, '7', '8', '9', '-', '4', '5', '6', '+', '1', // 4
	'2', '3', '0', '.',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 5
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 6
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 7

	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, '=',   0,   0, // 8
	  0, '@', ':', '_',   0,   0,   0,   0,   0,   0,   0,   0,  13,   0,   0,   0, // 9
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // A
	  0,   0,   0, ',',   0, '/',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // B
};

static const byte Convert_Shift [256] =
{
  //  0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
	  0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',   8,   9, // 0
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',  13,   0, 'A', 'S', // 1
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0, '|', 'Z', 'X', 'C', 'V', // 2
	'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' ',   0,   0,   0,   0,   0,   0, // 3
	  0,   0,   0,   0,   0,   0,   0, '7', '8', '9', '-', '4', '5', '6', '+', '1', // 4
	'2', '3', '0', '.',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 5
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 6
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 7

	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, '=',   0,   0, // 8
	  0, '@', ':', '_',   0,   0,   0,   0,   0,   0,   0,   0,  13,   0,   0,   0, // 9
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // A
	  0,   0,   0, ',',   0, '/',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // B
};

static _go32_dpmi_seginfo oldkeyboardisr, newkeyboardisr;
static BOOL keyboardinited;

/*
================
=
= I_KeyboardISR
=
================
*/

void I_KeyboardISR (void)
{
	asm volatile ("cli; pusha");
// Get the scan code

	keyboardque[kbdhead&(KBDQUESIZE-1)] = _inbyte(0x60);
	kbdhead++;

// acknowledge the interrupt

	_outbyte(0x20,0x20);
	asm volatile ("popa; sti");
}

END_OF_FUNCTION(I_KeyboardISR)


static void I_StartupKeyboard (void)
{
#ifndef NOKBD
	LOCK_FUNCTION (I_KeyboardISR);
	LOCK_VARIABLE (keyboardque);
	LOCK_VARIABLE (kbdhead);

	asm volatile ("cli");
	newkeyboardisr.pm_offset = (int)I_KeyboardISR;
	newkeyboardisr.pm_selector = _go32_my_cs();
	_go32_dpmi_get_protected_mode_interrupt_vector (KEYBOARDINT, &oldkeyboardisr);
	_go32_dpmi_allocate_iret_wrapper (&newkeyboardisr);
	_go32_dpmi_set_protected_mode_interrupt_vector (KEYBOARDINT, &newkeyboardisr);
	keyboardinited = true;
	asm volatile ("sti");
#endif

//I_ReadKeys ();
}

void I_ShutdownKeyboard (void)
{
	short temp;

	if (keyboardinited) {
		_go32_dpmi_set_protected_mode_interrupt_vector (KEYBOARDINT, &oldkeyboardisr);
		_go32_dpmi_free_iret_wrapper (&newkeyboardisr);
	}
	// clear bios key buffer
	_dosmemgetw (0x41a, 1, &temp);
	_dosmemputw (&temp, 1, 0x41c);
}

static void I_ReadKeyboard (void)
{
	static BOOL special = false;
	static int lastk;
	int k;
	event_t ev;

	while (kbdtail < kbdhead)
	{
		k = keyboardque[kbdtail&(KBDQUESIZE-1)];
		kbdtail++;

		// extended keyboard shift key bullshit
		if ( (k&0x7f)==DIK_LSHIFT || (k&0x7f)==DIK_RSHIFT )
		{
			if ( keyboardque[(kbdtail-2)&(KBDQUESIZE-1)]==0xe0 )
				continue;
			k &= 0x80;
			k |= DIK_LSHIFT;
		}

		if (k==0xe0)
		{
			special = true;
			continue;				// special / pause keys
		}
		if (keyboardque[(kbdtail-2)&(KBDQUESIZE-1)] == 0xe1)
			continue;				// pause key bullshit

		if (k==0xc5 && keyboardque[(kbdtail-2)&(KBDQUESIZE-1)] == 0x9d)
		{
			ev.type = ev_keydown;
			ev.data1 = KEY_PAUSE;
			D_PostEvent (&ev);
			continue;
		}

		if (k&0x80)
			ev.type = ev_keyup;
		else
			ev.type = ev_keydown;

		k &= 0x7f;
		if (special)
		{
			special = false;
			switch (k)
			{
				case 0x47:
				case 0x48:
				case 0x49:
				case 0x4b:
				case 0x4d:
				case 0x4f:
				case 0x50:
				case 0x51:
				case 0x52:
				case 0x53:
					k |= 0x80;
					break;
			}
		}

		if ((k | (ev.type<<8)) == lastk)
			continue;	// ignore repeating keys

		lastk = k | (ev.type<<8);
		ev.data2 = 0;
		switch (k)
		{
			case DIK_LSHIFT:
				shiftdown = ev.type == ev_keydown;
				break;
			default:
				if (!menuactive &&
					(ConsoleState == c_falling || ConsoleState == c_down)) {
					switch (k) {
						case DIK_NUMPAD4:
							k = DIK_4;
							break;
						case DIK_NUMPAD6:
							k = DIK_6;
							break;
						case DIK_NUMPAD8:
							k = DIK_8;
							break;
						case DIK_NUMPAD2:
							k = DIK_2;
							break;
						case DIK_NUMPAD7:
							k = DIK_7;
							break;
						case DIK_NUMPAD9:
							k = DIK_9;
							break;
						case DIK_NUMPAD3:
							k = DIK_3;
							break;
						case DIK_NUMPAD1:
							k = DIK_1;
							break;
						case DIK_NUMPAD0:
							k = DIK_0;
							break;
						case DIK_NUMPAD5:
							k = DIK_5;
							break;
					}
				} else if (i_remapkeypad->value) {
					switch (k) {
						case DIK_NUMPAD4:
							k = DIK_LEFT;
							break;
						case DIK_NUMPAD6:
							k = DIK_RIGHT;
							break;
						case DIK_NUMPAD8:
							k = DIK_UP;
							break;
						case DIK_NUMPAD2:
							k = DIK_DOWN;
							break;
						case DIK_NUMPAD7:
							k = DIK_HOME;
							break;
						case DIK_NUMPAD9:
							k = DIK_PRIOR;
							break;
						case DIK_NUMPAD3:
							k = DIK_NEXT;
							break;
						case DIK_NUMPAD1:
							k = DIK_END;
							break;
						case DIK_NUMPAD0:
							k = DIK_INSERT;
							break;
						case DIK_DECIMAL:
							k = DIK_DELETE;
							break;
					}
				}
				ev.data2 = Convert[k];
				break;
		}
		ev.data1 = k;
		if (ConsoleState == c_falling || ConsoleState == c_down) {
			switch (k) {
				case DIK_DIVIDE:
					ev.data2 = '/';
					break;
				case DIK_MULTIPLY:
					ev.data2 = '*';
					break;
				case DIK_ADD:
					ev.data2 = '+';
					break;
				case DIK_SUBTRACT:
					ev.data2 = '-';
					break;
				case DIK_DECIMAL:
					ev.data2 = '.';
					break;
			}
		}
		if (shiftdown)
			ev.data3 = Convert_Shift[k];
		else
			ev.data3 = ev.data2;
		D_PostEvent (&ev);
	}
}


/****** Mouse Functions ******/

static BOOL mouse_present;
static int num_buttons;

static void I_StartupMouse (void)
{
	__dpmi_regs r;

	r.x.ax = 0;
	__dpmi_int (0x33, &r);		// Initialize mouse driver

	if (r.x.ax == 0) {
		mouse_present = false;
		return;
	}

	mouse_present = true;
	num_buttons = r.x.bx;
	if (num_buttons == 0xffff)
		num_buttons = 2;
	else if (num_buttons > 4)
		num_buttons = 4;
}

static void I_ShutdownMouse (void)
{
	// We don't need to do anything here.
}

static void ReadMouse (void)
{
	static int last_buttons;
	int mouse_buttons;

	event_t ev;
	__dpmi_regs r;

	if (!mouse_present || !usemouse->value)
		return;

	r.x.ax = 11;
	__dpmi_int (0x33, &r);

	ev.data2 = ((signed short)r.x.cx) * 2;
	ev.data3 = -(signed short)r.x.dx;

	if (ev.data2 || ev.data3) {
		ev.type = ev_mouse;
		ev.data1 = 0;
		D_PostEvent (&ev);
	}

	r.x.ax = 3;
	__dpmi_int (0x33, &r);

	mouse_buttons = r.x.bx;

	if (mouse_buttons != last_buttons) {
		int rollnow = mouse_buttons;
		int rollthen = last_buttons;
		int i;
		
		last_buttons = mouse_buttons;

		ev.data2 = ev.data3 = 0;

		for (i = 0; i < num_buttons; i++, rollnow >>= 1, rollthen >>= 1) {
			if ((rollnow & 1) != (rollthen & 1)) {
				ev.type = (rollnow & 1) ? ev_keydown : ev_keyup;
				ev.data1 = i + KEY_MOUSE1;
				D_PostEvent (&ev);
			}
		}
	}
}


/****** Joystick Functions ******/

//==================================================
//
// joystick vars
//
//==================================================

BOOL joystickpresent;
extern unsigned int joystickx, joysticky;
BOOL I_ReadJoystick (void);			// returns false if not connected

int joyxl, joyxh, joyyl, joyyh;

#if 0
BOOL WaitJoyButton (void)
{
	int oldbuttons, buttons;

	oldbuttons = 0;
	do
	{
		I_WaitVBL (1);
		buttons = ((inp(0x201) >> 4)&1)^1;
		if (buttons != oldbuttons)
		{
			oldbuttons = buttons;
			continue;
		}

		if ((lastpress& 0x7f) == 1)
		{
			joystickpresent = false;
			return false;
		}
	} while (!buttons);

	do
	{
		I_WaitVBL (1);
		buttons =  ((inp(0x201) >> 4)&1)^1;
		if (buttons != oldbuttons)
		{
			oldbuttons = buttons;
			continue;
		}

		if ((lastpress & 0x7f) == 1)
		{
			joystickpresent = false;
			return false;
		}
	} while ( buttons);

	return true;
}



/*
===============
=
= I_StartupJoystick
=
===============
*/

int basejoyx, basejoyy;

static void I_StartupJoystick (void)
{
	int centerx, centery;

	joystickpresent = 0;
	if (Args.CheckParm ("-nojoy") || !usejoystick->value)
		return;

	if (!I_ReadJoystick ())
	{
		joystickpresent = false;
		DPrintf ("joystick not found ",0);
		return;
	}
	Printf (PRINT_HIGH, "joystick found\n");
	joystickpresent = true;

	Printf (PRINT_HIGH, "CENTER the joystick and press button 1:");
	if (!WaitJoyButton ())
		return;
	I_ReadJoystick ();
	centerx = joystickx;
	centery = joysticky;

	Printf (PRINT_HIGH, "\nPush the joystick to the UPPER LEFT corner and press button 1:");
	if (!WaitJoyButton ())
		return;
	I_ReadJoystick ();
	joyxl = (centerx + joystickx)/2;
	joyyl = (centerx + joysticky)/2;

	Printf (PRINT_HIGH, "\nPush the joystick to the LOWER RIGHT corner and press button 1:");
	if (!WaitJoyButton ())
		return;
	I_ReadJoystick ();
	joyxh = (centerx + joystickx)/2;
	joyyh = (centery + joysticky)/2;
	Printf (PRINT_HIGH, "\n");
}

/*
===============
=
= I_JoystickEvents
=
===============
*/

static void I_JoystickEvents (void)
{
	event_t ev;

//
// joystick events
//
	if (!joystickpresent)
		return;

	I_ReadJoystick ();
	ev.type = ev_joystick;
	ev.data1 =  ((inp(0x201) >> 4)&15)^15;

	if (joystickx < joyxl)
		ev.data2 = -1;
	else if (joystickx > joyxh)
		ev.data2 = 1;
	else
		ev.data2 = 0;
	if (joysticky < joyyl)
		ev.data3 = -1;
	else if (joysticky > joyyh)
		ev.data3 = 1;
	else
		ev.data3 = 0;

	D_PostEvent (&ev);
}
#endif	//0

/****** Input Event Generation ******/

void I_StartTic (void)
{
	ReadMouse ();
	I_ReadKeyboard ();
}

void I_StartFrame (void)
{
#if 0
	I_JoystickEvents ();
#endif
}