/*
** c_console.cpp
** Implements the console itself
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "templates.h"
#include "p_setup.h"

#include "version.h"
#include "g_game.h"
#include "c_bind.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "i_video.h"
#include "g_input.h"
#include "v_palette.h"
#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "sbar.h"
#include "s_sound.h"
#include "doomstat.h"
#include "d_gui.h"
#include "cmdlib.h"
#include "d_net.h"
#include "d_event.h"
#include "d_player.h"
#include "gstrings.h"
#include "c_consolebuffer.h"
#include "g_levellocals.h"
#include "vm.h"

FString FStringFormat(VM_ARGS); // extern from thingdef_data.cpp

#include "gi.h"

#define LEFTMARGIN 8
#define RIGHTMARGIN 8
#define BOTTOMARGIN 12


CUSTOM_CVAR(Int, con_buffersize, -1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	// ensure a minimum size
	if (self >= 0 && self < 128) self = 128;
}

FConsoleBuffer *conbuffer;

static void C_TabComplete (bool goForward);
static bool C_TabCompleteList ();
static bool TabbedLast;		// True if last key pressed was tab
static bool TabbedList;		// True if tab list was shown
CVAR(Bool, con_notablist, false, CVAR_ARCHIVE)


static FTextureID conback;
static uint32_t conshade;
static bool conline;

extern int		gametic;
extern bool		automapactive;	// in AM_map.c
extern bool		advancedemo;

extern FBaseCVar *CVars;
extern FConsoleCommand *Commands[FConsoleCommand::HASH_SIZE];

unsigned	ConCols;
int			ConWidth;
bool		vidactive = false;
bool		cursoron = false;
int			ConBottom, ConScroll, RowAdjust;
int			CursorTicker;
constate_e	ConsoleState = c_up;


static int TopLine, InsertLine;

static void ClearConsole ();

struct GameAtExit
{
	GameAtExit(FString str) : Command(str) {}

	GameAtExit *Next;
	FString Command;
};

static GameAtExit *ExitCmdList;

#define SCROLLUP 1
#define SCROLLDN 2
#define SCROLLNO 0

EXTERN_CVAR (Bool, show_messages)

static unsigned int TickerAt, TickerMax;
static bool TickerPercent;
static const char *TickerLabel;

static bool TickerVisible;
static bool ConsoleDrawing;

// Buffer for AddToConsole()
static char *work = NULL;
static int worklen = 0;

CVAR(Float, con_notifytime, 3.f, CVAR_ARCHIVE)
CVAR(Bool, con_centernotify, false, CVAR_ARCHIVE)
CUSTOM_CVAR(Int, con_scaletext, 0, CVAR_ARCHIVE)		// Scale notify text at high resolutions?
{
	if (self < 0) self = 0;
}

CUSTOM_CVAR(Int, con_scale, 0, CVAR_ARCHIVE)
{
	if (self < 0) self = 0;
}

CUSTOM_CVAR(Float, con_alpha, 0.75f, CVAR_ARCHIVE)
{
	if (self < 0.f) self = 0.f;
	if (self > 1.f) self = 1.f;
}

// Command to run when Ctrl-D is pressed at start of line
CVAR(String, con_ctrl_d, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


struct History
{
	struct History *Older;
	struct History *Newer;
	FString String;
};

struct FCommandBuffer
{
	FString Text;	// The actual command line text
	unsigned CursorPos;
	unsigned StartPos;	// First character to display

	FString YankBuffer;	// Deleted text buffer
	bool AppendToYankBuffer;	// Append consecutive deletes to buffer

	FCommandBuffer()
	{
		CursorPos = StartPos = 0;
	}

	FCommandBuffer(const FCommandBuffer &o)
	{
		Text = o.Text;
		CursorPos = o.CursorPos;
		StartPos = o.StartPos;
	}

	void Draw(int x, int y, int scale, bool cursor)
	{
		if (scale == 1)
		{
			screen->DrawChar(ConFont, CR_ORANGE, x, y, '\x1c', TAG_DONE);
			screen->DrawText(ConFont, CR_ORANGE, x + ConFont->GetCharWidth(0x1c), y,
				&Text[StartPos], TAG_DONE);

			if (cursor)
			{
				screen->DrawChar(ConFont, CR_YELLOW,
					x + ConFont->GetCharWidth(0x1c) + (CursorPos - StartPos) * ConFont->GetCharWidth(0xb),
					y, '\xb', TAG_DONE);
			}
		}
		else
		{
			screen->DrawChar(ConFont, CR_ORANGE, x, y, '\x1c',
				DTA_VirtualWidth, screen->GetWidth() / scale,
				DTA_VirtualHeight, screen->GetHeight() / scale,
				DTA_KeepRatio, true, TAG_DONE);

			screen->DrawText(ConFont, CR_ORANGE, x + ConFont->GetCharWidth(0x1c), y,
				&Text[StartPos],
				DTA_VirtualWidth, screen->GetWidth() / scale,
				DTA_VirtualHeight, screen->GetHeight() / scale,
				DTA_KeepRatio, true, TAG_DONE);

			if (cursor)
			{
				screen->DrawChar(ConFont, CR_YELLOW,
					x + ConFont->GetCharWidth(0x1c) + (CursorPos - StartPos) * ConFont->GetCharWidth(0xb),
					y, '\xb',
					DTA_VirtualWidth, screen->GetWidth() / scale,
					DTA_VirtualHeight, screen->GetHeight() / scale,
					DTA_KeepRatio, true, TAG_DONE);
			}
		}
	}

	void MakeStartPosGood()
	{
		int n = StartPos;
		unsigned cols = ConCols / active_con_scale();

		if (StartPos >= Text.Len())
		{ // Start of visible line is beyond end of line
			n = CursorPos - cols + 2;
		}
		if ((CursorPos - StartPos) >= cols - 2)
		{ // The cursor is beyond the visible part of the line
			n = CursorPos - cols + 2;
		}
		if (StartPos > CursorPos)
		{ // The cursor is in front of the visible part of the line
			n = CursorPos;
		}
		StartPos = MAX(0, n);
	}

	unsigned WordBoundaryRight()
	{
		unsigned index = CursorPos;
		while (index < Text.Len() && Text[index] == ' ') {
			index++;
		}
		while (index < Text.Len() && Text[index] != ' ') {
			index++;
		}
		return index;
	}

	unsigned WordBoundaryLeft()
	{
		int index = CursorPos - 1;
		while (index > -1 && Text[index] == ' ') {
			index--;
		}
		while (index > -1 && Text[index] != ' ') {
			index--;
		}
		return (unsigned)index + 1;
	}

	void CursorStart()
	{
		CursorPos = 0;
		StartPos = 0;
	}

	void CursorEnd()
	{
		CursorPos = (unsigned)Text.Len();
		StartPos = 0;
		MakeStartPosGood();
	}

	void CursorLeft()
	{
		if (CursorPos > 0)
		{
			CursorPos--;
			MakeStartPosGood();
		}
	}

	void CursorRight()
	{
		if (CursorPos < Text.Len())
		{
			CursorPos++;
			MakeStartPosGood();
		}
	}

	void CursorWordLeft()
	{
		CursorPos = WordBoundaryLeft();
		MakeStartPosGood();
	}

	void CursorWordRight()
	{
		CursorPos = WordBoundaryRight();
		MakeStartPosGood();
	}

	void DeleteLeft()
	{
		if (CursorPos > 0)
		{
			Text.Remove(CursorPos - 1, 1);
			CursorPos--;
			MakeStartPosGood();
		}
	}

	void DeleteRight()
	{
		if (CursorPos < Text.Len())
		{
			Text.Remove(CursorPos, 1);
			MakeStartPosGood();
		}
	}

	void DeleteWordLeft()
	{
		if (CursorPos > 0)
		{
			unsigned index = WordBoundaryLeft();
			if (AppendToYankBuffer) {
				YankBuffer = FString(&Text[index], CursorPos - index) + YankBuffer;
			} else {
				YankBuffer = FString(&Text[index], CursorPos - index);
			}
			Text.Remove(index, CursorPos - index);
			CursorPos = index;
			MakeStartPosGood();
		}
	}

	void DeleteLineLeft()
	{
		if (CursorPos > 0)
		{
			if (AppendToYankBuffer) {
				YankBuffer = FString(&Text[0], CursorPos) + YankBuffer;
			} else {
				YankBuffer = FString(&Text[0], CursorPos);
			}
			Text.Remove(0, CursorPos);
			CursorStart();
		}
	}

	void DeleteLineRight()
	{
		if (CursorPos < Text.Len())
		{
			if (AppendToYankBuffer) {
				YankBuffer += FString(&Text[CursorPos], Text.Len() - CursorPos);
			} else {
				YankBuffer = FString(&Text[CursorPos], Text.Len() - CursorPos);
			}
			Text.Truncate(CursorPos);
			CursorEnd();
		}
	}

	void AddChar(int character)
	{
		///FIXME: Not Unicode-aware
		if (CursorPos == Text.Len())
		{
			Text += char(character);
		}
		else
		{
			char foo = char(character);
			Text.Insert(CursorPos, &foo, 1);
		}
		CursorPos++;
		MakeStartPosGood();
	}

	void AddString(FString clip)
	{
		if (clip.IsNotEmpty())
		{
			// Only paste the first line.
			long brk = clip.IndexOfAny("\r\n\b");
			if (brk >= 0)
			{
				clip.Truncate(brk);
			}
			if (Text.IsEmpty())
			{
				Text = clip;
			}
			else
			{
				Text.Insert(CursorPos, clip);
			}
			CursorPos += (unsigned)clip.Len();
			MakeStartPosGood();
		}
	}

	void SetString(FString str)
	{
		Text = str;
		CursorPos = (unsigned)Text.Len();
		MakeStartPosGood();
	}
};
static FCommandBuffer CmdLine;

#define MAXHISTSIZE 50
static struct History *HistHead = NULL, *HistTail = NULL, *HistPos = NULL;
static int HistSize;

#define NUMNOTIFIES 4
#define NOTIFYFADETIME 6

struct FNotifyText
{
	int TimeOut;
	int PrintLevel;
	FString Text;
};

struct FNotifyBuffer
{
public:
	FNotifyBuffer();
	void AddString(int printlevel, FString source);
	void Shift(int maxlines);
	void Clear() { Text.Clear(); }
	void Tick();
	void Draw();

private:
	TArray<FNotifyText> Text;
	int Top;
	int TopGoal;
	enum { NEWLINE, APPENDLINE, REPLACELINE } AddType;
};
static FNotifyBuffer NotifyStrings;

CUSTOM_CVAR(Int, con_notifylines, NUMNOTIFIES, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)
{
	NotifyStrings.Shift(self);
}


int PrintColors[PRINTLEVELS+2] = { CR_RED, CR_GOLD, CR_GRAY, CR_GREEN, CR_GREEN, CR_GOLD };

static void setmsgcolor (int index, int color);

FILE *Logfile = NULL;


FIntCVar msglevel ("msg", 0, CVAR_ARCHIVE);

CUSTOM_CVAR (Int, msg0color, 6, CVAR_ARCHIVE)
{
	setmsgcolor (0, self);
}

CUSTOM_CVAR (Int, msg1color, 5, CVAR_ARCHIVE)
{
	setmsgcolor (1, self);
}

CUSTOM_CVAR (Int, msg2color, 2, CVAR_ARCHIVE)
{
	setmsgcolor (2, self);
}

CUSTOM_CVAR (Int, msg3color, 3, CVAR_ARCHIVE)
{
	setmsgcolor (3, self);
}

CUSTOM_CVAR (Int, msg4color, 3, CVAR_ARCHIVE)
{
	setmsgcolor (4, self);
}

CUSTOM_CVAR (Int, msgmidcolor, 5, CVAR_ARCHIVE)
{
	setmsgcolor (PRINTLEVELS, self);
}

CUSTOM_CVAR (Int, msgmidcolor2, 4, CVAR_ARCHIVE)
{
	setmsgcolor (PRINTLEVELS+1, self);
}

struct TextQueue
{
	TextQueue (bool notify, int printlevel, const char *text)
		: Next(NULL), bNotify(notify), PrintLevel(printlevel), Text(text)
	{
	}
	TextQueue *Next;
	bool bNotify;
	int PrintLevel;
	FString Text;
};

TextQueue *EnqueuedText, **EnqueuedTextTail = &EnqueuedText;

void EnqueueConsoleText (bool notify, int printlevel, const char *text)
{
	TextQueue *queued = new TextQueue (notify, printlevel, text);
	*EnqueuedTextTail = queued;
	EnqueuedTextTail = &queued->Next;
}

void DequeueConsoleText ()
{
	TextQueue *queued = EnqueuedText;

	while (queued != NULL)
	{
		TextQueue *next = queued->Next;
		if (queued->bNotify)
		{
			NotifyStrings.AddString(queued->PrintLevel, queued->Text);
		}
		else
		{
			AddToConsole (queued->PrintLevel, queued->Text);
		}
		delete queued;
		queued = next;
	}
	EnqueuedText = NULL;
	EnqueuedTextTail = &EnqueuedText;
}

void C_InitConback()
{
	conback = TexMan.CheckForTexture ("CONBACK", ETextureType::MiscPatch);

	if (!conback.isValid())
	{
		conback = TexMan.GetTexture (gameinfo.TitlePage, ETextureType::MiscPatch);
		conshade = MAKEARGB(175,0,0,0);
		conline = true;
	}
	else
	{
		conshade = 0;
		conline = false;
	}
}

void C_InitConsole (int width, int height, bool ingame)
{
	int cwidth, cheight;

	vidactive = ingame;
	if (ConFont != NULL)
	{
		cwidth = ConFont->GetCharWidth ('M');
		cheight = ConFont->GetHeight();
	}
	else
	{
		cwidth = cheight = 8;
	}
	ConWidth = (width - LEFTMARGIN - RIGHTMARGIN);
	ConCols = ConWidth / cwidth;

	if (conbuffer == NULL) conbuffer = new FConsoleBuffer;
}

//==========================================================================
//
// CCMD atexit
//
//==========================================================================

UNSAFE_CCMD (atexit)
{
	if (argv.argc() == 1)
	{
		Printf ("Registered atexit commands:\n");
		GameAtExit *record = ExitCmdList;
		while (record != NULL)
		{
			Printf ("%s\n", record->Command.GetChars());
			record = record->Next;
		}
		return;
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		GameAtExit *record = new GameAtExit(argv[i]);
		record->Next = ExitCmdList;
		ExitCmdList = record;
	}
}

//==========================================================================
//
// C_DeinitConsole
//
// Executes the contents of the atexit cvar, if any, at quit time.
// Then releases all of the console's memory.
//
//==========================================================================

void C_DeinitConsole ()
{
	GameAtExit *cmd = ExitCmdList;

	while (cmd != NULL)
	{
		GameAtExit *next = cmd->Next;
		AddCommandString (cmd->Command.LockBuffer());
		delete cmd;
		cmd = next;
	}

	// Free command history
	History *hist = HistTail;

	while (hist != NULL)
	{
		History *next = hist->Newer;
		delete hist;
		hist = next;
	}
	HistTail = HistHead = HistPos = NULL;

	// Free cvars allocated at runtime
	FBaseCVar *var, *next, **nextp;
	for (var = CVars, nextp = &CVars; var != NULL; var = next)
	{
		next = var->m_Next;
		if (var->GetFlags() & CVAR_UNSETTABLE)
		{
			delete var;
			*nextp = next;
		}
		else
		{
			nextp = &var->m_Next;
		}
	}

	// Free alias commands. (i.e. The "commands" that can be allocated
	// at runtime.)
	for (size_t i = 0; i < countof(Commands); ++i)
	{
		FConsoleCommand *cmd = Commands[i];

		while (cmd != NULL)
		{
			FConsoleCommand *next = cmd->m_Next;
			if (cmd->IsAlias())
			{
				delete cmd;
			}
			cmd = next;
		}
	}

	// Make sure all tab commands are cleared before the memory for
	// their names is deallocated.
	C_ClearTabCommands ();

	// Free AddToConsole()'s work buffer
	if (work != NULL)
	{
		free (work);
		work = NULL;
		worklen = 0;
	}

	if (conbuffer != NULL)
	{
		delete conbuffer;
		conbuffer = NULL;
	}
}

static void ClearConsole ()
{
	if (conbuffer != NULL)
	{
		conbuffer->Clear();
	}
	TopLine = InsertLine = 0;
}

static void setmsgcolor (int index, int color)
{
	if ((unsigned)color >= (unsigned)NUM_TEXT_COLORS)
		color = 0;
	PrintColors[index] = color;
}

extern int DisplayWidth;

FNotifyBuffer::FNotifyBuffer()
{
	Top = TopGoal = 0;
	AddType = NEWLINE;
}

void FNotifyBuffer::Shift(int maxlines)
{
	if (maxlines >= 0 && Text.Size() > (unsigned)maxlines)
	{
		Text.Delete(0, Text.Size() - maxlines);
	}
}

void FNotifyBuffer::AddString(int printlevel, FString source)
{
	FBrokenLines *lines;
	int i, width;

	if ((printlevel != 128 && !show_messages) ||
		source.IsEmpty() ||
		gamestate == GS_FULLCONSOLE ||
		gamestate == GS_DEMOSCREEN ||
		con_notifylines == 0)
		return;

	if (ConsoleDrawing)
	{
		EnqueueConsoleText (true, printlevel, source);
		return;
	}

	width = DisplayWidth / active_con_scaletext();

	if (AddType == APPENDLINE && Text.Size() > 0 && Text[Text.Size() - 1].PrintLevel == printlevel)
	{
		FString str = Text[Text.Size() - 1].Text + source;
		lines = V_BreakLines (SmallFont, width, str);
	}
	else
	{
		lines = V_BreakLines (SmallFont, width, source);
		if (AddType == APPENDLINE)
		{
			AddType = NEWLINE;
		}
	}

	if (lines == NULL)
		return;

	for (i = 0; lines[i].Width >= 0; i++)
	{
		FNotifyText newline;

		newline.Text = lines[i].Text;
		newline.TimeOut = gametic + int(con_notifytime * TICRATE);
		newline.PrintLevel = printlevel;
		if (AddType == NEWLINE || Text.Size() == 0)
		{
			if (con_notifylines > 0)
			{
				Shift(con_notifylines - 1);
			}
			Text.Push(newline);
		}
		else
		{
			Text[Text.Size() - 1] = newline;
		}
		AddType = NEWLINE;
	}

	V_FreeBrokenLines (lines);
	lines = NULL;

	switch (source[source.Len()-1])
	{
	case '\r':	AddType = REPLACELINE;	break;
	case '\n':	AddType = NEWLINE;		break;
	default:	AddType = APPENDLINE;	break;
	}

	TopGoal = 0;
}

void AddToConsole (int printlevel, const char *text)
{
	conbuffer->AddText(printlevel, text, Logfile);
}

/* Adds a string to the console and also to the notify buffer */
int PrintString (int printlevel, const char *outline)
{
	if (printlevel < msglevel || *outline == '\0')
	{
		return 0;
	}

	if (printlevel != PRINT_LOG)
	{
		I_PrintStr (outline);

		AddToConsole (printlevel, outline);
		if (vidactive && screen && SmallFont)
		{
			NotifyStrings.AddString(printlevel, outline);
		}
	}
	else if (Logfile != NULL)
	{
		fputs (outline, Logfile);
		fflush (Logfile);
	}
	return (int)strlen (outline);
}

