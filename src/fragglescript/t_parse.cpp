// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2000 Simon Howard
// Copyright(C) 2002-2008 Christoph Oelckers
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
// Parsing.
//
// Takes lines of code, or groups of lines and runs them.
// The main core of FraggleScript
//
// By Simon Howard
//
//---------------------------------------------------------------------------
//

/* includes ************************/
#include <stdarg.h>
#include "t_script.h"
#include "v_text.h"
#include "g_levellocals.h"

CVAR(Bool, script_debug, false, 0)

/************ Divide into tokens **************/
#define isnum(c) ( ((c)>='0' && (c)<='9') || (c)=='.')

//==========================================================================
//
// NextToken: end this token, go onto the next
//
//==========================================================================

void FParser::NextToken()
{
	if(Tokens[NumTokens-1][0] || TokenType[NumTokens-1]==string_)
    {
		NumTokens++;
		Tokens[NumTokens-1] = Tokens[NumTokens-2] + strlen(Tokens[NumTokens-2]) + 1;
		Tokens[NumTokens-1][0] = 0;
    }
	
	// get to the next token, ignoring spaces, newlines,
	// useless chars, comments etc
	
	while(1)
    {
		// empty whitespace
		if(*Rover && (*Rover==' ' || *Rover<32))
		{
			while((*Rover==' ' || *Rover<32) && *Rover) Rover++;
		}
		// end-of-script?
		if(!*Rover)
		{
			if(Tokens[0][0])
			{
				// line contains text, but no semicolon: an error
				script_error("missing ';'\n");
			}
			// empty line, end of command-list
			return;
		}
		break;  // otherwise
    }
	
	if(NumTokens>1 && *Rover == '(' && TokenType[NumTokens-2] == name_)
		TokenType[NumTokens-2] = function;
	
	if(*Rover == '{' || *Rover == '}')
    {
		if(*Rover == '{')
		{
			BraceType = bracket_open;
			Section = Script->FindSectionStart(Rover);
		}
		else            // closing brace
		{
			BraceType = bracket_close;
			Section = Script->FindSectionEnd(Rover);
		}
		if(!Section)
		{
			script_error("section not found!\n");
			return;
		}
    }
	else if(*Rover == ':')  // label
    {
		// ignore the label : reset
		NumTokens = 1;
		Tokens[0][0] = 0; TokenType[NumTokens-1] = name_;
		Rover++;        // ignore
    }
	else if(*Rover == '\"')
    {
		TokenType[NumTokens-1] = string_;
		if(TokenType[NumTokens-2] == string_) NumTokens--;   // join strings
		Rover++;
    }
	else
    {
		TokenType[NumTokens-1] = isop(*Rover) ? operator_ : isnum(*Rover) ? number : name_;
    }
}

//==========================================================================
//
// return an escape sequence (prefixed by a '\')
// do not use all C escape sequences
//
//==========================================================================

static char escape_sequence(char c)
{
	if(c == 'n') return '\n';
	if(c == '\\') return '\\';
	if(c == '"') return '"';
	if(c == '?') return '?';
	if(c == 'a') return '\a'; // alert beep
	if(c == 't') return '\t'; //tab
	
	return c;
}

//==========================================================================
//
// add_char: add one character to the current token
//
//==========================================================================

static void add_char(char *tokn, char c)
{
	char *out = tokn + strlen(tokn);
	
	out[0] = c;
	out[1] = 0;
}

//==========================================================================
//
// get_tokens.
// Take a string, break it into tokens.
//
// individual tokens are stored inside the tokens[] array
// tokentype is also used to hold the type for each token:
//
//   name: a piece of text which starts with an alphabet letter.
//         probably a variable name. Some are converted into
//         function types later on in find_brackets
//   number: a number. like '12' or '1337'
//   operator: an operator such as '&&' or '+'. All FraggleScript
//             operators are either one character, or two character
//             (if 2 character, 2 of the same char or ending in '=')
//   string: a text string that was enclosed in quote "" marks in
//           the original text
//   unset: shouldn't ever end up being set really.
//   function: a function name (found in second stage parsing)
//
//==========================================================================

