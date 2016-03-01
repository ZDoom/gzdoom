// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2000 Simon Howard
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// 'Special' stuff
//
// if(), int statements, etc.
//
// By Simon Howard
//
//---------------------------------------------------------------------------
//
// FraggleScript is from SMMU which is under the GPL. Technically, 
// therefore, combining the FraggleScript code with the non-free 
// ZDoom code is a violation of the GPL.
//
// As this may be a problem for you, I hereby grant an exception to my 
// copyright on the SMMU source (including FraggleScript). You may use 
// any code from SMMU in (G)ZDoom, provided that:
//
//    * For any binary release of the port, the source code is also made 
//      available.
//    * The copyright notice is kept on any file containing my code.
//
//

#include "t_script.h"

//==========================================================================
//
// ending brace found in parsing
//
//==========================================================================

void FParser::spec_brace()
{
	if(BraceType != bracket_close)  // only deal with closing } braces
		return;
	
	// if() requires nothing to be done
	if(Section->type == st_if || Section->type == st_else) 
		return;
	
	// if a loop, jump back to the start of the loop
	if(Section->type == st_loop)
    {
		Rover = Script->SectionLoop(Section);
		return;
    }
}

//==========================================================================
//
// 'if' statement -- haleyjd: changed to bool for else/elseif
//
//==========================================================================

bool FParser::spec_if()
{
	int endtoken;
	svalue_t eval;
	
	
	if((endtoken = FindOperator(0, NumTokens-1, ")")) == -1)
    {
		script_error("parse error in if statement\n");
		return false;
    }
	
	// 2 to skip past the 'if' and '('
	EvaluateExpression(eval, 2, endtoken-1);
	bool ifresult = !!intvalue(eval);
	
	if(Section && BraceType == bracket_open && endtoken == NumTokens-1)
    {
		// {} braces
		if(!ifresult)       // skip to end of section
			Rover = Script->SectionEnd(Section) + 1;
    }
	else if(ifresult) // if() without {} braces
	{
		// nothing to do ?
		if(endtoken != NumTokens-1)
			EvaluateExpression(eval, endtoken+1, NumTokens-1);
	}
	
	return ifresult;
}

//==========================================================================
//
// 'elseif' statement
//
//==========================================================================

bool FParser::spec_elseif(bool lastif)
{
	int endtoken;
	svalue_t eval;
	
	if((endtoken = FindOperator(0, NumTokens-1, ")")) == -1)
	{
		script_error("parse error in elseif statement\n");
		return false;
	}
	
	if(lastif)
	{
		Rover = Script->SectionEnd(Section) + 1;
		return true;
	}
	// 2 to skip past the 'elseif' and '('
	EvaluateExpression(eval, 2, endtoken-1);
	bool ifresult = !!intvalue(eval);
	
	if(Section && BraceType == bracket_open
		&& endtoken == NumTokens-1)
    {
		// {} braces
		if(!ifresult)       // skip to end of section
			Rover = Script->SectionEnd(Section) + 1;
    }
	else if(ifresult)    // elseif() without {} braces
	{
		// nothing to do ?
		if(endtoken != NumTokens-1)
			EvaluateExpression(eval, endtoken+1, NumTokens-1);
	}
	
	return ifresult;
}

//==========================================================================
//
// 'else' statement
//
//==========================================================================

void FParser::spec_else(bool lastif)
{
	if(lastif)
		Rover = Script->SectionEnd(Section) + 1;
}

//==========================================================================
//
// while() loop
//
//==========================================================================

void FParser::spec_while()
{
	int endtoken;
	svalue_t eval;
	
	if(!Section)
    {
		script_error("no {} section given for loop\n");
		return;
    }
	
	if( (endtoken = FindOperator(0, NumTokens-1, ")")) == -1)
    {
		script_error("parse error in loop statement\n");
		return;
    }
	
	EvaluateExpression(eval, 2, endtoken-1);
	
	// skip if no longer valid
	if(!intvalue(eval)) Rover = Script->SectionEnd(Section) + 1;
}

//==========================================================================
//
// for() loop
//
//==========================================================================

void FParser::spec_for()
{
	svalue_t eval;
	int start;
	int comma1, comma2;     // token numbers of the seperating commas
	
	if(!Section)
    {
		script_error("need {} delimiters for for()\n");
		return;
    }
	
	// is a valid section
	
	start = 2;     // skip "for" and "(": start on third token(2)
	
	// find the seperating commas first
	
	if( (comma1 = FindOperator(start, NumTokens-1, ",")) == -1
		|| (comma2 = FindOperator(comma1+1, NumTokens-1, ",")) == -1)
    {
		script_error("incorrect arguments to for()\n");  // haleyjd:
		return;                                          // said if()
    }
	
	// are we looping back from a previous loop?
	if(Section == PrevSection)
    {
		// do the loop 'action' (third argument)
		EvaluateExpression(eval, comma2+1, NumTokens-2);
		
		// check if we should run the loop again (second argument)
		EvaluateExpression(eval, comma1+1, comma2-1);
		if(!intvalue(eval))
		{
			// stop looping
			Rover = Script->SectionEnd(Section) + 1;
		}
    }
	else
    {
		// first time: starting the loop
		// just evaluate the starting expression (first arg)
		EvaluateExpression(eval, start, comma1-1);
    }
}

