/* re2c lesson 001_upn_calculator, calc_006, (c) M. Boerger 2006 - 2007 */
/*!ignore:re2c

- avoiding YYFILL()
  . We use the inplace configuration re2c:yyfill to suppress generation of
    YYFILL() blocks. This of course means we no longer have to provide the
    macro.
  . We also drop the YYMARKER stuff since we know that re2c does not generate 
    it for this example.
  . Since re2c does no longer check for out of data situations we must do this.
    For that reason we first reintroduce our zero rule and second we need to
    ensure that the scanner does not take more than one bytes in one go.
    
    In the example suppose "0" is passed. The scanner reads the first "0" and
    then is in an undecided state. The scanner can earliest decide on the next 
    char what  the token is. In case of a zero the input ends and it was a 
    number, 0 to be precise. In case of a digit it is an octal number and the 
    next character needs to be read. In case of any other character the scanner
    will detect an error with the any rule [^]. 
    
    Now the above shows that the scanner may read two characters directly. But 
    only if the first is a "0". So we could easily check that if the first char
    is "0" and the next char is a digit then yet another charcter is present.
    But we require our inut to be zero terminated. And that means we do not 
    have to check anything for this scanner.
    
    However with other rule sets re2c might read more then one character in a 
    row. In those cases it is normally hard to impossible to avoid YYFILL.

- optimizing the generated code by using -s command line switch of re2c
  . This tells re2c to generate code that uses if statements rather 
    then endless switch/case expressions where appropriate. Note that the 
    generated code now requires the input to be unsigned char rather than char
    due to the way comparisons are generated.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEBUG(stmt) stmt

int  stack[4];
int  depth = 0;

int push_num(const unsigned char *t, const unsigned char *l, int radix)
{
	int num = 0;
	
	if (depth >= sizeof(stack))
	{
		return 3;
	}

	--t;
	while(++t < l)
	{
		num = num * radix + (*t - (unsigned char)'0');
	}
	DEBUG(printf("Num: %d\n", num));

	stack[depth++] = num;
	return 0;
}

int stack_add()
{
	if (depth < 2) return 4;
	
	--depth;
	stack[depth-1] = stack[depth-1] + stack[depth];
	DEBUG(printf("+\n"));
	return 0;
}

int stack_sub()
{
	if (depth < 2) return 4;

	--depth;
	stack[depth-1] = stack[depth-1] - stack[depth];
	DEBUG(printf("-\n"));
	return 0;
}

int scan(char *s)
{
	unsigned char *p = (unsigned char*)s;
	unsigned char *t;
	int res = 0;
	
#define YYCTYPE         unsigned char
#define YYCURSOR        p
	
	while(!res)
	{
		t = p;
/*!re2c
	re2c:indent:top    = 2;
	re2c:yyfill:enable = 0;

	DIGIT	= [0-9] ;
	OCT		= "0" DIGIT+ ;
	INT		= "0" | ( [1-9] DIGIT* ) ;
	WS		= [ \t]+ ;

	WS		{ continue; }
	OCT		{ res = push_num(t, p, 8);	continue; }
	INT		{ res = push_num(t, p, 10); continue; }
	"+"		{ res = stack_add();		continue; }
	"-"		{ res = stack_sub();		continue; }
	"\000"  { res = depth == 1 ? 0 : 2;	break; }
	[^]		{ res = 1; 					continue; }
*/
	}
	return res;
}

int main(int argc, char **argv)
{
	if (argc > 1)
	{
		char *inp;
		int res = 0, argp = 0, len;
		
		while(!res && ++argp < argc)
		{
			inp = strdup(argv[argp]);
			len = strlen(inp);
			if (inp[0] == '\"' && inp[len-1] == '\"')
			{
				inp[len - 1] = '\0';
				++inp;
			}
			res = scan(inp);
			free(inp);
		}
		switch(res)
		{
		case 0:
			printf("Result: %d\n", stack[0]);
			return 0;
		case 1:
			fprintf(stderr, "Illegal character in input.\n");
			return 1;
		case 2:
			fprintf(stderr, "Premature end of input.\n");
			return 2;
		case 3:
			fprintf(stderr, "Stack overflow.\n");
			return 3;
		case 4:
			fprintf(stderr, "Stack underflow.\n");
			return 4;
		}
	}
	else
	{
		fprintf(stderr, "%s <expr>\n", argv[0]);
		return 0;
	}
}