char *FParser::GetTokens(char *s)
{
	char *tokn = NULL;

	Rover = s;
	NumTokens = 1;
	Tokens[0][0] = 0; TokenType[NumTokens-1] = name_;
	
	Section = NULL;   // default to no section found
	
	NextToken();
	LineStart = Rover;      // save the start
	
	if(*Rover)
	{
		while(1)
		{
			tokn = Tokens[NumTokens-1];
			if(Section)
			{
				// a { or } section brace has been found
				break;        // stop parsing now
			}
			else if(TokenType[NumTokens-1] != string_)
			{
				if(*Rover == ';') break;     // check for end of command ';'
			}
			
			switch(TokenType[NumTokens-1])
			{
			case unset:
			case string_:
				while(*Rover != '\"')     // dedicated loop for speed
				{
					if(*Rover == '\\')       // escape sequences
					{
						Rover++;
						if (*Rover>='0' && *Rover<='9')
						{
							add_char(tokn, TEXTCOLOR_ESCAPE);
							add_char(tokn, *Rover+'A'-'0');
						}
						else add_char(tokn, escape_sequence(*Rover));
					}
					else
						add_char(tokn, *Rover);
					Rover++;
				}
				Rover++;
				NextToken();       // end of this token
				continue;
				
			case operator_:
				// all 2-character operators either end in '=' or
				// are 2 of the same character
				// do not allow 2-characters for brackets '(' ')'
				// which are still being considered as operators
				
				// operators are only 2-char max, do not need
				// a seperate loop
				
				if((*tokn && *Rover != '=' && *Rover!=*tokn) ||
					*tokn == '(' || *tokn == ')')
				{
					// end of operator
					NextToken();
					continue;
				}
				add_char(tokn, *Rover);
				break;
				
			case number:
				
				// haleyjd: 8-17
				// add while number chars are read
				
				while(isnum(*Rover))       // dedicated loop
					add_char(tokn, *Rover++);
				NextToken();
				continue;
				
			case name_:
				
				// add the chars
				
				while(!isop(*Rover))        // dedicated loop
					add_char(tokn, *Rover++);
				NextToken();
				continue;
				
			default:
				break;
			}
			Rover++;
		}
	}
		
	// check for empty last token
	
	if(!tokn || !tokn[0])
	{
		NumTokens = NumTokens - 1;
	}
	
	Rover++;
	return Rover;
}


//==========================================================================
//
// PrintTokens: add one character to the current token
//
//==========================================================================

void FParser::PrintTokens()	// DEBUG
{
	int i;
	for (i = 0; i < NumTokens; i++)
	{
		Printf("\n'%s' \t\t --", Tokens[i]);
		switch (TokenType[i])
		{
		case string_:
			Printf("string");
			break;
		case operator_:
			Printf("operator");
			break;
		case name_:
			Printf("name");
			break;
		case number:
			Printf("number");
			break;
		case unset:
			Printf("duh");
			break;
		case function:
			Printf("function name");
			break;
		}
	}
	Printf("\n");
	if (Section)
		Printf("current section: offset %i\n", Section->start_index);
}


//==========================================================================
//
// Parses a block of script code
//
//==========================================================================

void FParser::Run(char *rover, char *data, char *end)
{
	Rover = rover;
	try
	{
		PrevSection = NULL;  // clear it
		
		while(*Rover)   // go through the script executing each statement
		{
			// past end of script?
			if(Rover > end)
				break;
			
			PrevSection = Section; // store from prev. statement
			
			// get the line and tokens
			GetTokens(Rover);
			
			if(!NumTokens)
			{
				if(Section)       // no tokens but a brace
				{
					// possible } at end of loop:
					// refer to spec.c
					spec_brace();
				}
				
				continue;  // continue to next statement
			}
			
			if(script_debug) PrintTokens();   // debug
			RunStatement();         // run the statement
		}
	}
	catch (const CFsError &err)
	{
		ErrorMessage(err.msg);
	}
	catch (const CFsTerminator &)
	{
		// The script has signalled that it wants to be terminated in an orderly fashion.
	}
}

//==========================================================================
//
// decide what to do with it
//
// NB this stuff is a bit hardcoded:
//    it could be nicer really but i'm
//    aiming for speed
//
// if() and while() will be mistaken for functions
// during token processing
//
//==========================================================================

