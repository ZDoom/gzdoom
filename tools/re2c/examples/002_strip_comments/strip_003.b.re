/* re2c lesson 002_strip_comments, strip_003.b, (c) M. Boerger 2006 - 2007 */
/*!ignore:re2c

- more complexity
  . Right now we strip out trailing white space and new lines after a comment
    block. This can be a problem when the comment block was not preceeded by 
    a new line.
  . The solution is to use trailing contexts.

-  trailing contexts
  . Re2c allows to check for a portion of input and only recognize it when it 
    is followed by another portion. This is called a trailing context.
  . The trailing context is not part of the identified input. That means that
    it follows exactly at the cursor. A consequence is that the scanner has
    already read more input and on the next run you need to restore begining
    of input, in our case s.tok, from the cursor, here s.cur, rather then 
    restoring to the beginning of the buffer. This way the scanner can reuse
    the portion it has already read.
  . The position of the trailing context is stored in YYCTXMARKER for which
    a pointer variable needs to be provided.
  . As with YYMARKER the corrsponding variable needs to be corrected if we 
    shift in some buffer.
  . Still this is not all we need to solve the problem. What is left is that
    the information whether we detected a trailing context was detected has to 
    be stored somewhere. This is done by the new variable nlcomment.

- formatting
  . Until now we only used single line expression code and we always had the 
    opening { on the same line as the rule itself. If we have multiline rule
    code and care for formatting we can no longer rely on re2c. Now we have 
    to indent the rule code ourself. Also we need to take care of the opening
    {. If we keep it on the same line as the rule then re2c will indent it 
    correctly and the emitted #line informations will be correct. If we place
    it on the next line then the #line directive will also point to that line
    and not to the rule.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*!max:re2c */
#define	BSIZE	128

#if BSIZE < YYMAXFILL
# error BSIZE must be greater YYMAXFILL
#endif

#define	YYCTYPE		unsigned char
#define	YYCURSOR	s.cur
#define	YYLIMIT		s.lim
#define YYMARKER	s.mrk
#define YYCTXMARKER s.ctx
#define	YYFILL(n)	{ if ((res = fill(&s, n)) >= 0) break; }

typedef struct Scanner
{
	FILE			*fp;
	unsigned char	*cur, *tok, *lim, *eof, *ctx, *mrk;
	unsigned char 	buffer[BSIZE];
} Scanner;

int fill(Scanner *s, int len)
{
	if (!len)
	{
		s->cur = s->tok = s->lim = s->mrk = s->buffer;
		s->eof = 0;
	}
	if (!s->eof)
	{
		int got, cnt = s->tok - s->buffer;

		if (cnt > 0)
		{
			memcpy(s->buffer, s->tok, s->lim - s->tok);
			s->tok -= cnt;
			s->cur -= cnt;
			s->lim -= cnt;
			s->mrk -= cnt;
			s->ctx -= cnt;
		}
		cnt = BSIZE - cnt;
		if ((got = fread(s->lim, 1, cnt, s->fp)) != cnt)
		{
			s->eof = &s->lim[got];
		}
		s->lim += got;
	}
	else if (s->cur + len > s->eof)
	{
		return 0; /* not enough input data */
	}
	return -1;
}

void echo(Scanner *s)
{
	fwrite(s->tok, 1, s->cur - s->tok, stdout);
}

int scan(FILE *fp)
{
	int  res = 0;
	int  nlcomment = 0;
    Scanner s;

	if (!fp)
	{
		return 1; /* no file was opened */
	}

    s.fp = fp;
	
	fill(&s, 0);

	for(;;)
	{
		s.tok = s.cur;
/*!re2c
	re2c:indent:top = 2;
	
	NL			= "\r"? "\n" ;
	WS			= [\r\n\t ] ;
	ANY			= [^] ;

	"/" "/"		{ goto cppcomment; }
	NL / "/""*"	{ echo(&s); nlcomment = 1; continue; }
	"/" "*"		{ goto comment; }
	ANY			{ fputc(*s.tok, stdout); continue; }
*/
comment:
		s.tok = s.cur;
/*!re2c
	"*" "/"		{ goto commentws; }
	ANY			{ goto comment; }
*/
commentws:
		s.tok = s.cur;
/*!re2c
	NL? "/" "*"	{ goto comment; }
	NL			{
					if (!nlcomment)
					{
						echo(&s);
					}
					nlcomment = 0;
					continue;
				}
	WS			{ goto commentws; }
	ANY			{ echo(&s); nlcomment = 0; continue; }
*/
cppcomment:
		s.tok = s.cur;
/*!re2c
	NL			{ echo(&s); continue; }
	ANY			{ goto cppcomment; }
*/
	}

	if (fp != stdin)
	{
		fclose(fp); /* close only if not stdin */
	}
	return res; /* return result */
}

int main(int argc, char **argv)
{
	if (argc > 1)
	{
		return scan(!strcmp(argv[1], "-") ? stdin : fopen(argv[1], "r"));
	}
	else
	{
		fprintf(stderr, "%s <expr>\n", argv[0]);
		return 1;
	}
}
