/* re2c lesson 001_upn_calculator, calc_002, (c) M. Boerger 2006 - 2007 */
/*!ignore:re2c

- making use of YYFILL

  . Here we modified the scanner to not require strlen() on the call. Instead
    we compute limit on the fly. That is whenever more input is needed we 
    search for the terminating \0 in the next n chars the scanner needs.
  . If there is not enough input we quit the scanner.
  . Note that in lesson_001 YYLIMIT was a character pointer computed only once.
    Here is of course also of type YYCTYPE but a variable that gets reevaluated
    by YYFILL().
  . To make the code smaller we take advantage of the fact that our loop has no
    break so far. This allows us to use break here and have the code that is 
    used for YYFILL() not contain the printf in every occurence. That way the 
    generated code gets smaller.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int fill(char *p, int n, char **l)
{
	while (*++p && n--) ;
	* l = p;
	return n <= 0;
}

int scan(char *s)
{
	char *p = s;
	char *l = s;
	char *q = 0;
#define YYCTYPE         char
#define YYCURSOR        p
#define YYLIMIT         l
#define YYMARKER        q
#define YYFILL(n)		{ if (!fill(p, n, &l)) break; }
	
	for(;;)
	{
/*!re2c
	re2c:indent:top = 2;
	"0"[0-9]+	{ printf("Oct\n");	continue; }
	[1-9][0-9]*	{ printf("Num\n");	continue; }
	"0"			{ printf("Num\n");	continue; }
	"+"			{ printf("+\n");	continue; }
	"-"			{ printf("+\n");	continue; }
	"\000"		{ printf("EOF\n");	return 0; }
	[^]			{ printf("ERR\n");	return 1; }
*/
	}
	printf("OOD\n"); return 2;
}

int main(int argc, char **argv)
{
	if (argc > 1)
	{
		return scan(argv[1]);
	}
	else
	{
		fprintf(stderr, "%s <expr>\n", argv[0]);
		return 0;
	}
}