extern bool gameisdead;

int VPrintf (int printlevel, const char *format, va_list parms)
{
	if (gameisdead)
		return 0;

	FString outline;
	outline.VFormat (format, parms);
	return PrintString (printlevel, outline.GetChars());
}

int Printf (int printlevel, const char *format, ...)
{
	va_list argptr;
	int count;

	va_start (argptr, format);
	count = VPrintf (printlevel, format, argptr);
	va_end (argptr);

	return count;
}

int Printf (const char *format, ...)
{
	va_list argptr;
	int count;

	va_start (argptr, format);
	count = VPrintf (PRINT_HIGH, format, argptr);
	va_end (argptr);

	return count;
}

int DPrintf (int level, const char *format, ...)
{
	va_list argptr;
	int count;

	if (developer >= level)
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
	NotifyStrings.Clear();
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

int consoletic = 0;
void C_Ticker()
{
	static int lasttic = 0;
	consoletic++;

	if (lasttic == 0)
		lasttic = consoletic - 1;

	if (con_buffersize > 0)
	{
		conbuffer->ResizeBuffer(con_buffersize);
	}

	if (ConsoleState != c_up)
	{
		if (ConsoleState == c_falling)
		{
			ConBottom += (consoletic - lasttic) * (SCREENHEIGHT * 2 / 25);
			if (ConBottom >= SCREENHEIGHT / 2)
			{
				ConBottom = SCREENHEIGHT / 2;
				ConsoleState = c_down;
			}
		}
		else if (ConsoleState == c_rising)
		{
			ConBottom -= (consoletic - lasttic) * (SCREENHEIGHT * 2 / 25);
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

	lasttic = consoletic;
	NotifyStrings.Tick();
}

void FNotifyBuffer::Tick()
{
	if (TopGoal > Top)
	{
		Top++;
	}
	else if (TopGoal < Top)
	{
		Top--;
	}

	// Remove lines from the beginning that have expired.
	unsigned i;
	for (i = 0; i < Text.Size(); ++i)
	{
		if (Text[i].TimeOut != 0 && Text[i].TimeOut > gametic)
			break;
	}
	if (i > 0)
	{
		Text.Delete(0, i);
	}
}

void FNotifyBuffer::Draw()
{
	bool center = (con_centernotify != 0.f);
	int line, lineadv, color, j;
	bool canskip;
	
	if (gamestate == GS_FULLCONSOLE || gamestate == GS_DEMOSCREEN/* || menuactive != MENU_Off*/)
		return;

	line = Top;
	canskip = true;

	lineadv = SmallFont->GetHeight ();

	for (unsigned i = 0; i < Text.Size(); ++ i)
	{
		FNotifyText &notify = Text[i];

		if (notify.TimeOut == 0)
			continue;

		j = notify.TimeOut - gametic;
		if (j > 0)
		{
			if (!show_messages && notify.PrintLevel != 128)
				continue;

			double alpha = (j < NOTIFYFADETIME) ? 1. * j / NOTIFYFADETIME : 1;

			if (notify.PrintLevel >= PRINTLEVELS)
				color = CR_UNTRANSLATED;
			else
				color = PrintColors[notify.PrintLevel];

			int scale = active_con_scaletext();
			if (!center)
				screen->DrawText (SmallFont, color, 0, line, notify.Text,
					DTA_VirtualWidth, screen->GetWidth() / scale,
					DTA_VirtualHeight, screen->GetHeight() / scale,
					DTA_KeepRatio, true,
					DTA_Alpha, alpha, TAG_DONE);
			else
				screen->DrawText (SmallFont, color, (screen->GetWidth() -
					SmallFont->StringWidth (notify.Text) * scale) / 2 / scale,
					line, notify.Text,
					DTA_VirtualWidth, screen->GetWidth() / scale,
					DTA_VirtualHeight, screen->GetHeight() / scale,
					DTA_KeepRatio, true,
					DTA_Alpha, alpha, TAG_DONE);
			line += lineadv;
			canskip = false;
		}
		else
		{
			if (canskip)
			{
				Top += lineadv;
				line += lineadv;
			}
			notify.TimeOut = 0;
		}
	}
	if (canskip)
	{
		Top = TopGoal;
	}
}

void C_InitTicker (const char *label, unsigned int max, bool showpercent)
{
	TickerPercent = showpercent;
	TickerMax = max;
	TickerLabel = label;
	TickerAt = 0;
}

void C_SetTicker (unsigned int at, bool forceUpdate)
{
	TickerAt = at > TickerMax ? TickerMax : at;
}

void C_DrawConsole ()
{
	static int oldbottom = 0;
	int lines, left, offset;

	int textScale = active_con_scale();

	left = LEFTMARGIN;
	lines = (ConBottom/textScale-ConFont->GetHeight()*2)/ConFont->GetHeight();
	if (-ConFont->GetHeight() + lines*ConFont->GetHeight() > ConBottom/textScale - ConFont->GetHeight()*7/2)
	{
		offset = -ConFont->GetHeight()/2;
		lines--;
	}
	else
	{
		offset = -ConFont->GetHeight();
	}

	oldbottom = ConBottom;

	if (ConsoleState == c_up)
	{
		NotifyStrings.Draw();
		return;
	}
	else if (ConBottom)
	{
		int visheight;
		FTexture *conpic = TexMan[conback];

		visheight = ConBottom;

		screen->DrawTexture (conpic, 0, visheight - screen->GetHeight(),
			DTA_DestWidth, screen->GetWidth(),
			DTA_DestHeight, screen->GetHeight(),
			DTA_ColorOverlay, conshade,
			DTA_Alpha, (gamestate != GS_FULLCONSOLE) ? (double)con_alpha : 1.,
			DTA_Masked, false,
			TAG_DONE);
		if (conline && visheight < screen->GetHeight())
		{
			screen->Clear (0, visheight, screen->GetWidth(), visheight+1, 0, 0);
		}

		if (ConBottom >= 12)
		{
			if (textScale == 1)
				screen->DrawText (ConFont, CR_ORANGE, SCREENWIDTH - 8 -
					ConFont->StringWidth (GetVersionString()),
					ConBottom / textScale - ConFont->GetHeight() - 4,
					GetVersionString(), TAG_DONE);
			else
				screen->DrawText(ConFont, CR_ORANGE, SCREENWIDTH / textScale - 8 -
					ConFont->StringWidth(GetVersionString()),
					ConBottom / textScale - ConFont->GetHeight() - 4,
					GetVersionString(),
					DTA_VirtualWidth, screen->GetWidth() / textScale,
					DTA_VirtualHeight, screen->GetHeight() / textScale,
					DTA_KeepRatio, true, TAG_DONE);

			if (TickerMax)
			{
				char tickstr[256];
				const int tickerY = ConBottom / textScale - ConFont->GetHeight() - 4;
				size_t i;
				int tickend = ConCols / textScale - SCREENWIDTH / textScale / 90 - 6;
				int tickbegin = 0;

				if (TickerLabel)
				{
					tickbegin = (int)strlen (TickerLabel) + 2;
					mysnprintf (tickstr, countof(tickstr), "%s: ", TickerLabel);
				}
				if (tickend > 256 - ConFont->GetCharWidth(0x12))
					tickend = 256 - ConFont->GetCharWidth(0x12);
				tickstr[tickbegin] = 0x10;
				memset (tickstr + tickbegin + 1, 0x11, tickend - tickbegin);
				tickstr[tickend + 1] = 0x12;
				tickstr[tickend + 2] = ' ';
				if (TickerPercent)
				{
					mysnprintf (tickstr + tickend + 3, countof(tickstr) - tickend - 3,
						"%d%%", Scale (TickerAt, 100, TickerMax));
				}
				else
				{
					tickstr[tickend+3] = 0;
				}
				if (textScale == 1)
					screen->DrawText (ConFont, CR_BROWN, LEFTMARGIN, tickerY, tickstr, TAG_DONE);
				else
					screen->DrawText (ConFont, CR_BROWN, LEFTMARGIN, tickerY, tickstr,
						DTA_VirtualWidth, screen->GetWidth() / textScale,
						DTA_VirtualHeight, screen->GetHeight() / textScale,
						DTA_KeepRatio, true, TAG_DONE);

				// Draw the marker
				i = LEFTMARGIN+5+tickbegin*8 + Scale (TickerAt, (int32_t)(tickend - tickbegin)*8, TickerMax);
				if (textScale == 1)
					screen->DrawChar (ConFont, CR_ORANGE, (int)i, tickerY, 0x13, TAG_DONE);
				else
					screen->DrawChar(ConFont, CR_ORANGE, (int)i, tickerY, 0x13,
						DTA_VirtualWidth, screen->GetWidth() / textScale,
						DTA_VirtualHeight, screen->GetHeight() / textScale,
						DTA_KeepRatio, true, TAG_DONE);

				TickerVisible = true;
			}
			else
			{
				TickerVisible = false;
			}
		}

	}

	if (menuactive != MENU_Off)
	{
		return;
	}

	if (lines > 0)
	{
		// No more enqueuing because adding new text to the console won't touch the actual print data.
		conbuffer->FormatText(ConFont, ConWidth / textScale);
		unsigned int consolelines = conbuffer->GetFormattedLineCount();
		FBrokenLines **blines = conbuffer->GetLines();
		FBrokenLines **printline = blines + consolelines - 1 - RowAdjust;

		int bottomline = ConBottom / textScale - ConFont->GetHeight()*2 - 4;

		ConsoleDrawing = true;

		for(FBrokenLines **p = printline; p >= blines && lines > 0; p--, lines--)
		{
			if (textScale == 1)
			{
				screen->DrawText(ConFont, CR_TAN, LEFTMARGIN, offset + lines * ConFont->GetHeight(), (*p)->Text, TAG_DONE);
			}
			else
			{
				screen->DrawText(ConFont, CR_TAN, LEFTMARGIN, offset + lines * ConFont->GetHeight(), (*p)->Text,
					DTA_VirtualWidth, screen->GetWidth() / textScale,
					DTA_VirtualHeight, screen->GetHeight() / textScale,
					DTA_KeepRatio, true, TAG_DONE);
			}
		}

		ConsoleDrawing = false;

		if (ConBottom >= 20)
		{
			if (gamestate != GS_STARTUP)
			{
				// Make a copy of the command line, in case an input event is handled
				// while we draw the console and it changes.
				FCommandBuffer command(CmdLine);
				command.Draw(left, bottomline, textScale, cursoron);
			}
			if (RowAdjust && ConBottom >= ConFont->GetHeight()*7/2)
			{
				// Indicate that the view has been scrolled up (10)
				// and if we can scroll no further (12)
				if (textScale == 1)
					screen->DrawChar (ConFont, CR_GREEN, 0, bottomline, RowAdjust == conbuffer->GetFormattedLineCount() ? 12 : 10, TAG_DONE);
				else
					screen->DrawChar(ConFont, CR_GREEN, 0, bottomline, RowAdjust == conbuffer->GetFormattedLineCount() ? 12 : 10,
						DTA_VirtualWidth, screen->GetWidth() / textScale,
						DTA_VirtualHeight, screen->GetHeight() / textScale,
						DTA_KeepRatio, true, TAG_DONE);
			}
		}
	}
}

void C_FullConsole ()
{
	if (demoplayback)
		G_CheckDemoStatus ();
	D_QuitNetGame ();
	advancedemo = false;
	ConsoleState = c_down;
	HistPos = NULL;
	TabbedLast = false;
	TabbedList = false;
	if (gamestate != GS_STARTUP)
	{
		gamestate = GS_FULLCONSOLE;
		level.Music = "";
		S_Start ();
		P_FreeLevelData ();
		V_SetBlend (0,0,0,0);
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
	else if (!chatmodeon && (ConsoleState == c_up || ConsoleState == c_rising) && menuactive == MENU_Off)
	{
		ConsoleState = c_falling;
		HistPos = NULL;
		TabbedLast = false;
		TabbedList = false;
	}
	else if (gamestate != GS_FULLCONSOLE && gamestate != GS_STARTUP)
	{
		ConsoleState = c_rising;
		C_FlushDisplay ();
	}
}

void C_HideConsole ()
{
	if (gamestate != GS_FULLCONSOLE)
	{
		ConsoleState = c_up;
		ConBottom = 0;
		HistPos = NULL;
	}
}

DEFINE_ACTION_FUNCTION(_Console, HideConsole)
{
	C_HideConsole();
	return 0;
}

DEFINE_ACTION_FUNCTION(_Console, Printf)
{
	PARAM_PROLOGUE;
	FString s = FStringFormat(param, defaultparam, numparam, ret, numret);
	Printf("%s\n", s.GetChars());
	return 0;
}

static bool C_HandleKey (event_t *ev, FCommandBuffer &buffer)
{
	int data1 = ev->data1;
	bool keepappending = false;

	switch (ev->subtype)
	{
	default:
		return false;

	case EV_GUI_Char:
		if (ev->data2)
		{
			// Bash-style shortcuts
			if (data1 == 'b')
			{
				buffer.CursorWordLeft();
				break;
			}
			else if (data1 == 'f')
			{
				buffer.CursorWordRight();
				break;
			}
		}
		// Add keypress to command line
		buffer.AddChar(data1);
		HistPos = NULL;
		TabbedLast = false;
		TabbedList = false;
		break;

	case EV_GUI_WheelUp:
	case EV_GUI_WheelDown:
		if (!(ev->data3 & GKM_SHIFT))
		{
			data1 = GK_PGDN + EV_GUI_WheelDown - ev->subtype;
		}
		else
		{
			data1 = GK_DOWN + EV_GUI_WheelDown - ev->subtype;
		}
		// Intentional fallthrough

	case EV_GUI_KeyDown:
	case EV_GUI_KeyRepeat:
		switch (data1)
		{
		case '\t':
			// Try to do tab-completion
			C_TabComplete ((ev->data3 & GKM_SHIFT) ? false : true);
			break;

		case GK_PGUP:
			if (ev->data3 & (GKM_SHIFT|GKM_CTRL))
			{ // Scroll console buffer up one page
				RowAdjust += (SCREENHEIGHT-4)/active_con_scale() /
					((gamestate == GS_FULLCONSOLE || gamestate == GS_STARTUP) ? ConFont->GetHeight() : ConFont->GetHeight()*2) - 3;
			}
			else if (RowAdjust < conbuffer->GetFormattedLineCount())
			{ // Scroll console buffer up
				if (ev->subtype == EV_GUI_WheelUp)
				{
					RowAdjust += 3;
				}
				else
				{
					RowAdjust++;
				}
				if (RowAdjust > conbuffer->GetFormattedLineCount())
				{
					RowAdjust = conbuffer->GetFormattedLineCount();
				}
			}
			break;

		case GK_PGDN:
			if (ev->data3 & (GKM_SHIFT|GKM_CTRL))
			{ // Scroll console buffer down one page
				const int scrollamt = (SCREENHEIGHT-4)/active_con_scale() /
					((gamestate == GS_FULLCONSOLE || gamestate == GS_STARTUP) ? ConFont->GetHeight() : ConFont->GetHeight()*2) - 3;
				if (RowAdjust < scrollamt)
				{
					RowAdjust = 0;
				}
				else
				{
					RowAdjust -= scrollamt;
				}
			}
			else if (RowAdjust > 0)
			{ // Scroll console buffer down
				if (ev->subtype == EV_GUI_WheelDown)
				{
					RowAdjust = MAX (0, RowAdjust - 3);
				}
				else
				{
					RowAdjust--;
				}
			}
			break;

		case GK_HOME:
			if (ev->data3 & GKM_CTRL)
			{ // Move to top of console buffer
				RowAdjust = conbuffer->GetFormattedLineCount();
			}
			else
			{ // Move cursor to start of line
				buffer.CursorStart();
			}
			break;

		case GK_END:
			if (ev->data3 & GKM_CTRL)
			{ // Move to bottom of console buffer
				RowAdjust = 0;
			}
			else
			{ // Move cursor to end of line
				buffer.CursorEnd();
			}
			break;

		case GK_LEFT:
			// Move cursor left one character
			buffer.CursorLeft();
			break;

		case GK_RIGHT:
			// Move cursor right one character
			buffer.CursorRight();
			break;

		case '\b':
			// Erase character to left of cursor
			buffer.DeleteLeft();
			TabbedLast = false;
			TabbedList = false;
			break;

		case GK_DEL:
			// Erase character under cursor
			buffer.DeleteRight();
			TabbedLast = false;
			TabbedList = false;
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
				buffer.SetString(HistPos->String);
			}

			TabbedLast = false;
			TabbedList = false;
			break;

		case GK_DOWN:
			// Move to next entry in the command history
			if (HistPos && HistPos->Newer)
			{
				HistPos = HistPos->Newer;
				buffer.SetString(HistPos->String);
			}
			else
			{
				HistPos = NULL;
				buffer.SetString("");
			}
			TabbedLast = false;
			TabbedList = false;
			break;

		case 'X':
			if (ev->data3 & GKM_CTRL)
			{
				buffer.SetString("");
				TabbedLast = TabbedList = false;
			}
			break;

		case 'D':
			if (ev->data3 & GKM_CTRL && buffer.Text.Len() == 0)
			{ // Control-D pressed on an empty line
				if (strlen(con_ctrl_d) == 0)
				{
					break;	// Replacement is empty, so do nothing
				}
				buffer.SetString(*con_ctrl_d);
			}
			else
			{
				break;
			}
			// Intentional fall-through for command(s) added with Ctrl-D

		case '\r':
			// Execute command line (ENTER)

			buffer.Text.StripLeftRight();
			Printf(127, TEXTCOLOR_WHITE "]%s\n", buffer.Text.GetChars());

			if (buffer.Text.Len() == 0)
			{
				 // Command line is empty, so do nothing to the history
			}
			else if (HistHead && HistHead->String.CompareNoCase(buffer.Text) == 0)
			{
				// Command line was the same as the previous one,
				// so leave the history list alone
			}
			else
			{
				// Command line is different from last command line,
				// or there is nothing in the history list,
				// so add it to the history list.

				History *temp = new History;
				temp->String = buffer.Text;
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
					delete HistTail->Older;
					HistTail->Older = NULL;
				}
				else
				{
					HistSize++;
				}
			}
			HistPos = NULL;
			{
				// Work with a copy of command to avoid side effects caused by
				// exception raised during execution, like with 'error' CCMD.
				// It's problematic to maintain FString's lock symmetry.
				static TArray<char> command;
				const size_t length = buffer.Text.Len();

				command.Resize(unsigned(length + 1));
				memcpy(&command[0], buffer.Text.GetChars(), length);
				command[length] = '\0';

				buffer.SetString("");

				AddCommandString(&command[0]);
			}
			TabbedLast = false;
			TabbedList = false;
			break;
		
		case '`':
			// Check to see if we have ` bound to the console before accepting
			// it as a way to close the console.
			if (Bindings.GetBinding(KEY_GRAVE).CompareNoCase("toggleconsole"))
			{
				break;
			}
		case GK_ESCAPE:
			// Close console and clear command line. But if we're in the
			// fullscreen console mode, there's nothing to fall back on
			// if it's closed, so open the main menu instead.
			if (gamestate == GS_STARTUP)
			{
				return false;
			}
			else if (gamestate == GS_FULLCONSOLE)
			{
				C_DoCommand ("menu_main");
			}
			else
			{
				buffer.SetString("");
				HistPos = NULL;
				C_ToggleConsole ();
			}
			break;

		case 'C':
		case 'V':
			TabbedLast = false;
			TabbedList = false;
#ifdef __APPLE__
			if (ev->data3 & GKM_META)
#else // !__APPLE__
			if (ev->data3 & GKM_CTRL)
#endif // __APPLE__
			{
				if (data1 == 'C')
				{ // copy to clipboard
					if (buffer.Text.IsNotEmpty())
					{
						I_PutInClipboard(buffer.Text);
					}
				}
				else
				{ // paste from clipboard
					buffer.AddString(I_GetFromClipboard(false));
					HistPos = NULL;
				}
				break;
			}
			break;

		// Bash-style shortcuts
		case 'A':
			if (ev->data3 & GKM_CTRL)
			{
				buffer.CursorStart();
			}
			break;
		case 'E':
			if (ev->data3 & GKM_CTRL)
			{
				buffer.CursorEnd();
			}
			break;
		case 'W':
			if (ev->data3 & GKM_CTRL)
			{
				buffer.DeleteWordLeft();
				keepappending = true;
				TabbedLast = false;
				TabbedList = false;
			}
			break;
		case 'U':
			if (ev->data3 & GKM_CTRL)
			{
				buffer.DeleteLineLeft();
				keepappending = true;
				TabbedLast = false;
				TabbedList = false;
			}
			break;
		case 'K':
			if (ev->data3 & GKM_CTRL)
			{
				buffer.DeleteLineRight();
				keepappending = true;
				TabbedLast = false;
				TabbedList = false;
			}
			break;
		case 'Y':
			if (ev->data3 & GKM_CTRL)
			{
				buffer.AddString(buffer.YankBuffer);
				TabbedLast = false;
				TabbedList = false;
				HistPos = NULL;
			}
			break;
		}
		break;

#ifdef __unix__
	case EV_GUI_MButtonDown:
		buffer.AddString(I_GetFromClipboard(true));
		HistPos = NULL;
		break;
#endif
	}

	buffer.AppendToYankBuffer = keepappending;

	// Ensure that the cursor is always visible while typing
	CursorTicker = C_BLINKRATE;
	cursoron = 1;
	return true;
}

bool C_Responder (event_t *ev)
{
	if (ev->type != EV_GUI_Event ||
		ConsoleState == c_up ||
		ConsoleState == c_rising ||
		menuactive != MENU_Off)
	{
		return false;
	}

	return C_HandleKey(ev, CmdLine);
}

CCMD (history)
{
	struct History *hist = HistTail;

	while (hist)
	{
		Printf ("   %s\n", hist->String.GetChars());
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
	int last = argv.argc()-1;
	for (int i = 1; i <= last; ++i)
	{
		FString formatted = strbin1 (argv[i]);
		Printf ("%s%s", formatted.GetChars(), i!=last ? " " : "\n");
	}
}

/* Printing in the middle of the screen */

CVAR (Float, con_midtime, 3.f, CVAR_ARCHIVE)

static const char bar1[] = TEXTCOLOR_RED "\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
						  "\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_TAN "\n";
static const char bar2[] = TEXTCOLOR_RED "\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
						  "\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_GREEN "\n";
static const char bar3[] = TEXTCOLOR_RED "\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
						  "\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_NORMAL "\n";

void C_MidPrint (FFont *font, const char *msg)
{
	if (StatusBar == NULL || screen == NULL)
		return;

	if (msg != NULL)
	{
		AddToConsole (-1, bar1);
		AddToConsole (-1, msg);
		AddToConsole (-1, bar3);

		StatusBar->AttachMessage (Create<DHUDMessage>(font, msg, 1.5f, 0.375f, 0, 0,
			(EColorRange)PrintColors[PRINTLEVELS], con_midtime), MAKE_ID('C','N','T','R'));
	}
	else
	{
		StatusBar->DetachMessage (MAKE_ID('C','N','T','R'));
	}
}

void C_MidPrintBold (FFont *font, const char *msg)
{
	if (msg)
	{
		AddToConsole (-1, bar2);
		AddToConsole (-1, msg);
		AddToConsole (-1, bar3);

		StatusBar->AttachMessage (Create<DHUDMessage> (font, msg, 1.5f, 0.375f, 0, 0,
			(EColorRange)PrintColors[PRINTLEVELS+1], con_midtime), MAKE_ID('C','N','T','R'));
	}
	else
	{
		StatusBar->DetachMessage (MAKE_ID('C','N','T','R'));
	}
}

DEFINE_ACTION_FUNCTION(_Console, MidPrint)
{
	PARAM_PROLOGUE;
	PARAM_POINTER_NOT_NULL(fnt, FFont);
	PARAM_STRING(text);
	PARAM_BOOL_DEF(bold);

	const char *txt = text[0] == '$'? GStrings(&text[1]) : text.GetChars();
	if (!bold) C_MidPrint(fnt, txt);
	else C_MidPrintBold(fnt, txt);
	return 0;
}

/****** Tab completion code ******/

struct TabData
{
	int UseCount;
	FName TabName;

	TabData()
	: UseCount(0)
	{
	}

	TabData(const char *name)
	: UseCount(1), TabName(name)
	{
	}

	TabData(const TabData &other)
	: UseCount(other.UseCount), TabName(other.TabName)
	{
	}
};

static TArray<TabData> TabCommands (TArray<TabData>::NoInit);
static int TabPos;				// Last TabCommand tabbed to
static int TabStart;			// First char in CmdLine to use for tab completion
static int TabSize;				// Size of tab string

static bool FindTabCommand (const char *name, int *stoppos, int len)
{
	FName aname(name);
	unsigned int i;
	int cval = 1;

	for (i = 0; i < TabCommands.Size(); i++)
	{
		if (TabCommands[i].TabName == aname)
		{
			*stoppos = i;
			return true;
		}
		cval = strnicmp (TabCommands[i].TabName.GetChars(), name, len);
		if (cval >= 0)
			break;
	}

	*stoppos = i;

	return (cval == 0);
}

void C_AddTabCommand (const char *name)
{
	int pos;

	if (FindTabCommand (name, &pos, INT_MAX))
	{
		TabCommands[pos].UseCount++;
	}
	else
	{
		TabData tab(name);
		TabCommands.Insert (pos, tab);
	}
}

void C_RemoveTabCommand (const char *name)
{
	if (TabCommands.Size() == 0)
	{
		// There are no tab commands that can be removed.
		// This is important to skip construction of aname 
		// in case the NameManager has already been destroyed.
		return;
	}

	FName aname(name, true);

	if (aname == NAME_None)
	{
		return;
	}
	for (unsigned int i = 0; i < TabCommands.Size(); ++i)
	{
		if (TabCommands[i].TabName == aname)
		{
			if (--TabCommands[i].UseCount == 0)
			{
				TabCommands.Delete(i);
			}
			break;
		}
	}
}

void C_ClearTabCommands ()
{
	TabCommands.Clear();
}

static int FindDiffPoint (FName name1, const char *str2)
{
	const char *str1 = name1.GetChars();
	int i;

	for (i = 0; tolower(str1[i]) == tolower(str2[i]); i++)
		if (str1[i] == 0 || str2[i] == 0)
			break;

	return i;
}

static void C_TabComplete (bool goForward)
{
	unsigned i;
	int diffpoint;

	if (!TabbedLast)
	{
		bool cancomplete;

		// Skip any spaces at beginning of command line
		for (i = 0; i < CmdLine.Text.Len(); ++i)
		{
			if (CmdLine.Text[i] != ' ')
				break;
		}
		if (i == CmdLine.Text.Len())
		{ // Line was nothing but spaces
			return;
		}
		TabStart = i;

		TabSize = (int)CmdLine.Text.Len() - TabStart;

		if (!FindTabCommand(&CmdLine.Text[TabStart], &TabPos, TabSize))
			return;		// No initial matches

		// Show a list of possible completions, if more than one.
		if (TabbedList || con_notablist)
		{
			cancomplete = true;
		}
		else
		{
			cancomplete = C_TabCompleteList ();
			TabbedList = true;
		}

		if (goForward)
		{ // Position just before the list of completions so that when TabPos
		  // gets advanced below, it will be at the first one.
			--TabPos;
		}
		else
		{ // Find the last matching tab, then go one past it.
			while (++TabPos < (int)TabCommands.Size())
			{
				if (FindDiffPoint(TabCommands[TabPos].TabName, &CmdLine.Text[TabStart]) < TabSize)
				{
					break;
				}
			}
		}
		TabbedLast = true;
		if (!cancomplete)
		{
			return;
		}
	}

	if ((goForward && ++TabPos == (int)TabCommands.Size()) ||
		(!goForward && --TabPos < 0))
	{
		TabbedLast = false;
		CmdLine.Text.Truncate(TabSize);
	}
	else
	{
		diffpoint = FindDiffPoint(TabCommands[TabPos].TabName, &CmdLine.Text[TabStart]);

		if (diffpoint < TabSize)
		{
			// No more matches
			TabbedLast = false;
			CmdLine.Text.Truncate(TabSize - TabStart);
		}
		else
		{
			CmdLine.Text.Truncate(TabStart);
			CmdLine.Text << TabCommands[TabPos].TabName << ' ';
		}
	}
	CmdLine.CursorPos = (unsigned)CmdLine.Text.Len();
	CmdLine.MakeStartPosGood();
}

static bool C_TabCompleteList ()
{
	int nummatches, i;
	size_t maxwidth;
	int commonsize = INT_MAX;

	nummatches = 0;
	maxwidth = 0;

	for (i = TabPos; i < (int)TabCommands.Size(); ++i)
	{
		if (FindDiffPoint (TabCommands[i].TabName, &CmdLine.Text[TabStart]) < TabSize)
		{
			break;
		}
		else
		{
			if (i > TabPos)
			{
				// This keeps track of the longest common prefix for all the possible
				// completions, so we can fill in part of the command for the user if
				// the longest common prefix is longer than what the user already typed.
				int diffpt = FindDiffPoint (TabCommands[i-1].TabName, TabCommands[i].TabName.GetChars());
				if (diffpt < commonsize)
				{
					commonsize = diffpt;
				}
			}
			nummatches++;
			maxwidth = MAX (maxwidth, strlen (TabCommands[i].TabName.GetChars()));
		}
	}
	if (nummatches > 1)
	{
		size_t x = 0;
		maxwidth += 3;
		Printf (TEXTCOLOR_BLUE "Completions for %s:\n", CmdLine.Text.GetChars());
		for (i = TabPos; nummatches > 0; ++i, --nummatches)
		{
			// [Dusk] Print console commands blue, CVars green, aliases red.
			const char* colorcode = "";
			FConsoleCommand* ccmd;
			if (FindCVar (TabCommands[i].TabName, NULL))
				colorcode = TEXTCOLOR_GREEN;
			else if ((ccmd = FConsoleCommand::FindByName (TabCommands[i].TabName)) != NULL)
			{
				if (ccmd->IsAlias())
					colorcode = TEXTCOLOR_RED;
				else
					colorcode = TEXTCOLOR_LIGHTBLUE;
			}

			Printf ("%s%-*s", colorcode, int(maxwidth), TabCommands[i].TabName.GetChars());
			x += maxwidth;
			if (x > ConCols / active_con_scale() - maxwidth)
			{
				x = 0;
				Printf ("\n");
			}
		}
		if (x != 0)
		{
			Printf ("\n");
		}
		// Fill in the longest common prefix, if it's longer than what was typed.
		if (TabSize != commonsize)
		{
			TabSize = commonsize;
			CmdLine.Text.Truncate(TabStart);
			CmdLine.Text.AppendCStrPart(TabCommands[TabPos].TabName.GetChars(), commonsize);
			CmdLine.CursorPos = (unsigned)CmdLine.Text.Len();
		}
		return false;
	}
	return true;
}
