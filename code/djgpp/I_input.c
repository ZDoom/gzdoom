#include <stdlib.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>

#include "m_argv.h"
#include "i_input.h"
#include "i_system.h"
#include "v_video.h"
#include "d_main.h"
#include "c_consol.h"
#include "c_cvars.h"
#include "i_video.h"


static void I_StartupKeyboard (void);
static void I_ShutdownKeyboard (void);
static void I_StartupJoystick (void);
static void I_JoystickEvents (void);
static void mouse_init (void);
static void mouse_uninit (void);

#define KEYBOARDINT		9

#define _outbyte(x,y) (outp(x,y))
#define _outhword(x,y) (outpw(x,y))

#define _inbyte(x) (inp(x))
#define _inhword(x) (inpw(x))

#define SC_RSHIFT		0x36
#define SC_LSHIFT		0x2a
#define SC_UPARROW		0x48
#define SC_DOWNARROW	0x50
#define SC_LEFTARROW	0x4b
#define SC_RIGHTARROW	0x4d


#define KBDQUESIZE 32
static byte keyboardque[KBDQUESIZE];
static int kbdtail, kbdhead;
static BOOL shiftdown;

// Used by the console for making keys repeat
int KeyRepeatDelay;
int KeyRepeatRate;

extern constate_e ConsoleState;

cvar_t *i_remapkeypad;
cvar_t *usejoystick;
cvar_t *usemouse;

BOOL I_InitInput (void)
{
	atexit (I_ShutdownInput);

	Printf (PRINT_HIGH, "I_StartupMouse\n");
	mouse_init ();
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
	mouse_uninit ();
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

static int lastpress;

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

	keyboardque[kbdhead&(KBDQUESIZE-1)] = lastpress = _inbyte(0x60);
	kbdhead++;

// acknowledge the interrupt

	_outbyte(0x20,0x20);
	asm volatile ("popa; sti");
}
void __End_of_I_KeyboardISR (void) { }


static void I_StartupKeyboard (void)
{
#ifndef NOKBD
	_go32_dpmi_lock_code (I_KeyboardISR, (long)__End_of_I_KeyboardISR - (long)I_KeyboardISR);
	_go32_dpmi_lock_data ((void *)keyboardque, sizeof(keyboardque));
	_go32_dpmi_lock_data ((void *)kbdhead, sizeof(kbdhead));
	_go32_dpmi_lock_data ((void *)lastpress, sizeof(lastpress));

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

		if (k == lastk)
			continue;	// ignore repeating keys

		lastk = k;
		// extended keyboard shift key bullshit
		if ( (k&0x7f)==SC_LSHIFT || (k&0x7f)==SC_RSHIFT )
		{
			if ( keyboardque[(kbdtail-2)&(KBDQUESIZE-1)]==0xe0 )
				continue;
			k &= 0x80;
			k |= SC_LSHIFT;
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

		ev.data1 = k;
		ev.data2 = 0;
		switch (k)
		{
			case SC_LSHIFT:
				shiftdown = ev.type == ev_keydown;
				break;
			default:
				ev.data2 = Convert[k];
				break;
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

static void mouse_init (void)
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

static void mouse_uninit (void)
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
	if (M_CheckParm ("-nojoy") || !usejoystick->value)
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