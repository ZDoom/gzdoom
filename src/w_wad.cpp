
//**************************************************************************
//**
//** w_wad.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: w_wad.c,v $
//** $Revision: 1.6 $
//** $Date: 95/10/06 20:56:47 $
//** $Author: cjr $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>

#include "m_alloc.h"
#include "doomtype.h"
#include "m_argv.h"
#include "i_system.h"
#include "cmdlib.h"
#include "w_wad.h"
#include "m_crc32.h"
#include "v_text.h"
#include "templates.h"

// MACROS ------------------------------------------------------------------

#define NULL_INDEX		(0xffff)

// TYPES -------------------------------------------------------------------

struct rffinfo_t
{
	// Should be "RFF\x18"
	DWORD		Magic;
	DWORD		Version;
	DWORD		DirOfs;
	DWORD		NumLumps;
};

union MergedHeader
{
	DWORD magic;
	wadinfo_t wad;
	rffinfo_t rff;
};

struct rfflump_t
{
	BYTE		IDontKnow[16];
	DWORD		FilePos;
	DWORD		Size;
	BYTE		IStillDontKnow[8];
	BYTE		Flags;
	char		Extension[3];
	char		Name[8];
	BYTE		WhatIsThis[4];
};

//
// WADFILE I/O related stuff.
//
struct FWadCollection::LumpRecord
{
	char		name[8];
	int			position;
	int			size;
	int			namespc;
	short		wadnum;
	WORD		flags;
};

enum
{
	LUMPF_BLOODCRYPT = 1	// Lump uses Blood-style encryption
};

class FWadCollection::WadFileRecord : public FileReader
{
public:
	WadFileRecord (FILE *file);
	~WadFileRecord ();

	char *Name;
	int FirstLump;
	int LastLump;
};


// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void W_SysWadInit ();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void BloodCrypt (void *data, int key, int len);
static void PrintLastError ();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FWadCollection Wads;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// uppercoppy
//
// [RH] Copy up to 8 chars, upper-casing them in the process
//==========================================================================

void uppercopy (char *to, const char *from)
{
	int i;

	for (i = 0; i < 8 && from[i]; i++)
		to[i] = toupper (from[i]);
	for (; i < 8; i++)
		to[i] = 0;
}

FWadCollection::FWadCollection ()
: FirstLumpIndex(NULL), NextLumpIndex(NULL), LumpInfo(NULL), Wads(NULL),
  NumLumps(0), NumWads(0)
{
}

//==========================================================================
//
// W_InitMultipleFiles
//
// Pass a null terminated list of files to use. All files are optional,
// but at least one file must be found. Lump names can appear multiple
// times. The name searcher looks backwards, so a later file can
// override an earlier one.
//
//==========================================================================

