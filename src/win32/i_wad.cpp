/*
** i_wad.cpp
**
** Machine-dependant WAD handling code. This is where the memory mapping
** happens.
**
**---------------------------------------------------------------------------
** Copyright 2003 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/


// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <set>
#include <map>

#include "doomtype.h"
#include "m_swap.h"
#include "m_argv.h"
#include "i_system.h"
#include "cmdlib.h"
#include "w_wad.h"
#include "templates.h"
#include "v_text.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct FWadFileHandle
{
	char *Name;
	int FirstLump;
	int LastLump;
	HANDLE FileHandle;
	HANDLE MappingHandle;
	const BYTE *ViewBase;
	bool DontMap;
};

union MergedHeader
{
	DWORD magic;
	wadinfo_t wad;
	rffinfo_t rff;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void BloodCrypt (void *data, int key, int len);
static void PrintLastError ();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int numwads;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TArray<FWadFileHandle> ActiveWads;
static std::set<const void *> NonMappedPointers;
static std::map<const void *, size_t> ExtraLumpPointers;
static size_t FirstExtraFile;
static int FirstExtraLump;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// strupr
//
//==========================================================================

#ifdef NO_STRUPR
void strupr (char *s)
{
    while (*s)
	*s++ = toupper (*s);
}
#endif

//==========================================================================
//
// W_SysWadInit
//
//==========================================================================

void W_SysWadInit ()
{
	numwads = FirstExtraFile = ActiveWads.Size();
	FirstExtraLump = numlumps;
}

//==========================================================================
//
// W_AddFile
//
// Files with a .wad extension are wadlink files with multiple lumps,
// other files are single lumps with the base filename for the lump name.
//
// [RH] Removed reload hack
//==========================================================================

void W_AddFile (const char *filename)
{
	FWadFileHandle	wadhandle;
	MergedHeader	header;
	lumpinfo_t*		lump_p;
	unsigned		i;
	HANDLE			handle;
	DWORD			bytesRead;
	int				startlump;
	wadlump_t*		fileinfo, *fileinfo2free;
	wadlump_t		singleinfo;

	// open the file and add to directory
	handle = CreateFileA (filename, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{ // Didn't find file
		Printf (TEXTCOLOR_RED " couldn't open %s\n", filename);
		PrintLastError ();
		return;
	}

	Printf (" adding %s", filename);
	startlump = numlumps;

	// [RH] Determine if file is a WAD based on its signature, not its name.
	if (!ReadFile (handle, &header, sizeof(header), &bytesRead, NULL) || bytesRead == 0)
	{
		Printf (TEXTCOLOR_RED " couldn't read %s\n", filename);
		PrintLastError ();
		CloseHandle (handle);
		return;
	}

	wadhandle.Name = copystring (filename);

	if (header.magic == IWAD_ID || header.magic == PWAD_ID)
	{ // This is a WAD file

		header.wad.NumLumps = LONG(header.wad.NumLumps);
		header.wad.InfoTableOfs = LONG(header.wad.InfoTableOfs);
		fileinfo = fileinfo2free = new wadlump_t[header.wad.NumLumps];
		SetFilePointer (handle, header.wad.InfoTableOfs, 0, FILE_BEGIN);
		ReadFile (handle, fileinfo, header.wad.NumLumps * sizeof(wadlump_t), &bytesRead, NULL);
		numlumps += header.wad.NumLumps;
		Printf (" (%ld lumps)", header.wad.NumLumps);
	}
	else if (header.magic == RFF_ID)
	{ // This is a Blood RFF file

		rfflump_t *lumps, *rff_p;
		int skipped = 0;

		header.rff.NumLumps = LONG(header.rff.NumLumps);
		header.rff.DirOfs = LONG(header.rff.DirOfs);
		lumps = new rfflump_t[header.rff.NumLumps];
		SetFilePointer (handle, header.rff.DirOfs, 0, FILE_BEGIN);
		ReadFile (handle, lumps, header.rff.NumLumps * sizeof(rfflump_t), &bytesRead, NULL);
		BloodCrypt (lumps, header.rff.DirOfs, header.rff.NumLumps * sizeof(rfflump_t));

		numlumps += header.rff.NumLumps;
		lumpinfo = (lumpinfo_t *)Realloc (lumpinfo, numlumps*sizeof(lumpinfo_t));
		lump_p = &lumpinfo[startlump];

		for (i = 0, rff_p = lumps; i < header.rff.NumLumps; ++i, ++rff_p)
		{
			if (rff_p->Extension[0] == 'S' && rff_p->Extension[1] == 'F' &&
				rff_p->Extension[2] == 'X')
			{
				lump_p->namespc = ns_bloodsfx;
			}
			else if (rff_p->Extension[0] == 'R' && rff_p->Extension[1] == 'A' &&
				rff_p->Extension[2] == 'W')
			{
				lump_p->namespc = ns_bloodraw;
			}
			else
			{
				//lump_p->namespc = ns_bloodmisc;
				--numlumps;
				++skipped;
				continue;
			}
			uppercopy (lump_p->name, rff_p->Name);
			lump_p->wadnum = (WORD)ActiveWads.Size ();
			lump_p->position = LONG(rff_p->FilePos);
			lump_p->size = LONG(rff_p->Size);
			lump_p->flags = (rff_p->Flags & 0x10) >> 4;
			lump_p++;
		}
		delete[] lumps;
		if (skipped != 0)
		{
			lumpinfo = (lumpinfo_t *)Realloc (lumpinfo, numlumps*sizeof(lumpinfo_t));
		}
	}
	else
	{ // This is just a single lump file
		char name[PATH_MAX];

		fileinfo2free = NULL;
		fileinfo = &singleinfo;
		singleinfo.FilePos = 0;
		singleinfo.Size = LONG(GetFileSize (handle, NULL));
		ExtractFileBase (filename, name);
		strupr (name);
		strncpy (singleinfo.Name, name, 8);
		numlumps++;
	}
	Printf ("\n");

	// Fill in lumpinfo
	if (header.magic != RFF_ID)
	{
		lumpinfo = (lumpinfo_t *)Realloc (lumpinfo, numlumps*sizeof(lumpinfo_t));
		lump_p = &lumpinfo[startlump];
		for (i = startlump; i < (unsigned)numlumps; i++, lump_p++, fileinfo++)
		{
			// [RH] Convert name to uppercase during copy
			uppercopy (lump_p->name, fileinfo->Name);
			lump_p->wadnum = (WORD)ActiveWads.Size ();
			lump_p->position = LONG(fileinfo->FilePos);
			lump_p->size = LONG(fileinfo->Size);
			lump_p->namespc = ns_global;
			lump_p->flags = 0;
		}

		if (fileinfo2free)
		{
			delete[] fileinfo2free;
		}
	}

	wadhandle.FileHandle = handle;
	wadhandle.MappingHandle = NULL;
	wadhandle.ViewBase = NULL;
	wadhandle.DontMap = false;
	wadhandle.FirstLump = startlump;
	wadhandle.LastLump = numlumps - 1;
	ActiveWads.Push (wadhandle);
}

//==========================================================================
//
// W_ReadLump
//
// Loads the lump into the given buffer, which must be >= W_LumpLength().
//
//==========================================================================

void W_ReadLump (int lump, void *dest)
{
	lumpinfo_t *l;
	FWadFileHandle *wad;
	
	if (lump >= numlumps)
	{
		I_Error ("W_ReadLump: %i >= numlumps",lump);
	}
	l = lumpinfo + lump;
	wad = &ActiveWads[l->wadnum];
	if (wad->ViewBase != NULL)
	{
		memcpy (dest, wad->ViewBase + l->position, l->size);
	}
	else
	{
		DWORD bytesRead;

		SetFilePointer (wad->FileHandle, l->position, 0, FILE_BEGIN);
		ReadFile (wad->FileHandle, dest, l->size, &bytesRead, NULL);
		if (bytesRead < (DWORD)l->size)
		{
			I_Error ("W_ReadLump: only read %lu of %i on lump %i\n",
				bytesRead, l->size, lump);	
		}
	}
	if (l->flags & LUMPF_BLOODCRYPT)
	{
		BloodCrypt (dest, 0, MIN(l->size, 256));
	}
}

//==========================================================================
//
// W_MapLumpNum
//
//==========================================================================

void *W_MapLumpNum (int lump, bool writeable)
{
	lumpinfo_t *l;
	FWadFileHandle *wad;
	void *ptr;

	if ((unsigned)lump >= (unsigned)numlumps)
	{
		I_Error ("W_MapLumpNum: %u >= numlumps",lump);
	}

	// If the wad is not yet mapped, try to make it so.
	l = &lumpinfo[lump];
	wad = &ActiveWads[l->wadnum];
	if (wad->ViewBase == NULL && !wad->DontMap)
	{
		if (wad->MappingHandle == NULL)
		{
			wad->MappingHandle = CreateFileMapping (wad->FileHandle,
				NULL, PAGE_READONLY, 0, 0, NULL);
			if (wad->MappingHandle == NULL)
			{ // Assume it will fail in the future, too.
				wad->DontMap = true;
			}
		}
		wad->ViewBase = (BYTE *)MapViewOfFile (wad->MappingHandle, FILE_MAP_READ, 0, 0, 0);
		if (wad->ViewBase == NULL)
		{
			CloseHandle (wad->MappingHandle);
			wad->DontMap = true;
		}
	}

	// Blood encryption requires write access to the lump.
	if (writeable || l->flags & LUMPF_BLOODCRYPT)
	{
		ptr = Malloc (l->size);
		NonMappedPointers.insert (ptr);
		W_ReadLump (lump, ptr);
		return ptr;
	}

	// The file is memory-mapped, so just return a pointer into it.
	if ((size_t)l->wadnum >= FirstExtraFile)
	{
		ExtraLumpPointers.insert (std::pair<const void *, size_t>
			(wad->ViewBase + l->position, l->wadnum));
	}
	return (BYTE *)wad->ViewBase + l->position;
}

//==========================================================================
//
// W_UnMapLump
//
//==========================================================================

void W_UnMapLump (const void *data)
{
	std::set<const void *>::iterator iter;

	// If the pointer is in the set, then free it from the heap.
	iter = NonMappedPointers.find (data);
	if (iter != NonMappedPointers.end())
	{
		NonMappedPointers.erase (iter);
		free ((LPVOID)data);
	}

	// If this is an "extra" lump, unmap the associated file.
	std::map<const void *, size_t>::iterator it2;
	it2 = ExtraLumpPointers.find (data);
	if (it2 != ExtraLumpPointers.end())
	{
		FWadFileHandle *wad = &ActiveWads[it2->second];
		ExtraLumpPointers.erase (it2);
		if (UnmapViewOfFile ((LPVOID)wad->ViewBase))
		{
			wad->ViewBase = NULL;
		}
	}
}

//===========================================================================
//
// W_FakeLump
//
// Adds another file to the wad list after initialization has occurred.
//
//===========================================================================

int W_FakeLump (const char *filename)
{
	size_t i;

	for (i = FirstExtraFile; i < ActiveWads.Size(); ++i)
	{
		if (stricmp (filename, ActiveWads[i].Name) == 0)
		{
			return FirstExtraLump + (i - FirstExtraFile);
		}
	}

	// The new lump does not need to be accessible to GetNumForName,
	// so no need to update the hash table.
	W_AddFile (filename);
	return numlumps - 1;
}

//==========================================================================
//
// W_GetWadName
//
// Returns the name of the given wad.
//
//==========================================================================

const char *W_GetWadName (int wadnum)
{
	const char *name, *slash;

	if ((unsigned int)wadnum >= ActiveWads.Size ())
	{
		return NULL;
	}

	name = ActiveWads[wadnum].Name;
	slash = strrchr (name, '/');
	return slash != NULL ? slash+1 : name;
}

//==========================================================================
//
// W_GetWadFullName
//
// Returns the name of the given wad, including any path
//
//==========================================================================

const char *W_GetWadFullName (int wadnum)
{
	if ((unsigned int)wadnum >= ActiveWads.Size ())
	{
		return NULL;
	}

	return ActiveWads[wadnum].Name;
}

static void BloodCrypt (void *data, int key, int len)
{
	int p = (BYTE)key, i;

	for (i = 0; i < len; ++i)
	{
		((BYTE *)data)[i] ^= (unsigned char)(p+(i>>1));
	}
}

//==========================================================================
//
// PrintLastError
//
//==========================================================================

void PrintLastError ()
{
	char *lpMsgBuf;
	FormatMessageA( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR)&lpMsgBuf,
		0,
		NULL 
	);
	Printf (TEXTCOLOR_RED "  %s\n", lpMsgBuf);
	// Free the buffer.
	LocalFree( lpMsgBuf );
}
