/* re2c lesson 001_upn_calculator, calc_003, (c) M. Boerger 2006 - 2007 */
/*!ignore:re2c

- making use of YYFILL

  . Again provide the length of the input to generate the limit only once. Now
    we can use YYFILL() to detect the end and simply return since YYFILL() is 
    only being used if the next scanner run might use more chars then YYLIMIT
    allows.
  . Note that we now use (s+l+2) instead of (s+l) as we did in lesson_001. In 
    the first lesson we did not quit from YYFILL() and used a special rule to
    detect the end of input. Here we use the fact that we know the exact end
    of input and that this length does not include the terminating zero. Since
    YYLIMIT points to the first character behind the used buffer we use "+ 2".
    If we would use "+1" we could drop the "\000" rule but could no longer
    distinguish between end of input and out of data.

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
#define YYLIMIT         (s+l+2)
#define YYMARKER        q
#define YYFILL(n)		{ printf("OOD\n"); return 2; }
	
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
	return 0;
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
		return 0;
	}
}
