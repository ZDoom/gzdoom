/* re2c lesson 001_upn_calculator, calc_004, (c) M. Boerger 2006 - 2007 */
/*!ignore:re2c

- making use of definitions
  . We provide complex rules as definitions. We can even have definitions made
    up from other definitions. And we could also use definitions as part of 
    rules and not only as full rules as shown in this lesson.

- showing the tokens
  . re2c does not store the beginning of a token on its own but we can easily 
    do this by providing variable, in our case t, that is set to YYCURSOR on
    every loop. If we were not using a loop here the token, we could have used
    s instead of a new variable instead.
  . As we use the token for an output function that requires a terminating zero
    we copy the token. Alternatively we could store the end of the token, then
    replace it with a zero character and replace it after the token has been 
    used. However that approach is not always acceptable.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char * tokendup(const char *t, const char *l)
{
	size_t n = l -t + 1;
	char *r = (char*)malloc(n);
	
	memmove(r, t, n-1);
	r[n] = '\0';
	return r;
}

int scan(char *s, int l)
{
	char *p = s;
	char *q = 0;
	char *t;
#define YYCTYPE         char
#define YYCURSOR        p
#define YYLIMIT         (s+l+2)
#define YYMARKER        q
#define YYFILL(n)		{ printf("OOD\n"); return 2; }
	
	for(;;)
	{
		t = p;
/*!re2c
	re2c:indent:top = 2;

	DIGIT	= [0-9] ;
	OCT		= "0" DIGIT+ ;
	INT		= "0" | ( [1-9] DIGIT* ) ;

	OCT			{ t = tokendup(t, p); printf("Oct: %s\n", t); free(t); continue; }
	INT			{ t = tokendup(t, p); printf("Num: %s\n", t); free(t); continue; }
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
