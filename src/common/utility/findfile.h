#pragma once
// Directory searching routines

#include <stdint.h>
#include "zstring.h"

enum
{
	ZPATH_MAX = 260
};

#ifndef _WIN32

#include <dirent.h>

struct findstate_t
{
private:
	FString path;
    struct dirent **namelist;
    int current;
    int count;

	friend void *I_FindFirst(const char *filespec, findstate_t *fileinfo);
	friend int I_FindNext(void *handle, findstate_t *fileinfo);
	friend const char *I_FindName(findstate_t *fileinfo);
	friend int I_FindAttr(findstate_t *fileinfo);
	friend int I_FindClose(void *handle);
};

int I_FindAttr (findstate_t *fileinfo); 

inline const char *I_FindName(findstate_t *fileinfo)
{
	return (fileinfo->namelist[fileinfo->current]->d_name);
}

#define FA_RDONLY	1
#define FA_HIDDEN	2
#define FA_SYSTEM	4
#define FA_DIREC	8
#define FA_ARCH		16


#else
	
// Mirror WIN32_FIND_DATAW in <winbase.h>

struct findstate_t
{
private:
	struct FileTime
	{
		uint32_t lo, hi;
	};
	struct WinData
	{
		uint32_t Attribs;
		FileTime Times[3];
		uint32_t Size[2];
		uint32_t Reserved[2];
		wchar_t Name[ZPATH_MAX];
		wchar_t AltName[14];
	};
	WinData FindData;
	FString UTF8Name;

	friend void *I_FindFirst(const char *filespec, findstate_t *fileinfo);
	friend int I_FindNext(void *handle, findstate_t *fileinfo);
	friend const char *I_FindName(findstate_t *fileinfo);
	friend int I_FindAttr(findstate_t *fileinfo);
};


const char *I_FindName(findstate_t *fileinfo);
inline int I_FindAttr(findstate_t *fileinfo)
{
	return fileinfo->FindData.Attribs;
}

#define FA_RDONLY	0x00000001
#define FA_HIDDEN	0x00000002
#define FA_SYSTEM	0x00000004
#define FA_DIREC	0x00000010
#define FA_ARCH		0x00000020

#endif

void *I_FindFirst (const char *filespec, findstate_t *fileinfo);
int I_FindNext (void *handle, findstate_t *fileinfo);
int I_FindClose (void *handle);

class FConfigFile;

bool D_AddFile(TArray<FString>& wadfiles, const char* file, bool check, int position, FConfigFile* config);
void D_AddWildFile(TArray<FString>& wadfiles, const char* value, const char *extension, FConfigFile* config);
void D_AddConfigFiles(TArray<FString>& wadfiles, const char* section, const char* extension, FConfigFile* config);
void D_AddDirectory(TArray<FString>& wadfiles, const char* dir, const char *filespec, FConfigFile* config);
const char* BaseFileSearch(const char* file, const char* ext, bool lookfirstinprogdir, FConfigFile* config);
