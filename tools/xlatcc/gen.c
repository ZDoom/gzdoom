#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "xlat.h"

#define FILE_STACK	32

SimpleTranslator Simple[65536];
BoomTranslator Boomish[MAX_BOOMISH];

int NumBoomish;

FILE *Source;
char *SourceName;
int SourceLine;

static FILE *SourceStack[FILE_STACK];
static char *NameStack[FILE_STACK];
static int LineStack[FILE_STACK];
static int SourceStackSize;

void yyparse (void);

int CountSimpleTranslators (void)
{
	int i, count;

	for (i = 1, count = 0; i < 65536; i++)
	{
		if (Simple[i].NewSpecial != 0)
		{
			count++;
			for ( ; i < 65536 && Simple[i].NewSpecial != 0; i++)
				;
		}
	}
	return count;
}

void WriteSimpleTranslators (FILE *file)
{
	int i, start, count;

	for (count = 0, i = 1; i < 65536; i++)
	{
		if (Simple[i].NewSpecial != 0)
		{
			start = i;
			for ( ; i < 65536; i++)
			{
				if (Simple[i].NewSpecial == 0)
				{
					break;
				}
			}

			i--;

			// start = first linetype #
			// i = last linetype #

			putc (start >> 8, file);
			putc (start & 255, file);
			putc (i >> 8, file);
			putc (i & 255, file);

			for (; start <= i; start++)
			{
				putc (Simple[start].Flags, file);
				putc (Simple[start].NewSpecial, file);
				putc (Simple[start].Args[0], file);
				putc (Simple[start].Args[1], file);
				putc (Simple[start].Args[2], file);
				putc (Simple[start].Args[3], file);
				putc (Simple[start].Args[4], file);
				count++;
			}
		}
	}
	printf ("Wrote %d normal linetypes\n", count);
}

// BOOM translators are stored on disk as:
//
// WORD <first linetype in range>
// WORD <last linetype in range>
// BYTE <new special>
// repeat [BYTE op BYTES parms]
//
// op consists of some bits:
//
// 76543210
// ||||||++-- Dest is arg[(op&3)+1] (arg[0] is always tag)
// |||||+---- 0 = store, 1 = or with existing value
// ||||+----- 0 = this is normal, 1 = x-op in next byte
// ++++------ # of elements in list, or 0 to always use a constant value
//
// If a constant value is used, parms is a single byte containing that value.
// Otherwise, parms has the format:
//
// WORD <value to AND with linetype>
// repeat [WORD <if result is this> BYTE <use this>]
//
// These x-ops are defined:
//
// 0 = end of this BOOM translator
// 1 = dest is flags

void WriteBoomTranslators (FILE *file)
{
	int i, j, k;

	for (i = 0; i < NumBoomish; i++)
	{
		putc (Boomish[i].FirstLinetype >> 8, file);
		putc (Boomish[i].FirstLinetype & 255, file);
		putc (Boomish[i].LastLinetype >> 8, file);
		putc (Boomish[i].LastLinetype & 255, file);
		putc (Boomish[i].NewSpecial, file);

		for (j = 0; j < MAX_BOOMISH_EXEC; j++)
		{
			if (Boomish[i].Args[j].bDefined)
			{
				BYTE op;
				
				if (Boomish[i].Args[j].ArgNum < 4)
				{
					op = Boomish[i].Args[j].ArgNum;
				}
				else
				{
					op = 8;
				}
				if (Boomish[i].Args[j].bOrExisting)
				{
					op |= 4;
				}
				if (!Boomish[i].Args[j].bUseConstant)
				{
					op |= Boomish[i].Args[j].ListSize << 4;
				}
				putc (op, file);
				if (op & 8)
				{
					putc (Boomish[i].Args[j].ArgNum - 3, file);
				}
				if (Boomish[i].Args[j].bUseConstant)
				{
					putc (Boomish[i].Args[j].ConstantValue, file);
				}
				else
				{
					putc (Boomish[i].Args[j].AndValue >> 8, file);
					putc (Boomish[i].Args[j].AndValue & 255, file);
					for (k = 0; k < Boomish[i].Args[j].ListSize; k++)
					{
						putc (Boomish[i].Args[j].ResultFilter[k] >> 8, file);
						putc (Boomish[i].Args[j].ResultFilter[k] & 255, file);
						putc (Boomish[i].Args[j].ResultValue[k], file);
					}
				}
			}
		}
		putc (8, file);
		putc (0, file);
	}
	printf ("Wrote %d BOOM-style generalized linetypes\n", NumBoomish);
}

void WriteTranslatorFile (FILE *file, int count)
{
	putc ('N', file);
	putc ('O', file);
	putc ('R', file);
	putc ('M', file);
	putc (count >> 8, file);
	putc (count & 255, file);
	WriteSimpleTranslators (file);

	putc ('B', file);
	putc ('O', file);
	putc ('O', file);
	putc ('M', file);
	putc (NumBoomish >> 8, file);
	putc (NumBoomish & 255, file);
	WriteBoomTranslators (file);
}

void IncludeFile (const char *name)
{
	FILE *newsource;

	if (Source != NULL)
	{
		if (SourceStackSize == FILE_STACK)
		{
			printf ("Too many files included. Skipping %s\n", name);
			return;
		}
		SourceStack[SourceStackSize] = Source;
		LineStack[SourceStackSize] = SourceLine;
		NameStack[SourceStackSize] = SourceName;
		SourceStackSize++;
	}
	newsource = fopen (name, "r");
	if (newsource == NULL)
	{
		printf ("Cannot open %s\n", name);
		if (Source != NULL)
		{
			SourceStackSize--;
		}
		return;
	}

	Source = newsource;
	SourceLine = 1;
	SourceName = strdup (name);
}

bool EndFile ()
{
	if (Source != NULL)
	{
		fclose (Source);
	}
	if (SourceName != NULL)
	{
		free (SourceName);
	}
	if (SourceStackSize)
	{
		Source = SourceStack[--SourceStackSize];
		SourceName = NameStack[SourceStackSize];
		SourceLine = LineStack[SourceStackSize];

		return 1;
	}
	return 0;
}

int main (int argc, char **argv)
{
	FILE *output;
	int count;

	if (argc != 3)
	{
		printf ("Usage: %s <source file> <output file>\n", argv[0]);
		return -1;
	}
#if !defined(NDEBUG) && 0
	ParseTrace(fopen("trace.txt", "w"), ":");
#endif
	IncludeFile (argv[1]);
	yyparse ();
	count = CountSimpleTranslators ();
	if ((count | NumBoomish) != 0)
	{
		output = fopen (argv[2], "wb");
		if (output == NULL)
		{
			printf ("Could not open %s\n", argv[2]);
			return -2;
		}
		WriteTranslatorFile (output, count);
		fclose (output);
	}
	else
	{
		printf ("No linedef types to translate\n");
	}
	return 0;
}
