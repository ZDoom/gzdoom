/* re2c lesson 002_strip_comments, strip_002.s, (c) M. Boerger 2006 - 2007 */
/*!ignore:re2c

- complexity
  . When a comment is preceeded by a new line and followed by whitespace and a 
    new line then we can drop the trailing whitespace and new line.
  . Additional to what we strip out already what about two consequtive comment 
    blocks? When two comments are only separated by whitespace we want to drop 
    both. In other words when detecting the end of a comment block we need to 
    check whether it is followed by only whitespace and the a new comment in
    which case we continure ignoring the input. If it is followed only by white
    space and a new line we strip out the new white space and new line. In any
    other case we start outputting all that follows.
    But we cannot simply use the following two rules:
	  "*" "/" WS* "/" "*" { continue; }
	  "*" "/" WS* NL      { continue; }
	The main problem is that WS* can get bigger then our buffer, so we need a 
	new scanner.
  . Meanwhile our scanner gets a bit more complex and we have to add two more 
    things. First the scanner code now uses a YYMARKER to store backtracking 
    information.

- backtracking information
  . When the scanner has two rules that can have the same beginning but a 
    different ending then it needs to store the position that identifies the
    common part. This is called backtracking. As mentioned above re2c expects
    you to provide compiler define YYMARKER and a pointer variable.
  . When shifting buffer contents as done in our fill function the marker needs
    to be corrected, too.

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
#define	YYFILL(n)	{ if ((res = fill(&s, n)) >= 0) break; }

typedef struct Scanner
{
	FILE			*fp;
	unsigned char	*cur, *tok, *lim, *eof, *mrk;
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
	NL			{ echo(&s); continue; }
	WS			{ goto commentws; }
	ANY			{ echo(&s); continue; }
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
