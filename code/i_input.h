#ifndef __I_INPUT_H__
#define __I_INPUT_H__
#include <dinput.h>


#define DINPUT_BUFFERSIZE           32

LPDIRECTINPUT			g_pdi;
LPDIRECTINPUTDEVICE		g_pKey;
LPDIRECTINPUTDEVICE		g_pMouse;

HANDLE					g_hevtKey;
HANDLE					g_hevtMouse;

//Other globals
int MouseCurX,MouseCurY,GDx,GDy;

extern HINSTANCE		g_hInst;					/* My instance handle */
extern BOOL				g_fActive;					/* Am I the active window? */

void KeyRead(void);
BOOL DI_Init2(void);
void DITerm2(void);

void MouseRead(HWND hwnd);
BOOL DI_Init(HWND hwnd);
void DITerm(void);

void I_GetEvent(void);

#endif
