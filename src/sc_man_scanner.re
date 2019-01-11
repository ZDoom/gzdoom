/*
**
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
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


#define	YYCTYPE		unsigned char
#define	YYCURSOR	cursor
#define	YYLIMIT		limit
#define	YYMARKER	marker
#define YYFILL(n)	{}
#if 0	// As long as the buffer ends with '\n', we need do nothing special for YYFILL.
	// This buffer must be as large as the largest YYFILL call
	YYCTYPE eofbuf[9];
#define	YYFILL(n)	\
	{ if(!sc_End) { \
	   if(n == 2) { eofbuf[0] = *cursor; } \
	   else if(n >= 3 && n <= 9) { memcpy(eofbuf, cursor, n-1); } \
	   eofbuf[n-1] = '\n'; \
	   cursor = eofbuf; \
	   limit = eofbuf + n - 1; \
	   sc_End = true; } \
	} \
	assert(n <= sizeof eofbuf)	// Semicolon intentionally omitted
#endif

//#define YYDEBUG(s,c) { Printf ("%d: %02x\n", s, c); }
#define YYDEBUG(s,c)

	const char *cursor = ScriptPtr;
	const char *limit = ScriptEndPtr;

std1:
	tok = YYCURSOR;
std2:
/*!re2c
	any	= [\000-\377];
	WSP	= ([\000- ]\[\n]);
	NWS = (any\[\000- ]);
	O	= [0-7];
	D	= [0-9];
	L	= [a-zA-Z_];
	H	= [a-fA-F0-9];
	E	= [Ee] [+-]? D+;
	FS	= [fF];
	IS	= [uUlL];
	ESC	= [\\] ([abcfnrtv?'"\\] | "x" H+ | O+);

	TOK1 = [{}|=];
	TOKC = [{}|=/`~!@#$%^&*()\[\]\\?\-=+;:<>,.];

	STOP1 = (TOK1|["/;]);
	STOPC = (TOKC|["]);

	TOK2 = (NWS\STOP1);
	TOKC2 = (NWS\STOPC);
*/
#define RET(x)	TokenType = (x); goto normal_token;
	if (tokens && StateMode != 0)
	{
	/*!re2c
		"/*"						{ goto comment; }	/* C comment */
		"//" (any\"\n")* "\n"		{ goto newline; }	/* C++ comment */
		("#region"|"#endregion") (any\"\n")* "\n"
									{ goto newline; }	/* Region blocks [mxd] */

		(["](([\\]["])|[^"])*["])	{ goto string_const; }
		'stop'						{ RET(TK_Stop); }
		'wait'						{ RET(TK_Wait); }
		'fail'						{ RET(TK_Fail); }
		'loop'						{ RET(TK_Loop); }
		'goto'						{ StateMode = 0; StateOptions = false; RET(TK_Goto); }
		":"							{ RET(':'); }
		";"							{ RET(';'); }
		"}"							{ StateMode = 0; StateOptions = false; RET('}'); }
		
		WSP+						{ goto std1; }
		"\n"						{ goto newline; }
		
		TOKS = (NWS\[/":;}]);
		TOKS* ([/] (TOKS\[*]) TOKS*)*
									{ RET(TK_NonWhitespace); }
		
	*/
	}
	else if (tokens)	// A well-defined scanner, based on the c.re example.
	{
	/*!re2c
		"/*"						{ goto comment; }	/* C comment */
		"//" (any\"\n")* "\n"		{ goto newline; }	/* C++ comment */
		("#region"|"#endregion") (any\"\n")* "\n"
									{ goto newline; }	/* Region blocks [mxd] */

		/* C Keywords */
		'break'						{ RET(TK_Break); }
		'case'						{ RET(TK_Case); }
		'const'						{ RET(TK_Const); }
		'continue'					{ RET(TK_Continue); }
		'default'					{ RET(TK_Default); }
		'do'						{ RET(TK_Do); }
		'else'						{ RET(TK_Else); }
		'for'						{ RET(TK_For); }
		'goto'						{ RET(TK_Goto); }
		'if'						{ RET(TK_If); }
		'return'					{ RET(TK_Return); }
		'switch'					{ RET(TK_Switch); }
		'until'						{ RET(TK_Until); }
		'volatile'					{ RET(TK_Volatile); }
		'while'						{ RET(TK_While); }

		/* Type names */
		'bool'						{ RET(TK_Bool); }
		'float'						{ RET(TK_Float); }
		'double'					{ RET(TK_Double); }
		'char'						{ RET(TK_Char); }
		'byte'						{ RET(TK_Byte); }
		'sbyte'						{ RET(TK_SByte); }
		'short'						{ RET(TK_Short); }
		'ushort'					{ RET(TK_UShort); }
		'int8'						{ RET(TK_Int8); }
		'uint8'						{ RET(TK_UInt8); }
		'int16'						{ RET(TK_Int16); }
		'uint16'					{ RET(TK_UInt16); }
		'int'						{ RET(TK_Int); }
		'uint'						{ RET(TK_UInt); }
		'long'						{ RET(TK_Long); }
		'ulong'						{ RET(TK_ULong); }
		'void'						{ RET(TK_Void); }
		'struct'					{ RET(TK_Struct); }
		'class'						{ RET(TK_Class); }
		'enum'						{ RET(TK_Enum); }
		'name'						{ RET(TK_Name); }
		'string'					{ RET(TK_String); }
		'sound'						{ RET(TK_Sound); }
		'state'						{ RET(TK_State); }
		'color'						{ RET(TK_Color); }
		'vector2'					{ RET(TK_Vector2); }
		'vector3'					{ RET(TK_Vector3); }
		'map'						{ RET(TK_Map); }
		'array'						{ RET(TK_Array); }
		'in'						{ RET(TK_In); }
		'sizeof'					{ RET(TK_SizeOf); }
		'alignof'					{ RET(TK_AlignOf); }

		/* Other keywords from UnrealScript */
		'abstract'					{ RET(TK_Abstract); }
		'foreach'					{ RET(TK_ForEach); }
		'true'						{ RET(TK_True); }
		'false'						{ RET(TK_False); }
		'none'						{ RET(TK_None); }
		'auto'						{ RET(TK_Auto); }
		'property'					{ RET(TK_Property); }
		'flagdef'					{ RET(ParseVersion >= MakeVersion(3, 7, 0)? TK_FlagDef : TK_Identifier); }
		'native'					{ RET(TK_Native); }
		'var'						{ RET(TK_Var); }
		'out'						{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Out : TK_Identifier); }
		'static'					{ RET(TK_Static); }
		'transient'					{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Transient : TK_Identifier); }
		'final'						{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Final : TK_Identifier); }
		'extend'					{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Extend : TK_Identifier); }
		'protected'					{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Protected : TK_Identifier); }
		'private'					{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Private : TK_Identifier); }
		'dot'						{ RET(TK_Dot); }
		'cross'						{ RET(TK_Cross); }
		'virtual'					{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Virtual : TK_Identifier); }
		'override'					{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Override : TK_Identifier); }
		'vararg'					{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_VarArg : TK_Identifier); }
		'ui'						{ RET(ParseVersion >= MakeVersion(2, 4, 0)? TK_UI : TK_Identifier); }
		'play'						{ RET(ParseVersion >= MakeVersion(2, 4, 0)? TK_Play : TK_Identifier); }
		'clearscope'				{ RET(ParseVersion >= MakeVersion(2, 4, 0)? TK_ClearScope : TK_Identifier); }
		'virtualscope'				{ RET(ParseVersion >= MakeVersion(2, 4, 0)? TK_VirtualScope : TK_Identifier); }
		'super'						{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Super : TK_Identifier); }
		'stop'						{ RET(TK_Stop); }
		'null'						{ RET(TK_Null); }

		'is'						{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Is : TK_Identifier); }
		'replaces'					{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Replaces : TK_Identifier); }
		'states'					{ RET(TK_States); }
		'meta'						{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Meta : TK_Identifier); }
		'deprecated'				{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Deprecated : TK_Identifier); }
		'version'					{ RET(ParseVersion >= MakeVersion(2, 4, 0)? TK_Version : TK_Identifier); }
		'action'					{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Action : TK_Identifier); }
		'readonly'					{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_ReadOnly : TK_Identifier); }
		'internal'					{ RET(ParseVersion >= MakeVersion(3, 4, 0)? TK_Internal : TK_Identifier); }
		'let'						{ RET(ParseVersion >= MakeVersion(1, 0, 0)? TK_Let : TK_Identifier); }

		/* Actor state options */
		'bright'					{ RET(StateOptions ? TK_Bright : TK_Identifier); }
		'fast'						{ RET(StateOptions ? TK_Fast : TK_Identifier); }
		'slow'						{ RET(StateOptions ? TK_Slow : TK_Identifier); }
		'nodelay'					{ RET(StateOptions ? TK_NoDelay : TK_Identifier); }
		'canraise'					{ RET(StateOptions ? TK_CanRaise : TK_Identifier); }
		'offset'					{ RET(StateOptions ? TK_Offset : TK_Identifier); }
		'light'						{ RET(StateOptions ? TK_Light : TK_Identifier); }
		
		/* other DECORATE top level keywords */
		'#include'					{ RET(TK_Include); }

		L (L|D)*					{ RET(TK_Identifier); }

		("0" [xX] H+ IS?IS?) | ("0" D+ IS?IS?) | (D+ IS?IS?)
									{ RET(TK_IntConst); }

		(D+ E FS?) | (D* "." D+ E? FS?) | (D+ "." D* E? FS?)
									{ RET(TK_FloatConst); }

		(["](([\\]["])|[^"])*["])
									{ goto string_const; }

		(['] (any\[\n'])* ['])
									{ RET(TK_NameConst); }

		".."						{ RET(TK_DotDot); }
		"..."						{ RET(TK_Ellipsis); }
		">>>="						{ RET(TK_URShiftEq); }
		">>="						{ RET(TK_RShiftEq); }
		"<<="						{ RET(TK_LShiftEq); }
		"+="						{ RET(TK_AddEq); }
		"-="						{ RET(TK_SubEq); }
		"*="						{ RET(TK_MulEq); }
		"/="						{ RET(TK_DivEq); }
		"%="						{ RET(TK_ModEq); }
		"&="						{ RET(TK_AndEq); }
		"^="						{ RET(TK_XorEq); }
		"|="						{ RET(TK_OrEq); }
		">>>"						{ RET(TK_URShift); }
		">>"						{ RET(TK_RShift); }
		"<<"						{ RET(TK_LShift); }
		"++"						{ RET(TK_Incr); }
		"--"						{ RET(TK_Decr); }
		"&&"						{ RET(TK_AndAnd); }
		"||"						{ RET(TK_OrOr); }
		"<="						{ RET(TK_Leq); }
		">="						{ RET(TK_Geq); }
		"=="						{ RET(TK_Eq); }
		"!="						{ RET(TK_Neq); }
		"~=="						{ RET(TK_ApproxEq); }
		"<>="						{ RET(TK_LtGtEq); }
		"**"						{ RET(TK_MulMul); }
		"::"						{ RET(TK_ColonColon); }
		"->"						{ RET(TK_Arrow); }
		";"							{ StateOptions = false; RET(';'); }
		"{"							{ StateOptions = false; RET('{'); }
		"}"							{ RET('}'); }
		","							{ RET(','); }
		":"							{ RET(':'); }
		"="							{ RET('='); }
		"("							{ RET('('); }
		")"							{ RET(')'); }
		"["							{ RET('['); }
		"]"							{ RET(']'); }
		"."							{ RET('.'); }
		"&"							{ RET('&'); }
		"!"							{ RET('!'); }
		"~"							{ RET('~'); }
		"-"							{ RET('-'); }
		"+"							{ RET('+'); }
		"*"							{ RET('*'); }
		"/"							{ RET('/'); }
		"%"							{ RET('%'); }
		"<"							{ RET('<'); }
		">"							{ RET('>'); }
		"^"							{ RET('^'); }
		"|"							{ RET('|'); }
		"?"							{ RET('?'); }
		"#"							{ RET('#'); }
		"@"							{ RET('@'); }

		[ \t\v\f\r]+				{ goto std1; }
		"\n"						{ goto newline; }
		any
		{
			ScriptError ("Unexpected character: %c (ASCII %d)\n", *tok, *tok);
			goto std1;
		}
	*/
	}
	if (!CMode)	// The classic Hexen scanner.
	{
	/*!re2c
		"/*"						{ goto comment; }	/* C comment */
		("//"|";") (any\"\n")* "\n"	{ goto newline; }	/* C++/Hexen comment */
		("#region"|"#endregion") (any\"\n")* "\n"
									{ goto newline; }	/* Region blocks [mxd] */

		WSP+						{ goto std1; }		/* whitespace */
		"\n"						{ goto newline; }
		"\""						{ goto string; }

		TOK1						{ goto normal_token; }

		/* Regular tokens may contain /, but they must not contain comment starts */
		TOK2* ([/] (TOK2\[*]) TOK2*)*	{ goto normal_token; }

		any							{ goto normal_token; }	/* unknown character */
	*/
	}
	else		// A modified Hexen scanner for DECORATE.
	{
	/*!re2c
		"/*"					{ goto comment; }	/* C comment */
		"//" (any\"\n")* "\n"	{ goto newline; }	/* C++ comment */
		("#region"|"#endregion") (any\"\n")* "\n"
									{ goto newline; }	/* Region blocks [mxd] */

		WSP+					{ goto std1; }		/* whitespace */
		"\n"					{ goto newline; }
		"\""					{ goto string; }

		[-]						{ goto negative_check; }
		((D* [.] D+) | (D+ [.] D*))	{ goto normal_token; }	/* decimal number */
		(D+ E FS?) | (D* "." D+ E? FS?) | (D+ "." D* E? FS?)	{ goto normal_token; }	/* float with exponent */
		"::"					{ goto normal_token; }
		"&&"					{ goto normal_token; }
		"=="					{ goto normal_token; }
		"||"					{ goto normal_token; }
		"<<"					{ goto normal_token; }
		">>"					{ goto normal_token; }
		TOKC					{ goto normal_token; }
		TOKC2+					{ goto normal_token; }

		any						{ goto normal_token; }	/* unknown character */
	*/
	}