void FWadCollection::InitMultipleFiles (wadlist_t **filenames)
{
	int numfiles;

	// open all the files, load headers, and count lumps
	numfiles = 0;
	NumWads = 0;
	NumLumps = 0;
	LumpInfo = NULL; // will be realloced as lumps are added

	while (*filenames)
	{
		wadlist_t *next = (*filenames)->next;
		int baselump = NumLumps;
		char name[PATH_MAX];

		// [RH] Automatically append .wad extension if none is specified.
		strcpy (name, (*filenames)->name);
		FixPathSeperator (name);
		DefaultExtension (name, ".wad");

		AddFile (name);
		free (*filenames);
		*filenames = next;

		// The first two files are always zdoom.wad and the IWAD, which
		// do not contain skins.
		if (++numfiles > 2)
			SkinHack (baselump);
	}

	if (NumLumps == 0)
	{
		I_FatalError ("W_InitMultipleFiles: no files found");
	}

	// [RH] Merge sprite and flat groups.
	//		(We don't need to bother with patches, since
	//		Doom doesn't use markers to identify them.)
	MergeLumps ("S_START", "S_END", ns_sprites);
	MergeLumps ("F_START", "F_END", ns_flats);
	MergeLumps ("C_START", "C_END", ns_colormaps);
	MergeLumps ("A_START", "A_END", ns_acslibrary);
	MergeLumps ("TX_START", "TX_END", ns_newtextures);

	// [RH] Set up hash table
	FirstLumpIndex = new WORD[NumLumps];
	NextLumpIndex = new WORD[NumLumps];
	InitHashChains ();
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

void FWadCollection::AddFile (const char *filename)
{
	WadFileRecord	*wadinfo;
	MergedHeader	header;
	LumpRecord*		lump_p;
	unsigned		i;
	FILE*			handle;
	int				startlump;
	wadlump_t*		fileinfo = NULL, *fileinfo2free = NULL;
	wadlump_t		singleinfo;

	// open the file and add to directory
	handle = fopen (filename, "rb");
	if (handle == NULL)
	{ // Didn't find file
		Printf (TEXTCOLOR_RED " couldn't open %s\n", filename);
		PrintLastError ();
		return;
	}

	Printf (" adding %s", filename);
	startlump = NumLumps;

	wadinfo = new WadFileRecord (handle);

	// [RH] Determine if file is a WAD based on its signature, not its name.
	if (wadinfo->Read (&header, sizeof(header)) == 0)
	{
		Printf (TEXTCOLOR_RED " couldn't read %s\n", filename);
		PrintLastError ();
		delete wadinfo;
		return;
	}

	wadinfo->Name = copystring (filename);

	if (header.magic == IWAD_ID || header.magic == PWAD_ID)
	{ // This is a WAD file

		header.wad.NumLumps = LONG(header.wad.NumLumps);
		header.wad.InfoTableOfs = LONG(header.wad.InfoTableOfs);
		fileinfo = fileinfo2free = new wadlump_t[header.wad.NumLumps];
		wadinfo->Seek (header.wad.InfoTableOfs, SEEK_SET);
		wadinfo->Read (fileinfo, header.wad.NumLumps * sizeof(wadlump_t));
		NumLumps += header.wad.NumLumps;
		Printf (" (%ld lumps)", header.wad.NumLumps);
	}
	else if (header.magic == RFF_ID)
	{ // This is a Blood RFF file

		rfflump_t *lumps, *rff_p;
		int skipped = 0;

		header.rff.NumLumps = LONG(header.rff.NumLumps);
		header.rff.DirOfs = LONG(header.rff.DirOfs);
		lumps = new rfflump_t[header.rff.NumLumps];
		wadinfo->Seek (header.rff.DirOfs, SEEK_SET);
		wadinfo->Read (lumps, header.rff.NumLumps * sizeof(rfflump_t));
		BloodCrypt (lumps, header.rff.DirOfs, header.rff.NumLumps * sizeof(rfflump_t));

		NumLumps += header.rff.NumLumps;
		LumpInfo = (LumpRecord *)Realloc (LumpInfo, NumLumps*sizeof(LumpRecord));
		lump_p = &LumpInfo[startlump];

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
				--NumLumps;
				++skipped;
				continue;
			}
			uppercopy (lump_p->name, rff_p->Name);
			lump_p->wadnum = (WORD)NumWads;
			lump_p->position = LONG(rff_p->FilePos);
			lump_p->size = LONG(rff_p->Size);
			lump_p->flags = (rff_p->Flags & 0x10) >> 4;
			lump_p++;
		}
		delete[] lumps;
		if (skipped != 0)
		{
			LumpInfo = (LumpRecord *)Realloc (LumpInfo, NumLumps*sizeof(LumpRecord));
		}
	}
	else
	{ // This is just a single lump file
		char name[PATH_MAX];

		fileinfo2free = NULL;
		fileinfo = &singleinfo;
		singleinfo.FilePos = 0;
		singleinfo.Size = LONG(wadinfo->GetLength());
		ExtractFileBase (filename, name);
		strupr (name);
		strncpy (singleinfo.Name, name, 8);
		NumLumps++;
	}
	Printf ("\n");

	// Fill in lumpinfo
	if (header.magic != RFF_ID)
	{
		LumpInfo = (LumpRecord *)Realloc (LumpInfo, NumLumps*sizeof(LumpRecord));
		lump_p = &LumpInfo[startlump];
		for (i = startlump; i < (unsigned)NumLumps; i++, lump_p++, fileinfo++)
		{
			// [RH] Convert name to uppercase during copy
			uppercopy (lump_p->name, fileinfo->Name);
			lump_p->wadnum = (WORD)NumWads;
			lump_p->position = LONG(fileinfo->FilePos);
			lump_p->size = LONG(fileinfo->Size);
			lump_p->namespc = ns_global;
			lump_p->flags = 0;
		}

		if (fileinfo2free)
		{
			delete[] fileinfo2free;
		}

		ScanForFlatHack (startlump);
	}

	wadinfo->FirstLump = startlump;
	wadinfo->LastLump = NumLumps - 1;
	Wads = (WadFileRecord **)Realloc (Wads, (++NumWads)*sizeof(WadFileRecord*));
	Wads[NumWads-1] = wadinfo;
}

