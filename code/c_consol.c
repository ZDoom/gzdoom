#include "m_alloc.h"
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "dstrings.h"
#include "c_consol.h"
#include "c_cvars.h"
#include "c_dispch.h"
#include "hu_stuff.h"
#include "i_input.h"
#include "i_system.h"
#include "m_swap.h"
#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "z_zone.h"
#include "r_main.h"
#include "r_draw.h"
#include "st_stuff.h"

#include "doomstat.h"

static void C_TabComplete (void);
static BOOL TabbedLast;		// Last key pressed was tab


static screen_t conback;		// As of 1.13, Console backdrop is just another screen


extern int		gametic;
extern BOOL		automapactive;	// in AM_map.c

typedef enum cstate_t {
	up=0, down=1, falling=2, rising=3
} constate;


int			ConRows, ConCols, PhysRows;
char		*Lines, *Last = NULL;
BOOL		vidactive = false, gotconback = false;
BOOL		cursoron = false;
int			ShowRows, SkipRows, ConsoleTicker, ConBottom, ConScroll, RowAdjust;
int			CursorTicker, ScrollState = 0;
int			ConsoleState = up;
char		VersionString[8];

event_t		RepeatEvent;		// always type ev_keydown
int			RepeatCountdown;
BOOL		KeysShifted;


#define SCROLLUP 1
#define SCROLLDN 2
#define SCROLLNO 0

extern cvar_t *showMessages;
extern BOOL message_dontfuckwithme;		// Who came up with this name?

extern BOOL chat_on;

struct History {
	struct History *Older;
	struct History *Newer;
	char String[1];
};

// CmdLine[0]  = # of chars on command line
// CmdLine[1]  = cursor position
// CmdLine[2+] = command line (max 255 chars + NULL)
// CmdLine[259]= offset from beginning of cmdline to display
static byte CmdLine[260];

static byte printxormask;

#define MAXHISTSIZE 50
static struct History *HistHead = NULL, *HistTail = NULL, *HistPos = NULL;
static int HistSize;

cvar_t *NotifyTime;
static byte NotifyStrings[4][256];

FILE *Logfile = NULL;


extern patch_t *hu_font[HU_FONTSIZE];	// from hu_stuff.c

void C_HandleKey (event_t *ev, byte *buffer, int len);


static int C_hu_CharWidth (int c)
{
	c = toupper((c++) & 0x7f) - HU_FONTSTART;
	if (c < 0 || c >= HU_FONTSIZE)
		return 4;

	return hu_font[c]->width;
}

void C_InitConsole (int width, int height, BOOL ingame)
{
	int row;
	char *zap;
	char *old;
	int cols, rows;

	if ( (vidactive = ingame) ) {
		if (!gotconback) {
			BOOL stylize = false;
			patch_t *patch;
			int num;

			num = W_CheckNumForName ("CONBACK");
			if (num == -1) {
				num = W_CheckNumForName ("TITLEPIC");
				stylize = true;
			}

			patch = W_CacheLumpNum (num, PU_CACHE);

			V_AllocScreen (&conback, SHORT(patch->width), SHORT(patch->height), 8);
			V_LockScreen (&conback);

			V_DrawPatch (0, 0, &conback, patch);

			if (stylize) {
				byte *fadetable = W_CacheLumpName ("COLORMAP", PU_CACHE), f, *v, *i;
				int x, y;

				for (y = 0; y < conback.height; y++) {
					i = conback.buffer + conback.pitch * y;
					if (y < 8 || y > 191) {
						if (y < 8)
							f = y;
						else
							f = 199 - y;
						v = fadetable + (30 - f) * 256;
						for (x = 0; x < conback.width; x++) {
							*i = v[*i];
							i++;
						}
					} else {
						for (x = 0; x < conback.width; x++) {
							if (x <= 8)
								v = fadetable + (30 - x) * 256;
							else if (x > 312)
								v = fadetable + (x - 289) * 256;
							*i = v[*i];
							i++;
						}
					}
				}
			}
			sprintf (VersionString, "v%d.%d", VERSION / 100, VERSION % 100);

			V_UnlockScreen (&conback);

			gotconback = true;
		}
	}

	cols = ConCols;
	rows = ConRows;

	ConCols = width / 8 - 2;
	PhysRows = height / 8;
	ConRows = PhysRows * 10;

	old = Lines;
	Lines = Malloc (ConRows * (ConCols + 2) + 1);

	for (row = 0, zap = Lines; row < ConRows; row++, zap += ConCols + 2) {
		zap[0] = 0;
		zap[1] = 0;
	}

	Last = Lines + (ConRows - 1) * (ConCols + 2);

	if (old) {
		char string[256];
		FILE *templog;

		templog = Logfile;		// Don't log our reformatting pass
		Logfile = NULL;

		for (row = 0, zap = old; row < rows; row++, zap += cols + 2) {
			memcpy (string, &zap[2], zap[1]);
			if (!zap[0]) {
				string[zap[1]] = '\n';
				string[zap[1]+1] = 0;
			} else {
				string[zap[1]] = 0;
			}
			Printf ("%s", string);
		}
		free (old);
		ShowRows = 0;

		Logfile = templog;
	}
}