negative_check:
	// re2c doesn't have enough state to handle '-' as the start of a negative number
	// and as its own token, so help it out a little.
	TokenType = '-';
	if (YYCURSOR >= YYLIMIT)
	{
		goto normal_token;
	}
	if (*YYCURSOR >= '0' && *YYCURSOR <= '9')
	{
		goto std2;
	}
	if (*YYCURSOR != '.' || YYCURSOR+1 >= YYLIMIT)
	{
		goto normal_token;
	}
	if (*(YYCURSOR+1) >= '0' && *YYCURSOR <= '9')
	{
		goto std2;
	}
	goto normal_token;

comment:
/*!re2c
	"*/"
		{
			if (YYCURSOR >= YYLIMIT)
			{
				ScriptPtr = ScriptEndPtr;
				return_val = false;
				goto end;
			}
			goto std1;
		}
	"\n"
		{
			if (YYCURSOR >= YYLIMIT)
			{
				ScriptPtr = ScriptEndPtr;
				return_val = false;
				goto end;
			}
			Line++;
			Crossed = true;
			goto comment;
		}
	any				{ goto comment; }
*/

newline:
	if (YYCURSOR >= YYLIMIT)
	{
		ScriptPtr = ScriptEndPtr;
		return_val = false;
		goto end;
	}
	Line++;
	Crossed = true;
	goto std1;

