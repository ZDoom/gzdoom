
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
static bool SC_ScanString (bool tokens);
static void CheckOpen (void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int sc_TokenType;
char *sc_String;
unsigned int sc_StringLen;
int sc_Number;
double sc_Float;
FName sc_Name;
int sc_Line;
bool sc_End;
bool sc_Crossed;
bool sc_FileScripts = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FString ScriptName;
static char *ScriptBuffer;
static char *ScriptPtr;
static char *ScriptEndPtr;
static char StringBuffer[MAX_STRING_SIZE];
static bool ScriptOpen = false;
static int ScriptSize;
static bool AlreadyGot = false;
static int AlreadyGotLine;
static bool LastGotToken = false;
static char *LastGotPtr;
static int LastGotLine;
static bool FreeScript = false;
static char *SavedScriptPtr;
static int SavedScriptLine;
static bool CMode;
static bool Escape = true;
static bool AtTermed;

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
	BYTE *filebuf;

	SC_Close ();
	ScriptSize = M_ReadFile (name, &filebuf);
	ScriptBuffer = new char[ScriptSize + 1];
	memcpy (ScriptBuffer, filebuf, ScriptSize);
	ScriptBuffer[ScriptSize++] = '\n';
	delete[] filebuf;
	ScriptName = name;	// This is used for error messages so the full file name is preferable
	FreeScript = true;
	SC_PrepareScript ();
}

//==========================================================================
//
// SC_OpenMem
//
// Prepares a script that is already in memory for parsing. The caller is
// responsible for freeing it, if needed. You MUST ensure that the script
// ends with the newline character '\n'.
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
	ScriptBuffer = new char[ScriptSize + 1];
	Wads.ReadLump (lump, ScriptBuffer);
	ScriptBuffer[ScriptSize++] = '\n';
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
	LastGotToken = false;
	LastGotPtr = NULL;
	LastGotLine = 1;
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
		LastGotToken = false;
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
// SC_SetEscape
//
// Turns the escape sequence \" in strings on or off. If it's off, that
// means you can't include quotation marks inside strings.
//
//==========================================================================

void SC_SetEscape (bool esc)
{
	Escape = esc;
}

//==========================================================================
//
// SC_ScanString
//
// Set tokens true if you want sc_TokenType to be set.
//
//==========================================================================

static bool SC_ScanString (bool tokens)
{
	char *marker, *tok;
	bool return_val;

	CheckOpen();
	if (AlreadyGot)
	{
		AlreadyGot = false;
		if (!tokens || LastGotToken)
		{
			return true;
		}
		ScriptPtr = LastGotPtr;
		sc_Line = LastGotLine;
	}

	sc_Crossed = false;
	if (ScriptPtr >= ScriptEndPtr)
	{
		sc_End = true;
		return false;
	}

	LastGotPtr = ScriptPtr;
	LastGotLine = sc_Line;

	// In case the generated scanner does not use marker, avoid compiler warnings.
	marker;
#include "sc_man_scanner.h"
	LastGotToken = tokens;
	return return_val;
}

//==========================================================================
//
// SC_GetString
//
//==========================================================================