void C_AddNotifyString (const char *source)
{
	brokenlines_t *lines;
	int i;

	if (!(lines = V_BreakLines (320, source)))
		return;

	for (i = 0; lines[i].width != -1; i++) {
		memmove (NotifyStrings[0], NotifyStrings[1], 256 * 3);
		strcpy (NotifyStrings[3], lines[i].string);
	}

	V_FreeBrokenLines (lines);

	ShowRows += i;

	if (ShowRows > 4)
		ShowRows = 4;

	if (showMessages->value == 0.0)
		ShowRows = message_dontfuckwithme;

	ConsoleTicker = (int)(NotifyTime->value * TICRATE) + gametic;
}

/* Provide our own Printf() that is sensitive of the
 * console status (in or out of game)
 */
int PrintString (const char *outline) {
	const char *cp, *newcp;
	static int xp = 0;
	int newxp;
	BOOL scroll;

	if (Logfile) {
		fputs (outline, Logfile);
//#ifdef _DEBUG
		fflush (Logfile);
//#endif
	}

	if (vidactive)
		C_AddNotifyString (outline);

	cp = outline;
	while (*cp) {
		for (newcp = cp, newxp = xp;
			*newcp != '\n' && *newcp != '\0' && newxp < ConCols;
			 newcp++, newxp++) {
			if (*newcp == '\x8') {
				if (xp) xp--;
				newxp = xp;
				cp++;
			}
		}

		if (*cp) {
			const char *poop;
			int x;

			for (x = xp, poop = cp; poop < newcp; poop++, x++) {
				Last[x+2] = ((*poop) < 32) ? (*poop) : ((*poop) ^ printxormask);
			}

			if (Last[1] < xp + (newcp - cp))
				Last[1] = xp + (newcp - cp);

			if (*newcp == '\n' || xp == ConCols) {
				if (*newcp != '\n') {
					Last[0] = 1;
				}
				memmove (Lines, Lines + (ConCols + 2), (ConCols + 2) * (ConRows - 1));
				Last[0] = 0;
				Last[1] = 0;
				newxp = 0;
				scroll = true;
			} else {
				scroll = false;
			}
			if (!vidactive) {
				I_PrintStr (xp, cp, newcp - cp, scroll);
			}

			if (scroll) {
				SkipRows = 1;
				RowAdjust = 0;
			} else {
				SkipRows = 0;
			}

			xp = newxp;

			if (*newcp == '\n')
				cp = newcp + 1;
			else
				cp = newcp;
		}
	}

	printxormask = 0;

	return strlen (outline);
}

