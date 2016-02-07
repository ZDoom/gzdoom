/* re2c lesson 001_upn_calculator, calc_001, (c) M. Boerger 2006 - 2007 */
/*!ignore:re2c

- basic interface for string reading

  . We define the macros YYCTYPE, YYCURSOR, YYLIMIT, YYMARKER, YYFILL
  . YYCTYPE is the type re2c operates on or in other words the type that 
    it generates code for. While it is not a big difference when we were
    using 'unsigned char' here we would need to run re2c with option -w
    to fully support types with sieof() > 1.
  . YYCURSOR is used internally and holds the current scanner position. In
    expression handlers, the code blocks after re2c expressions, this can be 
    used to identify the end of the token.
  . YYMARKER is not always being used so we set an initial value to avoid
    a compiler warning. Here we could also omit it compleley.
  . YYLIMIT stores the end of the input. Unfortunatley we have to use strlen() 
    in this lesson. In the next example we see one way to get rid of it.
  . We use a 'for(;;)'-loop around the scanner block. We could have used a
    'while(1)'-loop instead but some compilers generate a warning for it.
  . To make the output more readable we use 're2c:indent:top' scanner 
    configuration that configures re2c to prepend a single tab (the default)
    to the beginning of each output line.
  . The following lines are expressions and for each expression we output the 
    token name and continue the scanner loop.
  . The second last token detects the end of our input, the terminating zero in
    our input string. In other scanners detecting the end of input may vary.
    For example binary code may contain \0 as valid input.
  . The last expression accepts any input character. It tells re2c to accept 
    the opposit of the empty range. This includes numbers and our tokens but
    as re2c goes from top to botton when evaluating the expressions this is no 
    problem.
  . The first three rules show that re2c actually prioritizes the expressions 
    from top to bottom. Octal number require a starting "0" and the actual 
    number. Normal numbers start with a digit greater 0. And zero is finally a
    special case. A single "0" is detected by the last rule of this set. And
    valid ocal number is already being detected by the first rule. This even
    includes multi "0" sequences that in octal notation also means zero.
    Another way would be to only use two rules:
    "0" [0-9]+
    "0" | ( [1-9] [0-9]* )
    A full description of re2c rule syntax can be found in the manual.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int scan(char *s, int l)
{
	char *p = s;
	char *q = 0;
#define YYCTYPE         char
#define YYCURSOR        p
#define YYLIMIT         (s+l)
#define YYMARKER        q
#define YYFILL(n)
	
	for(;;)
	{
/*!re2c
	re2c:indent:top = 2;
	"0"[0-9]+	{ printf("Oct\n");	continue; }
	[1-9][0-9]*	{ printf("Num\n");	continue; }
	"0"			{ printf("Num\n");	continue; }
	"+"			{ printf("+\n");	continue; }
	"-"			{ printf("-\n");	continue; }
	"\000"		{ printf("EOF\n");	return 0; }
	[^]			{ printf("ERR\n");	return 1; }
*/
	}
}

int main(int argc, char **argv)
{
	if (argc > 1)
	{
		return scan(argv[1], strlen(argv[1]));
	}
	else
	{
		fprintf(stderr, "%s <expr>\n", argv[0]);
		return 1;
	}
}
