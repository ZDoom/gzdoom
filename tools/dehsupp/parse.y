%{
#include <malloc.h>
#include <stdlib.h>
#include <search.h>
#include <string.h>
#include <stdio.h>

typedef enum { false, true } bool;
typedef short name;

// verbose doesn't seem to help all that much
//#define YYERROR_VERBOSE 1

int yyerror (char *s);
int yylex (void);
int yyparse ();

FILE *Source, *Dest;
int SourceLine;
int ErrorCount;


void WriteWord (int word);
void WriteLabel (char *label);
void WriteWords (int count, short *array);
void WriteBytes (int count, char *array);

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


static bool FindToken (char *tok, int *type);

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

struct StringList *NameList, **NameListLast = &NameList;
int NameCount;

static name AddName (char *name);
static name FindName (char *name);

static void SortNames ();

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

static void AddAction (char *name);
static int FindAction (char *name);

name *ActionsList;
unsigned char *HeightsArray;
unsigned char *ActionMap;
unsigned short *CodePMap;
char *SpriteNames;
struct StateMapE *StateMaps;
name *SoundMaps;
name *InfoNamesArray;
struct ThingBitsE *ThingBitsMap;
struct RenderStylesE *RenderStylesMap;

int ActionsListSize, MaxActionsListSize;
int HeightsSize, MaxHeightsSize;
int ActionMapSize, MaxActionMapSize;
int CodePMapSize, MaxCodePMapSize;
int SpriteNamesSize, MaxSpriteNamesSize;
int StateMapsSize, MaxStateMapsSize;
int SoundMapsSize, MaxSoundMapsSize;
int InfoNamesSize, MaxInfoNamesSize;
int ThingBitsMapSize, MaxThingBitsMapSize;
int RenderStylesSize, MaxRenderStylesSize;

static void AddHeight (int h);
static void AddActionMap (char *name);
static void AddCodeP (int codep);
static void AddSpriteName (char *name);
static void AddStateMap (char *name, int type, int count);
static void AddSoundMap (char *sound);
static void AddInfoName (char *sound);
static void AddThingBits (char *name, int bitnum, int flagnum);
static void AddRenderStyle (char *name, int stylenum);

%}

%union {
	int val;
	char sym[80];
	char string[80];
}

%token <val> NUM
%token <sym> SYM
%token <string> STRING

%type <val> exp
%type <val> StateType;

%token PRINT
%token ENDL
%token Actions
%token OrgHeights
%token ActionList
%token CodePConv
%token OrgSprNames
%token StateMap
%token SoundMap
%token InfoNames
%token ThingBits
%token FirstState
%token SpawnState
%token DeathState
%token RenderStyles

%left '|'
%left '^'
%left '&'
%left '-' '+'
%left '*' '/'
%left NEG

%start translation_unit

%%

exp:
		  NUM
		| exp '+' exp		{ $$ = $1 + $3; }
		| exp '-' exp		{ $$ = $1 - $3; }
		| exp '*' exp		{ $$ = $1 * $3; }
		| exp '/' exp		{ $$ = $1 / $3; }
		| exp '|' exp		{ $$ = $1 | $3; }
		| exp '&' exp		{ $$ = $1 & $3; }
		| exp '^' exp		{ $$ = $1 ^ $3; }
		| '-' exp	%prec NEG	{ $$ = -$2 }
		| '(' exp ')'		{ $$ = $2; }
;

translation_unit:	/* empty line */
		| translation_unit external_declaration
;

print_statement:
		  PRINT '(' print_list ')'
		  {
			  printf ("\n");
		  }
;

print_list:
		  /* EMPTY */
		| print_item
		| print_item ',' print_list
;

print_item:
		  STRING { printf ("%s", $1); }
		| exp { printf ("%d", $1); }
		| ENDL { printf ("\n"); }
;

external_declaration:
		  print_statement
		| ActionsDef
		| OrgHeightsDef
		| ActionListDef
		| CodePConvDef
		| OrgSprNamesDef
		| StateMapDef
		| SoundMapDef
		| InfoNamesDef
		| ThingBitsDef
		| RenderStylesDef
;

ActionsDef:
		Actions '{' ActionsList '}' ';'
;

ActionsList:
		  /* empty */
		| SYM					{ AddAction ($1); }
		| ActionsList ',' SYM	{ AddAction ($3); }
