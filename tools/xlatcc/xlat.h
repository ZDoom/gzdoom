#include <stdio.h>

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned char bool;

#define MAX_BOOMISH			16
#define MAX_BOOMISH_EXEC	32

#define SIMPLE_HASTAGAT1	(1<<5)		// (tag, x, x, x, x)
#define SIMPLE_HASTAGAT2	(2<<5)		// (x, tag, x, x, x)
#define SIMPLE_HASTAGAT3	(3<<5)		// (x, x, tag, x, x)
#define SIMPLE_HASTAGAT4	(4<<5)		// (x, x, x, tag, x)
#define SIMPLE_HASTAGAT5	(5<<5)		// (x, x, x, x, tag)

#define SIMPLE_HASLINEID	(6<<5)		// (tag, lineid, x, x, x, x)
#define SIMPLE_HAS2TAGS		(7<<5)		// (tag, tag, x, x, x)

typedef struct
{
	BYTE Flags;
	BYTE NewSpecial;
	BYTE Args[5];
} SimpleTranslator;

typedef struct
{
	bool bDefined;
	bool bOrExisting;
	bool bUseConstant;
	BYTE ListSize;
	BYTE ArgNum;
	BYTE ConstantValue;
	WORD AndValue;
	WORD ResultFilter[15];
	BYTE ResultValue[15];
} BoomArg;

typedef struct
{
	WORD FirstLinetype;
	WORD LastLinetype;
	BYTE NewSpecial;
	BoomArg Args[MAX_BOOMISH_EXEC];
} BoomTranslator;

extern SimpleTranslator Simple[65536];
extern BoomTranslator Boomish[MAX_BOOMISH];

extern int NumBoomish;

extern FILE *Source;
extern char *SourceName;
extern int SourceLine;

void IncludeFile (const char *name);
bool EndFile ();

int yyerror (char *s);
