/*
** decorations.cpp
** Loads custom actors out of DECORATE lumps.
**
**---------------------------------------------------------------------------
** Copyright 2002-2007 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include "actor.h"
#include "info.h"
#include "sc_man.h"
#include "tarray.h"
#include "w_wad.h"
#include "templates.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "thingdef.h"
#include "vectors.h"

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void ParseActor();
void ParseClass();
void ParseGlobalConst();
void ParseGlobalEnum();
void FinishThingdef();
void ParseOldDecoration(EDefinitionType def);

// STATIC FUNCTION PROTOTYPES --------------------------------------------

//==========================================================================
//
// ParseDecorate
//
// Parses a single DECORATE lump
//
//==========================================================================

static void ParseDecorate ()
{
	int recursion=0;
	int lump;

	// Get actor class name.
	while (true)
	{
		SC_SavePos();
		if (!SC_GetToken ())
		{
			if (recursion==0) return;
			SC_Close();
			SC_RestoreScriptState();
			recursion--;
			continue;
		}
		switch (sc_TokenType)
		{
		case TK_Include:
			SC_MustGetString();
			// This is not using SC_Open because it can print a more useful error message when done here
			lump = Wads.CheckNumForFullName(sc_String);

			// Try a normal WAD name lookup only if it's a proper name without path separator and
			// not longer than 8 characters.
			if (lump==-1 && strlen(sc_String) <= 8 && !strchr(sc_String, '/')) 
				lump = Wads.CheckNumForName(sc_String);

			if (lump==-1) 
				SC_ScriptError("Lump '%s' not found", sc_String);

			SC_SaveScriptState();
			SC_OpenLumpNum(lump, sc_String);
			recursion++;
			break;

		case TK_Class:
			ParseClass ();
			break;

		case TK_Const:
			ParseConstant (&RUNTIME_CLASS(AActor)->Symbols, RUNTIME_CLASS(AActor));
			break;

		case TK_Enum:
			ParseEnum (&RUNTIME_CLASS(AActor)->Symbols, RUNTIME_CLASS(AActor));
			break;

		case TK_Pickup:
			ParseOldDecoration (DEF_Pickup);
			break;

		case TK_Breakable:
			ParseOldDecoration (DEF_BreakableDecoration);
			break;

		case TK_Projectile:
			ParseOldDecoration (DEF_Projectile);
			break;

		case ';':
			// ';' is the start of a comment in the non-cmode parser which
			// is used to parse parts of the DECORATE lump. If we don't add 
			// a check here the user will only get weird non-informative
			// error messages if a semicolon is found.
			SC_ScriptError("Unexpected ';'");
			break;

		case TK_Identifier:
			// 'ACTOR' cannot be a keyword because it is also needed as a class identifier
			// so let's do a special case for this.
			if (SC_Compare("ACTOR"))
			{
				ParseActor ();
				break;
			}

		default:
			// Yuck! Too bad that there's no better way to check this properly
			SC_RestorePos();
			ParseOldDecoration(DEF_Decoration);
			break;
		}
	}
}

//==========================================================================
//
// LoadDecorations
//
// Called from FActor::StaticInit()
//
//==========================================================================

void LoadDecorations ()
{
	int lastlump, lump;

	lastlump = 0;
	while ((lump = Wads.FindLump ("DECORATE", &lastlump)) != -1)
	{
		SC_OpenLumpNum (lump, Wads.GetLumpFullName(lump));
		ParseDecorate ();
		SC_Close ();
	}
	FinishThingdef();
}

