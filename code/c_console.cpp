#include "m_alloc.h"
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "version.h"
#include "g_game.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "i_video.h"
#include "i_input.h"
#include "m_swap.h"
#include "v_palette.h"
#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "z_zone.h"
#include "r_main.h"
#include "r_draw.h"
#include "sbar.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "d_gui.h"

#include "gi.h"

#define CONSOLESIZE	16384	// Number of characters to store in console
#define CONSOLELINES 256	// Max number of lines of console text
#define LINEMASK (CONSOLELINES-1)

#define LEFTMARGIN 8
#define RIGHTMARGIN 8
#define BOTTOMARGIN 12

static void C_TabComplete ();
static BOOL TabbedLast;		// Last key pressed was tab


static DCanvas *conback;

extern int		gametic;
extern bool		automapactive;	// in AM_map.c
extern BOOL		advancedemo;

int			ConCols, PhysRows;
BOOL		vidactive = false, gotconback = false;
BOOL		cursoron = false;
int			ConBottom, ConScroll, RowAdjust;
int			CursorTicker;
constate_e	ConsoleState = c_up;
char		VersionString[16];

static char ConsoleBuffer[CONSOLESIZE];
static char *Lines[CONSOLELINES];
static bool LineJoins[CONSOLELINES];

static int TopLine, InsertLine;
static char *BufferRover = ConsoleBuffer;

static void AddToConsole (int printlevel, const char *text);
static void ClearConsole ();


#define SCROLLUP 1
#define SCROLLDN 2
#define SCROLLNO 0

EXTERN_CVAR (Bool, show_messages)

static unsigned int TickerAt, TickerMax;
static const char *TickerLabel;

struct History
{
	struct History *Older;
	struct History *Newer;
	char String[1];
};

// CmdLine[0]  = # of chars on command line
// CmdLine[1]  = cursor position
// CmdLine[2+] = command line (max 255 chars + NULL)
// CmdLine[259]= offset from beginning of cmdline to display
static byte CmdLine[260];

#define MAXHISTSIZE 50
static struct History *HistHead = NULL, *HistTail = NULL, *HistPos = NULL;
static int HistSize;

#define NUMNOTIFIES 4

CVAR (Float, con_notifytime, 3.f, CVAR_ARCHIVE)
CVAR (Bool, con_centernotify, false, CVAR_ARCHIVE)
CVAR (Bool, con_scaletext, false, CVAR_ARCHIVE)		// Scale notify text at high resolutions?

static struct NotifyText
{
	int timeout;
	int printlevel;
	byte text[256];
} NotifyStrings[NUMNOTIFIES];

#define PRINTLEVELS 5
int PrintColors[PRINTLEVELS+1] = { CR_RED, CR_GOLD, CR_GRAY, CR_GREEN, CR_GREEN, CR_GOLD };

static void setmsgcolor (int index, int color);

FILE *Logfile = NULL;


FIntCVar msglevel ("msg", 0, CVAR_ARCHIVE);

CUSTOM_CVAR (Int, msg0color, 6, CVAR_ARCHIVE)
{
	setmsgcolor (0, *var);
}

CUSTOM_CVAR (Int, msg1color, 5, CVAR_ARCHIVE)
{
	setmsgcolor (1, *var);
}

CUSTOM_CVAR (Int, msg2color, 2, CVAR_ARCHIVE)
{
	setmsgcolor (2, *var);
}

CUSTOM_CVAR (Int, msg3color, 3, CVAR_ARCHIVE)
{
	setmsgcolor (3, *var);
}

CUSTOM_CVAR (Int, msg4color, 3, CVAR_ARCHIVE)
{
	setmsgcolor (4, *var);
}

CUSTOM_CVAR (Int, msgmidcolor, 5, CVAR_ARCHIVE)
{
	setmsgcolor (PRINTLEVELS, *var);
}

static void maybedrawnow ()
{
	if (vidactive &&
		((gameaction != ga_nothing && ConsoleState == c_down)
		|| gamestate == GS_STARTUP))
	{
		static size_t lastprinttime = 0;
		size_t nowtime = I_GetTime();

		if (nowtime - lastprinttime > 1)
		{
			screen->Lock (false);
			C_DrawConsole ();
			screen->Update ();
			lastprinttime = nowtime;
		}
	}
}