int VPrintf (const char *format, va_list parms)
{
	char outline[8192];

	vsprintf (outline, format, parms);
	return PrintString (outline);
}

int Printf (const char *format, ...)
{
	va_list argptr;
	int count;

	va_start (argptr, format);
	count = VPrintf (format, argptr);
	va_end (argptr);

	return count;
}

int Printf_Bold (const char *format, ...)
{
	va_list argptr;
	int count;

	printxormask = 0x80;
	va_start (argptr, format);
	count = VPrintf (format, argptr);
	va_end (argptr);

	return count;
}

int DPrintf (const char *format, ...)
{
	va_list argptr;
	int count;

	if (developer->value) {
		va_start (argptr, format);
		count = VPrintf (format, argptr);
		va_end (argptr);
		return count;
	} else {
		return 0;
	}
}

void C_FlushDisplay (void)
{
	ShowRows = 0;
}

void C_NewModeAdjust (void)
{
	C_InitConsole (screens[0].width, screens[0].height, true);
	ShowRows = 0;
	if (ConBottom > screens[0].height / 16 || ConsoleState == down)
		ConBottom = screens[0].height / 16;
}

void C_Ticker (void)
{
	static int lasttic = 0;

	if (lasttic == 0)
		lasttic = gametic - 1;

	if (ConsoleState != up) {
		// Handle repeating keys
		switch (ScrollState) {
			case SCROLLUP:
				RowAdjust++;
				break;

			case SCROLLDN:
				if (RowAdjust)
					RowAdjust--;
				break;

			default:
				if (RepeatCountdown) {
					if (--RepeatCountdown == 0) {
						RepeatCountdown = KeyRepeatRate;
						C_HandleKey (&RepeatEvent, CmdLine, 255);
					}
				}
				break;
		}

		if (ConsoleState == falling) {
			ConBottom += (gametic - lasttic) * (screens[0].height / 100);
			if (ConBottom >= screens[0].height / 16) {
				ConBottom = screens[0].height / 16;
				ConsoleState = down;
			}
		} else if (ConsoleState == rising) {
			ConBottom -= (gametic - lasttic) * (screens[0].height / 100);
			if (ConBottom <= 0) {
				ConsoleState = up;
				ConBottom = 0;
			}
		}

		if (SkipRows + RowAdjust + ConBottom + 1 > ConRows) {
			RowAdjust = ConRows - SkipRows - ConBottom;
		}
	}

	if (ShowRows && gametic >= ConsoleTicker) {
		ShowRows--;
		ConsoleTicker = (int)(NotifyTime->value * TICRATE) + gametic;
	}

	if (--CursorTicker <= 0) {
		cursoron ^= 1;
		CursorTicker = C_BLINKRATE;
	}

	lasttic = gametic;
}
extern byte *WhiteMap;
extern byte *Ranges;

static void C_DrawNotifyText (void)
{
	int lines, fact;
	
	if (ShowRows == 0 || gamestate != GS_LEVEL || chat_on || menuactive)
		return;

	fact = 8 * CleanYfac;

	if (viewactive && (viewwindowy || viewwindowx)) {
		C_EraseLines (0, (ShowRows + (ShowRows < 4)) * fact);
	}

	{
		byte *holdmap = WhiteMap;

		// Use normal red map. Quick hack until this is truly customizable.
		WhiteMap = Ranges + 6 * 256;

		for (lines = 0; lines < ShowRows; lines++) {
			V_DrawTextClean (0, (ShowRows - lines - 1) * fact, NotifyStrings[3 - lines]);
		}

		WhiteMap = holdmap;
	}
}