//==========================================================================
//
// Variable Creation 
//
// called for each individual variable in a statement
//
//==========================================================================

void FParser::CreateVariable(int newvar_type, DFsScript *newvar_script, int start, int stop)
{
	if(TokenType[start] != name_)
    {
		script_error("invalid name for variable: '%s'\n", Tokens[start]);
		return;
    }
	
	// check if already exists, only checking
	// the current script
	if(newvar_script->VariableForName (Tokens[start]))
	{
		// In Eternity this was fatal and in Legacy it was ignored
		// So make this a warning.
		Printf("FS: redefined symbol: '%s'\n", Tokens[start]);
		return;  // already one
	}
	
	// haleyjd: disallow mobj references in the hub script --
	// they cause dangerous dangling references and are of no
	// potential use
	if(newvar_script != Script && newvar_type == svt_mobj)
	{
		script_error("cannot create mobj reference in hub script\n");
		return;
	}
	
	newvar_script->NewVariable (Tokens[start], newvar_type);
	
	if(stop != start) 
	{
		svalue_t scratch;
		EvaluateExpression(scratch, start, stop);
	}
}

//==========================================================================
//
// divide a statement (without type prefix) into individual
// variables to create them using create_variable
//
//==========================================================================

void FParser::ParseVarLine(int newvar_type, DFsScript *newvar_script, int start)
{
	int starttoken = start, endtoken;
	
	while(1)
    {
		endtoken = FindOperator(starttoken, NumTokens-1, ",");
		if(endtoken == -1) break;
		CreateVariable(newvar_type, newvar_script, starttoken, endtoken-1);
		starttoken = endtoken+1;  //start next after end of this one
    }
	// dont forget the last one
	CreateVariable(newvar_type, newvar_script, starttoken, NumTokens-1);
}

//==========================================================================
//
// variable definition
//
//==========================================================================

bool FParser::spec_variable()
{
	int start = 0;
	
	int newvar_type = -1;                 // init to -1
	DFsScript *newvar_script = Script;   // use current script
	
	// check for 'hub' keyword to make a hub variable
	if(!strcmp(Tokens[start], "hub"))
	{
		// The hub script doesn't work so it's probably safest to store the variable locally.
		//newvar_script = &hub_script;
		start++;  // skip first token
	}
	
	// now find variable type
	if(!strcmp(Tokens[start], "const"))
	{
		newvar_type = svt_const;
		start++;
	}
	else if(!strcmp(Tokens[start], "string"))
	{
		newvar_type = svt_string;
		start++;
	}
	else if(!strcmp(Tokens[start], "int"))
	{
		newvar_type = svt_int;
		start++;
	}
	else if(!strcmp(Tokens[start], "mobj"))
	{
		newvar_type = svt_mobj;
		start++;
	}
	else if(!strcmp(Tokens[start], "fixed") || !strcmp(Tokens[start], "float"))
	{                                   
		newvar_type = svt_fixed;        
		start++;                        
	}
	else if(!strcmp(Tokens[start], "script"))     // check for script creation
	{
		spec_script();
		return true;       // used tokens
	}
	
	// are we creating a new variable?
	if(newvar_type != -1)
    {
		ParseVarLine(newvar_type, newvar_script, start);
		return true;       // used tokens
    }
	
	return false; // not used: try normal parsing
}


//==========================================================================
//
// ADD SCRIPT
//
// when the level is first loaded, all the
// scripts are simply stored in the levelscript.
// before the level starts, this script is
// preprocessed and run like any other. This allows
// the individual scripts to be derived from the
// levelscript. When the interpreter detects the
// 'script' keyword this function is called
//
//==========================================================================

void FParser::spec_script()
{
	int scriptnum;
	int datasize;
	DFsScript *newscript;
	
	scriptnum = 0;
	
	if(!Section)
    {
		script_error("need seperators for newscript\n");
		return;
    }
	
	// presume that the first token is "newscript"
	
	if(NumTokens < 2)
    {
		script_error("need newscript number\n");
		return;
    }
	
	svalue_t result;
	EvaluateExpression(result, 1, NumTokens-1);
	scriptnum = intvalue(result);
	
	if(scriptnum < 0)
    {
		script_error("invalid newscript number\n");
		return;
    }
	
	newscript = new DFsScript;
	
	// add to scripts list of parent
	Script->children[scriptnum] = newscript;
	GC::WriteBarrier(Script, newscript);
	
	// copy newscript data
	// workout newscript size: -2 to ignore { and }
	datasize = (Section->end_index - Section->start_index - 2);
	
	// alloc extra 10 for safety
	newscript->data = (char *)malloc(datasize+10);
	
	// copy from parent newscript (levelscript) 
	// ignore first char which is {
	memcpy(newscript->data, Script->SectionStart(Section) + 1, datasize);
	
	// tack on a 0 to end the string
	newscript->data[datasize] = '\0';
	
	newscript->scriptnum = scriptnum;
	newscript->parent = Script; // remember parent
	
	// preprocess the newscript now
	newscript->Preprocess();
    
	// we dont want to run the newscript, only add it
	// jump past the newscript in parsing
	
	Rover = Script->SectionEnd(Section) + 1;
}

