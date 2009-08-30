#define	YYCTYPE		char
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
	FS	= [fFlL];
	IS	= [uUlL];
	ESC	= [\\] ([abcfnrtv?'"\\] | "x" H+ | O+);

	TOK1 = [{}|=];
	TOKC = [{}|=/`~!@#$%^&*()\[\]\\?\-=+;:<>,.];

	STOP1 = (TOK1|["/;]);
	STOPC = (TOKC|["]);

	TOK2 = (NWS\STOP1);
	TOKC2 = (NWS\STOPC);
*/
	if (tokens)	// A well-defined scanner, based on the c.re example.
	{
#define RET(x)	TokenType = x; goto normal_token;
	/*!re2c
		"/*"						{ goto comment; }	/* C comment */
		"//" (any\"\n")* "\n"		{ goto newline; }	/* C++ comment */

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
		'int'						{ RET(TK_Int); }
		'uint'						{ RET(TK_UInt); }
		'long'						{ RET(TK_Long); }
		'ulong'						{ RET(TK_ULong); }
		'void'						{ RET(TK_Void); }
		'struct'					{ RET(TK_Struct); }
		'class'						{ RET(TK_Class); }
		'mode'						{ RET(TK_Mode); }
		'enum'						{ RET(TK_Enum); }
		'name'						{ RET(TK_Name); }
		'string'					{ RET(TK_String); }
		'sound'						{ RET(TK_Sound); }
		'state'						{ RET(TK_State); }
		'color'						{ RET(TK_Color); }

		/* Other keywords from UnrealScript */
		'abstract'					{ RET(TK_Abstract); }
		'foreach'					{ RET(TK_ForEach); }
		'true'						{ RET(TK_True); }
		'false'						{ RET(TK_False); }
		'none'						{ RET(TK_None); }
		'new'						{ RET(TK_New); }
		'instanceof'				{ RET(TK_InstanceOf); }
		'auto'						{ RET(TK_Auto); }
		'exec'						{ RET(TK_Exec); }
		'defaultproperties'			{ RET(TK_DefaultProperties); }
		'native'					{ RET(TK_Native); }
		'out'						{ RET(TK_Out); }
		'ref'						{ RET(TK_Ref); }
		'event'						{ RET(TK_Event); }
		'static'					{ RET(TK_Static); }
		'transient'					{ RET(TK_Transient); }
		'final'						{ RET(TK_Final); }
		'throws'					{ RET(TK_Throws); }
		'extends'					{ RET(TK_Extends); }
		'public'					{ RET(TK_Public); }
		'protected'					{ RET(TK_Protected); }
		'private'					{ RET(TK_Private); }
		'dot'						{ RET(TK_Dot); }
		'cross'						{ RET(TK_Cross); }
		'ignores'					{ RET(TK_Ignores); }
		'localized'					{ RET(TK_Localized); }
		'latent'					{ RET(TK_Latent); }
		'singular'					{ RET(TK_Singular); }
		'config'					{ RET(TK_Config); }
		'coerce'					{ RET(TK_Coerce); }
		'iterator'					{ RET(TK_Iterator); }
		'optional'					{ RET(TK_Optional); }
		'export'					{ RET(TK_Export); }
		'virtual'					{ RET(TK_Virtual); }
		'super'						{ RET(TK_Super); }
		'global'					{ RET(TK_Global); }
		'self'						{ RET(TK_Self); }
		'stop'						{ RET(TK_Stop); }

		/* Needed for decorate action functions */
		'action'					{ RET(TK_Action); }

		/* other DECORATE top level keywords */
		'#include'					{ RET(TK_Include); }
		'fixed_t'					{ RET(TK_Fixed_t); }
		'angle_t'					{ RET(TK_Angle_t); }
		'abs'						{ RET(TK_Abs); }
		'random'					{ RET(TK_Random); }
		'random2'					{ RET(TK_Random2); }

		L (L|D)*					{ RET(TK_Identifier); }

		("0" [xX] H+ IS?) | ("0" D+ IS?) | (D+ IS?)
									{ RET(TK_IntConst); }

		(D+ E FS?) | (D* "." D+ E? FS?) | (D+ "." D* E? FS?)
									{ RET(TK_FloatConst); }

		(["](([\\]["])|[^"])*["])
									{ RET(TK_StringConst); }

		(['] (any\[\n'])* ['])
									{ RET(TK_NameConst); }

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
		";"							{ RET(';'); }
		"{"							{ RET('{'); }
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