normal_token:
	ScriptPtr = (YYCURSOR >= YYLIMIT) ? ScriptEndPtr : cursor;
	StringLen = int(ScriptPtr - tok);
	if (tokens && (TokenType == TK_StringConst || TokenType == TK_NameConst))
	{
		StringLen -= 2;
		if (StringLen >= MAX_STRING_SIZE)
		{
			BigStringBuffer = FString(tok+1, StringLen);
		}
		else
		{
			memcpy (StringBuffer, tok+1, StringLen);
		}
		if (StateMode && TokenType == TK_StringConst)
		{
			TokenType = TK_NonWhitespace;
		}
	}
	else
	{
		if (StringLen >= MAX_STRING_SIZE)
		{
			BigStringBuffer = FString(tok, StringLen);
		}
		else
		{
			memcpy (StringBuffer, tok, StringLen);
		}
	}
	if (tokens && StateMode)
	{ // State mode is exited after two consecutive TK_NonWhitespace tokens
		if (TokenType == TK_NonWhitespace)
		{
			StateMode--;
		}
		else
		{
			StateMode = 2;
		}
	}
	if (StringLen < MAX_STRING_SIZE)
	{
		String = StringBuffer;
		StringBuffer[StringLen] = '\0';
	}
	else
	{
		String = BigStringBuffer.LockBuffer();
	}
	return_val = true;
	goto end;

