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
// Open it
//
//==========================================================================

static bool OpenWAD(FResourceFile* file, FileSystemFilterInfo*, FileSystemMessageFunc Printf)
{
	wadinfo_t header;
	uint32_t InfoTableOfs;
	bool isBigEndian = false; // Little endian is assumed until proven otherwise
	auto Reader = file->GetContainerReader();
	auto wadSize = Reader->GetLength();

	Reader->Read(&header, sizeof(header));
	uint32_t NumLumps = LittleLong(header.NumLumps);
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
			Printf(FSMessageLevel::Error, "%s: Bad directory offset.\n", file->GetFileName());
			return false;
		}
	}

	Reader->Seek(InfoTableOfs, FileReader::SeekSet);
	auto fd = Reader->Read(NumLumps * sizeof(wadlump_t));
	auto fileinfo = (const wadlump_t*)fd.data();

	auto Entries = file->AllocateEntries(NumLumps);

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
			Printf(FSMessageLevel::Warning, "%s: Lump name %.8s contains invalid characters\n", file->GetFileName(), fileinfo[i].Name);
			n[0] = 0;
		}

		Entries[i].FileName = nullptr;
		Entries[i].Position = isBigEndian ? BigLong(fileinfo[i].FilePos) : LittleLong(fileinfo[i].FilePos);
		Entries[i].CompressedSize = Entries[i].Length = isBigEndian ? BigLong(fileinfo[i].Size) : LittleLong(fileinfo[i].Size);

		Entries[i].Flags = ishigh? RESFF_SHORTNAME | RESFF_COMPRESSED : RESFF_SHORTNAME;
		Entries[i].Method = ishigh == 1? METHOD_LZSS : METHOD_STORED;
		Entries[i].FileName = file->NormalizeFileName(n, 0, true);
		// This doesn't set up the namespace yet.
	}
	for (uint32_t i = 0; i < NumLumps; i++)
	{
		if (Entries[i].Method == METHOD_LZSS)
		{
			// compressed size is implicit.
			Entries[i].CompressedSize = (i == NumLumps - 1 ? Reader->GetLength() : Entries[i + 1].Position) - Entries[i].Position;
		}
	}

	file->GenerateHash(); // Do this before the lump processing below.
	return true;
}

//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckWad(const char *filename, FileReader &file, FileSystemFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	char head[4];

	if (file.GetLength() >= 12)
	{
		file.Seek(0, FileReader::SeekSet);
		file.Read(&head, 4);
		file.Seek(0, FileReader::SeekSet);
		if (!memcmp(head, "IWAD", 4) || !memcmp(head, "PWAD", 4))
		{
			auto rf = new FResourceFile(filename, file, sp, FResourceFile::NO_FOLDERS | FResourceFile::NO_EXTENSIONS | FResourceFile::SHORTNAMES);
			if (OpenWAD(rf, filter, Printf)) return rf;

			file = rf->Destroy();
		}
	}
	return NULL;
}

}