//==========================================================================
//
// W_CheckIfWadLoaded
//
// Returns true if the specified wad is loaded, false otherwise.
// If a fully-qualified path is specified, then the wad must match exactly.
// Otherwise, any wad with that name will work, whatever its path.
//
//==========================================================================

bool FWadCollection::CheckIfWadLoaded (const char *name)
{
	size_t i;

	if (strrchr (name, '/') != NULL)
	{
		for (i = 0; i < NumWads; ++i)
		{
			if (stricmp (GetWadFullName (i), name) == 0)
			{
				return true;
			}
		}
	}
	else
	{
		for (i = 0; i < NumWads; ++i)
		{
			if (stricmp (GetWadName (i), name) == 0)
			{
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// W_NumLumps
//
//==========================================================================

int FWadCollection::GetNumLumps () const
{
	return NumLumps;
}

//==========================================================================
//
// W_CheckNumForName
//
// Returns -1 if name not found. The version with a third parameter will
// look exclusively in the specified wad for the lump.
//
// [RH] Changed to use hash lookup ala BOOM instead of a linear search
// and namespace parameter
//==========================================================================

int FWadCollection::CheckNumForName (const char *name, int space)
{
	char uname[8];
	WORD i;

	uppercopy (uname, name);
	i = FirstLumpIndex[LumpNameHash (uname) % NumLumps];

	while (i != NULL_INDEX &&
		(*(__int64 *)&LumpInfo[i].name != *(__int64 *)&uname ||
		 LumpInfo[i].namespc != space))
	{
		i = NextLumpIndex[i];
	}

	return i != NULL_INDEX ? i : -1;
}

int FWadCollection::CheckNumForName (const char *name, int space, int wadnum)
{
	char uname[8];
	WORD i;

	if (wadnum < 0)
	{
		return CheckNumForName (name, space);
	}

	uppercopy (uname, name);
	i = FirstLumpIndex[LumpNameHash (uname) % NumLumps];

	while (i != NULL_INDEX &&
		(*(__int64 *)&LumpInfo[i].name != *(__int64 *)&uname ||
		 LumpInfo[i].namespc != space ||
		 LumpInfo[i].wadnum != wadnum))
	{
		i = NextLumpIndex[i];
	}

	return i != NULL_INDEX ? i : -1;
}

//==========================================================================
//
// W_GetNumForName
//
// Calls W_CheckNumForName, but bombs out if not found.
//
//==========================================================================

int FWadCollection::GetNumForName (const char *name)
{
	int	i;

	i = CheckNumForName (name);

	if (i == -1)
		I_Error ("W_GetNumForName: %s not found!", name);

	return i;
}


//==========================================================================
//
// W_LumpLength
//
// Returns the buffer size needed to load the given lump.
//
//==========================================================================

int FWadCollection::LumpLength (int lump) const
{
	if ((size_t)lump >= NumLumps)
	{
		I_Error ("W_LumpLength: %i >= NumLumps",lump);
	}

	return LumpInfo[lump].size;
}

//==========================================================================
//
// GetLumpOffset
//
// Returns the offset from the beginning of the file to the lump.
//
//==========================================================================

int FWadCollection::GetLumpOffset (int lump) const
{
	if ((size_t)lump >= NumLumps)
	{
		I_Error ("W_LumpLength: %i >= NumLumps",lump);
	}

	return LumpInfo[lump].position;
}

//==========================================================================
//
// W_LumpNameHash
//
// NOTE: s should already be uppercase, in contrast to the BOOM version.
//
// Hash function used for lump names.
// Must be mod'ed with table size.
// Can be used for any 8-character names.
//
//==========================================================================

DWORD FWadCollection::LumpNameHash (const char *s)
{
	const DWORD *table = GetCRCTable ();;
	DWORD hash = 0xffffffff;
	int i;

	for (i = 8; i > 0 && *s; --i, ++s)
	{
		hash = CRC1 (hash, *s, table);
	}
	return hash ^ 0xffffffff;
}

//==========================================================================
//
// W_InitHashChains
//
// Prepares the lumpinfos for hashing.
// (Hey! This looks suspiciously like something from Boom! :-)
//
//==========================================================================

void FWadCollection::InitHashChains (void)
{
	char name[8];
	unsigned int i, j;

	// Mark all buckets as empty
	memset (FirstLumpIndex, 255, NumLumps*sizeof(FirstLumpIndex[0]));
	memset (NextLumpIndex, 255, NumLumps*sizeof(FirstLumpIndex[0]));

	// Now set up the chains
	for (i = 0; i < (unsigned)NumLumps; i++)
	{
		uppercopy (name, LumpInfo[i].name);
		j = LumpNameHash (name) % NumLumps;
		NextLumpIndex[i] = FirstLumpIndex[j];
		FirstLumpIndex[j] = i;
	}
}

//==========================================================================
//
// IsMarker
//
// (from BOOM)
//
//==========================================================================

bool FWadCollection::IsMarker (const FWadCollection::LumpRecord *lump, const char *marker) const
{
	if (lump->namespc != ns_global)
	{
		return false;
	}
	if (strncmp (lump->name, marker, 8) == 0)
	{
		// If the previous lump was of the form FF_END and this one is
		// of the form F_END, ignore this as a marker
		if (marker[2] == 'E' && lump > LumpInfo)
		{
			if ((lump - 1)->name[0] == *marker &&
				strncmp ((lump - 1)->name + 1, marker, 7) == 0)
			{
				return false;
			}
		}
		return true;
	}
	// Treat double-character markers the same as single-character markers.
	// (So if FF_START appears in the wad, it will be treated as if it is F_START.
	// However, TTX_STAR will not be treated the same as TX_START because it
	// is not a single-character marker.)
	if (marker[1] == '_' &&
		lump->name[0] == *marker &&
		strncmp (lump->name + 1, marker, 7) == 0)
	{
		return true;
	}
	return false;
}

//==========================================================================
//
// ScanForFlatHack
//
// Try to detect wads that add extra flats by sticking an extra F_END
// at the end of the flat list without any corresponding FF_START.
// In other words, fix gothic2.wad.
//
//==========================================================================

void FWadCollection::ScanForFlatHack (int startlump)
{
	if (Args.CheckParm ("-noflathack"))
	{
		return;
	}

	for (int i = startlump; i < NumLumps; ++i)
	{
		if (LumpInfo[i].name[0] == 'F')
		{
			if (strcmp (LumpInfo[i].name + 1, "_START") == 0 ||
				strncmp (LumpInfo[i].name + 1, "F_START", 7) == 0)
			{
				// If the wad has a F_START/FF_START marker, don't
				// try to fix it.
				return;
			}

			// No need to look for FF_END, because Doom doesn't. One minor
			// nitpick: Doom will look for the last F_END; this finds the first
			// one if there is more than one in the file. Too bad. If there's
			// more than one F_END, this algorithm won't be able to properly
			// determine where to put the F_START anyway.
			if (strcmp (LumpInfo[i].name + 1, "_END") == 0)
			{
				int j;

				// When F_END is found, back up past any lumps of length
				// 4096, then insert an F_START marker.
				for (j = i - 1; LumpInfo[j].size == 4096; --j)
				{
				}
				++NumLumps;
				LumpInfo = (LumpRecord *)Realloc (LumpInfo, NumLumps*sizeof(LumpRecord));
				for (; i > j; --i)
				{
					LumpInfo[i+1] = LumpInfo[i];
				}
				++j;
				strcpy (LumpInfo[j].name, "F_START");
				LumpInfo[j].size = 0;
				LumpInfo[j].namespc = ns_global;
				LumpInfo[j].flags = 0;
				return;
			}
		}
	}
}

//==========================================================================
//
// MergeLumps
//
// Merge multiple tagged groups into one
// Basically from BOOM, too, although I tried to write it independently.
//
//==========================================================================

void FWadCollection::MergeLumps (const char *start, const char *end, int space)
{
	char ustart[8], uend[8];
	LumpRecord *newlumpinfos;
	int newlumps, oldlumps;
    size_t i;
	BOOL insideBlock;

	uppercopy (ustart, start);
	uppercopy (uend, end);

	newlumpinfos = new LumpRecord[NumLumps];

	newlumps = 0;
	oldlumps = 0;
	insideBlock = false;

	for (i = 0; i < NumLumps; i++)
	{
		if (!insideBlock)
		{
			// Check if this is the start of a block
			if (IsMarker (LumpInfo + i, ustart))
			{
				insideBlock = true;

				// Create start marker if we haven't already
				if (!newlumps)
				{
					newlumps++;
					strncpy (newlumpinfos[0].name, ustart, 8);
					newlumpinfos[0].wadnum = -1;
					newlumpinfos[0].position =
						newlumpinfos[0].size = 0;
					newlumpinfos[0].namespc = ns_global;
				}
			}
			else
			{
				// Copy lumpinfo down this list
				LumpInfo[oldlumps++] = LumpInfo[i];
			}
		}
		else
		{
			if (i && LumpInfo[i].wadnum != LumpInfo[i-1].wadnum)
			{
				// Blocks cannot span multiple files
				insideBlock = false;
				LumpInfo[oldlumps++] = LumpInfo[i];
			}
			else if (IsMarker (LumpInfo + i, uend))
			{
				// It is the end of a block. We'll add the end marker once
				// we've processed everything.
				insideBlock = false;
			}
			else
			{
				newlumpinfos[newlumps] = LumpInfo[i];
				newlumpinfos[newlumps++].namespc = space;
			}
		}
	}

	// Now copy the merged lumps to the end of the old list
	// and create the end marker entry.

	if (newlumps)
	{
		if (size_t(oldlumps + newlumps) > NumLumps)
			LumpInfo = (LumpRecord *)Realloc (LumpInfo, oldlumps + newlumps);

		memcpy (LumpInfo + oldlumps, newlumpinfos, sizeof(LumpRecord) * newlumps);

		NumLumps = oldlumps + newlumps;
		
		strncpy (LumpInfo[NumLumps].name, uend, 8);
		LumpInfo[NumLumps].wadnum = -1;
		LumpInfo[NumLumps].position =
			LumpInfo[NumLumps].size = 0;
		LumpInfo[NumLumps].namespc = ns_global;
		NumLumps++;
	}

	delete[] newlumpinfos;
}

//==========================================================================
//
// W_FindLump
//
// Find a named lump. Specifically allows duplicates for merging of e.g.
// SNDINFO lumps.
//
//==========================================================================

int FWadCollection::FindLump (const char *name, int *lastlump)
{
	char name8[8];
	LumpRecord *lump_p;

	uppercopy (name8, name);

	lump_p = LumpInfo + *lastlump;
	while (lump_p < LumpInfo + NumLumps)
	{
		if (*(__int64 *)&lump_p->name == *(__int64 *)&name8)
		{
			int lump = lump_p - LumpInfo;
			*lastlump = lump + 1;
			return lump;
		}
		lump_p++;
	}

	*lastlump = NumLumps;
	return -1;
}

//==========================================================================
//
// W_CheckLumpName
//
//==========================================================================

bool FWadCollection::CheckLumpName (int lump, const char *name)
{
	if ((size_t)lump >= NumLumps)
		return false;

	return !strnicmp (LumpInfo[lump].name, name, 8);
}

//==========================================================================
//
// W_GetLumpName
//
//==========================================================================

void FWadCollection::GetLumpName (char *to, int lump) const
{
	if ((size_t)lump >= NumLumps)
		*to = 0;
	else
		uppercopy (to, LumpInfo[lump].name);
}

//==========================================================================
//
// GetLumpNamespace
//
//==========================================================================

int FWadCollection::GetLumpNamespace (int lump) const
{
	if ((size_t)lump >= NumLumps)
		return ns_global;
	else
		return LumpInfo[lump].namespc;
}

//==========================================================================
//
// W_GetLumpFile
//
//==========================================================================

int FWadCollection::GetLumpFile (int lump) const
{
	if ((size_t)lump >= NumLumps)
		return -1;
	return LumpInfo[lump].wadnum;
}

//==========================================================================
//
// W_ReadLump
//
// Loads the lump into the given buffer, which must be >= W_LumpLength().
//
//==========================================================================

void FWadCollection::ReadLump (int lump, void *dest)
{
	FWadLump lumpr = OpenLumpNum (lump);
	long size = lumpr.GetLength ();
	long numread = lumpr.Read (dest, size);

	if (numread != size)
	{
		I_Error ("W_ReadLump: only read %ld of %ld on lump %i\n",
			numread, size, lump);	
	}
	if (LumpInfo[lump].flags & LUMPF_BLOODCRYPT)
	{
		BloodCrypt (dest, 0, MIN<int> (size, 256));
	}
}

//==========================================================================
//
// ReadLump - variant 2
//
// Loads the lump into a newly created buffer and returns it.
//
//==========================================================================

FMemLump FWadCollection::ReadLump (int lump)
{
	FWadLump lumpr = OpenLumpNum (lump);
	long size = lumpr.GetLength ();
	BYTE *dest = new BYTE[size];
	long numread = lumpr.Read (dest, size);

	if (numread != size)
	{
		I_Error ("W_ReadLump: only read %ld of %ld on lump %i\n",
			numread, size, lump);	
	}
	if (LumpInfo[lump].flags & LUMPF_BLOODCRYPT)
	{
		BloodCrypt (dest, 0, MIN<int> (size, 256));
	}
	return FMemLump (dest);
}

//==========================================================================
//
// OpenLumpNum
//
// Returns a copy of the file object for a lump's wad and positions its
// file pointer at the beginning of the lump.
//
//==========================================================================

FWadLump FWadCollection::OpenLumpNum (int lump)
{
	LumpRecord *l;
	WadFileRecord *wad;

	if ((unsigned)lump >= (unsigned)NumLumps)
	{
		I_Error ("W_MapLumpNum: %u >= NumLumps",lump);
	}

	l = &LumpInfo[lump];
	wad = Wads[l->wadnum];

#if 0
	// Blood encryption?
	if (writeable || l->flags & LUMPF_BLOODCRYPT)
	{
		ptr = Malloc (l->size);
		NonMappedPointers.insert (ptr);
		W_ReadLump (lump, ptr);
		return ptr;
	}
#endif

	wad->Seek (l->position, SEEK_SET);
	return FWadLump (*wad, l->size);
}

//==========================================================================
//
// ReopenLumpNum
//
// Similar to OpenLumpNum, except a new, independant file object is created
// for the lump's wad. Use this when you won't read the lump's data all at
// once (e.g. for streaming an Ogg Vorbis stream from a wad as music).
//
//==========================================================================

FWadLump *FWadCollection::ReopenLumpNum (int lump)
{
	LumpRecord *l;
	WadFileRecord *wad;
	FILE *f;

	if ((unsigned)lump >= (unsigned)NumLumps)
	{
		I_Error ("W_MapLumpNum: %u >= NumLumps",lump);
	}

	l = &LumpInfo[lump];
	wad = Wads[l->wadnum];

	f = fopen (wad->Name, "rb");
	if (f == NULL)
	{
		I_Error ("Could not reopen %s\n", wad->Name);
	}

	fseek (f, l->position, SEEK_SET);
	return new FWadLump (f, l->size);
}

//==========================================================================
//
// W_GetWadName
//
// Returns the name of the given wad.
//
//==========================================================================

const char *FWadCollection::GetWadName (int wadnum) const
{
	const char *name, *slash;

	if ((DWORD)wadnum >= NumWads)
	{
		return NULL;
	}

	name = Wads[wadnum]->Name;
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

const char *FWadCollection::GetWadFullName (int wadnum) const
{
	if ((unsigned int)wadnum >= NumWads)
	{
		return NULL;
	}

	return Wads[wadnum]->Name;
}


//==========================================================================
//
// W_SkinHack
//
// Tests a wad file to see if it contains an S_SKIN marker. If it does,
// every lump in the wad is moved into a new namespace. Because skins are
// only supposed to replace player sprites, sounds, or faces, this should
// not be a problem. Yes, there are skins that replace more than that, but
// they are such a pain, and breaking them like this was done on purpose.
// This also renames any S_SKINxx lumps to just S_SKIN.
//==========================================================================

void FWadCollection::SkinHack (int baselump)
{
	bool skinned = false;
	size_t i;

	for (i = baselump; i < NumLumps; i++)
	{
		if (LumpInfo[i].name[0] == 'S' &&
			LumpInfo[i].name[1] == '_' &&
			LumpInfo[i].name[2] == 'S' &&
			LumpInfo[i].name[3] == 'K' &&
			LumpInfo[i].name[4] == 'I' &&
			LumpInfo[i].name[5] == 'N')
		{ // Wad has at least one skin.
			LumpInfo[i].name[6] = LumpInfo[i].name[7] = 0;
			if (!skinned)
			{
				skinned = true;
				size_t j;

				for (j = baselump; j < NumLumps; j++)
				{
					// Using the baselump as the namespace is safe, because
					// zdoom.wad guarantees the first possible baselump
					// passed to this function is a largish number.
					LumpInfo[j].namespc = baselump;
				}
			}
		}
	}
}

//==========================================================================
//
// BloodCrypt
//
//==========================================================================

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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void PrintLastError ()
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
#else
static void PrintLastError ()
{
}
#endif

// WadFileRecord ------------------------------------------------------------

FWadCollection::WadFileRecord::WadFileRecord (FILE *file)
: FileReader(file), Name(NULL), FirstLump(0), LastLump(0)
{
}

FWadCollection::WadFileRecord::~WadFileRecord ()
{
	if (Name != NULL)
	{
		delete[] Name;
	}
}

// FWadLump -----------------------------------------------------------------

FWadLump::FWadLump ()
: FileReader ()
{
}

FWadLump::FWadLump (const FWadLump &copy)
{
	File = copy.File;
}

FWadLump::FWadLump (const FileReader &other, long length)
: FileReader (other, length)
{
}

FWadLump::FWadLump (FILE *file, long length)
: FileReader (file, length)
{
}

// FMemLump -----------------------------------------------------------------

FMemLump::FMemLump ()
: Block (NULL)
{
}

#ifdef __GNUC__
FMemLump::FMemLump (const FMemLump &copy)
#else
FMemLump::FMemLump (FMemLump &copy)
#endif
{
	Block = copy.Block;
	const_cast<FMemLump *>(&copy)->Block = NULL;
}

#ifdef __GNUC__
FMemLump &FMemLump::operator = (const FMemLump &copy)
#else
FMemLump &FMemLump::operator = (FMemLump &copy)
#endif
{
	if (Block != NULL)
	{
		delete[] Block;
	}
	Block = copy.Block;
	const_cast<FMemLump *>(&copy)->Block = NULL;
	return *this;
}

FMemLump::FMemLump (BYTE *data)
: Block (data)
{
}

FMemLump::~FMemLump ()
{
	if (Block != NULL)
	{
		delete[] Block;
	}
}
