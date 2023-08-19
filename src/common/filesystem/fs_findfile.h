#pragma once
// Directory searching routines

#include <stdint.h>
#include <string>

enum
{
	ZPATH_MAX = 260
};

#ifndef _WIN32

#include <dirent.h>

struct fs_findstate_t
{
private:
	std::string path;
    struct dirent **namelist;
    int current;
    int count;

	friend void *FS_FindFirst(const char *filespec, fs_findstate_t *fileinfo);
	friend int FS_FindNext(void *handle, fs_findstate_t *fileinfo);
	friend const char *FS_FindName(fs_findstate_t *fileinfo);
	friend int FS_FindAttr(fs_findstate_t *fileinfo);
	friend int FS_FindClose(void *handle);
};

int FS_FindAttr (fs_findstate_t *fileinfo); 

inline const char *FS_FindName(fs_findstate_t *fileinfo)
{
	return (fileinfo->namelist[fileinfo->current]->d_name);
}

enum
{
	FA_RDONLY	= 1,
	FA_HIDDEN	= 2,
	FA_SYSTEM	= 4,
	FA_DIREC	= 8,
	FA_ARCH		= 16,
};


#else


struct fs_findstate_t
{
private:
	struct FileTime
	{
		uint32_t lo, hi;
	};
	// Mirror WIN32_FIND_DATAW in <winbase.h>. We cannot pull in the Windows header here as it would pollute all consumers' symbol space.
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
	std::string UTF8Name;

	friend void *FS_FindFirst(const char *filespec, fs_findstate_t *fileinfo);
	friend int FS_FindNext(void *handle, fs_findstate_t *fileinfo);
	friend const char *FS_FindName(fs_findstate_t *fileinfo);
	friend int FS_FindAttr(fs_findstate_t *fileinfo);
};


const char *FS_FindName(fs_findstate_t *fileinfo);
inline int FS_FindAttr(fs_findstate_t *fileinfo)
{
	return fileinfo->FindData.Attribs;
}

enum
{
	FA_RDONLY	= 1,
	FA_HIDDEN	= 2,
	FA_SYSTEM	= 4,
	FA_DIREC	= 16,
	FA_ARCH		= 32,
};

#endif

void *FS_FindFirst (const char *filespec, fs_findstate_t *fileinfo);
int FS_FindNext (void *handle, fs_findstate_t *fileinfo);
int FS_FindClose (void *handle);
std::string FS_FullPath(const char* directory);