void C_DrawConsole (void)
{
	char *zap;
	int lines, left;
	static int oldbottom = 0;

	left = 8;
	lines = ConBottom - 2;
	zap = Last - (SkipRows + RowAdjust) * (ConCols + 2);

	if ((ConBottom < oldbottom) && (gamestate == GS_LEVEL) && (viewwindowx || viewwindowy)
		&& !automapactive) {
		C_EraseLines (ConBottom * 8, oldbottom * 8);
	}

	oldbottom = ConBottom;

	if (ConsoleState == up) {
		C_DrawNotifyText ();
		return;
	} else if (ConBottom) {
		int visheight, realheight;

		visheight = ConBottom * 8;
		realheight = (visheight * conback.height) / screens[0].height;

		V_Blit (&conback, 0, conback.height - realheight, conback.width, realheight,
				&screens[0], 0, 0, screens[0].width, visheight);

		if (ConBottom >= 2)
			V_PrintStr (screens[0].width - 8 - strlen(VersionString) * 8,
						ConBottom * 8 - 16,
						VersionString, strlen(VersionString), 0x80);
	}

	if (lines > 0) {
		for (; lines; lines--) {
			V_PrintStr (left, -8 + lines * 8, &zap[2], zap[1], 0);
			zap -= ConCols + 2;
		}
		if (ConBottom > 1) {
			V_PrintStr (left, (ConBottom - 2) * 8, "]", 1, 0x80);
#define MIN(a,b) (((a)<(b))?(a):(b))
			V_PrintStr (left + 8, (ConBottom - 2) * 8,
						&CmdLine[2+CmdLine[259]],
						MIN(CmdLine[0] - CmdLine[259], ConCols - 1), 0);
#undef MIN
			if (cursoron) {
				V_PrintStr (left + 8 + (CmdLine[1] - CmdLine[259])* 8,
							(ConBottom - 2) * 8, "\xb", 1, 0);
			}
			if (RowAdjust && ConBottom > 2) {
				char c;

				// Indicate that the view has been scrolled up...
				if (SkipRows + RowAdjust + ConBottom != ConRows)
					c = '^';
				// and if we can scroll no further
				else
					c = '*';
				V_PrintStr (0, (ConBottom - 3) * 8 + 6, &c, 1, 0x80);
			}
		}
	}
}

void C_ToggleConsole (void)
{
	// Only let the console go up if chat mode is on
	if (!chat_on && (ConsoleState == up || ConsoleState == rising)) {
		ConsoleState = falling;
		HistPos = NULL;
		TabbedLast = false;
	} else {
		ConsoleState = rising;
	}
}

void C_HideConsole (void)
{
	ConsoleState = up;
	ConBottom = 0;
	HistPos = NULL;
}

static void makestartposgood (void)
{
	int n;
	int pos = CmdLine[259];
	int curs = CmdLine[1];
	int len = CmdLine[0];

	n = pos;

	if (pos >= len) {
		// Start of visible line is beyond end of line

		n = curs - ConCols + 2;
	}
	if ((curs - pos) >= ConCols - 2) {
		// The cursor is beyond the visible part of the line

		n = curs - ConCols + 2;
	}
	if (pos > curs) {
		// The cursor is in front of the visible part of the line

		n = curs;
	}
	if (n < 0)
		n = 0;
	CmdLine[259] = n;
}