;

OrgHeightsDef:
		OrgHeights '{' OrgHeightsList '}' ';'
;

OrgHeightsList:
		  /* empty */
		| exp					 { AddHeight ($1); }
		| OrgHeightsList ',' exp { AddHeight ($3); }
;

ActionListDef:
		ActionList '{' ActionListList '}' ';'
;

ActionListList:
		  /* empty */
		| SYM					{ AddActionMap ($1); }
		| ActionListList ',' SYM { AddActionMap ($3); }
;

CodePConvDef:
		CodePConv '{' CodePConvList '}' ';'
;

CodePConvList:
		  /* empty */
		| exp					{ AddCodeP ($1); }
		| CodePConvList ',' exp { AddCodeP ($3); }
;

OrgSprNamesDef:
		OrgSprNames '{' OrgSprNamesList '}' ';'
;

OrgSprNamesList:
		  /* empty */
		| SYM					{ AddSpriteName ($1); }
		| OrgSprNamesList ',' SYM { AddSpriteName ($3); }
;

StateMapDef:
		StateMap '{' StateMapList '}' ';'
;

StateMapList:
		  /* empty */
		| StateMapEntry
		| StateMapList ',' StateMapEntry
;

StateMapEntry:
		SYM ',' StateType ',' exp	{ AddStateMap ($1, $3, $5); }
;

StateType:
		  FirstState	{ $$ = 0; }
		| SpawnState	{ $$ = 1; }
		| DeathState	{ $$ = 2; }
;

SoundMapDef:
		SoundMap '{' SoundMapList '}' ';'
;

SoundMapList:
		  /*empty */
		| STRING					{ AddSoundMap ($1); }
		| SoundMapList ',' STRING	{ AddSoundMap ($3); }
;

InfoNamesDef:
		InfoNames '{' InfoNamesList '}' ';'
;

InfoNamesList:
		  /* empty */
		| SYM						{ AddInfoName ($1); }
		| InfoNamesList ',' SYM		{ AddInfoName ($3); }
;

ThingBitsDef:
		ThingBits '{' ThingBitsList '}' ';'
;

ThingBitsList:
		  /* empty */
		| ThingBitsEntry
		| ThingBitsList ',' ThingBitsEntry
;

ThingBitsEntry:
		exp ',' exp ',' SYM		{ AddThingBits ($5, $1, $3); }
;

RenderStylesDef:
		RenderStyles '{' RenderStylesList '}' ';'
;

RenderStylesList:
		  /* empty */
		| RenderStylesEntry
		| RenderStylesList ',' RenderStylesEntry
;

RenderStylesEntry:
		exp ',' SYM				{ AddRenderStyle ($3, $1); }
;

%%

#include <ctype.h>
#include <stdio.h>

// bison.simple #defines const to nothing
#ifdef const
#undef const
#endif

int yylex (void)
{
	char token[80];
	int toksize;
	int buildup;
	int c;

loop:
	if (Source == NULL)
	{
		return 0;
	}
	while (isspace (c = fgetc (Source)) && c != EOF)
	{
		if (c == '\n')
		{
			SourceLine++;
		}
	}

	if (c == EOF)
	{
		return 0;
	}
	if (isdigit (c))
	{
		buildup = c - '0';
		if (c == '0')
		{
			c = fgetc (Source);
			if (c == 'x' || c == 'X')
			{
				for (;;)
				{
					c = fgetc (Source);
					if (isdigit (c))
					{
						buildup = (buildup<<4) + c - '0';
					}
					else if (c >= 'a' && c <= 'f')
					{
						buildup = (buildup<<4) + c - 'a' + 10;
					}
					else if (c >= 'A' && c <= 'F')
					{
						buildup = (buildup<<4) + c - 'A' + 10;
					}
					else
					{
						ungetc (c, Source);
						yylval.val = buildup;
						return NUM;
					}
				}
			}
			else
			{
				ungetc (c, Source);
			}
		}
		while (isdigit (c = fgetc (Source)))
		{
			buildup = buildup*10 + c - '0';
		}
		ungetc (c, Source);
		yylval.val = buildup;
		return NUM;
	}
	if (isalpha (c))
	{
		token[0] = c;
		toksize = 1;
		while (toksize < 79 && (isalnum (c = fgetc (Source)) || c == '_'))
		{
			token[toksize++] = c;
		}
		token[toksize] = 0;
		if (toksize == 79 && isalnum (c))
		{
			while (isalnum (c = fgetc (Source)))
				;
		}
		ungetc (c, Source);
		if (FindToken (token, &buildup))
		{
			return buildup;
		}
		strcpy (yylval.sym, token);
		return SYM;
	}
	if (c == '/')
	{
		c = fgetc (Source);
		if (c == '*')
		{
			for (;;)
			{
				while ((c = fgetc (Source)) != '*' && c != EOF)
				{
					if (c == '\n')
						SourceLine++;
				}
				if (c == EOF)
					return 0;
				if ((c = fgetc (Source)) == '/')
					goto loop;
				if (c == EOF)
					return 0;
				ungetc (c, Source);
			}
		}
		else if (c == '/')
		{
			while ((c = fgetc (Source)) != '\n' && c != EOF)
				;
			if (c == '\n')
				SourceLine++;
			else if (c == EOF)
				return 0;
			goto loop;
		}
		else
		{
			ungetc (c, Source);
			return '/';
		}
	}
	if (c == '"')
	{
		int tokensize = 0;
		while ((c = fgetc (Source)) != '"' && c != EOF)
		{
			if (c == '\\')
			{
				c = fgetc (Source);
				if (c != '"' && c != EOF)
				{
					ungetc (c, Source);
					c = '\\';
				}
			}
			if (tokensize < 79)
			{
				yylval.string[tokensize++] = c;
			}
		}
		yylval.string[tokensize] = 0;
		return STRING;
	}
	return c;
}

static bool FindToken (char *tok, int *type)
{
	static const char tokens[][13] =
	{
		"endl", "print", "Actions",
		"OrgHeights", "ActionList", "CodePConv", "OrgSprNames",
		"StateMap", "SoundMap", "InfoNames", "ThingBits",
		"DeathState", "SpawnState", "FirstState", "RenderStyles"
	};
	static const short types[] =
	{
		ENDL, PRINT, Actions,
		OrgHeights, ActionList, CodePConv, OrgSprNames,
		StateMap, SoundMap, InfoNames, ThingBits,
		DeathState, SpawnState, FirstState, RenderStyles
	};
	int i;

	for (i = sizeof(tokens)/sizeof(tokens[0])-1; i >= 0; i--)
	{
		if (strcmp (tok, tokens[i]) == 0)
		{
			*type = types[i];
			return 1;
		}
	}
	return 0;
}

static name FindName (char *name)
{
	struct StringList *probe = NameList;
	int count = 0;

	while (probe != NULL)
	{
		if (stricmp (probe->String, name) == 0)
		{
			return count;
		}
		count++;
		probe = probe->Next;
	}
	return -1;
}

static name AddName (char *name)
{
	struct StringList *newone;
	int index = FindName (name);

	if (index != -1)
		return index;

	newone = malloc (sizeof(*newone) + strlen(name));
	strcpy (newone->String, name);
	newone->Next = NULL;
	*NameListLast = newone;
	NameListLast = &newone->Next;
	return NameCount++;
}

static int FindAction (char *namei)
{
	int name = FindName (namei);

	if (name != -1)
	{
		int i;

		for (i = 0; i < ActionsListSize; i++)
		{
			if (ActionsList[i] == name)
				return i;
		}
	}
	printf ("Line %d: Unknown action %s\n", SourceLine, namei);
	ErrorCount++;
	return -1;
}

static void AddAction (char *name)
{
	int newname = AddName (name);
	if (ActionsListSize == MaxActionsListSize)
	{
		MaxActionsListSize = MaxActionsListSize ? MaxActionsListSize * 2 : 256;
		ActionsList = realloc (ActionsList, MaxActionsListSize*sizeof(*ActionsList));
	}
	ActionsList[ActionsListSize++] = newname;
}

static void AddHeight (int h)
{
	if (MaxHeightsSize == HeightsSize)
	{
		MaxHeightsSize = MaxHeightsSize ? MaxHeightsSize * 2 : 256;
		HeightsArray = realloc (HeightsArray, MaxHeightsSize);
	}
	HeightsArray[HeightsSize++] = h;
}

static void AddActionMap (char *name)
{
	int index = FindAction (name);

	if (index != -1)
	{
		if (ActionMapSize == MaxActionMapSize)
		{
			MaxActionMapSize = MaxActionMapSize ? MaxActionMapSize * 2 : 256;
			ActionMap = realloc (ActionMap, MaxActionMapSize*sizeof(*ActionMap));
		}
		ActionMap[ActionMapSize++] = index;
	}
}

static void AddCodeP (int codep)
{
	if (CodePMapSize == MaxCodePMapSize)
	{
		MaxCodePMapSize = MaxCodePMapSize ? MaxCodePMapSize * 2 : 256;
		CodePMap = realloc (CodePMap, MaxCodePMapSize*sizeof(*CodePMap));
	}
	CodePMap[CodePMapSize++] = codep;
}

static void AddSpriteName (char *name)
{
	if (strlen (name) != 4)
	{
		printf ("Line %d: Sprite name %s must be 4 characters\n", SourceLine, name);
		ErrorCount++;
		return;
	}
	if (SpriteNamesSize == MaxSpriteNamesSize)
	{
		MaxSpriteNamesSize = MaxSpriteNamesSize ? MaxSpriteNamesSize * 2 : 256*4;
		SpriteNames = realloc (SpriteNames, MaxSpriteNamesSize*sizeof(*SpriteNames));
	}
	SpriteNames[SpriteNamesSize+0] = toupper (name[0]);
	SpriteNames[SpriteNamesSize+1] = toupper (name[1]);
	SpriteNames[SpriteNamesSize+2] = toupper (name[2]);
	SpriteNames[SpriteNamesSize+3] = toupper (name[3]);
	SpriteNamesSize += 4;
}

static void AddSoundMap (char *name)
{
	if (SoundMapsSize == MaxSoundMapsSize)
	{
		MaxSoundMapsSize = MaxSoundMapsSize ? MaxSoundMapsSize * 2 : 256;
		SoundMaps = realloc (SoundMaps, MaxSoundMapsSize*sizeof(*SoundMaps));
	}
	SoundMaps[SoundMapsSize++] = AddName (name);
}

static void AddInfoName (char *name)
{
	if (InfoNamesSize == MaxInfoNamesSize)
	{
		MaxInfoNamesSize = MaxInfoNamesSize ? MaxInfoNamesSize * 2 : 256;
		InfoNamesArray = realloc (InfoNamesArray, MaxInfoNamesSize*sizeof(*InfoNamesArray));
	}
	InfoNamesArray[InfoNamesSize++] = AddName (name);
}

static void AddStateMap (char *name, int state, int count)
{
	if (count == 0)
	{
		printf ("Line %d: Count is 0. Is this right?\n", SourceLine);
		return;
	}
	if ((unsigned)count > 255)
	{
		printf ("Line %d: Count must be in the range 1-255\n", SourceLine);
		ErrorCount++;
	}
	if (StateMapsSize == MaxStateMapsSize)
	{
		MaxStateMapsSize = MaxStateMapsSize ? MaxStateMapsSize*2 : 256;
		StateMaps = realloc (StateMaps, MaxStateMapsSize*sizeof(*StateMaps));
	}
	StateMaps[StateMapsSize].Name = AddName (name);
	StateMaps[StateMapsSize].State = state;
	StateMaps[StateMapsSize].Count = count;
	StateMapsSize++;
}

static void AddThingBits (char *name, int bitnum, int flagnum)
{
	if ((unsigned)bitnum > 31)
	{
		printf ("Line %d: Bit %d must be in the range 0-31\n", SourceLine, bitnum);
		ErrorCount++;
		return;
	}
	if (MaxThingBitsMapSize == ThingBitsMapSize)
	{
		MaxThingBitsMapSize = MaxThingBitsMapSize ? MaxThingBitsMapSize*2 : 128;
		ThingBitsMap = realloc (ThingBitsMap, MaxThingBitsMapSize*sizeof(*ThingBitsMap));
	}
	ThingBitsMap[ThingBitsMapSize].Name = AddName (name);
	ThingBitsMap[ThingBitsMapSize].BitNum = bitnum;
	ThingBitsMap[ThingBitsMapSize].FlagNum = flagnum;
	ThingBitsMapSize++;
}