//==========================================================================
//
// evaluate_function: once parse.c is pretty
//      sure it has a function to run it calls
//      this. evaluate_function makes sure that
//      it is a function call first, then evaluates all
//      the arguments given to the function.
//      these are built into an argc/argv-style
//      list. the function 'handler' is then called.
//
//==========================================================================

void FParser::EvaluateFunction(svalue_t &result, int start, int stop)
{
	DFsVariable *func = NULL;
	int startpoint, endpoint;
	
	// the arguments need to be built locally in case of
	// function returns as function arguments eg
	// print("here is a random number: ", rnd() );
	
	int argc;
	svalue_t argv[MAXARGS];
	
	if(TokenType[start] != function || TokenType[stop] != operator_
		|| Tokens[stop][0] != ')' )
	{
		script_error("misplaced closing paren\n");
	}
	
	// all the functions are stored in the global script
	else if( !(func = global_script->VariableForName (Tokens[start]))  )
	{
		script_error("no such function: '%s'\n",Tokens[start]);
	}
	
	else if(func->type != svt_function && func->type != svt_linespec)
	{
		script_error("'%s' not a function\n", Tokens[start]);
	}
	
	// build the argument list
	// use a C command-line style system rather than
	// a system using a fixed length list
	
	argc = 0;
	endpoint = start + 2;   // ignore the function name and first bracket
	
	while(endpoint < stop)
    {
		startpoint = endpoint;
		endpoint = FindOperator(startpoint, stop-1, ",");
		
		// check for -1: no more ','s 
		if(endpoint == -1)
		{               // evaluate the last expression
			endpoint = stop;
		}
		if(endpoint-1 < startpoint)
			break;
		
		EvaluateExpression(argv[argc], startpoint, endpoint-1);
		endpoint++;    // skip the ','
		argc++;
    }
	
	// store the arguments in the global arglist
	t_argc = argc;
	t_argv = argv;
	
	// haleyjd: return values can propagate to void functions, so
	// t_return needs to be cleared now
	
	t_return.type = svt_int;
	t_return.value.i = 0;
	
	// now run the function
	if (func->type == svt_function)
	{
		(this->*func->value.handler)();
	}
	else
	{
		RunLineSpecial(func->value.ls);
	}
	
	// return the returned value
	result = t_return;
}

//==========================================================================
//
// structure dot (.) operator
// there are not really any structs in FraggleScript, it's
// just a different way of calling a function that looks
// nicer. ie
//      a.b()  = a.b   =  b(a)
//      a.b(c) = b(a,c)
//
// this function is just based on the one above
//
//==========================================================================

void FParser::OPstructure(svalue_t &result, int start, int n, int stop)
{
	DFsVariable *func = NULL;
	
	// the arguments need to be built locally in case of
	// function returns as function arguments eg
	// print("here is a random number: ", rnd() );
	
	int argc;
	svalue_t argv[MAXARGS];
	
	// all the functions are stored in the global script
	if( !(func = global_script->VariableForName (Tokens[n+1]))  )
	{
		script_error("no such function: '%s'\n",Tokens[n+1]);
	}
	
	else if(func->type != svt_function)
	{
		script_error("'%s' not a function\n", Tokens[n+1]);
	}
	
	// build the argument list
	
	// add the left part as first arg
	
	EvaluateExpression(argv[0], start, n-1);
	argc = 1; // start on second argv
	
	if(stop != n+1)         // can be a.b not a.b()
    {
		int startpoint, endpoint;
		
		// ignore the function name and first bracket
		endpoint = n + 3;
		
		while(endpoint < stop)
		{
			startpoint = endpoint;
			endpoint = FindOperator(startpoint, stop-1, ",");
			
			// check for -1: no more ','s 
			if(endpoint == -1)
			{               // evaluate the last expression
				endpoint = stop;
			}
			if(endpoint-1 < startpoint)
				break;
			
			EvaluateExpression(argv[argc], startpoint, endpoint-1);
			endpoint++;    // skip the ','
			argc++;
		}
    }
	
	// store the arguments in the global arglist
	t_argc = argc;
	t_argv = argv;
	t_func = func->Name;
	
	// haleyjd: return values can propagate to void functions, so
	// t_return needs to be cleared now
	
	t_return.type = svt_int;
	t_return.value.i = 0;
	
	// now run the function
	(this->*func->value.handler)();
	
	// return the returned value
	result = t_return;
}