void C_HandleKey (event_t *ev, byte *buffer, int len)
{
	switch (ev->data1) {
		case KEY_TAB:
			// Try to do tab-completion
			C_TabComplete ();
			break;
		case KEY_PGUP:
			if (KeysShifted)
				// Move to top of console buffer
				RowAdjust = ConRows - SkipRows - ConBottom;
			else
				// Start scrolling console buffer up
				ScrollState = SCROLLUP;
			break;
		case KEY_PGDN:
			if (KeysShifted)
				// Move to bottom of console buffer
				RowAdjust = 0;
			else
				// Start scrolling console buffer down
				ScrollState = SCROLLDN;
			break;
		case KEY_HOME:
			// Move cursor to start of line

			buffer[1] = buffer[len+4] = 0;
			break;
		case KEY_END:
			// Move cursor to end of line

			buffer[1] = buffer[0];
			makestartposgood ();
			break;
		case KEY_LEFTARROW:
			// Move cursor left one character

			if (buffer[1]) {
				buffer[1]--;
				makestartposgood ();
			}
			break;
		case KEY_RIGHTARROW:
			// Move cursor right one character

			if (buffer[1] < buffer[0]) {
				buffer[1]++;
				makestartposgood ();
			}
			break;
		case KEY_BACKSPACE:
			// Erase character to left of cursor

			if (buffer[0] && buffer[1]) {
				char *c, *e;

				e = &buffer[buffer[0] + 2];
				c = &buffer[buffer[1] + 2];

				for (; c < e; c++)
					*(c - 1) = *c;
				
				buffer[0]--;
				buffer[1]--;
				if (buffer[len+4])
					buffer[len+4]--;
				makestartposgood ();
			}

			TabbedLast = false;
			break;
		case KEY_DEL:
			// Erase charater under cursor

			if (buffer[1] < buffer[0]) {
				char *c, *e;

				e = &buffer[buffer[0] + 2];
				c = &buffer[buffer[1] + 3];

				for (; c < e; c++)
					*(c - 1) = *c;

				buffer[0]--;
				makestartposgood ();
			}

			TabbedLast = false;
			break;
		case KEY_RALT:
		case KEY_RCTRL:
			// Do nothing
			break;
		case KEY_RSHIFT:
			// SHIFT was pressed
			KeysShifted = true;
			break;
		case KEY_UPARROW:
			// Move to previous entry in the command history

			if (HistPos == NULL) {
				HistPos = HistHead;
			} else if (HistPos->Older) {
				HistPos = HistPos->Older;
			}

			if (HistPos) {
				strcpy (&buffer[2], HistPos->String);
				buffer[0] = buffer[1] = strlen (&buffer[2]);
				buffer[len+4] = 0;
				makestartposgood();
			}

			TabbedLast = false;
			break;
		case KEY_DOWNARROW:
			// Move to next entry in the command history

			if (HistPos && HistPos->Newer) {
				HistPos = HistPos->Newer;
			
				strcpy (&buffer[2], HistPos->String);
				buffer[0] = buffer[1] = strlen (&buffer[2]);
			} else {
				HistPos = NULL;
				buffer[0] = buffer[1] = 0;
			}
			buffer[len+4] = 0;
			makestartposgood();
			TabbedLast = false;
			break;
		default:
			if (ev->data2 == '\r') {
				// Execute command line (ENTER)

				buffer[2 + buffer[0]] = 0;

				if (HistHead && stricmp (HistHead->String, &buffer[2]) == 0) {
					// Command line was the same as the previous one,
					// so leave the history list alone
				} else {
					// Command line is different from last command line,
					// or there is nothing in the history list,
					// so add it to the history list.

					struct History *temp = Malloc (sizeof(struct History) + buffer[0]);

					strcpy (temp->String, &buffer[2]);
					temp->Older = HistHead;
					if (HistHead) {
						HistHead->Newer = temp;
					}
					temp->Newer = NULL;
					HistHead = temp;

					if (!HistTail) {
						HistTail = temp;
					}

					if (HistSize == MAXHISTSIZE) {
						HistTail = HistTail->Newer;
						free (HistTail->Older);
						HistTail->Older = NULL;
					} else {
						HistSize++;
					}
				}
				HistPos = NULL;
				Printf ("]%s\n", &buffer[2]);
				buffer[0] = buffer[1] = buffer[len+4] = 0;
				AddCommandString (&buffer[2]);
				TabbedLast = false;
			} else if (ev->data2 == '`' || ev->data1 == KEY_ESCAPE) {
				// Close console, both ` and ESC clear command line

				buffer[0] = buffer[1] = buffer[len+4] = 0;
				HistPos = NULL;
				C_ToggleConsole ();
			} else if (ev->data3 < 32 || ev->data3 > 126) {
				// Do nothing
			} else {
				// Add keypress to command line

				if (buffer[0] < len) {
					char data = ev->data3;

					if (buffer[1] == buffer[0]) {
						buffer[buffer[0] + 2] = data;
					} else {
						char *c, *e;

						e = &buffer[buffer[0] + 1];
						c = &buffer[buffer[1] + 2];

						for (; e >= c; e--)
							*(e + 1) = *e;

						*c = data;
					}
					buffer[0]++;
					buffer[1]++;
					makestartposgood ();
					HistPos = NULL;
				}
				TabbedLast = false;
			}
			break;
	}
}

