#include "m_alloc.h"
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "dstrings.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "r_main.h"
#include "r_draw.h"
#include "st_stuff.h"

#include "doomstat.h"

extern int		gametic;
extern boolean	automapactive;	// in AM_map.c

typedef enum cstate_t {
	up=0, down, falling, rising
} constate;


int			ConRows, ConCols, PhysRows;
char		*Lines, *Last = NULL;
boolean		vidactive = false, gotconback = false;
boolean		cursoron = false;
int			ShowRows, SkipRows, ConsoleTicker, ConBottom, ConScroll, RowAdjust;
int			CursorTicker, ScrollState = 0;
byte		conback[320*200];
constate	ConsoleState = up;
char		VersionString[8];

#define SCROLLUP 1
#define SCROLLDN 2
#define SCROLLNO 0

extern cvar_t *showMessages;
extern boolean message_dontfuckwithme;		// Who came up with this name?

struct History {
	struct History *Older;
	struct History *Newer;
	char String[1];
};

// CmdLine[0]  = # of chars on command line
// CmdLine[1]  = cursor position
// CmdLine[2+] = command line (max 256 chars + NULL)
// CmdLine[259]= offset from beginning of cmdline to display
static byte CmdLine[260];

static byte printxormask;

#define MAXHISTSIZE 50
static struct History *HistHead = NULL, *HistTail = NULL, *HistPos = NULL;
static int HistSize;

cvar_t *NotifyTime;
static byte NotifyStrings[4][256];

static int lasttic;

static char ShiftLOT[] = {
	'\"', '(', ')', '*', '+', '<', '_', '>', '?',							//  9 39->
	')', '!', '@', '#', '$', '%', '^', '&', '*', '(', ':', ':', '<', '+',	// 14 ->61

	'{', '|', '}', '^', '_', '~', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',	// 14 91->
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',	// 14 ...
	'W', 'X', 'Y', 'Z'														//  4 ->122
};

FILE *Logfile = NULL;

extern M_StringWidth (byte *);	// from m_menu.c
extern patch_t *hu_font[HU_FONTSIZE];	// from hu_stuff.c

static int C_hu_CharWidth (int c)
{
	c = toupper((c++) & 0x7f) - HU_FONTSTART;
	if (c < 0 || c >= HU_FONTSIZE)
		return 4;

	return hu_font[c]->width;
}