void FParser::RunStatement()
{
	if(TokenType[0] == function)
    {
		if(!strcmp(Tokens[0], "if"))
		{
			Script->lastiftrue = spec_if();
			return;
		}
		else if(!strcmp(Tokens[0], "elseif"))
		{
			if(!PrevSection || 
				(PrevSection->type != st_if && 
				PrevSection->type != st_elseif))
			{
				script_error("elseif statement without if\n");
				return;
			}
			Script->lastiftrue = spec_elseif(Script->lastiftrue);
			return;
		}
		else if(!strcmp(Tokens[0], "else"))
		{
			if(!PrevSection ||
				(PrevSection->type != st_if &&
				PrevSection->type != st_elseif))
			{
				script_error("else statement without if\n");
				return;
			}
			spec_else(Script->lastiftrue);
			Script->lastiftrue = true;
			return;
		}
		else if(!strcmp(Tokens[0], "while"))
		{
			spec_while();
			return;
		}
		else if(!strcmp(Tokens[0], "for"))
		{
			spec_for();
			return;
		}
    }
	else if(TokenType[0] == name_)
    {
		// NB: goto is a function so is not here

		// Allow else without '()'
        if (!strcmp(Tokens[0], "else"))
        {
			if(!PrevSection ||
				(PrevSection->type != st_if &&
				PrevSection->type != st_elseif))
			{
				script_error("else statement without if\n");
				return;
			}
			spec_else(Script->lastiftrue);
			Script->lastiftrue = true;
			return;
        }

		// if a variable declaration, return now
		if(spec_variable()) return;
    }
	
	// just a plain expression
	svalue_t scratch;
	EvaluateExpression(scratch,0, NumTokens-1);
}

/***************** Evaluating Expressions ************************/

//==========================================================================
//
// find a token, ignoring things in brackets        
//
//==========================================================================

int FParser::FindOperator(int start, int stop, const char *value)
{
	int i;
	int bracketlevel = 0;
	
	for(i=start; i<=stop; i++)
    {
		// only interested in operators
		if(TokenType[i] != operator_) continue;
		
		// use bracketlevel to check the number of brackets
		// which we are inside
		bracketlevel += Tokens[i][0]=='(' ? 1 :
		Tokens[i][0]==')' ? -1 : 0;
		
		// only check when we are not in brackets
		if(!bracketlevel && !strcmp(value, Tokens[i]))
			return i;
    }
	
	return -1;
}

//==========================================================================
//
// go through tokens the same as find_operator, but backwards
//
//==========================================================================

int FParser::FindOperatorBackwards(int start, int stop, const char *value)
{
	int i;
	int bracketlevel = 0;
	
	for(i=stop; i>=start; i--)      // check backwards
    {
		// operators only
		
		if(TokenType[i] != operator_) continue;
		
		// use bracketlevel to check the number of brackets
		// which we are inside
		
		bracketlevel += Tokens[i][0]=='(' ? -1 :
		Tokens[i][0]==')' ? 1 : 0;
		
		// only check when we are not in brackets
		// if we find what we want, return it
		
		if(!bracketlevel && !strcmp(value, Tokens[i]))
			return i;
    }
	
	return -1;
}

//==========================================================================
//
// simple_evaluate is used once evalute_expression gets to the level
// where it is evaluating just one token
//
// converts number tokens into svalue_ts and returns
// the same with string tokens
// name tokens are considered to be variables and
// attempts are made to find the value of that variable
// command tokens are executed (does not return a svalue_t)
//
//==========================================================================

void FParser::SimpleEvaluate(svalue_t &returnvar, int n)
{
	DFsVariable *var;
	
	switch(TokenType[n])
    {
    case string_: 
		returnvar.type = svt_string;
		returnvar.string = Tokens[n];
		break;
		
    case number:
		if(strchr(Tokens[n], '.'))
		{
			returnvar.setDouble(atof(Tokens[n]));
		}
		else
		{
			returnvar.type = svt_int;
			returnvar.value.i = atoi(Tokens[n]);
		}
		break;
		
    case name_:   
		var = Script->FindVariable(Tokens[n], Level->FraggleScriptThinker->GlobalScript);
		if(!var)
		{
			script_error("unknown variable '%s'\n", Tokens[n]);
		}
		else var->GetValue(returnvar);
		
    default: 
		break;
    }
}

