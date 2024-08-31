/*
** file_wad.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright 2005-2009 Christoph Oelckers
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

#include <ctype.h>
#include "resourcefile.h"
#include "fs_filesystem.h"
#include "fs_swap.h"
#include "fs_stringpool.h"
#include "resourcefile.h"

namespace FileSys {
	using namespace byteswap;

struct wadinfo_t
{
	// Should be "IWAD" or "PWAD".
	uint32_t		Magic;
	uint32_t		NumLumps;
	uint32_t		InfoTableOfs;
};

struct wadlump_t
{
	uint32_t		FilePos;
	uint32_t		Size;
	char		Name[8];
};

//==========================================================================
//
// Wad file
//
//==========================================================================

class FWadFile : public FResourceFile
{
	bool IsMarker(int lump, const char *marker);
	void SetNamespace(const char *startmarker, const char *endmarker, namespace_t space, FileSystemMessageFunc Printf, bool flathack=false);
	void SkinHack (FileSystemMessageFunc Printf);

public:
	FWadFile(const char * filename, FileReader &file, StringPool* sp);
	bool Open(LumpFilterInfo* filter, FileSystemMessageFunc Printf);
};


//==========================================================================
//
// FWadFile::FWadFile
//
// Initializes a WAD file
//
//==========================================================================

FWadFile::FWadFile(const char *filename, FileReader &file, StringPool* sp)
	: FResourceFile(filename, file, sp)
{
}

//==========================================================================
//
// Open it
//
//==========================================================================

bool FWadFile::Open(LumpFilterInfo*, FileSystemMessageFunc Printf)
{
	wadinfo_t header;
	uint32_t InfoTableOfs;
	bool isBigEndian = false; // Little endian is assumed until proven otherwise
	auto wadSize = Reader.GetLength();

	Reader.Read(&header, sizeof(header));
	NumLumps = LittleLong(header.NumLumps);
	InfoTableOfs = LittleLong(header.InfoTableOfs);

	// Check to see if the little endian interpretation is valid
	// This should be sufficient to detect big endian wads.
	if (InfoTableOfs + NumLumps*sizeof(wadlump_t) > (unsigned)wadSize)
	{
		NumLumps = BigLong(header.NumLumps);
		InfoTableOfs = BigLong(header.InfoTableOfs);
		isBigEndian = true;

		// Check again to detect broken wads
		if (InfoTableOfs + NumLumps*sizeof(wadlump_t) > (unsigned)wadSize)
		{
			Printf(FSMessageLevel::Error, "%s: Bad directory offset.\n", FileName);
			return false;
		}
	}

	Reader.Seek(InfoTableOfs, FileReader::SeekSet);
	auto fd = Reader.Read(NumLumps * sizeof(wadlump_t));
	auto fileinfo = (const wadlump_t*)fd.data();

	AllocateEntries(NumLumps);

	for(uint32_t i = 0; i < NumLumps; i++)
	{
		// WAD only supports ASCII. It is also the only format which can use valid backslashes in its names.
		char n[9];
		int ishigh = 0;
		for (int j = 0; j < 8; j++)
		{
			if (fileinfo[i].Name[j] & 0x80) ishigh |= 1 << j;
			n[j] = tolower(fileinfo[i].Name[j]);
		}
		n[8] = 0;
		if (ishigh == 1) n[0] &= 0x7f;
		else if (ishigh > 1)
		{
			// This may not end up printing something proper because we do not know what encoding might have been used.
			Printf(FSMessageLevel::Warning, "%s: Lump name %.8s contains invalid characters\n", FileName, fileinfo[i].Name);
		}

		Entries[i].FileName = nullptr;
		Entries[i].Position = isBigEndian ? BigLong(fileinfo[i].FilePos) : LittleLong(fileinfo[i].FilePos);
		Entries[i].CompressedSize = Entries[i].Length = isBigEndian ? BigLong(fileinfo[i].Size) : LittleLong(fileinfo[i].Size);

		Entries[i].Namespace = ns_global;
		Entries[i].Flags = ishigh? RESFF_SHORTNAME | RESFF_COMPRESSED : RESFF_SHORTNAME;
		Entries[i].Method = ishigh == 1? METHOD_LZSS : METHOD_STORED;
		Entries[i].FileName = stringpool->Strdup(n);
		// This doesn't set up the namespace yet.
	}
	for (uint32_t i = 0; i < NumLumps; i++)
	{
		if (Entries[i].Method == METHOD_LZSS)
		{
			// compressed size is implicit.
			Entries[i].CompressedSize = (i == NumLumps - 1 ? Reader.GetLength() : Entries[i + 1].Position) - Entries[i].Position;
		}
	}


	GenerateHash(); // Do this before the lump processing below.

	SetNamespace("s_start", "s_end", ns_sprites, Printf);
	SetNamespace("f_start", "f_end", ns_flats, Printf, true);
	SetNamespace("c_start", "c_end", ns_colormaps, Printf);
	SetNamespace("a_start", "a_end", ns_acslibrary, Printf);
	SetNamespace("tx_start", "tx_end", ns_newtextures, Printf);
	SetNamespace("v_start", "v_end", ns_strifevoices, Printf);
	SetNamespace("hi_start", "hi_end", ns_hires, Printf);
	SetNamespace("vx_start", "vx_end", ns_voxels, Printf);
	SkinHack(Printf);

	return true;
}

//==========================================================================
//
// IsMarker
//
// (from BOOM)
//
//==========================================================================

inline bool FWadFile::IsMarker(int lump, const char *marker)
{
	if (Entries[lump].FileName[0] == marker[0])
	{
		return (!strcmp(Entries[lump].FileName, marker) ||
			(marker[1] == '_' && !strcmp(Entries[lump].FileName +1, marker)));
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
	unsigned int index;
};

void FWadFile::SetNamespace(const char *startmarker, const char *endmarker, namespace_t space, FileSystemMessageFunc Printf, bool flathack)
{
	bool warned = false;
	int numstartmarkers = 0, numendmarkers = 0;
	unsigned int i;
	std::vector<Marker> markers;

	for(i = 0; i < NumLumps; i++)
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

		Printf(FSMessageLevel::Warning, "%s: %s marker without corresponding %s found.\n", FileName, endmarker, startmarker);


		if (flathack)
		{
			// We have found no F_START but one or more F_END markers.
			// mark all lumps before the last F_END marker as potential flats.
			unsigned int end = markers[markers.size()-1].index;
			for(unsigned int ii = 0; ii < end; ii++)
			{
				if (Entries[ii].Length == 4096)
				{
					// We can't add this to the flats namespace but 
					// it needs to be flagged for the texture manager.
					Printf(FSMessageLevel::DebugNotify, "%s: Marking %s as potential flat\n", FileName, Entries[ii].FileName);
					Entries[ii].Flags |= RESFF_MAYBEFLAT;
				}
			}
		}
		return;
	}

	i = 0;
	while (i < markers.size())
	{
		int start, end;
		if (markers[i].markertype != 0)
		{
			Printf(FSMessageLevel::Warning, "%s: %s marker without corresponding %s found.\n", FileName, endmarker, startmarker);
			i++;
			continue;
		}
		start = i++;

		// skip over subsequent x_START markers
		while (i < markers.size() && markers[i].markertype == 0)
		{
			Printf(FSMessageLevel::Warning, "%s: duplicate %s marker found.\n", FileName, startmarker);
			i++;
			continue;
		}
		// same for x_END markers
		while (i < markers.size()-1 && (markers[i].markertype == 1 && markers[i+1].markertype == 1))
		{
			Printf(FSMessageLevel::Warning, "%s: duplicate %s marker found.\n", FileName, endmarker);
			i++;
			continue;
		}
		// We found a starting marker but no end marker. Ignore this block.
		if (i >= markers.size())
		{
			Printf(FSMessageLevel::Warning, "%s: %s marker without corresponding %s found.\n", FileName, startmarker, endmarker);
			end = NumLumps;
		}
		else
		{
			end = markers[i++].index;
		}

		// we found a marked block
		Printf(FSMessageLevel::DebugNotify, "%s: Found %s block at (%d-%d)\n", FileName, startmarker, markers[start].index, end);
		for(int j = markers[start].index + 1; j < end; j++)
		{
			if (Entries[j].Namespace != ns_global)
			{
				if (!warned)
				{
					Printf(FSMessageLevel::Warning, "%s: Overlapping namespaces found (lump %d)\n", FileName, j);
				}
				warned = true;
			}
			else if (space == ns_sprites && Entries[j].Length < 8)
			{
				// sf 26/10/99:
				// ignore sprite lumps smaller than 8 bytes (the smallest possible)
				// in size -- this was used by some dmadds wads
				// as an 'empty' graphics resource
				Printf(FSMessageLevel::DebugWarn, "%s: Skipped empty sprite %s (lump %d)\n", FileName, Entries[j].FileName, j);
			}
			else
			{
				Entries[j].Namespace = space;
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

void FWadFile::SkinHack (FileSystemMessageFunc Printf)
{
	// this being static is not a problem. The only relevant thing is that each skin gets a different number.
	static int namespc = ns_firstskin;
	bool skinned = false;
	bool hasmap = false;
	uint32_t i;

	for (i = 0; i < NumLumps; i++)
	{
		auto lump = &Entries[i];

		if (!strnicmp(lump->FileName, "S_SKIN", 6))
		{ // Wad has at least one skin.
			lump->FileName = "S_SKIN";
			if (!skinned)
			{
				skinned = true;
				uint32_t j;

				for (j = 0; j < NumLumps; j++)
				{
					Entries[j].Namespace = namespc;
				}
				namespc++;
			}
		}
		// needless to say, this check is entirely useless these days as map names can be more diverse..
		if ((lump->FileName[0] == 'M' &&
			 lump->FileName[1] == 'A' &&
			 lump->FileName[2] == 'P' &&
			 lump->FileName[3] >= '0' && lump->FileName[3] <= '9' &&
			 lump->FileName[4] >= '0' && lump->FileName[4] <= '9' &&
			 lump->FileName[5] == '\0')
			||
			(lump->FileName[0] == 'E' &&
			 lump->FileName[1] >= '0' && lump->FileName[1] <= '9' &&
			 lump->FileName[2] == 'M' &&
			 lump->FileName[3] >= '0' && lump->FileName[3] <= '9' &&
			 lump->FileName[4] == '\0'))
		{
			hasmap = true;
		}
	}
	if (skinned && hasmap)
	{
		Printf(FSMessageLevel::Attention, "%s: The maps will not be loaded because it has a skin.\n", FileName);
		Printf(FSMessageLevel::Attention, "You should remove the skin from the wad to play these maps.\n");
	}
}


//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckWad(const char *filename, FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	char head[4];

	if (file.GetLength() >= 12)
	{
		file.Seek(0, FileReader::SeekSet);
		file.Read(&head, 4);
		file.Seek(0, FileReader::SeekSet);
		if (!memcmp(head, "IWAD", 4) || !memcmp(head, "PWAD", 4))
		{
			auto rf = new FWadFile(filename, file, sp);
			if (rf->Open(filter, Printf)) return rf;

			file = rf->Destroy();
		}
	}
	return NULL;
}

}
