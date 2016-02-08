/* re2c lesson 001_upn_calculator, calc_008, (c) M. Boerger 2006 - 2007 */
/*!ignore:re2c

- using -b with signed character input
  . Since the code is being generated with -b switch re2c requires the internal
    character variable yych to use an unsigned character type. For that reason 
    the previous lessons had a conversion at the beginning of their scan() 
    function. Other re2c generated code often have the scanners work completely
    on unsigned input. Thus requesting a conversion.
    
    To avoid the conversion on input, re2c allows to do the conversion when
    reading the internal yych variable. To enable that conversion you need to
    use the implace configuration 're2c:yych:conversion' and set it to 1. This
    will change the generated code to insert conversions to YYCTYPE whenever
    yych is being read.

- More inplace configurations for better/nicer code
  . re2c allows to overwrite the generation of any define, label or variable
    used in the generated code. For example we overwrite the 'yych' variable
    name to 'curr' using inplace configuration 're2c:variable:yych = curr;'.

  . We further more use inplace configurations instead of defines. This allows
    to use correct conversions to 'unsigned char' instead of having to convert
    to 'YYCTYPE' when placing 're2c:define:YYCTYPE = "unsigned char";' infront 
    of 're2c:yych:conversion'. Note that we have to use apostrophies for the
    first setting as it contains a space.

  . Last but not least we use 're2c:labelprefix = scan' to change the prefix
    of generated labels.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEBUG(stmt) stmt

int  stack[4];
int  depth = 0;

int push_num(const char *t, const char *l, int radix)
{
	int num = 0;
	
	if (depth >= sizeof(stack))
	{
		return 3;
	}

	--t;
	while(++t < l)
	{
		num = num * radix + (*t - '0');
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
	DEBUG(printf("+\n"));
	return 0;
}

int scan(char *p)
{
	char *t;
	int res = 0;
	
	while(!res)
	{
		t = p;
/*!re2c
	re2c:define:YYCTYPE  = "unsigned char";
	re2c:define:YYCURSOR = p;
	re2c:variable:yych   = curr;
	re2c:indent:top      = 2;
	re2c:yyfill:enable   = 0;
	re2c:yych:conversion = 1;
	re2c:labelprefix     = scan;

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