bool SC_GetString ()
{
	return SC_ScanString (false);
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
// SC_GetToken
//
// Sets sc_Float, sc_Number, and sc_Name based on sc_TokenType.
//
//==========================================================================

bool SC_GetToken ()
{
	if (SC_ScanString (true))
	{
		if (sc_TokenType == TK_NameConst)
		{
			sc_Name = FName(sc_String);
		}
		else if (sc_TokenType == TK_IntConst)
		{
			char *stopper;
			sc_Number = strtol(sc_String, &stopper, 0);
			sc_Float = sc_Number;
		}
		else if (sc_TokenType == TK_FloatConst)
		{
			char *stopper;
			sc_Float = strtod(sc_String, &stopper);
		}
		else if (sc_TokenType == TK_StringConst)
		{
			strbin (sc_String);
		}
		return true;
	}
	return false;
}

//==========================================================================
//
// SC_MustGetAnyToken
//
//==========================================================================

void SC_MustGetAnyToken (void)
{
	if (SC_GetToken () == false)
	{
		SC_ScriptError ("Missing token (unexpected end of file).");
	}
}

//==========================================================================
//
// SC_TokenMustBe
//
//==========================================================================

void SC_TokenMustBe (int token)
{
	if (sc_TokenType != token)
	{
		FString tok1 = SC_TokenName(token);
		FString tok2 = SC_TokenName(sc_TokenType, sc_String);
		SC_ScriptError ("Expected %s but got %s instead.", tok1.GetChars(), tok2.GetChars());
	}
}

//==========================================================================
//
// SC_MustGetToken
//
//==========================================================================

void SC_MustGetToken (int token)
{
	SC_MustGetAnyToken ();
	SC_TokenMustBe(token);
}

//==========================================================================
//
// SC_CheckToken
//
// Checks if the next token matches the specified token. Returns true if
// it does. If it doesn't, it ungets it and returns false.
//==========================================================================

bool SC_CheckToken (int token)
{
	if (SC_GetToken ())
	{
		if (sc_TokenType == token)
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

bool SC_GetNumber (void)
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

bool SC_CheckNumber (void)
{
	char *stopper;

	//CheckOpen ();
	if (SC_GetString())
	{
		if (sc_String[0] == 0)
		{
			SC_UnGet();
			return false;
		}
		else if (strcmp (sc_String, "MAXINT") == 0)
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

bool SC_CheckFloat (void)
{
	char *stopper;

	//CheckOpen ();
	if (SC_GetString())
	{
		if (sc_String[0] == 0)
		{
			SC_UnGet();
			return false;
		}
	
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

bool SC_GetFloat (void)
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
	AlreadyGotLine = LastGotLine;	// in case of an error we want the line of the last token.
}

//==========================================================================
//
// SC_Check
//
// Returns true if another token is on the current line.
//
//==========================================================================

/*
bool SC_Check(void)
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

bool SC_Compare (const char *text)
{
	return (stricmp (text, sc_String) == 0);
}

//==========================================================================
//
// SC_TokenName
//
// Returns the name of a token.
//
//==========================================================================

FString SC_TokenName (int token, const char *string)
{
	static const char *const names[] =
	{
		"identifier",
		"string constant",
		"name constant",
		"integer constant",
		"float constant",
		"'...'",
		"'>>='",
		"'<<='",
		"'+='",
		"'-='",
		"'*='",
		"'/='",
		"'%='",
		"'&='",
		"'^='",
		"'|='",
		"'>>'",
		"'>>>'",
		"'<<'",
		"'++'",
		"'--'",
		"'&&'",
		"'||'",
		"'<='",
		"'>='",
		"'=='",
		"'!='",
		"'action'",
		"'break'",
		"'case'",
		"'const'",
		"'continue'",
		"'default'",
		"'do'",
		"'else'",
		"'for'",
		"'if'",
		"'return'",
		"'states'",
		"'switch'",
		"'until'",
		"'while'",
		"'bool'",
		"'float'",
		"'double'",
		"'char'",
		"'byte'",
		"'sbyte'",
		"'short'",
		"'ushort'",
		"'int'",
		"'uint'",
		"'long'",
		"'ulong'",
		"'void'",
		"'struct'",
		"'class'",
		"'mode'",
		"'enum'",
		"'name'",
		"'string'",
		"'sound'",
		"'state'",
		"'color'",
		"'goto'",
		"'abstract'",
		"'foreach'",
		"'true'",
		"'false'",
		"'none'",
		"'new'",
		"'instanceof'",
		"'auto'",
		"'exec'",
		"'defaultproperties'",
		"'native'",
		"'out'",
		"'ref'",
		"'event'",
		"'static'",
		"'transient'",
		"'volatile'",
		"'final'",
		"'throws'",
		"'extends'",
		"'public'",
		"'protected'",
		"'private'",
		"'dot'",
		"'cross'",
		"'ignores'",
		"'localized'",
		"'latent'",
		"'singular'",
		"'config'",
		"'coerce'",
		"'iterator'",
		"'optional'",
		"'export'",
		"'virtual'",
		"'super'",
		"'global'",
		"'self'",
		"'stop'",
		"'eval'",
		"'evalnot'",
		"'pickup'",
		"'breakable'",
		"'projectile'",
		"'#include'",
	};

	FString work;

	if (token > ' ' && token < 256)
	{
		work = '\'';
		work += token;
		work += '\'';
	}
	else if (token >= TK_Identifier && token < TK_LastToken)
	{
		work = names[token - TK_Identifier];
		if (string != NULL && token >= TK_Identifier && token <= TK_FloatConst)
		{
			work += ' ';
			char quote = (token == TK_StringConst) ? '"' : '\'';
			work += quote;
			work += string;
			work += quote;
		}
	}
	else
	{
		FString work;
		work.Format ("Unknown(%d)", token);
		return work;
	}
	return work;
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
		AlreadyGot? AlreadyGotLine : sc_Line, composed.GetChars());
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
	bool sc_End;
	bool sc_Crossed;
	bool sc_FileScripts;

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
		LastGotToken = false;
	}
}