//==========================================================================
//
// pointless_brackets checks to see if there are brackets surrounding
// an expression. eg. "(2+4)" is the same as just "2+4"
//
// because of the recursive nature of evaluate_expression, this function is
// neccesary as evaluating expressions such as "2*(2+4)" will inevitably
// lead to evaluating "(2+4)"
//
//==========================================================================

void FParser::PointlessBrackets(int *start, int *stop)
{
	int bracket_level, i;
	
	// check that the start and end are brackets
	
	while(Tokens[*start][0] == '(' && Tokens[*stop][0] == ')')
    {
		
		bracket_level = 0;
		
		// confirm there are pointless brackets..
		// if they are, bracket_level will only get to 0
		// at the last token
		// check up to <*stop rather than <=*stop to ignore
		// the last token
		
		for(i = *start; i<*stop; i++)
		{
			if(TokenType[i] != operator_) continue; // ops only
			bracket_level += (Tokens[i][0] == '(');
			bracket_level -= (Tokens[i][0] == ')');
			if(bracket_level == 0) return; // stop if braces stop before end
		}
		
		// move both brackets in
		
		*start = *start + 1;
		*stop = *stop - 1;
    }
}

//==========================================================================
//
// evaluate_expresion is the basic function used to evaluate
// a FraggleScript expression.
// start and stop denote the tokens which are to be evaluated.
//
// works by recursion: it finds operators in the expression
// (checking for each in turn), then splits the expression into
// 2 parts, left and right of the operator found.
// The handler function for that particular operator is then
// called, which in turn calls evaluate_expression again to
// evaluate each side. When it reaches the level of being asked
// to evaluate just 1 token, it calls simple_evaluate
//
//==========================================================================

void FParser::EvaluateExpression(svalue_t &result, int start, int stop)
{
	int i, n;
	
	// possible pointless brackets
	if(TokenType[start] == operator_ && TokenType[stop] == operator_)
		PointlessBrackets(&start, &stop);

	if(start == stop)       // only 1 thing to evaluate
    {
		SimpleEvaluate(result, start);
		return;
    }
	
	// go through each operator in order of precedence
	for(i=0; i<num_operators; i++)
    {
		// check backwards for the token. it has to be
		// done backwards for left-to-right reading: eg so
		// 5-3-2 is (5-3)-2 not 5-(3-2)
		
		if (operators[i].direction==forward)
		{
			n = FindOperatorBackwards(start, stop, operators[i].string);
		}
		else
		{
			n = FindOperator(start, stop, operators[i].string);
		}

		if( n != -1)
		{
			// call the operator function and evaluate this chunk of tokens
			(this->*operators[i].handler)(result, start, n, stop);
			return;
		}
    }
	
	if(TokenType[start] == function)
	{
		EvaluateFunction(result, start, stop);
		return;
	}
	
	// error ?
	{        
		FString tempstr;
		
		for(i=start; i<=stop; i++) tempstr << Tokens[i] << ' ';
		script_error("couldnt evaluate expression: %s\n",tempstr.GetChars());
	}
}


//==========================================================================
//
// intercepts an error message and inserts script/line information
//
//==========================================================================

void FS_Error(const char *error, ...)
{
	va_list argptr;
	char errortext[MAX_ERRORTEXT];

	va_start(argptr, error);
	myvsnprintf(errortext, MAX_ERRORTEXT, error, argptr);
	va_end(argptr);
	throw CFraggleScriptError(errortext);
}


void FParser::ErrorMessage(FString msg)
{
	int linenum = 0;

	// find the line number
	if(Rover >= Script->data && Rover <= Script->data+Script->len)
    {
		char *temp;
		for(temp = Script->data; temp<LineStart; temp++)
			if(*temp == '\n') linenum++;    // count EOLs
    }

	//lineinfo.Format("Script %d, line %d: ", Script->scriptnum, linenum);
	FS_Error("Script %d, line %d: %s", Script->scriptnum, linenum, msg.GetChars());
}

//==========================================================================
//
// throws an error message
//
//==========================================================================

void script_error(const char *s, ...)
{
	FString composed;
	va_list args;
	va_start(args, s);
	composed.VFormat(s, args);
	throw CFsError(composed);
}




// EOF