string_const:
	for (const char *c = tok; c < YYCURSOR; ++c)
	{
		if (*c == '\n') ++Line;
	}
	RET(TK_StringConst);

string:
	if (YYLIMIT != ScriptEndPtr)
	{
		ScriptPtr = ScriptEndPtr;
		return_val = false;
		goto end;
	}
	ScriptPtr = cursor;
	BigStringBuffer = "";
	for (StringLen = 0; cursor < YYLIMIT; ++cursor)
	{
		if (Escape && *cursor == '\\' && *(cursor + 1) == '"')
		{
			cursor++;
		}
		else if (*cursor == '\r' && *(cursor + 1) == '\n')
		{
			cursor++;	// convert CR-LF to simply LF
		}
		else if (*cursor == '"')
		{
			break;
		}
		if (*cursor == '\n')
		{
			if (CMode)
			{
				if (!Escape || StringLen == 0 || String[StringLen - 1] != '\\')
				{
					ScriptError ("Unterminated string constant");
				}
				else
				{
					StringLen--;		// overwrite the \ character with \n
				}
			}
			Line++;
			Crossed = true;
		}
		if (StringLen == MAX_STRING_SIZE)
		{
			BigStringBuffer.AppendCStrPart(StringBuffer, StringLen);
			StringLen = 0;
		}
		StringBuffer[StringLen++] = *cursor;
	}
	if (BigStringBuffer.IsNotEmpty() || StringLen == MAX_STRING_SIZE)
	{
		BigStringBuffer.AppendCStrPart(StringBuffer, StringLen);
		String = BigStringBuffer.LockBuffer();
		StringLen = int(BigStringBuffer.Len());
	}
	else
	{
		String = StringBuffer;
		StringBuffer[StringLen] = '\0';
	}
	ScriptPtr = cursor + 1;
	return_val = true;
end:
