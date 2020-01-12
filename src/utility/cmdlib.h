// cmdlib.h

#ifndef __CMDLIB__
#define __CMDLIB__


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include "zstring.h"

#if !defined(GUID_DEFINED)
#define GUID_DEFINED
typedef struct _GUID
{
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t	Data4[8];
} GUID;
#endif


// the dec offsetof macro doesnt work very well...
#define myoffsetof(type,identifier) ((size_t)&((type *)alignof(type))->identifier - alignof(type))

bool FileExists (const char *filename);
bool DirExists(const char *filename);
bool DirEntryExists (const char *pathname, bool *isdir = nullptr);
bool GetFileInfo(const char* pathname, size_t* size, time_t* time);

extern	FString progdir;

void	FixPathSeperator (char *path);
static void	inline FixPathSeperator (FString &path) { path.ReplaceChars('\\', '/'); }

void 	DefaultExtension (FString &path, const char *extension);

FString	ExtractFilePath (const char *path);
FString	ExtractFileBase (const char *path, bool keep_extension=false);

struct FScriptPosition;
bool	IsNum (const char *str);		// [RH] added

char	*copystring(const char *s);
void	ReplaceString (char **ptr, const char *str);

bool CheckWildcards (const char *pattern, const char *text);

void FormatGUID (char *buffer, size_t buffsize, const GUID &guid);

const char *myasctime ();

int strbin (char *str);
FString strbin1 (const char *start);

void CreatePath(const char * fn);

FString ExpandEnvVars(const char *searchpathstring);
FString NicePath(const char *path);

struct FFileList
{
	FString Filename;
	bool isDirectory;
};

bool ScanDirectory(TArray<FFileList> &list, const char *dirpath);
bool IsAbsPath(const char*);

FString M_ZLibError(int zerrnum);


#endif
