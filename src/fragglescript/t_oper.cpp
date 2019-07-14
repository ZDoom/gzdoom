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
// Operators
//
// Handler code for all the operators. The 'other half'
// of the parsing.
//
// By Simon Howard
//
//---------------------------------------------------------------------------
//

/* includes ************************/
#include "t_script.h"
#include "g_levellocals.h"


#define evaluate_leftnright(a, b, c) {\
	EvaluateExpression(left, (a), (b)-1); \
	EvaluateExpression(right, (b)+1, (c)); }\
	

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FParser::operator_t FParser::operators[]=
{
	{"=",   &FParser::OPequals,               backward},
	{"||",  &FParser::OPor,                   forward},
	{"&&",  &FParser::OPand,                  forward},
	{"|",   &FParser::OPor_bin,               forward},
	{"&",   &FParser::OPand_bin,              forward},
	{"==",  &FParser::OPcmp,                  forward},
	{"!=",  &FParser::OPnotcmp,               forward},
	{"<",   &FParser::OPlessthan,             forward},
	{">",   &FParser::OPgreaterthan,          forward},
	{"<=",  &FParser::OPlessthanorequal,      forward},
	{">=",  &FParser::OPgreaterthanorequal,   forward},
	
	{"+",   &FParser::OPplus,                 forward},
	{"-",   &FParser::OPminus,                forward},
	{"*",   &FParser::OPmultiply,             forward},
	{"/",   &FParser::OPdivide,               forward},
	{"%",   &FParser::OPremainder,            forward},
	{"~",   &FParser::OPnot_bin,              forward}, // haleyjd
	{"!",   &FParser::OPnot,                  forward},
	{"++",  &FParser::OPincrement,            forward},
	{"--",  &FParser::OPdecrement,            forward},
	{".",   &FParser::OPstructure,            forward},
};

int FParser::num_operators = sizeof(FParser::operators) / sizeof(FParser::operator_t);

