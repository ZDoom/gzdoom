// cmdlib.h

#ifndef __CMDLIB__
#define __CMDLIB__


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
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

template <typename T, size_t N>
char(&_ArraySizeHelper(T(&array)[N]))[N];

#define countof( array ) (sizeof( _ArraySizeHelper( array ) )) 

// the dec offsetof macro doesnt work very well...
#define myoffsetof(type,identifier) ((size_t)&((type *)alignof(type))->identifier - alignof(type))

bool FileExists (const char *filename);
bool FileReadable (const char *filename);
bool DirExists(const char *filename);
bool DirEntryExists (const char *pathname, bool *isdir = nullptr);
bool GetFileInfo(const char* pathname, size_t* size, time_t* time);

extern	FString progdir;

void	FixPathSeperator (char *path);
static void	inline FixPathSeperator (FString &path) { path.ReplaceChars('\\', '/'); }

void 	DefaultExtension (FString &path, const char *extension);
void NormalizeFileName(FString &str);

FString	ExtractFilePath (const char *path);
FString	ExtractFileBase (const char *path, bool keep_extension=false);
FString StripExtension(const char* path);

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

inline constexpr int32_t Scale(int32_t a, int32_t b, int32_t c)
{
	return (int32_t)(((int64_t)a * b) / c);
}

inline constexpr double Scale(double a, double b, double c)
{
	return (a * b) / c;
}

class FileReader;
struct MD5Context;

void md5Update(FileReader& file, MD5Context& md5, unsigned len);
void uppercopy(char* to, const char* from);

inline void fillshort(void* buff, size_t count, uint16_t clear)
{
	int16_t* b2 = (int16_t*)buff;
	for (size_t i = 0; i < count; ++i)
	{
		b2[i] = clear;
	}
}

template<typename T> inline constexpr int Sgn(const T& val) { return (val > 0) - (val < 0); }


inline int sizeToBits(int w)
{
	int j = 15;

	while ((j > 1) && ((1 << j) > w))
		j--;
	return j;
}


#endif