static void AddRenderStyle (char *name, int stylenum)
{
	if ((unsigned)stylenum > 255)
	{
		printf ("Line %d: %s must be in the range 0-255\n", SourceLine, name);
		ErrorCount++;
		return;
	}
	if (MaxRenderStylesSize == RenderStylesSize)
	{
		MaxRenderStylesSize = MaxRenderStylesSize ? MaxRenderStylesSize*2 : 16;
		RenderStylesMap = realloc (RenderStylesMap, MaxRenderStylesSize*sizeof(*RenderStylesMap));
	}
	RenderStylesMap[RenderStylesSize].Name = AddName (name);
	RenderStylesMap[RenderStylesSize].StyleNum = stylenum;
	RenderStylesSize++;
}

static int sortfunc (const void *a, const void *b)
{
	return stricmp (((struct StringSorter *)a)->Entry->String,
		((struct StringSorter *)b)->Entry->String);
}

static void SortNames ()
{
	name *remap = malloc (NameCount * sizeof(*remap));
	struct StringSorter *sorter = malloc (NameCount * sizeof(*sorter));
	struct StringList *probe, **prev;
	int i;

	for (i = 0, probe = NameList; probe != NULL; probe = probe->Next, i++)
	{
		sorter[i].OldName = i;
		sorter[i].Entry = probe;
	}

	// There are some warnings here, though I have no idea why.
	qsort (sorter, NameCount, sizeof(*sorter), sortfunc);

	for (i = 0, prev = &NameList; i < NameCount; i++)
	{
		*prev = sorter[i].Entry;
		prev = &sorter[i].Entry->Next;
	}
	*prev = NULL;

	for (i = 0; i < NameCount; i++)
	{
		remap[sorter[i].OldName] = i;
	}

	for (i = 0; i < ActionsListSize; i++)
	{
		ActionsList[i] = remap[ActionsList[i]];
	}
	for (i = 0; i < SoundMapsSize; i++)
	{
		SoundMaps[i] = remap[SoundMaps[i]];
	}
	for (i = 0; i < InfoNamesSize; i++)
	{
		InfoNamesArray[i] = remap[InfoNamesArray[i]];
	}
	for (i = 0; i < StateMapsSize; i++)
	{
		StateMaps[i].Name = remap[StateMaps[i].Name];
	}
	for (i = 0; i < ThingBitsMapSize; i++)
	{
		ThingBitsMap[i].Name = remap[ThingBitsMap[i].Name];
	}
	for (i = 0; i < RenderStylesSize; i++)
	{
		RenderStylesMap[i].Name = remap[RenderStylesMap[i].Name];
	}
}

int yyerror (char *s)
{
	printf ("Line %d: %s\n", SourceLine, s);
	ErrorCount++;
	return 0;
}

int main (int argc, char **argv)
{
	if (argc != 3)
	{
		printf ("Usage: dehsupp <infile> <outfile>\n");
		return -1;
	}
	Source = fopen (argv[1], "r");
	if (Source == NULL)
	{
		printf ("Could not open %s\n", argv[1]);
		return -2;
	}
	SourceLine = 1;
	yyparse ();
	fclose (Source);
	if (ErrorCount)
	{
		printf ("There were %d errors\n", ErrorCount);
		return -3;
	}
	SortNames ();
	Dest = fopen (argv[2], "wb");
	if (Dest == NULL)
	{
		printf ("Could not open %s\n", argv[2]);
		return -4;
	}
	WriteNameTable ();
	WriteActions ();
	WriteCodePConv ();
	WriteSprites ();
	WriteSounds ();
	WriteInfoNames ();
	WriteStates ();
	WriteActionMap ();
	WriteThingBits ();
	WriteRenderStyles ();
	WriteHeights ();
	WriteLabel ("END ");
	fclose (Dest);
	return 0;
}

void fail (int code, char *err)
{
	fclose (Dest);
	printf ("%s\n", err);
	exit (code);
}

void WriteWord (int word)
{
	putc (word >> 8, Dest);
	putc (word & 255, Dest);
}

void WriteLabel (char *label)
{
	fwrite (label, 1, 4, Dest);
}

void WriteWords (int count, short *array)
{
	int i;

	WriteWord (count);
	for (i = 0; i < count; i++)
	{
		WriteWord (array[i]);
	}
}

void WriteBytes (int count, char *array)
{
	WriteWord (count);
	fwrite (array, 1, count, Dest);
}

