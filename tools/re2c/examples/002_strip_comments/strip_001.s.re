/* re2c lesson 002_strip_comments, strip_001.s, (c) M. Boerger 2006 - 2007 */
/*!ignore:re2c

- basic interface for file reading
  . This scanner will read chunks of input from a file. The easiest way would 
    be to read the whole file into a memory buffer and use that a zero 
    terminated string.
  . Instead we want to read input chunks of a reasonable size as they are neede
    by the scanner. Thus we basically need YYFILL(n) to call fread(n).
  . Before we provide a buffer that we constantly reallocate we instead use
    one buffer that we get from the stack or global memory just once. When we 
    reach the end of the buffer we simply move the beginning of our input
    that is somewhere in our buffer to the beginning of our buffer and then
    append the next chunk of input to the correct end inside our buffer.
  . As re2c scanners might read more than one character we need to ensure our
    buffer is long enough. We can use re2c to inform about the maximum size 
    by placing a "!max:re2c" comment somewhere. This gets translated to a 
    "#define YYMAXFILL <n>" line where <n> is the maximum length value. This
    define can be used as precompiler condition.

- multiple scanner blocks
  . We use a main scanner block that outputs every input character unless the
    input is two /s or a / followed by a *. In the latter two cases we switch
    to a special c++ comment and a comment block respectively.
  . Both special blocks simply detect their end ignore any other character.
  . The c++ block is a bit special. Since the terminating new line needs to
    be output and that can either be a new line or a carridge return followed 
    by a new line.
  . In order to ensure that we do not read behind our buffer we reset the token
    pointer to the cursor on every scanner run.
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
#define	YYFILL(n)	{ if ((res = fill(&s, n)) >= 0) break; }

typedef struct Scanner
{
	FILE			*fp;
	unsigned char	*cur, *tok, *lim, *eof;
	unsigned char 	buffer[BSIZE];
} Scanner;

int fill(Scanner *s, int len)
{
	if (!len)
	{
		s->cur = s->tok = s->lim = s->buffer;
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
	ANY			= [^] ;

	"/" "/"		{ goto cppcomment; }
	"/" "*"		{ goto comment; }
	ANY			{ fputc(*s.tok, stdout); continue; }
*/
comment:
		s.tok = s.cur;
/*!re2c
	"*" "/"		{ continue; }
	ANY			{ goto comment; }
*/
cppcomment:
		s.tok = s.cur;
/*!re2c
	NL			{ fwrite(s.tok, 1, s.cur - s.tok, stdout); continue; }
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
