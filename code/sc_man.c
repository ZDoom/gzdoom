
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
#include "z_zone.h"

// MACROS ------------------------------------------------------------------

#define MAX_STRING_SIZE 64
#define ASCII_COMMENT (';')
#define CPP_COMMENT ('/')
#define ASCII_QUOTE (34)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void CheckOpen(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char *sc_String;
int sc_Number;
int sc_Line;
BOOL sc_End;
BOOL sc_Crossed;
BOOL sc_FileScripts = false;
char *sc_ScriptsDir = "";

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char ScriptName[16];
static char *ScriptBuffer;
static char *ScriptPtr;
static char *ScriptEndPtr;
static char StringBuffer[MAX_STRING_SIZE];
static BOOL ScriptOpen = false;
static int ScriptSize;
static BOOL AlreadyGot = false;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SC_Open
//
//==========================================================================

void SC_Open (const char *name)
{
	SC_OpenLumpNum (W_GetNumForName (name), name);
}

//==========================================================================
//
// SC_OpenLumpNum
//
//==========================================================================

void SC_OpenLumpNum (int lump, const char *name)
{
	SC_Close ();
	ScriptBuffer = (char *)W_CacheLumpNum (lump, PU_STATIC);
	ScriptSize = W_LumpLength (lump);
	strcpy (ScriptName, name);
	ScriptPtr = ScriptBuffer;
	ScriptEndPtr = ScriptPtr + ScriptSize;
	sc_Line = 1;
	sc_End = false;
	ScriptOpen = true;
	sc_String = StringBuffer;
	AlreadyGot = false;
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
		Z_Free (ScriptBuffer);
		ScriptOpen = false;
	}
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
		while (*ScriptPtr <= 32)
		{
			if (ScriptPtr >= ScriptEndPtr)
			{
				sc_End = true;
				return false;
			}
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
		if (*ScriptPtr != ASCII_COMMENT &&
			*ScriptPtr != CPP_COMMENT &&
			*(ScriptPtr+1) != CPP_COMMENT)
		{ // Found a token
			foundToken = true;
		}
		else
		{ // Skip comment
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
		while ((*ScriptPtr > 32) && (*ScriptPtr != ASCII_COMMENT))
		{
			*text++ = *ScriptPtr++;
			if (ScriptPtr == ScriptEndPtr
				|| text == &sc_String[MAX_STRING_SIZE-1])
			{
				break;
			}
		}
	}
	*text = 0;
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
		SC_ScriptError ("Missing string.");
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
		SC_ScriptError(NULL);
	}
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
		sc_Number = strtol (sc_String, &stopper, 0);
		if (*stopper != 0)
		{
			I_FatalError ("SC_GetNumber: Bad numeric constant \"%s\".\n"
				"Script %s, Line %d", sc_String, ScriptName, sc_Line);
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
// SC_MustGetNumber
//
//==========================================================================

void SC_MustGetNumber (void)
{
	if (SC_GetNumber() == false)
	{
		SC_ScriptError ("Missing integer.");
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

void SC_ScriptError (const char *message)
{
	if (message == NULL)
	{
		message = "Bad syntax.";
	}
	I_FatalError ("Script error, \"%s\" line %d: %s", ScriptName,
		sc_Line, message);
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
