#define	YYCTYPE		char
#define	YYCURSOR	cursor
#define	YYLIMIT		limit
#define	YYMARKER	marker

	// This buffer must be as large as the largest YYFILL call
	YYCTYPE eofbuf[2];
#define	YYFILL(n)	{ if(n == 2) { eofbuf[0] = *cursor; } eofbuf[n-1] = '\n'; cursor = eofbuf; limit = eofbuf + n - 1; sc_End = true; }

//#define YYDEBUG(s,c) { Printf ("%d: %02x\n", s, c); }
#define YYDEBUG(s,c)

	char *cursor = ScriptPtr;
	char *limit = ScriptEndPtr;

std:
	tok = YYCURSOR;
/*!re2c
	any	= [\000-\377];
	WSP	= ([\000- ]\[\n]);
	NWS = (any\[\000- ]);
	D	= [0-9];

	TOK1 = [{}|=];
	TOKC = [{}|=/`~!@#$%^&*()\[\]\\?\-=+;:<>,.];

	STOP1 = (TOK1|["/;]);
	STOPC = (TOKC|["]);

	TOK2 = (NWS\STOP1);
	TOKC2 = (NWS\STOPC);
*/
	if (!CMode)
	{
	/*!re2c
		"/*"						{ goto comment; }	/* C comment */
		("//"|";") (any\"\n")* "\n"	{ goto newline; }	/* C++/Hexen comment */

		WSP+						{ goto std; }		/* whitespace */
		"\n"						{ goto newline; }
		"\""						{ goto string; }

		TOK1						{ goto normal_token; }

		/* Regular tokens may contain /, but they must not contain comment starts */
		TOK2* ([/] (TOK2\[*])+ [*]*)* [/]?	{ goto normal_token; }

		any							{ goto normal_token; }	/* unknown character */
	*/
	}
	else
	{
	/*!re2c
		"/*"					{ goto comment; }	/* C comment */
		"//" (any\"\n")* "\n"	{ goto newline; }	/* C++ comment */

		WSP+					{ goto std; }		/* whitespace */
		"\n"					{ goto newline; }
		"\""					{ goto string; }

		[-]? D+ ([.]D*)?		{ goto normal_token; }	/* number */
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

comment:
/*!re2c
	"*/"			{ goto std; }
	"\n"
		{
			if (YYCURSOR >= YYLIMIT)
			{
				ScriptPtr = ScriptEndPtr;
				return false;
			}
			sc_Line++;
			sc_Crossed = true;
			goto comment;
		}
	any				{ goto comment; }
*/

newline:
	if (YYCURSOR >= YYLIMIT)
	{
		ScriptPtr = ScriptEndPtr;
		return false;
	}
	sc_Line++;
	sc_Crossed = true;
	goto std;

normal_token:
	ScriptPtr = (YYCURSOR >= YYLIMIT) ? ScriptEndPtr : cursor;
	sc_StringLen = MIN (ScriptPtr - tok, MAX_STRING_SIZE-1);
	memcpy (sc_String, tok, sc_StringLen);
	sc_String[sc_StringLen] = '\0';
	return true;

string:
	if (YYLIMIT != ScriptEndPtr)
	{
		ScriptPtr = ScriptEndPtr;
		return false;
	}
	ScriptPtr = cursor;
	for (sc_StringLen = 0; cursor < YYLIMIT; ++cursor)
	{
		if (Escape && *cursor == '\\' && *(cursor + 1) == '"')
		{
			cursor++;
		}
		else if (*cursor == '"')
		{
			break;
		}
		if (*cursor == '\n')
		{
			sc_Line++;
			sc_Crossed = true;
		}
		if (sc_StringLen < MAX_STRING_SIZE-1)
		{
			sc_String[sc_StringLen++] = *cursor;
		}
	}
	ScriptPtr = cursor + 1;
	sc_String[sc_StringLen] = '\0';
	return true;
