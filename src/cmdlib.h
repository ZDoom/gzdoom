// cmdlib.h

#ifndef __CMDLIB__
#define __CMDLIB__

#ifdef _MSC_VER
#pragma warning(disable : 4244)     // MIPS
#pragma warning(disable : 4136)     // X86
#pragma warning(disable : 4051)     // ALPHA

#pragma warning(disable : 4018)     // signed/unsigned mismatch
#pragma warning(disable : 4305)     // truncate from double to float
#endif

#include "doomtype.h"
#include "zstring.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>

// the dec offsetof macro doesnt work very well...
#define myoffsetof(type,identifier) ((size_t)&((type *)0)->identifier)

int		Q_filelength (FILE *f);
BOOL FileExists (const char *filename);

extern	char	progdir[1024];

void	FixPathSeperator (char *path);

void 	DefaultExtension (char *path, const char *extension);
void 	DefaultExtension (string &path, const char *extension);

void	ExtractFilePath (const char *path, char *dest);
void 	ExtractFileBase (const char *path, char *dest);

int		ParseHex (char *str);
int 	ParseNum (char *str);
BOOL	IsNum (char *str);		// [RH] added

char	*copystring(const char *s);
void	ReplaceString (char **ptr, const char *str);

bool CheckWildcards (const char *pattern, const char *text);

void FormatGUID (char *text, const GUID &guid);

char *myasctime ();

#endif
