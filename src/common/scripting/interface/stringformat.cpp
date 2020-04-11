/*
** thingdef_data.cpp
**
** DECORATE data tables
**
**---------------------------------------------------------------------------
** Copyright 2002-2008 Christoph Oelckers
** Copyright 2004-2008 Randy Heit
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

#include "zstring.h"
#include "vm.h"
#include "gstrings.h"
#include "v_font.h"
#include "types.h"



FString FStringFormat(VM_ARGS, int offset)
{
	PARAM_VA_POINTER(va_reginfo)	// Get the hidden type information array
	assert(va_reginfo[offset] == REGT_STRING);

	FString fmtstring = param[offset].s().GetChars();

	param += offset;
	numparam -= offset;
	va_reginfo += offset;

	// note: we don't need a real printf format parser.
	//       enough to simply find the subtitution tokens and feed them to the real printf after checking types.
	//       https://en.wikipedia.org/wiki/Printf_format_string#Format_placeholder_specification
	FString output;
	bool in_fmt = false;
	FString fmt_current;
	int argnum = 1;
	int argauto = 1;
	// % = starts
	//  [0-9], -, +, \s, 0, #, . continue
	//  %, s, d, i, u, fF, eE, gG, xX, o, c, p, aA terminate
	// various type flags are not supported. not like stuff like 'hh' modifier is to be used in the VM.
	// the only combination that is parsed locally is %n$...
	bool haveargnums = false;
	for (size_t i = 0; i < fmtstring.Len(); i++)
	{
		char c = fmtstring[i];
		if (in_fmt)
		{
			if (c == '*' && (fmt_current.Len() == 1 || (fmt_current.Len() == 2 && fmt_current[1] == '0')))
			{
				fmt_current += c;
			}
			else if ((c >= '0' && c <= '9') ||
				c == '-' || c == '+' || (c == ' ' && fmt_current.Back() != ' ') || c == '#' || c == '.')
			{
				fmt_current += c;
			}
			else if (c == '$') // %number$format
			{
				if (!haveargnums && argauto > 1)
					ThrowAbortException(X_FORMAT_ERROR, "Cannot mix explicit and implicit arguments.");
				FString argnumstr = fmt_current.Mid(1);
				if (!argnumstr.IsInt()) ThrowAbortException(X_FORMAT_ERROR, "Expected a numeric value for argument number, got '%s'.", argnumstr.GetChars());
				auto argnum64 = argnumstr.ToLong();
				if (argnum64 < 1 || argnum64 >= numparam) ThrowAbortException(X_FORMAT_ERROR, "Not enough arguments for format (tried to access argument %d, %d total).", argnum64, numparam);
				fmt_current = "%";
				haveargnums = true;
				argnum = int(argnum64);
			}
			else
			{
				fmt_current += c;

				switch (c)
				{
					// string
				case 's':
				{
					if (argnum < 0 && haveargnums)
						ThrowAbortException(X_FORMAT_ERROR, "Cannot mix explicit and implicit arguments.");
					in_fmt = false;
					// fail if something was found, but it's not a string
					if (argnum >= numparam) ThrowAbortException(X_FORMAT_ERROR, "Not enough arguments for format.");
					if (va_reginfo[argnum] != REGT_STRING) ThrowAbortException(X_FORMAT_ERROR, "Expected a string for format %s.", fmt_current.GetChars());
					// append
					output.AppendFormat(fmt_current.GetChars(), param[argnum].s().GetChars());
					if (!haveargnums) argnum = ++argauto;
					else argnum = -1;
					break;
				}

				// pointer
				case 'p':
				{
					if (argnum < 0 && haveargnums)
						ThrowAbortException(X_FORMAT_ERROR, "Cannot mix explicit and implicit arguments.");
					in_fmt = false;
					// fail if something was found, but it's not a string
					if (argnum >= numparam) ThrowAbortException(X_FORMAT_ERROR, "Not enough arguments for format.");
					if (va_reginfo[argnum] != REGT_POINTER) ThrowAbortException(X_FORMAT_ERROR, "Expected a pointer for format %s.", fmt_current.GetChars());
					// append
					output.AppendFormat(fmt_current.GetChars(), param[argnum].a);
					if (!haveargnums) argnum = ++argauto;
					else argnum = -1;
					break;
				}

				// int formats (including char)
				case 'd':
				case 'i':
				case 'u':
				case 'x':
				case 'X':
				case 'o':
				case 'c':
				case 'B':
				{
					if (argnum < 0 && haveargnums)
						ThrowAbortException(X_FORMAT_ERROR, "Cannot mix explicit and implicit arguments.");
					in_fmt = false;
					// append
					if (fmt_current[1] == '*' || fmt_current[2] == '*')
					{
						// fail if something was found, but it's not an int
						if (argnum+1 >= numparam) ThrowAbortException(X_FORMAT_ERROR, "Not enough arguments for format.");
						if (va_reginfo[argnum] != REGT_INT &&
							va_reginfo[argnum] != REGT_FLOAT) ThrowAbortException(X_FORMAT_ERROR, "Expected a numeric value for format %s.", fmt_current.GetChars());
						if (va_reginfo[argnum+1] != REGT_INT &&
							va_reginfo[argnum+1] != REGT_FLOAT) ThrowAbortException(X_FORMAT_ERROR, "Expected a numeric value for format %s.", fmt_current.GetChars());

						output.AppendFormat(fmt_current.GetChars(), param[argnum].ToInt(va_reginfo[argnum]), param[argnum + 1].ToInt(va_reginfo[argnum + 1]));
						argauto++;
					}
					else
					{
						// fail if something was found, but it's not an int
						if (argnum >= numparam) ThrowAbortException(X_FORMAT_ERROR, "Not enough arguments for format.");
						if (va_reginfo[argnum] != REGT_INT &&
							va_reginfo[argnum] != REGT_FLOAT) ThrowAbortException(X_FORMAT_ERROR, "Expected a numeric value for format %s.", fmt_current.GetChars());
						output.AppendFormat(fmt_current.GetChars(), param[argnum].ToInt(va_reginfo[argnum]));
					}
					if (!haveargnums) argnum = ++argauto;
					else argnum = -1;
					break;
				}

				// double formats
				case 'f':
				case 'F':
				case 'e':
				case 'E':
				case 'g':
				case 'G':
				case 'a':
				case 'A':
				{
					if (argnum < 0 && haveargnums)
						ThrowAbortException(X_FORMAT_ERROR, "Cannot mix explicit and implicit arguments.");
					in_fmt = false;
					if (fmt_current[1] == '*' || fmt_current[2] == '*')
					{
						// fail if something was found, but it's not an int
						if (argnum + 1 >= numparam) ThrowAbortException(X_FORMAT_ERROR, "Not enough arguments for format.");
						if (va_reginfo[argnum] != REGT_INT &&
							va_reginfo[argnum] != REGT_FLOAT) ThrowAbortException(X_FORMAT_ERROR, "Expected a numeric value for format %s.", fmt_current.GetChars());
						if (va_reginfo[argnum + 1] != REGT_INT &&
							va_reginfo[argnum + 1] != REGT_FLOAT) ThrowAbortException(X_FORMAT_ERROR, "Expected a numeric value for format %s.", fmt_current.GetChars());

						output.AppendFormat(fmt_current.GetChars(), param[argnum].ToInt(va_reginfo[argnum]), param[argnum + 1].ToDouble(va_reginfo[argnum + 1]));
						argauto++;
					}
					else
					{
						// fail if something was found, but it's not a float
						if (argnum >= numparam) ThrowAbortException(X_FORMAT_ERROR, "Not enough arguments for format.");
						if (va_reginfo[argnum] != REGT_INT &&
							va_reginfo[argnum] != REGT_FLOAT) ThrowAbortException(X_FORMAT_ERROR, "Expected a numeric value for format %s.", fmt_current.GetChars());
						// append
						output.AppendFormat(fmt_current.GetChars(), param[argnum].ToDouble(va_reginfo[argnum]));
					}
					if (!haveargnums) argnum = ++argauto;
					else argnum = -1;
					break;
				}

				default:
					// invalid character
					output += fmt_current;
					in_fmt = false;
					break;
				}
			}
		}
		else
		{
			if (c == '%')
			{
				if (i + 1 < fmtstring.Len() && fmtstring[i + 1] == '%')
				{
					output += '%';
					i++;
				}
				else
				{
					in_fmt = true;
					fmt_current = "%";
				}
			}
			else
			{
				output += c;
			}
		}
	}

	return output;
}

DEFINE_ACTION_FUNCTION(FStringStruct, Format)
{
	PARAM_PROLOGUE;
	FString s = FStringFormat(VM_ARGS_NAMES);
	ACTION_RETURN_STRING(s);
}

DEFINE_ACTION_FUNCTION(FStringStruct, AppendFormat)
{
	PARAM_SELF_STRUCT_PROLOGUE(FString);
	// first parameter is the self pointer
	FString s = FStringFormat(VM_ARGS_NAMES, 1);
	(*self) += s;
	return 0;
}

DEFINE_ACTION_FUNCTION(FStringStruct, AppendCharacter)
{
	PARAM_SELF_STRUCT_PROLOGUE(FString);
	PARAM_INT(c);
	self->AppendCharacter(c);
	return 0;
}

DEFINE_ACTION_FUNCTION(FStringStruct, DeleteLastCharacter)
{
	PARAM_SELF_STRUCT_PROLOGUE(FString);
	self->DeleteLastCharacter();
	return 0;
}
