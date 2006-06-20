
//**************************************************************************
//**
//** sc_man.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: sc_man.c,v $
//** $Revision: 1.3 $
//** $Date: 96/01/06 03:23:43 $
//** $Author: bgokey $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <stdlib.h>
#include "doomtype.h"
#include "i_system.h"
#include "sc_man.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "m_misc.h"
#include "templates.h"

// MACROS ------------------------------------------------------------------

#define MAX_STRING_SIZE 4096
#define ASCII_COMMENT (';')
#define CPP_COMMENT ('/')
#define C_COMMENT ('*')
#define ASCII_QUOTE ('"')
#define LUMP_SCRIPT 1
#define FILE_ZONE_SCRIPT 2

#define NORMAL_STOPCHARS			"{}|="
#define CMODE_STOPCHARS				"`~!@#$%^&*(){}[]/=\?+|;:<>,."
#define CMODE_STOPCHARS_NODECIMAL	"`~!@#$%^&*(){}[]/=\?+|;:<>,"

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void SC_PrepareScript (void);
static void CheckOpen (void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char *sc_String;
int sc_StringLen;
int sc_Number;
float sc_Float;
int sc_Line;
BOOL sc_End;
BOOL sc_Crossed;
BOOL sc_FileScripts = false;
char *sc_ScriptsDir = "";

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FString ScriptName;
static char *ScriptBuffer;
static char *ScriptPtr;
static char *ScriptEndPtr;
static char StringBuffer[MAX_STRING_SIZE];
static bool ScriptOpen = false;
static int ScriptSize;
static bool AlreadyGot = false;
static bool FreeScript = false;
static char *SavedScriptPtr;
static int SavedScriptLine;
static bool CMode;
static bool Escape=true;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SC_Open
//
//==========================================================================

void SC_Open (const char *name)
{
	int lump = Wads.CheckNumForFullName(name);
	if (lump==-1) lump = Wads.GetNumForName(name);
	SC_OpenLumpNum(lump, name);
}

//==========================================================================
//
// SC_OpenFile
//
// Loads a script (from a file). Uses the new/delete memory allocator for
// memory allocation and de-allocation.
//
//==========================================================================

void SC_OpenFile (const char *name)
{
	byte *filebuf;

	SC_Close ();
	ScriptSize = M_ReadFile (name, &filebuf);
	ScriptBuffer = (char *)filebuf;
	ScriptName = name;	// This is used for error messages so the full file name is preferable
	//ExtractFileBase (name, ScriptName);
	FreeScript = true;
	SC_PrepareScript ();
}

//==========================================================================
//
// SC_OpenMem
//
// Prepares a script that is already in memory for parsing. The caller is
// responsible for freeing it, if needed.
//
//==========================================================================

void SC_OpenMem (const char *name, char *buffer, int size)
{
	SC_Close ();
	ScriptSize = size;
	ScriptBuffer = buffer;
	ScriptName = name;
	FreeScript = false;
	SC_PrepareScript ();
}

//==========================================================================
//
// SC_OpenLumpNum
//
// Loads a script (from the WAD files).
//
//==========================================================================

void SC_OpenLumpNum (int lump, const char *name)
{
	SC_Close ();
	ScriptSize = Wads.LumpLength (lump);
	ScriptBuffer = new char[ScriptSize];
	Wads.ReadLump (lump, ScriptBuffer);
	ScriptName = name;
	FreeScript = true;
	SC_PrepareScript ();
}

//==========================================================================
//
// SC_PrepareScript
//
// Prepares a script for parsing.
//
//==========================================================================

static void SC_PrepareScript (void)
{
	ScriptPtr = ScriptBuffer;
	ScriptEndPtr = ScriptPtr + ScriptSize;
	sc_Line = 1;
	sc_End = false;
	ScriptOpen = true;
	sc_String = StringBuffer;
	AlreadyGot = false;
	SavedScriptPtr = NULL;
	CMode = false;
	Escape = true;
}

//==========================================================================
//
// SC_Close
//
//==========================================================================

void SC_Close (void)
{
	if (ScriptOpen)
	{
		if (ScriptBuffer)
		{
			if (FreeScript)
			{
				delete[] ScriptBuffer;
			}
		}
		ScriptBuffer = NULL;
		ScriptOpen = false;
	}
}

//==========================================================================
//
// SC_SavePos
//
// Saves the current script location for restoration later
//
//==========================================================================

void SC_SavePos (void)
{
	CheckOpen ();
	if (sc_End)
	{
		SavedScriptPtr = NULL;
	}
	else
	{
		SavedScriptPtr = ScriptPtr;
		SavedScriptLine = sc_Line;
	}
}

//==========================================================================
//
// SC_RestorePos
//
// Restores the previously saved script location
//
//==========================================================================

void SC_RestorePos (void)
{
	if (SavedScriptPtr)
	{
		ScriptPtr = SavedScriptPtr;
		sc_Line = SavedScriptLine;
		sc_End = false;
		AlreadyGot = false;
	}
}

//==========================================================================
//
// SC_SetCMode
//
// Enables/disables C mode. In C mode, more characters are considered to
// be whole words than in non-C mode.
//
//==========================================================================

void SC_SetCMode (bool cmode)
{
	CMode = cmode;
}

void SC_SetEscape (bool esc)
{
	Escape = esc;
}

//==========================================================================
//
// SC_GetString
//
//==========================================================================

BOOL SC_GetString ()
{
	char *marker, *tok;

	CheckOpen();
	if (AlreadyGot)
	{
		AlreadyGot = false;
		return true;
	}
	sc_Crossed = false;
	if (ScriptPtr >= ScriptEndPtr)
	{
		sc_End = true;
		return false;
	}

	// In case the generated scanner does not use marker, avoid compiler warnings.
	marker;
#include "sc_man_scanner.h"
}

//==========================================================================
//
// SC_MustGetString
//
//==========================================================================

void SC_MustGetString (void)
{
	if (SC_GetString () == false)
	{
		SC_ScriptError ("Missing string (unexpected end of file).");
	}
}

//==========================================================================
//
// SC_MustGetStringName
//
//==========================================================================

void SC_MustGetStringName (const char *name)
{
	SC_MustGetString ();
	if (SC_Compare (name) == false)
	{
		SC_ScriptError ("Expected '%s', got '%s'.", name, sc_String);
	}
}

//==========================================================================
//
// SC_CheckString
//
// Checks if the next token matches the specified string. Returns true if
// it does. If it doesn't, it ungets it and returns false.
//==========================================================================

bool SC_CheckString (const char *name)
{
	if (SC_GetString ())
	{
		if (SC_Compare (name))
		{
			return true;
		}
		SC_UnGet ();
	}
	return false;
}

//==========================================================================
//
// SC_GetNumber
//
//==========================================================================

BOOL SC_GetNumber (void)
{
	char *stopper;

	CheckOpen ();
	if (SC_GetString())
	{
		if (strcmp (sc_String, "MAXINT") == 0)
		{
			sc_Number = INT_MAX;
		}
		else
		{
			sc_Number = strtol (sc_String, &stopper, 0);
			if (*stopper != 0)
			{
				SC_ScriptError ("SC_GetNumber: Bad numeric constant \"%s\".", sc_String);
			}
		}
		sc_Float = (float)sc_Number;
		return true;
	}
	else
	{
		return false;
	}
}

//==========================================================================
//
// SC_MustGetNumber
//
//==========================================================================

void SC_MustGetNumber (void)
{
	if (SC_GetNumber() == false)
	{
		SC_ScriptError ("Missing integer (unexpected end of file).");
	}
}

//==========================================================================
//
// SC_CheckNumber
// similar to SC_GetNumber but ungets the token if it isn't a number 
// and does not print an error
//
//==========================================================================

BOOL SC_CheckNumber (void)
{
	char *stopper;

	//CheckOpen ();
	if (SC_GetString())
	{
		if (strcmp (sc_String, "MAXINT") == 0)
		{
			sc_Number = INT_MAX;
		}
		else
		{
			sc_Number = strtol (sc_String, &stopper, 0);
			if (*stopper != 0)
			{
				SC_UnGet();
				return false;
			}
		}
		sc_Float = (float)sc_Number;
		return true;
	}
	else
	{
		return false;
	}
}

//==========================================================================
//
// SC_CheckFloat
// [GRB] Same as SC_CheckNumber, only for floats
//
//==========================================================================

BOOL SC_CheckFloat (void)
{
	char *stopper;

	//CheckOpen ();
	if (SC_GetString())
	{
		sc_Float = strtod (sc_String, &stopper);
		if (*stopper != 0)
		{
			SC_UnGet();
			return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}


//==========================================================================
//
// SC_GetFloat
//
//==========================================================================

BOOL SC_GetFloat (void)
{
	char *stopper;

	CheckOpen ();
	if (SC_GetString())
	{
		sc_Float = (float)strtod (sc_String, &stopper);
		if (*stopper != 0)
		{
			I_Error ("SC_GetFloat: Bad numeric constant \"%s\".\n"
				"Script %s, Line %d\n", sc_String, ScriptName.GetChars(), sc_Line);
		}
		sc_Number = (int)sc_Float;
		return true;
	}
	else
	{
		return false;
	}
}

//==========================================================================
//
// SC_MustGetFloat
//
//==========================================================================

void SC_MustGetFloat (void)
{
	if (SC_GetFloat() == false)
	{
		SC_ScriptError ("Missing floating-point number (unexpected end of file).");
	}
}

//==========================================================================
//
// SC_UnGet
//
// Assumes there is a valid string in sc_String.
//
//==========================================================================

void SC_UnGet (void)
{
	AlreadyGot = true;
}

//==========================================================================
//
// SC_Check
//
// Returns true if another token is on the current line.
//
//==========================================================================

/*
BOOL SC_Check(void)
{
	char *text;

	CheckOpen();
	text = ScriptPtr;
	if(text >= ScriptEndPtr)
	{
		return false;
	}
	while(*text <= 32)
	{
		if(*text == '\n')
		{
			return false;
		}
		text++;
		if(text == ScriptEndPtr)
		{
			return false;
		}
	}
	if(*text == ASCII_COMMENT)
	{
		return false;
	}
	return true;
}
*/

//==========================================================================
//
// SC_MatchString
//
// Returns the index of the first match to sc_String from the passed
// array of strings, or -1 if not found.
//
//==========================================================================

int SC_MatchString (const char **strings)
{
	int i;

	for (i = 0; *strings != NULL; i++)
	{
		if (SC_Compare (*strings++))
		{
			return i;
		}
	}
	return -1;
}

//==========================================================================
//
// SC_MustMatchString
//
//==========================================================================

int SC_MustMatchString (const char **strings)
{
	int i;

	i = SC_MatchString (strings);
	if (i == -1)
	{
		SC_ScriptError (NULL);
	}
	return i;
}

//==========================================================================
//
// SC_Compare
//
//==========================================================================

BOOL SC_Compare (const char *text)
{
	return (stricmp (text, sc_String) == 0);
}

//==========================================================================
//
// SC_ScriptError
//
//==========================================================================

void STACK_ARGS SC_ScriptError (const char *message, ...)
{
	FString composed;

	if (message == NULL)
	{
		composed = "Bad syntax.";
	}
	else
	{
		va_list arglist;
		va_start (arglist, message);
		composed.VFormat (message, arglist);
		va_end (arglist);
	}

	I_Error ("Script error, \"%s\" line %d:\n%s\n", ScriptName.GetChars(),
		sc_Line, composed.GetChars());
}

//==========================================================================
//
// CheckOpen
//
//==========================================================================

static void CheckOpen(void)
{
	if (ScriptOpen == false)
	{
		I_FatalError ("SC_ call before SC_Open().");
	}
}


//==========================================================================
//
// Script state saving
// This allows recursive script execution
// This does not save the last token!
//
//==========================================================================

struct SavedScript
{
	int sc_Line;
	BOOL sc_End;
	BOOL sc_Crossed;
	BOOL sc_FileScripts;

	FString *ScriptName;
	char *ScriptBuffer;
	char *ScriptPtr;
	char *ScriptEndPtr;
	bool ScriptOpen;
	int ScriptSize;
	bool FreeScript;
	char *SavedScriptPtr;
	int SavedScriptLine;
	bool CMode;
	bool Escape;
};


static TArray<SavedScript> SavedScripts;

void SC_SaveScriptState()
{
	SavedScript ss;

	ss.sc_Line = sc_Line;
	ss.sc_End = sc_End;
	ss.sc_Crossed = sc_Crossed;
	ss.sc_FileScripts = sc_FileScripts;
	ss.ScriptName = ::new FString(ScriptName);
	ss.ScriptBuffer = ScriptBuffer;
	ss.ScriptPtr = ScriptPtr;
	ss.ScriptEndPtr = ScriptEndPtr;
	ss.ScriptOpen = ScriptOpen;
	ss.ScriptSize = ScriptSize;
	ss.FreeScript = FreeScript;
	ss.SavedScriptPtr = SavedScriptPtr;
	ss.SavedScriptLine = SavedScriptLine;
	ss.CMode = CMode;
	ss.Escape = Escape;
	SavedScripts.Push(ss);
	ScriptOpen = false;
}

void SC_RestoreScriptState()
{
	if (SavedScripts.Size()>0)
	{
		SavedScript ss;

		SavedScripts.Pop(ss);
		sc_Line = ss.sc_Line;
		sc_End = ss.sc_End;
		sc_Crossed = ss.sc_Crossed;
		sc_FileScripts = ss.sc_FileScripts;
		ScriptName = *ss.ScriptName;
		::delete ss.ScriptName;
		ScriptBuffer = ss.ScriptBuffer;
		ScriptPtr = ss.ScriptPtr;
		ScriptEndPtr = ss.ScriptEndPtr;
		ScriptOpen = ss.ScriptOpen;
		ScriptSize = ss.ScriptSize;
		FreeScript = ss.FreeScript;
		SavedScriptPtr = ss.SavedScriptPtr;
		SavedScriptLine = ss.SavedScriptLine;
		CMode = ss.CMode;
		Escape = ss.Escape;
		SavedScripts.ShrinkToFit();
		AlreadyGot = false;
	}
}
