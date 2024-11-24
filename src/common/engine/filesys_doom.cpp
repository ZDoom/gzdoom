/*
** filesys_doom.cpp
**
** the very special lump name lookup code for Doom's short names.
** Not useful in a generic system.
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2005-2024 Christoph Oelckers
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
**
*/

#include "filesystem.h"
#include "printf.h"

//==========================================================================
//
//
//
//==========================================================================

void FileSystem::InitHashChains()
{
	Super::InitHashChains();
	unsigned NumEntries = GetFileCount();
	for (unsigned i = 0; i < (unsigned)NumEntries; i++)
	{
		files[i].HashFirst = files[i].HashNext = NULL_INDEX;
	}
	// Now set up the chains
	for (unsigned i = 0; i < (unsigned)NumEntries; i++)
	{
		if (files[i].Namespace == ns_hidden || files[i].ShortName[0] == 0) continue;
		unsigned j = FileSys::MakeHash(files[i].ShortName, 8) % NumEntries;
		files[i].HashNext = files[j].HashFirst;
		files[j].HashFirst = i;
	}
}

//==========================================================================
//
//
//
//==========================================================================

static void UpperCopy(char* to, const char* from)
{
	int i;

	for (i = 0; i < 8 && from[i]; i++)
		to[i] = toupper(from[i]);
	for (; i < 8; i++)
		to[i] = 0;
	to[8] = 0;
}

//==========================================================================
//
//
//
//==========================================================================

void FileSystem::SetupName(int fileindex)
{
	const char* name = GetFileName(fileindex);
	int containerflags = GetContainerFlags(GetFileContainer(fileindex));
	int lflags = GetFileFlags(fileindex);

	if ((containerflags & wadflags) == wadflags)
	{
		UpperCopy(files[fileindex].ShortName, name);
	}
	else if ((lflags & FileSys::RESFF_EMBEDDED) || !*name)
	{
		files[fileindex].Namespace = ns_hidden;
	}
	else
	{
		if (lflags & FileSys::RESFF_FULLPATH) files[fileindex].Flags |= LUMPF_FULLPATH;	// copy for easier access in lookup function.
		auto slash = strrchr(name, '/');
		auto base = slash ? (slash + 1) : name;
		UpperCopy(files[fileindex].ShortName, base);
		auto dot = strrchr(files[fileindex].ShortName, '.');
		if (dot) while (*dot) *dot++ = 0;
	}
}


//==========================================================================
//
// IsMarker
//
// (from BOOM)
//
//==========================================================================

inline bool FileSystem::IsMarker(int lump, const char* marker) noexcept
{
	auto name = files[lump].ShortName;
	if (name[0] == marker[0])
	{
		return (!strcmp(name, marker) ||
			(marker[1] == '_' && !strcmp(name + 1, marker)));
	}
	else return false;
}

//==========================================================================
//
// SetNameSpace
//
// Sets namespace information for the lumps. It always looks for the first
// x_START and the last x_END lump, except when loading flats. In this case
// F_START may be absent and if that is the case all lumps with a size of
// 4096 will be flagged appropriately.
//
//==========================================================================

// This class was supposed to be local in the function but GCC
// does not like that.
struct Marker
{
	int markertype;
	int index;
};

