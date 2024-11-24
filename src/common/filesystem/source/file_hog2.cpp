/*
** file_hog.cpp
**
** reads Descent 3 .hog2 files
**
**---------------------------------------------------------------------------
** Copyright 2024 Christoph Oelckers
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
#include "fs_swap.h"

namespace FileSys {
    using namespace byteswap;



static bool OpenHog2(FResourceFile* rf, FileSystemFilterInfo* filter)
{
    auto Reader = rf->GetContainerReader();
    FileReader::Size length = Reader->GetLength();

	uint32_t numfiles = Reader->ReadUInt32();
	uint32_t offset = Reader->ReadUInt32();	    // offset to first file (end of file list)
	Reader->Seek(56, FileReader::SeekEnd);		// filled with FF
    auto Entries = rf->AllocateEntries((int)numfiles);
	
    for(uint32_t i = 0; i < numfiles; i++)
	{
        char name[37];
		Reader->Read(name, 36);
		name[36] = 0;
		Reader->ReadUInt32();
		uint32_t size = Reader->ReadUInt32();;
		Reader->ReadUInt32();
		
		FResourceEntry& Entry = Entries[i];
        Entry.Position = offset;
        Entry.CompressedSize = Entry.Length = size;
        Entry.Flags = RESFF_FULLPATH;
        Entry.CRC32 = 0;
        Entry.ResourceID = -1;
        Entry.Method = METHOD_STORED;
        Entry.FileName = rf->NormalizeFileName(name);
		offset += size;
    }
    rf->GenerateHash();
    return true;
}


//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile* CheckHog2(const char* filename, FileReader& file, FileSystemFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
    char head[4];

    if (file.GetLength() >= 68)
    {
        file.Seek(0, FileReader::SeekSet);
        file.Read(&head, 4);
        if (!memcmp(head, "HOG2", 4))
        {
            auto rf = new FResourceFile(filename, file, sp, 0);
            if (OpenHog2(rf, filter)) return rf;
            file = rf->Destroy();
        }
        file.Seek(0, FileReader::SeekSet);
    }
    return nullptr;
}


}

