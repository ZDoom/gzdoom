/*
** thingdef_states.cpp
**
** Actor definitions - the state parser
**
**---------------------------------------------------------------------------
** Copyright 2002-2007 Christoph Oelckers
** Copyright 2004-2007 Randy Heit
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
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
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

#include "actor.h"
#include "info.h"
#include "sc_man.h"
#include "tarray.h"
#include "templates.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "a_action.h"
#include "p_local.h"
#include "v_palette.h"
#include "doomerrors.h"
#include "thingdef.h"
#include "a_sharedglobal.h"
#include "s_sound.h"
#include "i_system.h"
#include "colormatcher.h"
#include "thingdef_exp.h"
#include "version.h"
#include "templates.h"

//==========================================================================
//***
// PrepareStateParameters
// creates an empty parameter list for a parameterized function call
//
//==========================================================================
int PrepareStateParameters(FState * state, int numparams, const PClass *cls)
{
	int paramindex=StateParams.Reserve(numparams, cls);
	state->ParameterIndex = paramindex+1;
	return paramindex;
}

//==========================================================================
//***
// DoActionSpecials
// handles action specials as code pointers
//
//==========================================================================
bool DoActionSpecials(FScanner &sc, FState & state, Baggage &bag)
{
	int i;
	int min_args, max_args;
	FString specname = sc.String;

	int special = P_FindLineSpecial(sc.String, &min_args, &max_args);

	if (special > 0 && min_args >= 0)
	{

		int paramindex=PrepareStateParameters(&state, 6, bag.Info->Class);

		StateParams.Set(paramindex, new FxConstant(special, sc));

		// Make this consistent with all other parameter parsing
		if (sc.CheckToken('('))
		{
			for (i = 0; i < 5;)
			{
				StateParams.Set(paramindex+i+1, ParseExpression (sc, bag.Info->Class));
				i++;
				if (!sc.CheckToken (',')) break;
			}
			sc.MustGetToken (')');
		}
		else i=0;

		if (i < min_args)
		{
			sc.ScriptError ("Too few arguments to %s", specname.GetChars());
		}
		if (i > max_args)
		{
			sc.ScriptError ("Too many arguments to %s", specname.GetChars());
		}

		state.SetAction(FindGlobalActionFunction("A_CallSpecial"), false);
		return true;
	}
	return false;
}

//==========================================================================
//***
// Reads a state label that may contain '.'s.
// processes a state block
//
//==========================================================================
static FString ParseStateString(FScanner &sc)
{
	FString statestring;

	sc.MustGetString();
	statestring = sc.String;
	if (sc.CheckString("::"))
	{
		sc.MustGetString ();
		statestring << "::" << sc.String;
	}
	while (sc.CheckString ("."))
	{
		sc.MustGetString ();
		statestring << "." << sc.String;
	}
	return statestring;
}

//==========================================================================
//***
// ParseStates
// parses a state block
//
//==========================================================================
void ParseStates(FScanner &sc, FActorInfo * actor, AActor * defaults, Baggage &bag)
{
	FString statestring;
	FState state;
	char lastsprite[5]="";

	sc.MustGetStringName ("{");
	sc.SetEscape(false);	// disable escape sequences in the state parser
	while (!sc.CheckString ("}") && !sc.End)
	{
		memset(&state,0,sizeof(state));
		statestring = ParseStateString(sc);
		if (!statestring.CompareNoCase("GOTO"))
		{
do_goto:	
			statestring = ParseStateString(sc);
			if (sc.CheckString ("+"))
			{
				sc.MustGetNumber ();
				statestring += '+';
				statestring += sc.String;
			}
			if (!bag.statedef.SetGotoLabel(statestring))
			{
				sc.ScriptError("GOTO before first state");
			}
		}
		else if (!statestring.CompareNoCase("STOP"))
		{
do_stop:
			if (!bag.statedef.SetStop())
			{
				sc.ScriptError("STOP before first state");
				continue;
			}
		}
		else if (!statestring.CompareNoCase("WAIT") || !statestring.CompareNoCase("FAIL"))
		{
			if (!bag.statedef.SetWait())
			{
				sc.ScriptError("%s before first state", sc.String);
				continue;
			}
		}
		else if (!statestring.CompareNoCase("LOOP"))
		{
			if (!bag.statedef.SetLoop())
			{
				sc.ScriptError("LOOP before first state");
				continue;
			}
		}
		else
		{
			sc.MustGetString();
			if (sc.Compare (":"))
			{
				do
				{
					bag.statedef.AddStateLabel(statestring);
					statestring = ParseStateString(sc);
					if (!statestring.CompareNoCase("GOTO"))
					{
						goto do_goto;
					}
					else if (!statestring.CompareNoCase("STOP"))
					{
						goto do_stop;
					}
					sc.MustGetString ();
				} while (sc.Compare (":"));
//				continue;
			}

			sc.UnGet ();

			if (statestring.Len() != 4)
			{
				sc.ScriptError ("Sprite names must be exactly 4 characters\n");
			}

			state.sprite = GetSpriteIndex(statestring);
			state.Misc1 = state.Misc2 = 0;
			state.ParameterIndex = 0;
			sc.MustGetString();
			statestring = sc.String;

			if (sc.CheckString("RANDOM"))
			{
				int min, max;

				sc.MustGetStringName("(");
				sc.MustGetNumber();
				min = clamp<int>(sc.Number, -1, SHRT_MAX);
				sc.MustGetStringName(",");
				sc.MustGetNumber();
				max = clamp<int>(sc.Number, -1, SHRT_MAX);
				sc.MustGetStringName(")");
				if (min > max)
				{
					swapvalues(min, max);
				}
				state.Tics = min;
				state.TicRange = max - min;
			}
			else
			{
				sc.MustGetNumber();
				state.Tics = clamp<int>(sc.Number, -1, SHRT_MAX);
				state.TicRange = 0;
			}

			while (sc.GetString() && !sc.Crossed)
			{
				if (sc.Compare("BRIGHT")) 
				{
					state.Fullbright = true;
					continue;
				}
				if (sc.Compare("FAST")) 
				{
					state.Fast = true;
					continue;
				}
				if (sc.Compare("SLOW")) 
				{
					state.Slow = true;
					continue;
				}
				if (sc.Compare("NODELAY"))
				{
					if (bag.statedef.GetStateLabelIndex(NAME_Spawn) == bag.statedef.GetStateCount())
					{
						state.NoDelay = true;
					}
					else
					{
						sc.ScriptMessage("NODELAY may only be used immediately after Spawn:");
					}
					continue;
				}
				if (sc.Compare("OFFSET"))
				{
					// specify a weapon offset
					sc.MustGetStringName("(");
					sc.MustGetNumber();
					state.Misc1 = sc.Number;
					sc.MustGetStringName (",");
					sc.MustGetNumber();
					state.Misc2 = sc.Number;
					sc.MustGetStringName(")");
					continue;
				}
				if (sc.Compare("LIGHT"))
				{
					sc.MustGetStringName("(");
					do
					{
						sc.MustGetString();
						#ifdef DYNLIGHT
							AddStateLight(&state, sc.String);
						#endif
					}
					while (sc.CheckString(","));
					sc.MustGetStringName(")");
					continue;
				}
				if (sc.Compare("CANRAISE"))
				{
					state.CanRaise = true;
					continue;
				}

				// Make the action name lowercase
				strlwr (sc.String);

				if (DoActionSpecials(sc, state, bag))
				{
					goto endofstate;
				}

				FName funcname = FName(sc.String, true);
				PSymbol *sym = bag.Info->Class->Symbols.FindSymbol (funcname, true);
				if (sym != NULL && sym->SymbolType == SYM_ActionFunction)
				{
					PSymbolActionFunction *afd = static_cast<PSymbolActionFunction *>(sym);
					state.SetAction(afd, false);
					if (!afd->Arguments.IsEmpty())
					{
						const char *params = afd->Arguments.GetChars();
						int numparams = (int)afd->Arguments.Len();
				
						int v;

						if (!islower(*params))
						{
							sc.MustGetStringName("(");
						}
						else
						{
							if (!sc.CheckString("(")) 
							{
								state.ParameterIndex = afd->defaultparameterindex+1;
								goto endofstate;
							}
						}
						
						int paramindex = PrepareStateParameters(&state, numparams, bag.Info->Class);
						int paramstart = paramindex;
						bool varargs = params[numparams - 1] == '+';
						int varargcount = 0;

						if (varargs)
						{
							paramindex++;
						}
						else if (afd->defaultparameterindex > -1)
						{
							StateParams.Copy(paramindex, afd->defaultparameterindex, int(afd->Arguments.Len()));
						}

						while (*params)
						{
							FxExpression *x;
							if ((*params == 'l' || *params == 'L') && sc.CheckNumber())
							{
								// Special case: State label as an offset
								if (sc.Number > 0 && statestring.Len() > 1)
								{
									sc.ScriptError("You cannot use state jumps commands with a jump offset on multistate definitions\n");
								}

								v=sc.Number;
								if (v<0)
								{
									sc.ScriptError("Negative jump offsets are not allowed");
								}

								if (v > 0)
								{
									x = new FxStateByIndex(bag.statedef.GetStateCount() + v, sc);
								}
								else
								{
									x = new FxConstant((FState*)NULL, sc);
								}
							}
							else
							{
								// Use the generic parameter parser for everything else
								x = ParseParameter(sc, bag.Info->Class, *params, false);
							}
							StateParams.Set(paramindex++, x);
							params++;
							if (varargs)
							{
								varargcount++;
							}
							if (*params)
							{
								if (*params == '+')
								{
									if (sc.CheckString(")"))
									{
										StateParams.Set(paramstart, new FxConstant(varargcount, sc));
										goto endofstate;
									}
									params--;
									StateParams.Reserve(1, bag.Info->Class);
								}
								else if ((islower(*params) || *params=='!') && sc.CheckString(")"))
								{
									goto endofstate;
								}
								sc.MustGetStringName (",");
							}
						}
						sc.MustGetStringName(")");
					}
					else 
					{
						sc.MustGetString();
						if (sc.Compare("("))
						{
							sc.ScriptError("You cannot pass parameters to '%s'\n", funcname.GetChars());
						}
						sc.UnGet();
					}
					goto endofstate;
				}
				sc.ScriptError("Invalid state parameter %s\n", sc.String);
			}
			sc.UnGet();
endofstate:
			if (!bag.statedef.AddStates(&state, statestring))
			{
				sc.ScriptError ("Invalid frame character string '%s'", statestring.GetChars());
			}
		}
	}
	sc.SetEscape(true);	// re-enable escape sequences
}

