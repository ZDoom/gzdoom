#include <stdio.h>
#include "parse.h"

typedef enum { false, true } bool;
typedef short name;
typedef unsigned char uchar;
typedef unsigned int  uint;

typedef struct Scanner {
	FILE		*fd;
	uchar		*bot, *tok, *ptr, *cur, *pos, *lim, *top, *eof;
	uint		line;
} Scanner;

struct Token
{
	int val;
	char *string;
};

int lex(Scanner *s, struct Token *tok);

int yyerror (char *s);
void yyparse (void);

extern FILE *Source, *Dest;
extern int SourceLine;
extern int ErrorCount;


void WriteWord (int word);
void WriteLabel (char *label);
void WriteWords (int count, short *array);
void WriteBytes (int count, unsigned char *array);

void WriteNameTable ();

void WriteActions ();
void WriteActionMap ();
void WriteHeights ();
void WriteCodePConv ();
void WriteSprites ();
void WriteStates ();
void WriteSounds ();
void WriteInfoNames ();
void WriteThingBits ();
void WriteRenderStyles ();


struct StringList
{
	struct StringList *Next;
	char String[1];
};

struct StringSorter
{
	name OldName;
	struct StringList *Entry;
};

extern struct StringList *NameList, **NameListLast;
extern int NameCount;

name AddName (char *name);
name FindName (char *name);

void SortNames ();

struct StateMapE
{
	name Name;
	unsigned char State;
	unsigned char Count;
};

struct ThingBitsE
{
	name Name;
	unsigned char BitNum;
	unsigned char FlagNum;
};

struct RenderStylesE
{
	name Name;
	unsigned char StyleNum;
};

void AddAction (char *name);
int FindAction (char *name);

extern name *ActionsList;
extern unsigned char *HeightsArray;
extern unsigned char *ActionMap;
extern unsigned short *CodePMap;
extern char *SpriteNames;
extern struct StateMapE *StateMaps;
extern name *SoundMaps;
extern name *InfoNamesArray;
extern struct ThingBitsE *ThingBitsMap;
extern struct RenderStylesE *RenderStylesMap;

extern int ActionsListSize, MaxActionsListSize;
extern int HeightsSize, MaxHeightsSize;
extern int ActionMapSize, MaxActionMapSize;
extern int CodePMapSize, MaxCodePMapSize;
extern int SpriteNamesSize, MaxSpriteNamesSize;
extern int StateMapsSize, MaxStateMapsSize;
extern int SoundMapsSize, MaxSoundMapsSize;
extern int InfoNamesSize, MaxInfoNamesSize;
extern int ThingBitsMapSize, MaxThingBitsMapSize;
extern int RenderStylesSize, MaxRenderStylesSize;

void AddHeight (int h);
void AddActionMap (char *name);
void AddCodeP (int codep);
void AddSpriteName (char *name);
void AddStateMap (char *name, int type, int count);
void AddSoundMap (char *sound);
void AddInfoName (char *sound);
void AddThingBits (char *name, int bitnum, int flagnum);
void AddRenderStyle (char *name, int stylenum);

