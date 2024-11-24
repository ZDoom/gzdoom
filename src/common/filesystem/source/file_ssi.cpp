/*
** file_grp.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright 2005-2020 Christoph Oelckers
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

#include "resourcefile.h"

namespace FileSys {

//==========================================================================
//
// Open it
// Note that SSIs can contain embedded GRPs which must be flagged accordingly.
//
//==========================================================================

static bool OpenSSI(FResourceFile* file, int version, int EntryCount, FileSystemFilterInfo*)
{
	uint32_t NumLumps = EntryCount * 2;
	auto Entries = file->AllocateEntries(NumLumps);
	auto Reader = file->GetContainerReader();
	

	int32_t j = (version == 2 ? 267 : 254) + (EntryCount * 121);
	for (uint32_t i = 0; i < NumLumps; i+=2)
	{
		char fn[13];
		int strlength = Reader->ReadUInt8();
		if (strlength > 12) strlength = 12;

		Reader->Read(fn, 12);
		fn[strlength] = 0;
		int flength = Reader->ReadInt32();

		Entries[i].Position = j;
		Entries[i].CompressedSize = Entries[i].Length = flength;
		Entries[i].Flags = 0;
		Entries[i].Method = METHOD_STORED;
		Entries[i].ResourceID = -1;
		Entries[i].FileName = file->NormalizeFileName(fn);
		if (strstr(fn, ".GRP")) Entries[i].Flags |= RESFF_EMBEDDED;

		// SSI files can swap the order of the extension's characters - but there's no reliable detection for this and it can be mixed inside the same container, 
		// so we have no choice but to create another file record for the altered name.
		std::swap(fn[strlength - 1], fn[strlength - 3]);

		Entries[i + 1].Position = j;
		Entries[i + 1].CompressedSize = Entries[i + 1].Length = flength;
		Entries[i + 1].Flags = 0;
		Entries[i + 1].ResourceID = -1;
		Entries[i + 1].FileName = file->NormalizeFileName(fn);
		Entries[i + 1].Method = METHOD_STORED;
		if (strstr(fn, ".GRP")) Entries[i + 1].Flags |= RESFF_EMBEDDED;

		j += flength;

		Reader->Seek(104, FileReader::SeekCur);
		file->GenerateHash();
	}
	return true;
}


//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile* CheckSSI(const char* filename, FileReader& file, FileSystemFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	char zerobuf[72];
	char buf[72];
	memset(zerobuf, 0, 72);

	auto skipstring = [&](size_t length)
	{
		size_t strlength = file.ReadUInt8();
		if (strlength > length) return false;
		size_t count = file.Read(buf, length);
		buf[length] = 0;
		if (count != length || strlen(buf) != strlength) return false;
		if (length != strlength && memcmp(buf + strlength, zerobuf, length - strlength)) return false;
		return true;
	};
	if (file.GetLength() >= 12)
	{
		// check if SSI
		// this performs several checks because there is no "SSI" magic
		int version = file.ReadInt32();
		if (version == 1 || version == 2) // if
		{
			int numfiles = file.ReadInt32();
			if (!skipstring(32)) return nullptr;
			if (version == 2 && !skipstring(12)) return nullptr;
			for (int i = 0; i < 3; i++)
			{
				if (!skipstring(70)) return nullptr;
			}
			auto ssi = new FResourceFile(filename, file, sp, FResourceFile::NO_FOLDERS | FResourceFile::SHORTNAMES);
			if (OpenSSI(ssi, version, numfiles, filter)) return ssi;
			file = ssi->Destroy();
		}
	}
	return nullptr;
}

}