BOOL C_Responder (event_t *ev)
{
	if (ConsoleState == up || ConsoleState == rising) {
		return false;
	}

	if (ev->type == ev_keyup) {
		if (ev->data1 == RepeatEvent.data1)
			RepeatCountdown = 0;

		switch (ev->data1) {
			case KEY_PGUP:
			case KEY_PGDN:
				ScrollState = SCROLLNO;
				break;
			case KEY_RSHIFT:
				KeysShifted = false;
				break;
			default:
				return false;
		}
	} else if (ev->type == ev_keydown) {
		// Some keys don't repeat
		if (ev->data1 == KEY_PGUP ||
			ev->data1 == KEY_PGDN ||
			ev->data1 == KEY_RSHIFT ||
			ev->data1 == KEY_TAB ||
			ev->data1 == KEY_ENTER ||
			ev->data1 == KEY_ESCAPE ||
			ev->data2 == '`')
			RepeatCountdown = 0;
		else
		// Others do.
			RepeatCountdown = KeyRepeatDelay;
		RepeatEvent = *ev;
		C_HandleKey (ev, CmdLine, 255);

	} else
		return false;

	return true;
}

void Cmd_History (player_t *plyr, int argc, char **argv)
{
	struct History *hist = HistTail;

	while (hist) {
		Printf ("   %s\n", hist->String);
		hist = hist->Newer;
	}
}

void Cmd_Clear (player_t *plyr, int argc, char **argv)
{
	int i;
	char *row = Lines;

	RowAdjust = ShowRows = 0;
	for (i = 0; i < ConRows; i++, row += ConCols + 2)
		row[1] = 0;
}

void Cmd_Echo (player_t *plyr, int argc, char **argv)
{
	if (argc > 1)
		Printf ("%s\n", BuildString (argc - 1, argv + 1));
}

/* Printing in the middle of the screen */

static brokenlines_t *MidMsg = NULL;
static int MidTicker = 0, MidLines;

void C_MidPrint (char *msg)
{
	int i;

	if (MidMsg)
		V_FreeBrokenLines (MidMsg);

	if ( (MidMsg = V_BreakLines (320, msg)) ) {
		MidTicker = (int)(NotifyTime->value * TICRATE) + gametic;

		for (i = 0; MidMsg[i].width != -1; i++)
			;

		MidLines = i;
	}
}

extern byte *WhiteMap, *Ranges;

void C_DrawMid (void)
{
	if (MidMsg) {
		int i, line, x, y;
		byte *holdwhite = WhiteMap;

		// Hack! Hack! I need to get mult-colored text output written...
		WhiteMap = Ranges + 5 * 256;
		y = 8 * CleanYfac;
		x = screens[0].width >> 1;
		for (i = 0, line = (ST_Y >> 1) - MidLines * 4 * CleanYfac; i < MidLines; i++, line += y) {
			V_DrawTextClean (x - (MidMsg[i].width >> 1) * CleanXfac, line, MidMsg[i].string);
		}

		if (gametic >= MidTicker) {
			V_FreeBrokenLines (MidMsg);
			MidMsg = NULL;
		}

		WhiteMap = holdwhite;
	}
}

