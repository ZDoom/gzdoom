
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

// MACROS ------------------------------------------------------------------

#define NULL_INDEX		(0xffff)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void W_AddFile (const char *filename);
void W_SysWadInit ();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void W_SkinHack (int baselump);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

lumpinfo_t *lumpinfo;
int numlumps;
WORD *FirstLumpIndex, *NextLumpIndex;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static DWORD numhashlumps;

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

void W_InitMultipleFiles (wadlist_t **filenames)
{
	int numfiles;

	// open all the files, load headers, and count lumps
	numfiles = 0;
	numlumps = 0;
	lumpinfo = NULL; // will be realloced as lumps are added

	while (*filenames)
	{
		wadlist_t *next = (*filenames)->next;
		int baselump = numlumps;
		char name[PATH_MAX];

		// [RH] Automatically append .wad extension if none is specified.
		strcpy (name, (*filenames)->name);
		FixPathSeperator (name);
		DefaultExtension (name, ".wad");

		W_AddFile (name);
		free (*filenames);
		*filenames = next;

		// The first two files are always zdoom.wad and the IWAD, which
		// do not contain skins.
		if (++numfiles > 2)
			W_SkinHack (baselump);
	}

	if (!numlumps)
	{
		I_FatalError ("W_InitMultipleFiles: no files found");
	}

	// [RH] Merge sprite and flat groups.
	//		(We don't need to bother with patches, since
	//		Doom doesn't use markers to identify them.)
	W_MergeLumps ("S_START", "S_END", ns_sprites);
	W_MergeLumps ("F_START", "F_END", ns_flats);
	W_MergeLumps ("C_START", "C_END", ns_colormaps);
	W_MergeLumps ("A_START", "A_END", ns_acslibrary);

	// [RH] Set up hash table
	FirstLumpIndex = new WORD[numlumps];
	NextLumpIndex = new WORD[numlumps];
	numhashlumps = numlumps;
	W_InitHashChains ();

	W_SysWadInit ();
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

bool W_CheckIfWadLoaded (const char *name)
{
	int i;

	if (strrchr (name, '/') != NULL)
	{
		for (i = 0; i < numwads; ++i)
		{
			if (stricmp (W_GetWadFullName (i), name) == 0)
			{
				return true;
			}
		}
	}
	else
	{
		for (i = 0; i < numwads; ++i)
		{
			if (stricmp (W_GetWadName (i), name) == 0)
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

int W_NumLumps (void)
{
	return numlumps;
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

int W_CheckNumForName (const char *name, int space)
{
	char uname[8];
	WORD i;

	uppercopy (uname, name);
	i = FirstLumpIndex[W_LumpNameHash (uname) % numhashlumps];

	while (i != NULL_INDEX &&
		(*(__int64 *)&lumpinfo[i].name != *(__int64 *)&uname ||
		 lumpinfo[i].namespc != space))
	{
		i = NextLumpIndex[i];
	}

	return i != NULL_INDEX ? i : -1;
}

int W_CheckNumForName (const char *name, int space, int wadnum)
{
	char uname[8];
	WORD i;

	if (wadnum < 0)
	{
		return W_CheckNumForName (name, space);
	}

	uppercopy (uname, name);
	i = FirstLumpIndex[W_LumpNameHash (uname) % numhashlumps];

	while (i != NULL_INDEX &&
		(*(__int64 *)&lumpinfo[i].name != *(__int64 *)&uname ||
		 lumpinfo[i].namespc != space ||
		 lumpinfo[i].wadnum != wadnum))
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

int W_GetNumForName (const char *name)
{
	int	i;

	i = W_CheckNumForName (name);

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

int W_LumpLength (int lump)
{
	if (lump >= numlumps)
	{
		I_Error ("W_LumpLength: %i >= numlumps",lump);
	}

	return lumpinfo[lump].size;
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

DWORD W_LumpNameHash (const char *s)
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

void W_InitHashChains (void)
{
	char name[8];
	unsigned int i, j;

	// Mark all buckets as empty
	memset (FirstLumpIndex, 255, numlumps*sizeof(FirstLumpIndex[0]));
	memset (NextLumpIndex, 255, numlumps*sizeof(FirstLumpIndex[0]));

	// Now set up the chains
	for (i = 0; i < (unsigned)numlumps; i++)
	{
		uppercopy (name, lumpinfo[i].name);
		j = W_LumpNameHash (name) % numhashlumps;
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

static BOOL IsMarker (const lumpinfo_t *lump, const char *marker)
{
	if (lump->namespc != ns_global)
	{
		return false;
	}
	if (strncmp (lump->name, marker, 8) == 0)
	{
		// If the previous lump was of the form FF_END and this one is
		// of the form F_END, ignore this as a marker
		if (marker[2] == 'E' && lump > lumpinfo)
		{
			if ((lump - 1)->name[0] == *marker &&
				strncmp ((lump - 1)->name + 1, marker, 7) == 0)
			{
				return false;
			}
		}
		return true;
	}
	if (lump->name[0] == *marker && strncmp (lump->name + 1, marker, 7) == 0)
	{
		return true;
	}
	return false;
}

//==========================================================================
//
// W_MergeLumps
//
// Merge multiple tagged groups into one
// Basically from BOOM, too, although I tried to write it independently.
//
//==========================================================================

void W_MergeLumps (const char *start, const char *end, int space)
{
	char ustart[8], uend[8];
	lumpinfo_t *newlumpinfos;
	int newlumps, oldlumps, i;
	BOOL insideBlock;
	int flatHack;

	uppercopy (ustart, start);
	uppercopy (uend, end);

	// Some pwads use an icky hack to get extra flats with regular Doom.
	// They have an F_END without a corresponding F_START or FF_START.
	// This tries to detect them.
	flatHack = 0;
	if (strcmp ("F_START", ustart) == 0 && !Args.CheckParm ("-noflathack"))
	{
		int fudge = 0, start = 0;

		for (i = 0; i < numlumps; i++)
		{
			if (IsMarker (lumpinfo + i, ustart))
				fudge++, start = i;
			else if (IsMarker (lumpinfo + i, uend))
				fudge--, flatHack = i;
		}
		if (start > flatHack)
			fudge--;
		if (fudge >= 0)
			flatHack = 0;
	}

	newlumpinfos = new lumpinfo_t[numlumps];

	newlumps = 0;
	oldlumps = 0;
	insideBlock = false;

	for (i = 0; i < numlumps; i++)
	{
		if (!insideBlock)
		{
			// Check if this is the start of a block
			if (IsMarker (lumpinfo + i, ustart))
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
				lumpinfo[oldlumps++] = lumpinfo[i];
			}
		}
		else
		{
			// Check if this is the end of a block
			if (flatHack)
			{
				if (flatHack == i)
				{
					insideBlock = false;
					flatHack = 0;
				}
				else
				{
					if (lumpinfo[i].size != 4096)
					{
						lumpinfo[oldlumps++] = lumpinfo[i];
					}
					else
					{
						newlumpinfos[newlumps] = lumpinfo[i];
						newlumpinfos[newlumps++].namespc = space;
					}
				}
			}
			else if (i && lumpinfo[i].wadnum != lumpinfo[i-1].wadnum)
			{
				// Blocks cannot span multiple files
				insideBlock = false;
				lumpinfo[oldlumps++] = lumpinfo[i];
			}
			else if (IsMarker (lumpinfo + i, uend))
			{
				// It is the end of a block. We'll add the end marker once
				// we've processed everything.
				insideBlock = false;
			}
			else
			{
				newlumpinfos[newlumps] = lumpinfo[i];
				newlumpinfos[newlumps++].namespc = space;
			}
		}
	}

	// Now copy the merged lumps to the end of the old list
	// and create the end marker entry.

	if (newlumps)
	{
		if (oldlumps + newlumps > numlumps)
			lumpinfo = (lumpinfo_t *)Realloc (lumpinfo, oldlumps + newlumps);

		memcpy (lumpinfo + oldlumps, newlumpinfos, sizeof(lumpinfo_t) * newlumps);

		numlumps = oldlumps + newlumps;
		
		strncpy (lumpinfo[numlumps].name, uend, 8);
		lumpinfo[numlumps].wadnum = -1;
		lumpinfo[numlumps].position =
			lumpinfo[numlumps].size = 0;
		lumpinfo[numlumps].namespc = ns_global;
		numlumps++;
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

int W_FindLump (const char *name, int *lastlump)
{
	char name8[8];
	lumpinfo_t *lump_p;

	uppercopy (name8, name);

	lump_p = lumpinfo + *lastlump;
	while (lump_p < lumpinfo + numlumps)
	{
		if (*(__int64 *)&lump_p->name == *(__int64 *)&name8)
		{
			int lump = lump_p - lumpinfo;
			*lastlump = lump + 1;
			return lump;
		}
		lump_p++;
	}

	*lastlump = numlumps;
	return -1;
}

//==========================================================================
//
// W_CheckLumpName
//
//==========================================================================

BOOL W_CheckLumpName (int lump, const char *name)
{
	if (lump >= numlumps)
		return false;

	return !strnicmp (lumpinfo[lump].name, name, 8);
}

//==========================================================================
//
// W_GetLumpName
//
//==========================================================================

void W_GetLumpName (char *to, int lump)
{
	if (lump >= numlumps)
		*to = 0;
	else
		uppercopy (to, lumpinfo[lump].name);
}

//==========================================================================
//
// W_GetLumpFile
//
//==========================================================================

int W_GetLumpFile (int lump)
{
	if (lump >= numlumps)
		return -1;
	return lumpinfo[lump].wadnum;
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

static void W_SkinHack (int baselump)
{
	bool skinned = false;
	int i;

	for (i = baselump; i < numlumps; i++)
	{
		if (lumpinfo[i].name[0] == 'S' &&
			lumpinfo[i].name[1] == '_' &&
			lumpinfo[i].name[2] == 'S' &&
			lumpinfo[i].name[3] == 'K' &&
			lumpinfo[i].name[4] == 'I' &&
			lumpinfo[i].name[5] == 'N')
		{ // Wad has at least one skin.
			lumpinfo[i].name[6] = lumpinfo[i].name[7] = 0;
			if (!skinned)
			{
				skinned = true;
				int j;

				for (j = baselump; j < numlumps; j++)
				{
					// Using the baselump as the namespace is safe, because
					// zdoom.wad guarantees the first possible baselump
					// passed to this function is a largish number.
					lumpinfo[j].namespc = baselump;
				}
			}
		}
	}
}