/***************** logic *********************/

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPequals(svalue_t &result, int start, int n, int stop)
{
	DFsVariable *var;
	
	var = Script->FindVariable(Tokens[start], Level->FraggleScriptThinker->GlobalScript);
	
	if(var)
    {
		EvaluateExpression(result, n+1, stop);
		var->SetValue(Level, result);
    }
	else
    {
		script_error("unknown variable '%s'\n", Tokens[start]);
    }
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPor(svalue_t &result, int start, int n, int stop)
{
	int exprtrue = false;
	
	// if first is true, do not evaluate the second
	
	EvaluateExpression(result, start, n-1);
	
	if(intvalue(result))
		exprtrue = true;
	else
    {
		EvaluateExpression(result, n+1, stop);
		exprtrue = !!intvalue(result);
    }
	
	result.type = svt_int;
	result.value.i = exprtrue;
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPand(svalue_t &result, int start, int n, int stop)
{
	int exprtrue = true;
	// if first is false, do not eval second
	
	EvaluateExpression(result, start, n-1);
	
	if(!intvalue(result) )
		exprtrue = false;
	else
    {
		EvaluateExpression(result, n+1, stop);
		exprtrue = !!intvalue(result);
    }
	
	result.type = svt_int;
	result.value.i = exprtrue;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPcmp(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	
	evaluate_leftnright(start, n, stop);
	
	result.type = svt_int;        // always an int returned
	
	if(left.type == svt_string && right.type == svt_string)
	{
		result.value.i = !strcmp(left.string, right.string);
		return;
	}
	
	// haleyjd: direct mobj comparison when both are mobj
	if(left.type == svt_mobj && right.type == svt_mobj)
	{
		// we can safely assume reference equivalency for
		// AActor's in all cases since they are static for the
		// duration of a level
		result.value.i = (left.value.mobj == right.value.mobj);
		return;
	}
	
	if(left.type == svt_fixed || right.type == svt_fixed)
	{
		result.value.i = (fixedvalue(left) == fixedvalue(right));
		return;
	}
	
	result.value.i = (intvalue(left) == intvalue(right));
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPnotcmp(svalue_t &result, int start, int n, int stop)
{
	OPcmp(result, start, n, stop);
	result.type = svt_int;
	result.value.i = !result.value.i;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPlessthan(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	
	evaluate_leftnright(start, n, stop);
	result.type = svt_int;
	
	// haleyjd: 8-17
	if(left.type == svt_fixed || right.type == svt_fixed)
		result.value.i = (fixedvalue(left) < fixedvalue(right));
	else
		result.value.i = (intvalue(left) < intvalue(right));
	
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPgreaterthan(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	
	evaluate_leftnright(start, n, stop);
	
	// haleyjd: 8-17
	result.type = svt_int;
	if(left.type == svt_fixed || right.type == svt_fixed)
		result.value.i = (fixedvalue(left) > fixedvalue(right));
	else
		result.value.i = (intvalue(left) > intvalue(right));
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPnot(svalue_t &result, int start, int n, int stop)
{
	EvaluateExpression(result, n+1, stop);
	
	result.value.i = !intvalue(result);
	result.type = svt_int;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPplus(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	
	evaluate_leftnright(start, n, stop);
	
  	if (left.type == svt_string)
    {
      	if (right.type == svt_string)
		{
			result.string.Format("%s%s", left.string.GetChars(), right.string.GetChars());
		}
      	else if (right.type == svt_fixed)
		{
			result.string.Format("%s%4.4f", left.string.GetChars(), floatvalue(right));
		}
      	else
		{
	  		result.string.Format("%s%i", left.string.GetChars(), intvalue(right));
		}
      	result.type = svt_string;
    }
	// haleyjd: 8-17
	else if(left.type == svt_fixed || right.type == svt_fixed)
	{
		result.type = svt_fixed;
		result.value.f = fixedvalue(left) + fixedvalue(right);
	}
	else
	{
		result.type = svt_int;
		result.value.i = intvalue(left) + intvalue(right);
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPminus(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	
	// do they mean minus as in '-1' rather than '2-1'?
	if(start == n)
	{
		// kinda hack, hehe
		EvaluateExpression(right, n+1, stop);
	}
	else
	{
		evaluate_leftnright(start, n, stop);
	}
	
	// haleyjd: 8-17
	if(left.type == svt_fixed || right.type == svt_fixed)
	{
		result.type = svt_fixed;
		result.value.f = fixedvalue(left) - fixedvalue(right);
	}
	else
	{
		result.type = svt_int;
		result.value.i = intvalue(left) - intvalue(right);
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPmultiply(svalue_t &result,int start, int n, int stop)
{
	svalue_t left, right;
	
	evaluate_leftnright(start, n, stop);
	
	// haleyjd: 8-17
	if(left.type == svt_fixed || right.type == svt_fixed)
	{
		result.setDouble(floatvalue(left) * floatvalue(right));
	}
	else
	{
		result.type = svt_int;
		result.value.i = intvalue(left) * intvalue(right);
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPdivide(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	
	evaluate_leftnright(start, n, stop);
	
	// haleyjd: 8-17
	if(left.type == svt_fixed || right.type == svt_fixed)
	{
		auto fr = floatvalue(right);
		
		if(fr == 0)
			script_error("divide by zero\n");
		else
		{
			result.setDouble(floatvalue(left) / fr);
		}
	}
	else
	{
		auto ir = intvalue(right);
		
		if(!ir)
			script_error("divide by zero\n");
		else
		{
			result.type = svt_int;
			result.value.i = intvalue(left) / ir;
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPremainder(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	int ir;
	
	evaluate_leftnright(start, n, stop);
	
	if(!(ir = intvalue(right)))
		script_error("divide by zero\n");
	else
    {
		result.type = svt_int;
		result.value.i = intvalue(left) % ir;
    }
}

/********** binary operators **************/

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPor_bin(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	
	evaluate_leftnright(start, n, stop);
	
	result.type = svt_int;
	result.value.i = intvalue(left) | intvalue(right);
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPand_bin(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	
	evaluate_leftnright(start, n, stop);
	
	result.type = svt_int;
	result.value.i = intvalue(left) & intvalue(right);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPnot_bin(svalue_t &result, int start, int n, int stop)
{
	EvaluateExpression(result, n+1, stop);
	
	result.value.i = ~intvalue(result);
	result.type = svt_int;
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPincrement(svalue_t &result, int start, int n, int stop)
{
	if(start == n)          // ++n
    {
		DFsVariable *var;
		
		var = Script->FindVariable(Tokens[stop], Level->FraggleScriptThinker->GlobalScript);
		if(!var)
		{
			script_error("unknown variable '%s'\n", Tokens[stop]);
		}
		var->GetValue(result);
		
		// haleyjd
		if(var->type != svt_fixed)
		{
			result.value.i = intvalue(result) + 1;
			result.type = svt_int;
			var->SetValue(Level, result);
		}
		else
		{
			result.setDouble(floatvalue(result)+1);
			var->SetValue(Level, result);
		}
    }
	else if(stop == n)     // n++
    {
		svalue_t newvalue;
		DFsVariable *var;
		
		var = Script->FindVariable(Tokens[start], Level->FraggleScriptThinker->GlobalScript);
		if(!var)
		{
			script_error("unknown variable '%s'\n", Tokens[start]);
		}
		var->GetValue(result);
		
		// haleyjd
		if(var->type != svt_fixed)
		{
			newvalue.type = svt_int;
			newvalue.value.i = intvalue(result) + 1;
			var->SetValue(Level, newvalue);
		}
		else
		{
			newvalue.setDouble(floatvalue(result)+1);
			var->SetValue(Level, newvalue);
		}
    }
	else
	{
		script_error("incorrect arguments to ++ operator\n");
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPdecrement(svalue_t &result, int start, int n, int stop)
{
	if(start == n)          // ++n
    {
		DFsVariable *var;
		
		var = Script->FindVariable(Tokens[stop], Level->FraggleScriptThinker->GlobalScript);
		if(!var)
		{
			script_error("unknown variable '%s'\n", Tokens[stop]);
		}
		var->GetValue(result);
		
		// haleyjd
		if(var->type != svt_fixed)
		{
			result.value.i = intvalue(result) - 1;
			result.type = svt_int;
			var->SetValue(Level, result);
		}
		else
		{
			result.setDouble(floatvalue(result)-1);
			result.type = svt_fixed;
			var->SetValue(Level, result);
		}
    }
	else if(stop == n)   // n++
    {
		svalue_t newvalue;
		DFsVariable *var;
		
		var = Script->FindVariable(Tokens[start], Level->FraggleScriptThinker->GlobalScript);
		if(!var)
		{
			script_error("unknown variable '%s'\n", Tokens[start]);
		}
		var->GetValue(result);
		
		// haleyjd
		if(var->type != svt_fixed)
		{
			newvalue.type = svt_int;
			newvalue.value.i = intvalue(result) - 1;
			var->SetValue(Level, newvalue);
		}
		else
		{
			newvalue.setDouble(floatvalue(result)-1);
			var->SetValue(Level, newvalue);
		}
    }
	else
	{
		script_error("incorrect arguments to ++ operator\n");
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPlessthanorequal(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	
	evaluate_leftnright(start, n, stop);
	
	result.type = svt_int;
	
	if(left.type == svt_fixed || right.type == svt_fixed)
		result.value.i = (fixedvalue(left) <= fixedvalue(right));
	else
		result.value.i = (intvalue(left) <= intvalue(right));
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FParser::OPgreaterthanorequal(svalue_t &result, int start, int n, int stop)
{
	svalue_t left, right;
	
	evaluate_leftnright(start, n, stop);
	
	result.type = svt_int;
	
	if(left.type == svt_fixed || right.type == svt_fixed)
		result.value.i = (fixedvalue(left) >= fixedvalue(right));
	else
		result.value.i = (intvalue(left) >= intvalue(right));
}