void C_EraseLines (int top, int bottom)
{
	if (top < viewwindowy) {
		// Erase top border
		R_VideoErase (0, top, screens[0].width,
					  (bottom <= viewwindowy) ? bottom : viewwindowy);
	}
	if (bottom > viewwindowy) {
		if (top < viewwindowy)
			top = viewwindowy;
		R_VideoErase (0, top, viewwindowx, bottom);				// Erase left border
		R_VideoErase (viewwindowx+realviewwidth, top, screens[0].width, bottom);	// Erase right border
	}
}


/****** Tab completion code ******/

static struct TabData {
	int UseCount;
	char *Name;
} *TabCommands = NULL;
static int NumTabCommands = 0;
static int TabPos;				// Last TabCommand tabbed to
static int TabStart;			// First char in CmdLine to use for tab completion
static int TabSize;				// Size of tab string

static BOOL FindTabCommand (char *name, int *stoppos, int len)
{
	int i, cval = 1;

	for (i = 0; i < NumTabCommands; i++) {
		cval = strnicmp (TabCommands[i].Name, name, len);
		if (cval >= 0)
			break;
	}

	*stoppos = i;

	return (cval == 0);
}

void C_AddTabCommand (char *name)
{
	int pos;

	if (FindTabCommand (name, &pos, MAXINT)) {
		TabCommands[pos].UseCount++;
	} else {
		NumTabCommands++;
		TabCommands = Realloc (TabCommands, sizeof(struct TabData) * NumTabCommands);
		if (pos < NumTabCommands - 1)
			memmove (TabCommands + pos + 1, TabCommands + pos,
					 (NumTabCommands - pos - 1) * sizeof(struct TabData));
		TabCommands[pos].Name = copystring (name);
		TabCommands[pos].UseCount = 1;
	}
}

void C_RemoveTabCommand (char *name)
{
	int pos;

	if (FindTabCommand (name, &pos, MAXINT)) {
		if (--TabCommands[pos].UseCount == 0) {
			NumTabCommands--;
			free (TabCommands[pos].Name);
			if (pos < NumTabCommands)
				memmove (TabCommands + pos, TabCommands + pos + 1,
						 (NumTabCommands - pos) * sizeof(struct TabData));
		}
	}
}

static int FindDiffPoint (const char *str1, const char *str2)
{
	int i;

	for (i = 0; tolower(str1[i]) == tolower(str2[i]); i++)
		if (str1[i] == 0 || str2[i] == 0)
			break;

	return i;
}

static void C_TabComplete (void)
{
	int i;
	int diffpoint;

	if (!TabbedLast) {
		// Skip any spaces at beginning of command line
		if (CmdLine[2] == ' ') {
			for (i = 0; i < CmdLine[0]; i++)
				if (CmdLine[2+i] != ' ')
					break;

			TabStart = i + 2;
		} else {
			TabStart = 2;
		}

		if (TabStart == CmdLine[0] + 2)
			return;		// Line was nothing but spaces

		TabSize = CmdLine[0] - TabStart + 2;

		if (!FindTabCommand (CmdLine + TabStart, &TabPos, TabSize))
			return;		// No initial matches

		TabPos--;
		TabbedLast = true;
	}

	if (++TabPos == NumTabCommands) {
		TabbedLast = false;
		CmdLine[0] = CmdLine[1] = TabSize;
	} else {
		diffpoint = FindDiffPoint (TabCommands[TabPos].Name, CmdLine + TabStart);

		if (diffpoint < TabSize) {
			// No more matches
			TabbedLast = false;
			CmdLine[0] = CmdLine[1] = TabSize + TabStart - 2;
		} else {		
			strcpy (CmdLine + TabStart, TabCommands[TabPos].Name);
			CmdLine[0] = CmdLine[1] = strlen (CmdLine + 2) + 1;
			CmdLine[CmdLine[0] + 1] = ' ';
		}
	}

	makestartposgood ();
}