void C_InitConsole (int width, int height, BOOL ingame)
{
	if ( (vidactive = ingame) )
	{
		if (!gotconback)
		{
			BOOL stylize = false;
			BOOL isRaw = false;
			patch_t *bg;
			int num;

			num = W_CheckNumForName ("CONBACK");
			if (num == -1)
			{
				stylize = true;
				num = W_GetNumForName (gameinfo.titlePage);
				isRaw = gameinfo.flags & GI_PAGESARERAW;
			}

			bg = (patch_t *)W_CacheLumpNum (num, PU_CACHE);

			if (isRaw)
				conback = I_NewStaticCanvas (320, 200);
			else
				conback = I_NewStaticCanvas (SHORT(bg->width), SHORT(bg->height));

			conback->Lock ();

			if (isRaw)
				conback->DrawBlock (0, 0, 320, 200, (byte *)bg);
			else
				conback->DrawPatch (bg, 0, 0);

			if (stylize)
			{
				byte *fadetable = (byte *)W_CacheLumpName ("COLORMAP", PU_CACHE), f, *v, *i;
				int x, y;

				for (y = 0; y < conback->GetHeight(); y++)
				{
					i = conback->GetBuffer() + conback->GetPitch() * y;
					if (y < 8 || y > 191)
					{
						if (y < 8)
							f = y;
						else
							f = 199 - y;
						v = fadetable + (30 - f) * 256;
						for (x = 0; x < conback->GetWidth(); x++)
						{
							*i = v[*i];
							i++;
						}
					}
					else
					{
						for (x = 0; x < conback->GetWidth(); x++)
						{
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
			conback->Unlock ();
			sprintf (VersionString, "v" DOTVERSIONSTR);
			gotconback = true;
		}
	}

	ConCols = (width - LEFTMARGIN - RIGHTMARGIN) / 8;
	PhysRows = height / 8;

	// If there is some text in the console buffer, reformat it
	// for the new resolution.
	if (TopLine != InsertLine)
	{
		// Note: Don't use new here, because we attach a handler to new in
		// i_main.cpp that calls I_FatalError if the allocation fails,
		// but we can gracefully handle such a condition here by just
		// clearing the console buffer.

		char *fmtBuff = (char *)malloc (CONSOLESIZE);
		char **fmtLines = (char **)malloc (CONSOLELINES*sizeof(char*)*4);
		int out;

		if (fmtBuff && fmtLines)
		{
			int in;
			char *fmtpos;
			bool newline = true;

			fmtpos = fmtBuff;
			memset (fmtBuff, 0, CONSOLESIZE);

			for (in = TopLine, out = 0; in != InsertLine; in = (in + 1) & LINEMASK)
			{
				int len = strlen (Lines[in]);

				if (fmtpos + len + 2 - fmtBuff > CONSOLESIZE)
				{
					break;
				}
				if (newline)
				{
					newline = false;
					fmtLines[out++] = fmtpos;
				}
				strcpy (fmtpos, Lines[in]);
				fmtpos += len;
				if (!LineJoins[in])
				{
					*fmtpos++ = '\n';
					fmtpos++;
					if (out == CONSOLELINES*4)
					{
						break;
					}
					newline = true;
				}
			}
		}

		ClearConsole ();

		if (fmtBuff && fmtLines)
		{
			int i;

			for (i = 0; i < out; i++)
			{
				AddToConsole (-1, fmtLines[i]);
			}
		}

		if (fmtBuff)
			free (fmtBuff);
		if (fmtLines)
			free (fmtLines);
	}

	if (ingame && gamestate == GS_STARTUP)
	{
		C_FullConsole ();
	}
}

static void ClearConsole ()
{
	RowAdjust = 0;
	TopLine = InsertLine = 0;
	BufferRover = ConsoleBuffer;
	memset (ConsoleBuffer, 0, CONSOLESIZE);
	memset (Lines, 0, sizeof(Lines));
	memset (LineJoins, 0, sizeof(LineJoins));
}

static void setmsgcolor (int index, int color)
{
	if ((unsigned)color >= (unsigned)NUM_TEXT_COLORS)
		color = 0;
	PrintColors[index] = color;
}

extern int DisplayWidth;

void C_AddNotifyString (int printlevel, const char *source)
{
	static enum
	{
		NEWLINE,
		APPENDLINE,
		REPLACELINE
	} addtype = NEWLINE;

	char *work;
	brokenlines_t *lines;
	int i, len, width;

	if ((printlevel != 128 && !*show_messages) ||
		!(len = strlen (source)) ||
		gamestate != GS_LEVEL)
		return;

	width = *con_scaletext ? DisplayWidth / CleanXfac : DisplayWidth;

	if (addtype == APPENDLINE && NotifyStrings[NUMNOTIFIES-1].printlevel == printlevel
		&& (work = (char *)malloc (strlen ((char *)NotifyStrings[NUMNOTIFIES-1].text)
									+ strlen (source) + 1)) )
	{
		sprintf (work, "%s%s", NotifyStrings[NUMNOTIFIES-1].text, source);
		lines = V_BreakLines (width, work);
		free (work);
	}
	else
	{
		lines = V_BreakLines (width, source);
		addtype = (addtype == APPENDLINE) ? NEWLINE : addtype;
	}

	if (lines == NULL)
		return;

	for (i = 0; lines[i].width != -1; i++)
	{
		if (addtype == NEWLINE)
			memmove (&NotifyStrings[0], &NotifyStrings[1], sizeof(struct NotifyText) * (NUMNOTIFIES-1));
		strcpy ((char *)NotifyStrings[NUMNOTIFIES-1].text, lines[i].string);
		NotifyStrings[NUMNOTIFIES-1].timeout = gametic + (int)(*con_notifytime * TICRATE);
		NotifyStrings[NUMNOTIFIES-1].printlevel = printlevel;
		addtype = NEWLINE;
	}

	V_FreeBrokenLines (lines);

	switch (source[len-1])
	{
	case '\r':	addtype = REPLACELINE;	break;
	case '\n':	addtype = NEWLINE;		break;
	default:	addtype = APPENDLINE;	break;
	}
}

static int FlushLines (const char *start, const char *stop)
{
	int i;

	for (i = TopLine; i != InsertLine; i = (i + 1) & LINEMASK)
	{
		if (Lines[i] < stop && Lines[i] + strlen (Lines[i]) > start)
		{
			Lines[i] = NULL;
		}
		else
		{
			break;
		}
	}
	if (i != TopLine)
		i = i;
	return i;
}

static void AddLine (const char *text, bool more, int len)
{
	if (BufferRover + len + 1 - ConsoleBuffer > CONSOLESIZE)
	{
		TopLine = FlushLines (BufferRover, ConsoleBuffer + CONSOLESIZE);
		BufferRover = ConsoleBuffer;
	}
	TopLine = FlushLines (BufferRover, BufferRover + len + 1);
	memcpy (BufferRover, text, len);
	BufferRover[len] = 0;
	Lines[InsertLine] = BufferRover;
	BufferRover += len + 1;
	LineJoins[InsertLine] = more;
	InsertLine = (InsertLine + 1) & LINEMASK;
	if (InsertLine == TopLine)
	{
		TopLine = (TopLine + 1) & LINEMASK;
	}
}

static void AddToConsole (int printlevel, const char *text)
{
	static enum
	{
		NEWLINE,
		APPENDLINE,
		REPLACELINE
	} addtype = NEWLINE;
	static char *work = NULL;
	static int worklen = 0;

	char *work_p;
	char *linestart;
	char cc;
	int size, len;
	int x;
	int maxwidth;

	len = strlen (text);
	size = len + 3;

	if (addtype != NEWLINE)
	{
		InsertLine = (InsertLine - 1) & LINEMASK;
		if (Lines[InsertLine] == NULL)
		{
			InsertLine = (InsertLine + 1) & LINEMASK;
			addtype = NEWLINE;
		}
		else
		{
			BufferRover = Lines[InsertLine];
		}
	}
	if (addtype == APPENDLINE)
	{
		size += strlen (Lines[InsertLine]);
	}
	if (size > worklen)
	{
		work = (char *)Realloc (work, size);
		worklen = size;
	}
	if (work == NULL)
	{
		work = TEXTCOLOR_RED "*** OUT OF MEMORY ***";
		worklen = 0;
	}
	else
	{
		if (addtype == APPENDLINE)
		{
			strcpy (work, Lines[InsertLine]);
			strcat (work, text);
		}
		else if (printlevel >= 0)
		{
			work[0] = TEXTCOLOR_ESCAPE;
			work[1] = 'A' + (printlevel == PRINT_HIGH ? CR_TAN :
						printlevel == 200 ? CR_GREEN :
						printlevel < PRINTLEVELS ? PrintColors[printlevel] :
						CR_TAN);
			cc = work[1];
			strcpy (work + 2, text);
		}
		else
		{
			strcpy (work, text);
			cc = CR_TAN;
		}
	}

	work_p = linestart = work;

	if (ConFont != NULL && screen != NULL)
	{
		x = 0;
		maxwidth = screen->GetWidth() - LEFTMARGIN - RIGHTMARGIN;

		while (*work_p)
		{
			if (*work_p == TEXTCOLOR_ESCAPE)
			{
				work_p++;
				if (*work_p)
					cc = *work_p++;
				continue;
			}
			int w = ConFont->GetCharWidth (*work_p);
			if (*work_p == '\n' || x + w > maxwidth)
			{
				AddLine (linestart, *work_p != '\n', work_p - linestart);
				if (*work_p == '\n')
				{
					x = 0;
					work_p++;
				}
				else
				{
					x = w;
				}
				if (*work_p)
				{
					linestart = work_p - 2;
					linestart[0] = TEXTCOLOR_ESCAPE;
					linestart[1] = cc;
				}
				else
				{
					linestart = work_p;
				}
			}
			else
			{
				x += w;
				work_p++;
			}
		}

		if (*linestart)
		{
			AddLine (linestart, true, work_p - linestart);
		}
	}
	else
	{
		while (*work_p)
		{
			if (*work_p++ == '\n')
			{
				AddLine (linestart, false, work_p - linestart - 1);
				linestart = work_p;
			}
		}
		if (*linestart)
		{
			AddLine (linestart, true, work_p - linestart);
		}
	}

	switch (text[len-1])
	{
	case '\r':	addtype = REPLACELINE;	break;
	case '\n':	addtype = NEWLINE;		break;
	default:	addtype = APPENDLINE;	break;
	}
}

/* Adds a string to the console and also to the notify buffer */
int PrintString (int printlevel, const char *outline)
{
	if (printlevel < *msglevel || *outline == '\0')
	{
		return 0;
	}

	if (Logfile)
	{
		fputs (outline, Logfile);
//#ifdef _DEBUG
		fflush (Logfile);
//#endif
	}

	AddToConsole (printlevel, outline);
	if (vidactive && screen)
	{
		C_AddNotifyString (printlevel, outline);
		maybedrawnow ();
	}
	return strlen (outline);
}

extern BOOL gameisdead;

int VPrintf (int printlevel, const char *format, va_list parms)
{
	char outline[8192];

	if (gameisdead)
		return 0;

	vsprintf (outline, format, parms);
	return PrintString (printlevel, outline);
}

int STACK_ARGS Printf (int printlevel, const char *format, ...)
{
	va_list argptr;
	int count;

	va_start (argptr, format);
	count = VPrintf (printlevel, format, argptr);
	va_end (argptr);

	return count;
}

int STACK_ARGS Printf_Bold (const char *format, ...)
{
	va_list argptr;
	int count;

	va_start (argptr, format);
	count = VPrintf (200, format, argptr);
	va_end (argptr);

	return count;
}

int STACK_ARGS DPrintf (const char *format, ...)
{
	va_list argptr;
	int count;

	if (*developer)
	{
		va_start (argptr, format);
		count = VPrintf (PRINT_HIGH, format, argptr);
		va_end (argptr);
		return count;
	}
	else
	{
		return 0;
	}
}

void C_FlushDisplay ()
{
	int i;

	for (i = 0; i < NUMNOTIFIES; i++)
		NotifyStrings[i].timeout = 0;
}

void C_AdjustBottom ()
{
	if (gamestate == GS_FULLCONSOLE || gamestate == GS_STARTUP)
		ConBottom = SCREENHEIGHT;
	else if (ConBottom > SCREENHEIGHT / 2 || ConsoleState == c_down)
		ConBottom = SCREENHEIGHT / 2;
}

void C_NewModeAdjust ()
{
	C_InitConsole (SCREENWIDTH, SCREENHEIGHT, true);
	C_FlushDisplay ();
	C_AdjustBottom ();
}

void C_Ticker ()
{
	static int lasttic = 0;

	if (lasttic == 0)
		lasttic = gametic - 1;

	if (ConsoleState != c_up)
	{
		if (ConsoleState == c_falling)
		{
			ConBottom += (gametic - lasttic) * (SCREENHEIGHT*2/25);
			if (ConBottom >= SCREENHEIGHT / 2)
			{
				ConBottom = SCREENHEIGHT / 2;
				ConsoleState = c_down;
			}
		}
		else if (ConsoleState == c_rising)
		{
			ConBottom -= (gametic - lasttic) * (SCREENHEIGHT*2/25);
			if (ConBottom <= 0)
			{
				ConsoleState = c_up;
				ConBottom = 0;
			}
		}
	}

	if (--CursorTicker <= 0)
	{
		cursoron ^= 1;
		CursorTicker = C_BLINKRATE;
	}

	lasttic = gametic;
}

static void C_DrawNotifyText ()
{
	bool center = (*con_centernotify != 0.f);
	int i, line, color;
	
	if (gamestate != GS_LEVEL || menuactive)
		return;

	line = 0;

	BorderTopRefresh = screen->GetPageCount ();

	for (i = 0; i < NUMNOTIFIES; i++)
	{
		if (NotifyStrings[i].timeout > gametic)
		{
			if (!*show_messages && NotifyStrings[i].printlevel != 128)
				continue;

			if (NotifyStrings[i].printlevel >= PRINTLEVELS)
				color = CR_UNTRANSLATED;
			else
				color = PrintColors[NotifyStrings[i].printlevel];

			if (*con_scaletext)
			{
				if (!center)
					screen->DrawTextClean (color, 0, line, NotifyStrings[i].text);
				else
					screen->DrawTextClean (color, (SCREENWIDTH -
						screen->StringWidth (NotifyStrings[i].text)*CleanXfac)/2,
						line, NotifyStrings[i].text);
				line += 8 * CleanYfac;
			}
			else
			{
				if (!center)
					screen->DrawText (color, 0, line, NotifyStrings[i].text);
				else
					screen->DrawText (color, (SCREENWIDTH -
						screen->StringWidth (NotifyStrings[i].text))/2,
						line, NotifyStrings[i].text);
				line += 8;
			}
		}
	}
}

void C_InitTicker (const char *label, unsigned int max)
{
	TickerMax = max;
	TickerLabel = label;
	TickerAt = 0;
	maybedrawnow ();
}

void C_SetTicker (unsigned int at)
{
	TickerAt = at > TickerMax ? TickerMax : at;
	maybedrawnow ();
}

void C_DrawConsole ()
{
	static int oldbottom = 0;
	int lines, left, offset;

	left = 8;
	lines = (ConBottom-16)/8;
	if (-8 + lines*8 > ConBottom - 28)
	{
		offset = -4;
		lines--;
	}
	else
	{
		offset = -8;
	}

	if ((ConBottom < oldbottom) && (gamestate == GS_LEVEL) && (viewwindowx || viewwindowy)
		&& !automapactive)
	{
		BorderNeedRefresh = screen->GetPageCount ();
	}

	oldbottom = ConBottom;

	if (ConsoleState == c_up)
	{
		C_DrawNotifyText ();
		return;
	}
	else if (ConBottom)
	{
		int visheight, realheight;

		visheight = ConBottom;
		realheight = (visheight * conback->GetHeight()) / SCREENHEIGHT;

		conback->Blit (0, conback->GetHeight() - realheight, conback->GetWidth(), realheight,
			screen, 0, 0, SCREENWIDTH, visheight);

		if (ConBottom >= 12)
		{
			screen->SetFont (ConFont);
			screen->DrawText (CR_ORANGE, SCREENWIDTH - 8 -
				screen->StringWidth (VersionString),
				ConBottom - ConFont->GetHeight() - 4,
				VersionString);
			if (TickerMax)
			{
				char tickstr[256];
				unsigned int i, tickend = ConCols - SCREENWIDTH / 90 - 6;
				unsigned int tickbegin = 0;

				if (TickerLabel)
				{
					tickbegin = strlen (TickerLabel) + 2;
					tickend -= tickbegin;
					sprintf (tickstr, "%s: ", TickerLabel);
				}
				if (tickend > 256 - 8)
					tickend = 256 - 8;
				tickstr[tickbegin] = -128;
				memset (tickstr + tickbegin + 1, 0x81, tickend - tickbegin);
				tickstr[tickend + 1] = -126;
				tickstr[tickend + 2] = ' ';
				i = tickbegin + 1 + (TickerAt * (tickend - tickbegin - 1)) / TickerMax;
				if (i > tickend)
					i = tickend;
				tickstr[i] = -125;
				sprintf (tickstr + tickend + 3, "%u%%", (TickerAt * 100) / TickerMax);
				screen->DrawText (CR_GREEN, 8, ConBottom - ConFont->GetHeight() - 4,
					tickstr);
			}
		}
	}

	if (menuactive)
	{
		screen->SetFont (SmallFont);
		return;
	}

	if (lines > 0)
	{
		int bottomline = ConBottom - ConFont->GetHeight()*2 - 4;
		int pos = (InsertLine - 1) & LINEMASK;
		int i;

		screen->SetFont (ConFont);

		for (i = RowAdjust; i; i--)
		{
			if (pos == TopLine)
			{
				RowAdjust = RowAdjust - i;
				break;
			}
			else
			{
				pos = (pos - 1) & LINEMASK;
			}
		}
		pos++;
		do
		{
			pos = (pos - 1) & LINEMASK;
			if (Lines[pos] != NULL)
			{
				screen->DrawText (CR_TAN, 8, offset + lines * ConFont->GetHeight(),
					Lines[pos]);
			}
			lines--;
		} while (pos != TopLine && lines > 0);
		if (ConBottom >= 20)
		{
			if (gamestate == GS_STARTUP)
			{
				screen->DrawText (CR_GREEN, 8, bottomline, DoomStartupTitle);
			}
			else
			{
				CmdLine[2+CmdLine[0]] = 0;
				screen->DrawText (CR_ORANGE, left, bottomline, "\x1c");
				screen->DrawText (CR_ORANGE, left + 8, bottomline,
					(char *)&CmdLine[2+CmdLine[259]]);
			}
			if (cursoron)
			{
				screen->DrawText (CR_YELLOW, left + 8 + (CmdLine[1] - CmdLine[259])* 8,
					bottomline, "\xb");
			}
			if (RowAdjust && ConBottom >= 28)
			{
				byte *data;
				int w, h, xo, yo;

				// Indicate that the view has been scrolled up (10)
				// and if we can scroll no further (12)
				data = ConFont->GetChar (pos == TopLine ? 12 : 10, &w, &h, &xo, &yo);
				screen->DrawMaskedBlock (-xo, bottomline-yo, w, h, data,
					ConFont->GetColorTranslation (CR_GREEN));
			}
		}
	}
	screen->SetFont (SmallFont);
}

void C_FullConsole ()
{
	if (demoplayback)
		G_CheckDemoStatus ();
	advancedemo = false;
	ConsoleState = c_down;
	HistPos = NULL;
	TabbedLast = false;
	if (gamestate != GS_STARTUP)
	{
		gamestate = GS_FULLCONSOLE;
		level.music = NULL;
		S_Start ();
		SN_StopAllSequences ();
		V_SetBlend (0,0,0,0);
		I_PauseMouse ();
	}
	else
	{
		C_AdjustBottom ();
	}
}

void C_ToggleConsole ()
{
	if (gamestate == GS_DEMOSCREEN || demoplayback)
	{
		gameaction = ga_fullconsole;
	}
	else if (!chatmodeon && (ConsoleState == c_up || ConsoleState == c_rising))
	{
		ConsoleState = c_falling;
		HistPos = NULL;
		TabbedLast = false;
		I_PauseMouse ();
	}
	else if (gamestate != GS_FULLCONSOLE && gamestate != GS_STARTUP)
	{
		ConsoleState = c_rising;
		C_FlushDisplay ();
		I_ResumeMouse ();
	}
}

void C_HideConsole ()
{
	if (gamestate != GS_FULLCONSOLE &&
		gamestate != GS_STARTUP &&
		ConsoleState != c_up)
	{
		ConsoleState = c_up;
		ConBottom = 0;
		HistPos = NULL;
		if (!menuactive)
		{
			I_ResumeMouse ();
		}
	}
}

static void makestartposgood ()
{
	int n;
	int pos = CmdLine[259];
	int curs = CmdLine[1];
	int len = CmdLine[0];

	n = pos;

	if (pos >= len)
	{ // Start of visible line is beyond end of line
		n = curs - ConCols + 2;
	}
	if ((curs - pos) >= ConCols - 2)
	{ // The cursor is beyond the visible part of the line
		n = curs - ConCols + 2;
	}
	if (pos > curs)
	{ // The cursor is in front of the visible part of the line
		n = curs;
	}
	if (n < 0)
		n = 0;
	CmdLine[259] = n;
}

static BOOL C_HandleKey (event_t *ev, byte *buffer, int len)
{
	switch (ev->data1)
	{
	case '\t':
		// Try to do tab-completion
		C_TabComplete ();
		break;

	case GK_PGUP:
		if (ev->data3 & GKM_SHIFT)
		{ // Move to top of console buffer
			RowAdjust = CONSOLELINES;
		}
		else if (RowAdjust < CONSOLELINES)
		{ // Scroll console buffer up
			RowAdjust++;
		}
		break;

	case GK_PGDN:
		if (ev->data3 & GKM_SHIFT)
		{ // Move to bottom of console buffer
			RowAdjust = 0;
		}
		else if (RowAdjust > 0)
		{ // Scroll console buffer down
			RowAdjust--;
		}
		break;

	case GK_HOME:
		// Move cursor to start of line
		buffer[1] = buffer[len+4] = 0;
		break;

	case GK_END:
		// Move cursor to end of line
		buffer[1] = buffer[0];
		makestartposgood ();
		break;

	case GK_LEFT:
		// Move cursor left one character
		if (buffer[1])
		{
			buffer[1]--;
			makestartposgood ();
		}
		break;

	case GK_RIGHT:
		// Move cursor right one character
		if (buffer[1] < buffer[0])
		{
			buffer[1]++;
			makestartposgood ();
		}
		break;

	case '\b':
		// Erase character to left of cursor
		if (buffer[0] && buffer[1])
		{
			char *c, *e;

			e = (char *)&buffer[buffer[0] + 2];
			c = (char *)&buffer[buffer[1] + 2];

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

	case GK_DEL:
		// Erase charater under cursor
		if (buffer[1] < buffer[0])
		{
			char *c, *e;

			e = (char *)&buffer[buffer[0] + 2];
			c = (char *)&buffer[buffer[1] + 3];

			for (; c < e; c++)
				*(c - 1) = *c;

			buffer[0]--;
			makestartposgood ();
		}
		TabbedLast = false;
		break;

	case GK_UP:
		// Move to previous entry in the command history
		if (HistPos == NULL)
		{
			HistPos = HistHead;
		}
		else if (HistPos->Older)
		{
			HistPos = HistPos->Older;
		}

		if (HistPos)
		{
			strcpy ((char *)&buffer[2], HistPos->String);
			buffer[0] = buffer[1] = strlen ((char *)&buffer[2]);
			buffer[len+4] = 0;
			makestartposgood();
		}

		TabbedLast = false;
		break;

	case GK_DOWN:
		// Move to next entry in the command history
		if (HistPos && HistPos->Newer)
		{
			HistPos = HistPos->Newer;
		
			strcpy ((char *)&buffer[2], HistPos->String);
			buffer[0] = buffer[1] = strlen ((char *)&buffer[2]);
		}
		else
		{
			HistPos = NULL;
			buffer[0] = buffer[1] = 0;
		}
		buffer[len+4] = 0;
		makestartposgood();
		TabbedLast = false;
		break;

	case '\r':
		// Execute command line (ENTER)

		buffer[2 + buffer[0]] = 0;

		if (HistHead && stricmp (HistHead->String, (char *)&buffer[2]) == 0)
		{
			// Command line was the same as the previous one,
			// so leave the history list alone
		}
		else
		{
			// Command line is different from last command line,
			// or there is nothing in the history list,
			// so add it to the history list.

			History *temp = (History *)Malloc (sizeof(struct History) + buffer[0]);

			strcpy (temp->String, (char *)&buffer[2]);
			temp->Older = HistHead;
			if (HistHead)
			{
				HistHead->Newer = temp;
			}
			temp->Newer = NULL;
			HistHead = temp;

			if (!HistTail)
			{
				HistTail = temp;
			}

			if (HistSize == MAXHISTSIZE)
			{
				HistTail = HistTail->Newer;
				free (HistTail->Older);
				HistTail->Older = NULL;
			}
			else
			{
				HistSize++;
			}
		}
		HistPos = NULL;
		Printf (127, "]%s\n", &buffer[2]);
		buffer[0] = buffer[1] = buffer[len+4] = 0;
		AddCommandString ((char *)&buffer[2]);
		TabbedLast = false;
		break;
	
	case '`':
	case GK_ESCAPE:
		// Close console and clear command line. But if we're in the
		// fullscreen console mode, there's nothing to fall back on
		// if it's closed.
		if (gamestate == GS_STARTUP)
		{
			return false;
		}
		else if (gamestate == GS_FULLCONSOLE)
		{
			AddCommandString ("menu_main");
		}
		else
		{
			buffer[0] = buffer[1] = buffer[len+4] = 0;
			HistPos = NULL;
			C_ToggleConsole ();
		}
		break;

	case 'C':
	case 'V':
		TabbedLast = false;
		if (ev->data3 & GKM_CTRL)
		{
			if (ev->data1 == 'C')
			{ // copy to clipboard
				if (buffer[0] > 0)
				{
					buffer[2 + buffer[0]] = 0;
					I_PutInClipboard ((char *)&buffer[2]);
				}
				break;
			}
			else
			{ // paste from clipboard
				char *clip = I_GetFromClipboard ();
				if (clip != NULL)
				{
					strtok (clip, "\r\n\b");
					int cliplen = strlen (clip);

					cliplen = MIN(len, cliplen);
					if (buffer[0] + cliplen > len)
					{
						cliplen = len - buffer[0];
					}

					if (cliplen > 0)
					{
						if (buffer[1] < buffer[0])
						{
							memmove (&buffer[2 + buffer[1] + cliplen],
									 &buffer[2 + buffer[1]], buffer[0] - buffer[1]);
						}
						memcpy (&buffer[2 + buffer[1]], clip, cliplen);
						buffer[0] += cliplen;
						buffer[1] += cliplen;
						makestartposgood ();
						HistPos = NULL;
					}
					delete[] clip;
				}
				break;
			}
		}
		// intentional fall-through
	default:
		if (ev->data2 >= ' ')
		{ // Add keypress to command line

			if (buffer[0] < len)
			{
				char data = ev->data2;

				if (buffer[1] == buffer[0])
				{
					buffer[buffer[0] + 2] = data;
				}
				else
				{
					char *c, *e;

					e = (char *)&buffer[buffer[0] + 1];
					c = (char *)&buffer[buffer[1] + 2];

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
	CursorTicker = C_BLINKRATE;
	cursoron = 1;
	return true;
}

BOOL C_Responder (event_t *ev)
{
	if (ev->type != EV_GUI_Event ||
		ConsoleState == c_up ||
		ConsoleState == c_rising ||
		menuactive)
	{
		return false;
	}

	if (ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_KeyRepeat)
	{
		return C_HandleKey (ev, CmdLine, 255);
	}

	return false;
}

CCMD (history)
{
	struct History *hist = HistTail;

	while (hist)
	{
		Printf (PRINT_HIGH, "   %s\n", hist->String);
		hist = hist->Newer;
	}
}

CCMD (clear)
{
	C_FlushDisplay ();
	ClearConsole ();
}

CCMD (echo)
{
	if (argc > 1)
	{
		char *str = BuildString (argc - 1, argv + 1);
		Printf (PRINT_HIGH, "%s\n", str);
		delete[] str;
	}
}

/* Printing in the middle of the screen */

CVAR (Float, con_midtime, 3.f, CVAR_ARCHIVE)

void C_MidPrint (const char *msg)
{
	if (msg)
	{
		char buff[1024];
		sprintf (buff, TEXTCOLOR_RED
			"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
			"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_TAN
			"\n\n%s\n" TEXTCOLOR_RED
			"\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
			"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_NORMAL "\n\n" ,
			msg);

		AddToConsole (-1, buff);

		StatusBar->AttachMessage (new FHUDMessage (msg, 1.5f, 0.375f,
			(EColorRange)PrintColors[PRINTLEVELS], *con_midtime), 'CNTR');
	}
	else
	{
		StatusBar->DetachMessage ('CNTR');
	}
}

/****** Tab completion code ******/

static struct TabData
{
	int UseCount;
	char *Name;
} *TabCommands = NULL;
static int NumTabCommands = 0;
static int TabPos;				// Last TabCommand tabbed to
static int TabStart;			// First char in CmdLine to use for tab completion
static int TabSize;				// Size of tab string

static BOOL FindTabCommand (const char *name, int *stoppos, int len)
{
	int i, cval = 1;

	for (i = 0; i < NumTabCommands; i++)
	{
		cval = strnicmp (TabCommands[i].Name, name, len);
		if (cval >= 0)
			break;
	}

	*stoppos = i;

	return (cval == 0);
}

void C_AddTabCommand (const char *name)
{
	int pos;

	if (FindTabCommand (name, &pos, MAXINT))
	{
		TabCommands[pos].UseCount++;
	}
	else
	{
		NumTabCommands++;
		TabCommands = (TabData *)Realloc (TabCommands, sizeof(struct TabData) * NumTabCommands);
		if (pos < NumTabCommands - 1)
			memmove (TabCommands + pos + 1, TabCommands + pos,
					 (NumTabCommands - pos - 1) * sizeof(struct TabData));
		TabCommands[pos].Name = copystring (name);
		TabCommands[pos].UseCount = 1;
	}
}

void C_RemoveTabCommand (const char *name)
{
	int pos;

	if (FindTabCommand (name, &pos, MAXINT))
	{
		if (--TabCommands[pos].UseCount == 0)
		{
			NumTabCommands--;
			delete[] TabCommands[pos].Name;
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

static void C_TabComplete ()
{
	int i;
	int diffpoint;

	if (!TabbedLast)
	{
		// Skip any spaces at beginning of command line
		if (CmdLine[2] == ' ')
		{
			for (i = 0; i < CmdLine[0]; i++)
				if (CmdLine[2+i] != ' ')
					break;

			TabStart = i + 2;
		}
		else
		{
			TabStart = 2;
		}

		if (TabStart == CmdLine[0] + 2)
			return;		// Line was nothing but spaces

		TabSize = CmdLine[0] - TabStart + 2;

		if (!FindTabCommand ((char *)(CmdLine + TabStart), &TabPos, TabSize))
			return;		// No initial matches

		TabPos--;
		TabbedLast = true;
	}

	if (++TabPos == NumTabCommands)
	{
		TabbedLast = false;
		CmdLine[0] = CmdLine[1] = TabSize;
	}
	else
	{
		diffpoint = FindDiffPoint (TabCommands[TabPos].Name, (char *)(CmdLine + TabStart));

		if (diffpoint < TabSize)
		{
			// No more matches
			TabbedLast = false;
			CmdLine[0] = CmdLine[1] = TabSize + TabStart - 2;
		}
		else
		{		
			strcpy ((char *)(CmdLine + TabStart), TabCommands[TabPos].Name);
			CmdLine[0] = CmdLine[1] = strlen ((char *)(CmdLine + 2)) + 1;
			CmdLine[CmdLine[0] + 1] = ' ';
		}
	}

	makestartposgood ();
}
