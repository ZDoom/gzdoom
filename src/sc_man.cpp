
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

// MACROS ------------------------------------------------------------------

#define MAX_STRING_SIZE 4096
#define ASCII_COMMENT (';')
#define CPP_COMMENT ('/')
#define C_COMMENT ('*')
#define ASCII_QUOTE (34)
#define LUMP_SCRIPT 1
#define FILE_ZONE_SCRIPT 2

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

static char ScriptName[128];
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

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SC_Open
//
//==========================================================================

void SC_Open (const char *name)
{
	SC_OpenLumpNum (Wads.GetNumForName (name), name);
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
	SC_Close ();
	ScriptSize = M_ReadFile (name, (byte **)&ScriptBuffer);
	ExtractFileBase (name, ScriptName);
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
	strcpy (ScriptName, name);
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
	strcpy (ScriptName, name);
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

//==========================================================================
//
// SC_GetString
//
//==========================================================================

BOOL SC_GetString (void)
{
	char *text;
	BOOL foundToken;

	CheckOpen();
	if (AlreadyGot)
	{
		AlreadyGot = false;
		return true;
	}
	foundToken = false;
	sc_Crossed = false;
	if (ScriptPtr >= ScriptEndPtr)
	{
		sc_End = true;
		return false;
	}
	while (foundToken == false)
	{
		while (ScriptPtr < ScriptEndPtr && *ScriptPtr <= ' ')
		{
			if (*ScriptPtr++ == '\n')
			{
				sc_Line++;
				sc_Crossed = true;
			}
		}
		if (ScriptPtr >= ScriptEndPtr)
		{
			sc_End = true;
			return false;
		}
		if ((CMode || *ScriptPtr != ASCII_COMMENT) &&
			!(ScriptPtr[0] == CPP_COMMENT && ScriptPtr < ScriptEndPtr - 1 &&
			  (ScriptPtr[1] == CPP_COMMENT || ScriptPtr[1] == C_COMMENT)))
		{ // Found a token
			foundToken = true;
		}
		else
		{ // Skip comment
			if (ScriptPtr[0] == CPP_COMMENT && ScriptPtr[1] == C_COMMENT)
			{	// C comment
				while (ScriptPtr[0] != C_COMMENT || ScriptPtr[1] != CPP_COMMENT)
				{
					if (ScriptPtr[0] == '\n')
					{
						sc_Line++;
						sc_Crossed = true;
					}
					ScriptPtr++;
					if (ScriptPtr >= ScriptEndPtr - 1)
					{
						sc_End = true;
						return false;
					}
				}
				ScriptPtr += 2;
			}
			else
			{	// C++ comment
				while (*ScriptPtr++ != '\n')
				{
					if (ScriptPtr >= ScriptEndPtr)
					{
						sc_End = true;
						return false;
					}
				}
				sc_Line++;
				sc_Crossed = true;
			}
		}
	}
	text = sc_String;
	if (*ScriptPtr == ASCII_QUOTE)
	{ // Quoted string
		ScriptPtr++;
		while (*ScriptPtr != ASCII_QUOTE)
		{
			*text++ = *ScriptPtr++;
			if (ScriptPtr == ScriptEndPtr
				|| text == &sc_String[MAX_STRING_SIZE-1])
			{
				break;
			}
		}
		ScriptPtr++;
	}
	else
	{ // Normal string
		static const char *stopchars;

		if (CMode)
		{
			stopchars = "`~!@#$%^&*(){}[]/=\?+|;:<>,";

			// '-' can be its own token, or it can be part of a negative number
			if (*ScriptPtr == '-')
			{
				*text++ = '-';
				ScriptPtr++;
				if (ScriptPtr < ScriptEndPtr || *ScriptPtr >= '0' && *ScriptPtr <= '9')
				{
					goto grabtoken;
				}
				goto gottoken;
			}
		}
		else
		{
			stopchars = "{}|=";
		}
		if (strchr (stopchars, *ScriptPtr))
		{
			*text++ = *ScriptPtr++;
		}
		else
		{
grabtoken:
			while ((*ScriptPtr > ' ') && (strchr (stopchars, *ScriptPtr) == NULL)
				&& (CMode || *ScriptPtr != ASCII_COMMENT)
				&& !(ScriptPtr[0] == CPP_COMMENT && (ScriptPtr < ScriptEndPtr - 1) &&
					 (ScriptPtr[1] == CPP_COMMENT || ScriptPtr[1] == C_COMMENT)))
			{
				*text++ = *ScriptPtr++;
				if (ScriptPtr == ScriptEndPtr
					|| text == &sc_String[MAX_STRING_SIZE-1])
				{
					break;
				}
			}
		}
	}
gottoken:
	*text = 0;
	sc_StringLen = text - sc_String;
	return true;
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
				"Script %s, Line %d\n", sc_String, ScriptName, sc_Line);
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
	char composed[2048];
	if (message == NULL)
	{
		message = "Bad syntax.";
	}

	va_list arglist;
	va_start (arglist, message);
	vsprintf (composed, message, arglist);
	va_end (arglist);

	I_Error ("Script error, \"%s\" line %d:\n%s\n", ScriptName,
		sc_Line, composed);
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
