#define	YYCTYPE		char
#define	YYCURSOR	cursor
#define	YYLIMIT		limit
#define	YYMARKER	marker

	// This buffer must be as large as the largest YYFILL call
	YYCTYPE eofbuf[3];
#define	YYFILL(n)	\
	{ if(!sc_End) { \
	   if(n == 2) { eofbuf[0] = *cursor; } \
	   else if(n == 3) { eofbuf[0] = *cursor; eofbuf[1] = *(cursor + 1); } \
	   eofbuf[n-1] = '\n'; \
	   cursor = eofbuf; \
	   limit = eofbuf + n - 1; \
	   sc_End = true; } \
	} \
	assert(n <= 3)	// Semicolon intentionally omitted

//#define YYDEBUG(s,c) { Printf ("%d: %02x\n", s, c); }
#define YYDEBUG(s,c)

	char *cursor = ScriptPtr;
	char *limit = ScriptEndPtr;

std1:
	tok = YYCURSOR;
std2:
/*!re2c
	any	= [\000-\377];
	WSP	= ([\000- ]\[\n]);
	NWS = (any\[\000- ]);
	D	= [0-9];
	X	= [0-9A-Fa-f];

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

		WSP+						{ goto std1; }		/* whitespace */
		"\n"						{ goto newline; }
		"\""						{ goto string; }

		TOK1						{ goto normal_token; }

		/* Regular tokens may contain /, but they must not contain comment starts */
		TOK2* ([/] (TOK2\[*]) TOK2*)*	{ goto normal_token; }

		any							{ goto normal_token; }	/* unknown character */
	*/
	}
	else
	{
	/*!re2c
		"/*"					{ goto comment; }	/* C comment */
		"//" (any\"\n")* "\n"	{ goto newline; }	/* C++ comment */

		WSP+					{ goto std1; }		/* whitespace */
		"\n"					{ goto newline; }
		"\""					{ goto string; }

		[-]						{ goto negative_check; }
		((D* [.] D+) | (D+ [.] D*))	{ goto normal_token; }	/* decimal number */
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
	"*/"			{ goto std1; }
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
	goto std1;

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
				if (!Escape || sc_StringLen == 0 || sc_String[sc_StringLen - 1] != '\\')
				{
					SC_ScriptError ("Unterminated string constant");
				}
				else
				{
					sc_StringLen--;		// overwrite the \ character with \n
				}
			}
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