void FileSystem::SetNamespace(int filenum, const char* startmarker, const char* endmarker, namespace_t space, FileSys::FileSystemMessageFunc Printf, bool flathack)
{
	using FileSys::FSMessageLevel;
	bool warned = false;
	int numstartmarkers = 0, numendmarkers = 0;
	TArray<Marker> markers;
	int FirstLump = GetFirstEntry(filenum);
	int LastLump = GetLastEntry(filenum);
	auto FileName = GetContainerName(filenum);

	for (int i = FirstLump; i <= LastLump; i++)
	{
		if (IsMarker(i, startmarker))
		{
			Marker m = { 0, i };
			markers.push_back(m);
			numstartmarkers++;
		}
		else if (IsMarker(i, endmarker))
		{
			Marker m = { 1, i };
			markers.push_back(m);
			numendmarkers++;
		}
	}

	if (numstartmarkers == 0)
	{
		if (numendmarkers == 0) return;	// no markers found

		if (Printf)
			Printf(FSMessageLevel::Warning, "%s: %s marker without corresponding %s found.\n", FileName, endmarker, startmarker);


		if (flathack)
		{
			// We have found no F_START but one or more F_END markers.
			// mark all lumps before the last F_END marker as potential flats.
			unsigned int end = markers[markers.size() - 1].index;
			for (int ii = FirstLump; ii <= LastLump; ii++)
			{
				if (FileLength(ii) == 4096)
				{
					// We can't add this to the flats namespace but 
					// it needs to be flagged for the texture manager.
					if (Printf) Printf(FSMessageLevel::DebugNotify, "%s: Marking %s as potential flat\n", FileName, files[ii].ShortName);
					files[ii].Namespace = ns_maybeflat;
				}
			}
		}
		return;
	}

	size_t i = 0;
	while (i < markers.size())
	{
		int start, end;
		if (markers[i].markertype != 0)
		{
			if (Printf) Printf(FSMessageLevel::Warning, "%s: %s marker without corresponding %s found.\n", FileName, endmarker, startmarker);
			i++;
			continue;
		}
		start = int(i++);

		// skip over subsequent x_START markers
		while (i < markers.size() && markers[i].markertype == 0)
		{
			if (Printf) Printf(FSMessageLevel::Warning, "%s: duplicate %s marker found.\n", FileName, startmarker);
			i++;
			continue;
		}
		// same for x_END markers
		while (i < markers.size() - 1 && (markers[i].markertype == 1 && markers[i + 1].markertype == 1))
		{
			if (Printf) Printf(FSMessageLevel::Warning, "%s: duplicate %s marker found.\n", FileName, endmarker);
			i++;
			continue;
		}
		// We found a starting marker but no end marker. Ignore this block.
		if (i >= markers.size())
		{
			if (Printf) Printf(FSMessageLevel::Warning, "%s: %s marker without corresponding %s found.\n", FileName, startmarker, endmarker);
			end = LastLump + 1;
		}
		else
		{
			end = markers[i++].index;
		}

		// we found a marked block
		if (Printf) Printf(FSMessageLevel::DebugNotify, "%s: Found %s block at (%d-%d)\n", FileName, startmarker, markers[start].index, end);
		for (int j = markers[start].index + 1; j < end; j++)
		{
			if (files[j].Namespace != ns_global)
			{
				if (!warned && Printf)
				{
					Printf(FSMessageLevel::Warning, "%s: Overlapping namespaces found (lump %d)\n", FileName, j);
				}
				warned = true;
			}
			else if (space == ns_sprites && FileLength(j) < 8)
			{
				// sf 26/10/99:
				// ignore sprite lumps smaller than 8 bytes (the smallest possible)
				// in size -- this was used by some dmadds wads
				// as an 'empty' graphics resource
				if (Printf) Printf(FSMessageLevel::DebugWarn, "%s: Skipped empty sprite %s (lump %d)\n", FileName, files[j].ShortName, j);
			}
			else
			{
				files[j].Namespace = space;
			}
		}
	}
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
//
//==========================================================================

void FileSystem::SkinHack(int filenum, FileSys::FileSystemMessageFunc Printf)
{
	using FileSys::FSMessageLevel;
	// this being static is not a problem. The only relevant thing is that each skin gets a different number.
	bool skinned = false;
	bool hasmap = false;

	int FirstLump = GetFirstEntry(filenum);
	int LastLump = GetLastEntry(filenum);
	auto FileName = GetContainerName(filenum);

	for (int i = FirstLump; i <= LastLump; i++)
	{
		auto lump = &files[i];

		if (!strnicmp(lump->ShortName, "S_SKIN", 6))
		{ // Wad has at least one skin.
			lump->ShortName[6] = 0;
			lump->ShortName[7] = 0;
			if (!skinned)
			{
				skinned = true;

				for (int j = FirstLump; j <= LastLump; j++)
				{
					files[j].Namespace = skin_namespc;
				}
				skin_namespc++;
			}
		}
		// needless to say, this check is entirely useless these days as map names can be more diverse..
		if ((lump->ShortName[0] == 'M' &&
			lump->ShortName[1] == 'A' &&
			lump->ShortName[2] == 'P' &&
			lump->ShortName[3] >= '0' && lump->ShortName[3] <= '9' &&
			lump->ShortName[4] >= '0' && lump->ShortName[4] <= '9' &&
			lump->ShortName[5] == '\0')
			||
			(lump->ShortName[0] == 'E' &&
				lump->ShortName[1] >= '0' && lump->ShortName[1] <= '9' &&
				lump->ShortName[2] == 'M' &&
				lump->ShortName[3] >= '0' && lump->ShortName[3] <= '9' &&
				lump->ShortName[4] == '\0'))
		{
			hasmap = true;
		}
	}
	if (skinned && hasmap && Printf)
	{
		Printf(FSMessageLevel::Attention, "%s: The maps will not be loaded because it has a skin.\n", FileName);
		Printf(FSMessageLevel::Attention, "You should remove the skin from the wad to play these maps.\n");
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FileSystem::SetupNamespace(int filenum, FileSys::FileSystemMessageFunc Printf)
{
	int flags = GetContainerFlags(filenum);

	// Set namespace for entries from WADs.
	if ((flags & wadflags) == wadflags)
	{
		SetNamespace(filenum, "S_START", "S_END", ns_sprites, Printf);
		SetNamespace(filenum, "F_START", "F_END", ns_flats, Printf, true);
		SetNamespace(filenum, "C_START", "C_END", ns_colormaps, Printf);
		SetNamespace(filenum, "A_START", "A_END", ns_acslibrary, Printf);
		SetNamespace(filenum, "TX_START", "TX_END", ns_newtextures, Printf);
		SetNamespace(filenum, "V_START", "V_END", ns_strifevoices, Printf);
		SetNamespace(filenum, "HI_START", "HI_END", ns_hires, Printf);
		SetNamespace(filenum, "VX_START", "VX_END", ns_voxels, Printf);
		SkinHack(filenum, Printf);
	}
	else if (!(flags & FResourceFile::NO_FOLDERS))
	{
		int FirstLump = GetFirstEntry(filenum);
		int LastLump = GetLastEntry(filenum);
		auto FileName = GetContainerName(filenum);

		for (int i = FirstLump; i <= LastLump; i++)
		{
			auto lump = &files[i];

			auto LongName = GetFileName(i);
			// Map some directories to WAD namespaces.
			// Note that some of these namespaces don't exist in WADS.
			// CheckNumForName will handle any request for these namespaces accordingly.
			int Namespace = !strncmp(LongName, "flats/", 6) ? ns_flats :
				!strncmp(LongName, "textures/", 9) ? ns_newtextures :
				!strncmp(LongName, "hires/", 6) ? ns_hires :
				!strncmp(LongName, "sprites/", 8) ? ns_sprites :
				!strncmp(LongName, "voxels/", 7) ? ns_voxels :
				!strncmp(LongName, "colormaps/", 10) ? ns_colormaps :
				!strncmp(LongName, "acs/", 4) ? ns_acslibrary :
				!strncmp(LongName, "voices/", 7) ? ns_strifevoices :
				!strncmp(LongName, "patches/", 8) ? ns_patches :
				!strncmp(LongName, "graphics/", 9) ? ns_graphics :
				!strncmp(LongName, "sounds/", 7) ? ns_sounds :
				!strncmp(LongName, "music/", 6) ? ns_music :
				!strchr(LongName, '/') ? ns_global :
				ns_hidden;

			lump->Namespace = Namespace;

			switch (Namespace)
			{
			case ns_hidden:
				memset(lump->ShortName, 0, sizeof(lump->ShortName));
				break;

			case ns_sprites:
			case ns_voxels:
			case ns_hires:
				// Since '\' can't be used as a file name's part inside a ZIP
				// we have to work around this for sprites because it is a valid
				// frame character.
				for (auto& c : lump->ShortName)
				{
					if (c == '^') c = '\\';
				}
				break;
			}
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================

bool FileSystem::InitFiles(std::vector<std::string>& filenames, FileSys::FileSystemFilterInfo* filter, FileSys::FileSystemMessageFunc Printf, bool allowduplicates)
{
	if (!Super::InitFiles(filenames, filter, Printf, allowduplicates)) return false;
	files.Resize(GetFileCount());
	memset(files.Data(), 0, sizeof(files[0]) * files.size());
	int numfiles = GetFileCount();
	for (int i = 0; i < numfiles; i++)
	{
		SetupName(i);
	}

	int numresfiles = GetContainerCount();
	for (int i = 0; i < numresfiles; i++)
	{
		SetupNamespace(i, Printf);
	}
	return true;
}

//==========================================================================
//
// CheckNumForName
//
// Returns -1 if name not found. The version with a third parameter will
// look exclusively in the specified wad for the lump.
//
// [RH] Changed to use hash lookup ala BOOM instead of a linear search
// and namespace parameter
//==========================================================================

int FileSystem::CheckNumForName(const char* name, int space) const
{
	char uname[9];
	uint32_t i;

	if (name == nullptr)
	{
		return -1;
	}

	// Let's not search for names that are longer than 8 characters and contain path separators
	// They are almost certainly full path names passed to this function.
	if (strlen(name) > 8 && strpbrk(name, "/."))
	{
		return -1;
	}

	UpperCopy(uname, name);
	i = files[FileSys::MakeHash(uname, 8) % files.Size()].HashFirst;

	while (i != NULL_INDEX)
	{
		auto& lump = files[i];

		if (!memcmp(lump.ShortName, uname, 8))
		{
			if (lump.Namespace == space) break;
			// If the lump is from one of the special namespaces exclusive to Zips
			// the check has to be done differently:
			// If we find a lump with this name in the global namespace that does not come
			// from a Zip return that. WADs don't know these namespaces and single lumps must
			// work as well.
			if (space > ns_specialzipdirectory && lump.Namespace == ns_global && !(lump.Flags & LUMPF_FULLPATH))
				break;
		}
		i = lump.HashNext;
	}

	return i != NULL_INDEX ? i : -1;
}

int FileSystem::CheckNumForName(const char* name, int space, int rfnum, bool exact) const
{
	char uname[9];
	uint32_t i;

	if (rfnum < 0)
	{
		return CheckNumForName(name, space);
	}

	UpperCopy(uname, name);
	i = files[FileSys::MakeHash(uname, 8) % files.Size()].HashFirst;

	// If exact is true if will only find lumps in the same WAD, otherwise
	// also those in earlier WADs.

	while (i != NULL_INDEX &&
		(memcmp(files[i].ShortName, uname, 8) || files[i].Namespace != space ||
			(exact ? (GetFileContainer(i) != rfnum) : (GetFileContainer(i) > rfnum))))
	{
		i = files[i].HashNext;
	}

	return i != NULL_INDEX ? i : -1;
}

//==========================================================================
//
// GetNumForName
//
// Calls CheckNumForName, but bombs out if not found.
//
//==========================================================================

int FileSystem::GetNumForName(const char* name, int space) const
{
	int	i;

	i = CheckNumForName(name, space);

	if (i == -1)
		throw FileSys::FileSystemException("GetNumForName: %s not found!", name);

	return i;
}



//==========================================================================
//
// returns a modifiable pointer to the short name
// 
// should only be called before the hash chains are set up.
// If done later this needs rehashing.
// 
// This is for custom setup through postprocessFunc
//
//==========================================================================

char* FileSystem::GetShortName(int i)
{
	if ((unsigned)i >= files.Size()) 
		throw FileSys::FileSystemException("GetShortName: Invalid index");
	return files[i].ShortName;
}

//==========================================================================
//
// W_FindLump
//
// Find a named lump. Specifically allows duplicates for merging of e.g.
// SNDINFO lumps.
//
//==========================================================================

int FileSystem::FindLump(const char* name, int* lastlump, bool anyns)
{
	if ((size_t)*lastlump >= files.size()) return -1;
	char name8[9];
	UpperCopy(name8, name);

	assert(lastlump != nullptr && *lastlump >= 0);

	const int last = (int)files.size();

	for(int lump = *lastlump; lump < last; lump++)
	{
		const FileEntry* const lump_p = &files[lump];
		if ((anyns || lump_p->Namespace == ns_global) && !memcmp(lump_p->ShortName, name8, 8))
		{
			*lastlump = lump + 1;
			return lump;
		}
	}

	*lastlump = last;
	return -1;
}

//==========================================================================
//
// W_FindLumpMulti
//
// Find a named lump. Specifically allows duplicates for merging of e.g.
// SNDINFO lumps. Returns everything having one of the passed names.
//
//==========================================================================

int FileSystem::FindLumpMulti(const char** names, int* lastlump, bool anyns, int* nameindex)
{
	assert(lastlump != nullptr && *lastlump >= 0);

	int last = files.Size();

	for (int lump = *lastlump; lump < last; lump++)
	{
		auto lump_p = &files[lump];
		if (anyns || lump_p->Namespace == ns_global)
		{
			for (const char** name = names; *name != nullptr; name++)
			{
				if (!strnicmp(*name, lump_p->ShortName, 8))
				{
					*lastlump = lump + 1;
					if (nameindex != nullptr) *nameindex = int(name - names);
					return lump;
				}
			}
		}
	}

	*lastlump = last;
	return -1;
}

//==========================================================================
//
// This function combines lookup from regular lists and Doom's special one.
//
//==========================================================================

int FileSystem::CheckNumForAnyName(const char* name, namespace_t namespc) const
{
	if (name != NULL)
	{
		// Check short names first to avoid interference from short names in the real file system.
		if (strlen(name) <= 8 && !strpbrk(name, "./"))
		{
			return CheckNumForName(name, namespc);
		}

		int lookup = Super::FindFile(name, false);
		if (lookup >= 0) return lookup;
	}
	return -1;
}

