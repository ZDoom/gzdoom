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
bool FileExists (const char *filename);

extern	char	progdir[1024];

void	FixPathSeperator (char *path);
static void	inline FixPathSeperator (FString &path) { path.ReplaceChars('\\', '/'); }

void 	DefaultExtension (char *path, const char *extension);
void 	DefaultExtension (FString &path, const char *extension);

FString	ExtractFilePath (const char *path);
FString	ExtractFileBase (const char *path, bool keep_extension=false);

int		ParseHex (char *str);
int 	ParseNum (char *str);
bool	IsNum (char *str);		// [RH] added

char	*copystring(const char *s);
void	ReplaceString (char **ptr, const char *str);

bool CheckWildcards (const char *pattern, const char *text);

void FormatGUID (char *text, const GUID &guid);

const char *myasctime ();

void strbin (char *str);

void CreatePath(const char * fn);

#endif