void C_InitConsole (int width, int height, boolean ingame)
{
	int row;
	char *zap;
	char *old;
	int cols, rows;

	if (vidactive = ingame) {
		if (!gotconback) {
			boolean stylize = false;
			patch_t *patch;
			int num;

			num = W_CheckNumForName ("CONBACK");
			if (num == -1) {
				num = W_CheckNumForName ("TITLEPIC");
				stylize = true;
			}

			patch = W_CacheLumpNum (num, PU_CACHE);

			V_DrawPatch (0, 0, 0, patch);

			if (stylize) {
				byte *fadetable = W_CacheLumpName ("COLORMAP", PU_CACHE), f, *v, *i;
				int x, y;

				for (y = 0; y < 200; y++) {
					i = screens[0] + SCREENPITCH * y;
					if (y < 8 || y > 191) {
						if (y < 8)
							f = y;
						else
							f = 199 - y;
						v = fadetable + (30 - f) * 256;
						for (x = 0; x < 320; x++) {
							*i = v[*i];
							i++;
						}
					} else {
						for (x = 0; x < 320; x++) {
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

			V_GetBlock (0, 0, 0, 320, 200, conback);

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

			Printf (string);
		}
		free (old);
		ShowRows = 0;

		Logfile = templog;
	}
}

void C_AddNotifyString (char *s)
{
	int splitpos, width;
	char *nl;

	if (!(nl = strchr (s, '\n')))
		// Only add strings with newlines
		return;

	// Split the string up into sub-strings, none of which are
	// wider than the screen when printed with the hu_font,
	// then store them in the NotifyStrings[] array.

	while (*s) {
		if (s == nl) {
			nl = strchr (s, '\n');
		}
		*nl = 0;
		splitpos = strlen (s) - 1;
		if ((width = M_StringWidth (s)) > CleanWidth) {
			while ((width -= C_hu_CharWidth (s[splitpos])) > CleanWidth) {
				splitpos--;
			}
		}
		splitpos++;
		memmove (NotifyStrings[0], NotifyStrings[1], 256 * 3);
		strncpy (NotifyStrings[3], s, splitpos);
		NotifyStrings[3][splitpos] = 0;
		s += splitpos;
		ShowRows++;
		if (s == nl) {
			*nl = '\n';
			s++;
		}
	}

	if (ShowRows > 4)
		ShowRows = 4;

	if (showMessages->value == 0.0)
		ShowRows = message_dontfuckwithme;

	ConsoleTicker = (int)(NotifyTime->value * TICRATE) + gametic;
}

/* Provide our own Printf() that is sensitive of the
 * console status (in or out of game)
 */
int VPrintf (const char *outline) {
	const char *cp, *newcp;
	static xp = 0;
	int newxp;
	boolean scroll;

	if (Logfile) {
		fputs (outline, Logfile);
#ifdef _DEBUG
		fflush (Logfile);
#endif
	}

#ifdef _MSC_VER
// C_AddNotifyString() is essentially const, since it
// leaves outline in the same state it was in when we
// passed it to the function
#pragma warning(disable: 4090)
#pragma warning(disable: 4024)
#endif
	if (vidactive)
		C_AddNotifyString (outline);
#ifdef _MSC_VER
#pragma warning(default: 4090)
#pragma warning(default: 4024)
#endif

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
				Last[x + 2] = (*poop) ^ printxormask;
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

int Printf (const char *format, ...)
{
	va_list argptr;
	char outline[512];

	va_start (argptr, format);
	vsprintf (outline, format, argptr);
	va_end (argptr);

	return VPrintf (outline);
}

int Printf_Bold (const char *format, ...)
{
	va_list argptr;
	char outline[512];

	va_start (argptr, format);
	vsprintf (outline, format, argptr);
	va_end (argptr);

	printxormask = 0x80;
	return VPrintf (outline);
}

void C_FlushDisplay (void)
{
	ShowRows = 0;
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
		}

		if (ConsoleState == falling) {
			ConBottom += (gametic - lasttic) * (SCREENHEIGHT / 100);
			if (ConBottom >= SCREENHEIGHT / 16) {
				ConBottom = SCREENHEIGHT / 16;
				ConsoleState = down;
			}
		} else if (ConsoleState == rising) {
			ConBottom -= (gametic - lasttic) * (SCREENHEIGHT / 100);
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

static void C_DrawNotifyText (char *zap)
{
	int lines, fact;
	
	if (ShowRows == 0 || gamestate != GS_LEVEL)
		return;

	fact = 8 * CleanYfac;

	if (viewactive && (viewwindowy || viewwindowx)) {
		C_EraseLines (0, (ShowRows + (ShowRows < 4)) * fact);
	}

	for (lines = 0; lines < ShowRows; lines++) {
		V_DrawTextClean (0, (ShowRows - lines - 1) * fact, NotifyStrings[3 - lines]);
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

	if ((ConBottom < oldbottom) && (gamestate == GS_LEVEL) && (viewwindowx || viewwindowy)) {
		C_EraseLines (ConBottom * 8, oldbottom * 8);
	}

	oldbottom = ConBottom;

	if (ConsoleState == up) {
		C_DrawNotifyText (zap);
		return;
	} else {
		unsigned xinc, yinc, x, y, ybot, xmax;
		byte *drawspot, *fromspot;

		xinc = (320 * 0x10000) / SCREENWIDTH;
		yinc = (200 * 0x10000) / SCREENHEIGHT;
		ybot = ConBottom * yinc * 8;
		drawspot = screens[0];
		xmax = SCREENWIDTH * xinc;

		if (xinc == 0x10000) {
			for (y = 0; y < ybot; y += yinc) {
				memcpy (drawspot, &conback[320 * (199 - ((ybot - y) >> 16))], 320);
				drawspot += SCREENPITCH;
			}
		} else {
			drawspot = screens[0];
			for (y = 0; y < ybot; y += yinc) {
				fromspot = &conback[320 * (199 - ((ybot - y) >> 16))];
				for (x = 0; x < xmax; x += xinc) {
					*drawspot++ = fromspot[x >> 16];
				}
			}
		}
		if (ConBottom >= 2)
			V_PrintStr (SCREENWIDTH - 8 - strlen(VersionString) * 8, ConBottom * 8 - 16, VersionString, strlen(VersionString), 0x80);
	}

	if (lines > 0) {
		for (; lines; lines--) {
			V_PrintStr (left, -8 + lines * 8, &zap[2], zap[1], 0);
			zap -= ConCols + 2;
		}
		if (ConBottom > 1) {
			V_PrintStr (left, (ConBottom - 2) * 8, "]", 1, 0x80);
#define MIN(a,b) (((a)<(b))?(a):(b))
			V_PrintStr (left + 8, (ConBottom - 2) * 8, &CmdLine[2+CmdLine[259]], MIN(CmdLine[0] - CmdLine[259], ConCols - 1), 0);
#undef MIN
			if (cursoron) {
				V_PrintStr (left + 8 + (CmdLine[1] - CmdLine[259])* 8, (ConBottom - 2) * 8, "\xb", 1, 0);
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
	if (ConsoleState == up || ConsoleState == rising) {
		ConsoleState = falling;
		HistPos = NULL;
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

boolean C_Responder (event_t *ev)
{
	static boolean shift = false;

	if (ConsoleState == up || ConsoleState == rising) {
		return false;
	}

	if (ev->type == ev_keyup) {
		switch (ev->data1) {
			case KEY_PGUP:
			case KEY_PGDN:
				ScrollState = SCROLLNO;
				break;
			case KEY_RSHIFT:
				shift = false;
				break;
			default:
				return false;
		}
		return true;
	}

	if (ev->type != ev_keydown)
		return false;

	switch (ev->data1) {
		case KEY_PGUP:
			if (shift)
				// Move to top of console buffer
				RowAdjust = ConRows - SkipRows - ConBottom;
			else
				// Start scrolling console buffer up
				ScrollState = SCROLLUP;
			break;
		case KEY_PGDN:
			if (shift)
				// Move to bottom of console buffer
				RowAdjust = 0;
			else
				// Start scrolling console buffer down
				ScrollState = SCROLLDN;
			break;
		case KEY_HOME:
			// Move cursor to start of line

			CmdLine[1] = CmdLine[259] = 0;
			break;
		case KEY_END:
			// Move cursor to end of line

			CmdLine[1] = CmdLine[0];
			makestartposgood ();
			break;
		case KEY_LEFTARROW:
			// Move cursor left one character

			if (CmdLine[1]) {
				CmdLine[1]--;
				makestartposgood ();
			}
			break;
		case KEY_RIGHTARROW:
			// Move cursor right one character

			if (CmdLine[1] < CmdLine[0]) {
				CmdLine[1]++;
				makestartposgood ();
			}
			break;
		case KEY_BACKSPACE:
			// Erase character to left of cursor

			if (CmdLine[0] && CmdLine[1]) {
				char *c, *e;

				e = &CmdLine[CmdLine[0] + 2];
				c = &CmdLine[CmdLine[1] + 2];

				for (; c < e; c++)
					*(c - 1) = *c;
				
				CmdLine[0]--;
				CmdLine[1]--;
				if (CmdLine[259])
					CmdLine[259]--;
				makestartposgood ();
			}
			break;
		case KEY_DEL:
			// Erase charater under cursor

			if (CmdLine[1] < CmdLine[0]) {
				char *c, *e;

				e = &CmdLine[CmdLine[0] + 2];
				c = &CmdLine[CmdLine[1] + 3];

				for (; c < e; c++)
					*(c - 1) = *c;

				CmdLine[0]--;
				makestartposgood ();
			}
			break;
		case KEY_RALT:
		case KEY_RCTRL:
			// Do nothing
			break;
		case KEY_RSHIFT:
			// SHIFT was pressed
			shift = true;
			break;
		case KEY_UPARROW:
			// Move to previous entry in the command history

			if (HistPos == NULL) {
				HistPos = HistHead;
			} else if (HistPos->Older) {
				HistPos = HistPos->Older;
			}

			if (HistPos) {
				strcpy (&CmdLine[2], HistPos->String);
				CmdLine[0] = CmdLine[1] = strlen (&CmdLine[2]);
				CmdLine[259] = 0;
				makestartposgood();
			}
			break;
		case KEY_DOWNARROW:
			// Move to next entry in the command history

			if (HistPos && HistPos->Newer) {
				HistPos = HistPos->Newer;
			
				strcpy (&CmdLine[2], HistPos->String);
				CmdLine[0] = CmdLine[1] = strlen (&CmdLine[2]);
			} else {
				HistPos = NULL;
				CmdLine[0] = CmdLine[1] = 0;
			}
			CmdLine[259] = 0;
			makestartposgood();
			break;
		default:
			if (ev->data2 == 13) {
				// Execute command line (ENTER)

				CmdLine[2 + CmdLine[0]] = 0;

				if (HistHead && stricmp (HistHead->String, &CmdLine[2]) == 0) {
					// Command line was the same as the previous one,
					// so leave the history list alone
				} else {
					// Command line is different from last command line,
					// or there is nothing in the history list,
					// so add it to the history list.

					struct History *temp = Malloc (sizeof(struct History) + CmdLine[0]);

					strcpy (temp->String, &CmdLine[2]);
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
				Printf ("]%s\n", &CmdLine[2]);
				CmdLine[0] = CmdLine[1] = CmdLine[259] = 0;
				AddCommandString (&CmdLine[2]);
			} else if (ev->data2 == '`' || ev->data1 == KEY_ESCAPE) {
				// Close console, both ` and ESC clear command line

				CmdLine[0] = CmdLine[1] = CmdLine[259] = 0;
				HistPos = NULL;
				C_ToggleConsole ();
			} else if (ev->data2 < 32 || ev->data2 > 126) {
				// Do nothing
			} else {
				// Add keypress to command line

				if (CmdLine[0] < 255) {
					char data = ev->data2;

					if (shift) {
						if (data >= 39 && data <= 61)
							data = ShiftLOT[data-39];
						else if (data >= 91 && data <= 122)
							data = ShiftLOT[data-68];
					}

					if (CmdLine[1] == CmdLine[0]) {
						CmdLine[CmdLine[0] + 2] = data;
					} else {
						char *c, *e;

						e = &CmdLine[CmdLine[0] + 1];
						c = &CmdLine[CmdLine[1] + 2];

						for (; e >= c; e--)
							*(e + 1) = *e;

						*c = data;
					}
					CmdLine[0]++;
					CmdLine[1]++;
					makestartposgood ();
					HistPos = NULL;
				}
			}
			break;
	}
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

static char *MidMsg = NULL;
static int MidTicker = 0, MidLines;

void C_MidPrint (char *msg)
{
	char *looker;

	if (MidMsg)
		free (MidMsg);

	if (msg && (MidMsg = malloc (strlen (msg) + 1))) {
		strcpy (MidMsg, msg);
		MidTicker = (int)(NotifyTime->value * TICRATE) + gametic;

		MidLines = 1;
		looker = MidMsg;

		while (*looker) {
			if (*looker == '\n')
				MidLines++;
			looker++;
		}
	}
}

void C_DrawMid (void)
{
	int line, len;
	char *looker, *start;

	if (MidMsg) {
		if (MidLines == 1) {
			len = strlen (MidMsg);
			if (SCREENWIDTH < 640)
				V_PrintStr ((SCREENWIDTH / 2) - (len * 4), (ST_Y/2) - 4, MidMsg, len, 0);
			else
				V_PrintStr2 ((SCREENWIDTH / 2) - (len * 8), (ST_Y/2) - 8, MidMsg, len, 0);
		} else {
			start = MidMsg;
			for (line = 0; line < MidLines; line++) {
				looker = start;
				while (*looker != 0 && *looker != '\n') {
					looker++;
				}
				len = looker - start;
				if (SCREENWIDTH < 640)
					V_PrintStr ((SCREENWIDTH / 2) - len * 4, (ST_Y/2) - MidLines * 4 + line * 8,
								start, len, 0);
				else
					V_PrintStr2 ((SCREENWIDTH / 2) - len * 8, (ST_Y/2) - MidLines * 8 + line * 16,
								start, len, 0);
				start = looker + 1;
			}
		}

		if (gametic >= MidTicker) {
			free (MidMsg);
			MidMsg = NULL;
		}
	}
}

void C_EraseLines (int top, int bottom)
{
	int ofs, y;

	bottom *= SCREENPITCH;
	ofs = top * SCREENPITCH;

	for (y = top; ofs < bottom; ofs += SCREENPITCH, y++) {
		if (y < viewwindowy || y >= viewwindowy + viewheight) {
			R_VideoErase (ofs, SCREENWIDTH); // erase entire line
		} else {
			R_VideoErase (ofs, viewwindowx); // erase left border
			R_VideoErase (ofs + viewwindowx + viewwidth, viewwindowx); // erase right border
		}
	}
}