void WriteNameTable ()
{
	struct StringList *probe;
	int i, size;

	WriteLabel ("NAME");
	// Count the length of each string, including nulls
	for (probe = NameList, size = 0; probe != NULL; probe = probe->Next)
	{
		size += (int)strlen (probe->String) + 1;
	}

	if (size == 0)
	{
		WriteWord (2);		// Size of this lump
		WriteWord (0);		// Number of names
		return;
	}
	size += NameCount*2 + 2;
	if (size >= 65536)
	{
		fail (-5, "Name table is larger than 64K");
	}
	WriteWord (size);		// Size of this lump
	WriteWord (NameCount);	// Number of names

	// Write each name's offset from the first name, which is stored
	// immediately after this list
	for (i = size = 0, probe = NameList; i < NameCount; i++, probe = probe->Next)
	{
		WriteWord (size);
		size += (int)strlen (probe->String) + 1;
	}

	// Write each name's string in order now
	for (probe = NameList; probe != NULL; probe = probe->Next)
	{
		fputs (probe->String, Dest);
		putc (0, Dest);
	}
}

typedef struct
{
	name Name;
	short Num;
} NameNum;

int sortfunc2 (const void *a, const void *b)
{
	return ((NameNum *)a)->Name - ((NameNum *)b)->Name;
}

void WriteActions ()
{
	NameNum *sorter = malloc (ActionsListSize * sizeof(*sorter));
	int i;
	
	WriteLabel ("ACTF");
	WriteWord (ActionsListSize);

	for (i = 0; i < ActionsListSize; i++)
	{
		sorter[i].Name = ActionsList[i];
		sorter[i].Num = i;
	}
	// warnings here. ignore.
	qsort (sorter, ActionsListSize, sizeof(*sorter), sortfunc2);
	for (i = 0; i < ActionsListSize; i++)
	{
		WriteWord (sorter[i].Name);
		WriteWord (sorter[i].Num);
	}
	free (sorter);
}

void WriteActionMap ()
{
	WriteLabel ("ACTM");
	WriteBytes (ActionMapSize, ActionMap);
}

void WriteHeights ()
{
	WriteLabel ("HIGH");
	WriteBytes (HeightsSize, HeightsArray);
}

void WriteCodePConv ()
{
	WriteLabel ("CODP");
	WriteWords (CodePMapSize, CodePMap);
}

void WriteSprites ()
{
	WriteLabel ("SPRN");
	WriteWord (SpriteNamesSize / 4);
	fwrite (SpriteNames, SpriteNamesSize, 1, Dest);
}

void WriteStates ()
{
	int i;

	WriteLabel ("STAT");
	WriteWord (StateMapsSize);
	for (i = 0; i < StateMapsSize; i++)
	{
		WriteWord (StateMaps[i].Name);
		putc (StateMaps[i].State, Dest);
		putc (StateMaps[i].Count, Dest);
	}
}

void WriteSounds ()
{
	WriteLabel ("SND ");
	WriteWords (SoundMapsSize, SoundMaps);
}

void WriteInfoNames ()
{
	WriteLabel ("INFN");
	WriteWords (InfoNamesSize, InfoNamesArray);
}

static int sortfunc3 (const void *a, const void *b)
{
	return ((struct ThingBitsE *)a)->Name - ((struct ThingBitsE *)b)->Name;
}

void WriteThingBits ()
{
	int i;

	WriteLabel ("TBIT");
	WriteWord (ThingBitsMapSize);
	// warnings here; ignore them
	qsort (ThingBitsMap, ThingBitsMapSize, sizeof(*ThingBitsMap), sortfunc3);
	for (i = 0; i < ThingBitsMapSize; i++)
	{
		WriteWord (ThingBitsMap[i].Name);
		putc (ThingBitsMap[i].BitNum | (ThingBitsMap[i].FlagNum<<5), Dest);
	}
}

static int sortfunc4 (const void *a, const void *b)
{
	return ((struct RenderStylesE *)a)->Name - ((struct RenderStylesE *)b)->Name;
}

void WriteRenderStyles ()
{
	int i;

	WriteLabel ("REND");
	WriteWord (RenderStylesSize);
	// More warnings; ignore
	qsort (RenderStylesMap, RenderStylesSize, sizeof(*RenderStylesMap), sortfunc4);
	for (i = 0; i < RenderStylesSize; i++)
	{
		WriteWord (RenderStylesMap[i].Name);
		putc (RenderStylesMap[i].StyleNum, Dest);
	}
}
