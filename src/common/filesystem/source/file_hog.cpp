/*
** file_hog.cpp
**
** reads Descent .hog files
**
**---------------------------------------------------------------------------
** Copyright 2023 Christoph Oelckers
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




static bool OpenHog(FResourceFile* rf, LumpFilterInfo* filter)
{
    auto Reader = rf->GetContainerReader();
    FileReader::Size length = Reader->GetLength();

    std::vector<FResourceEntry> entries;
    // Hogs store their data as a list of file records, each containing a name, length and the actual data.
    // To read the directory the entire file must be scanned.
    while (Reader->Tell() <= length)
    {
        char name[13];

        auto r = Reader->Read(&name, 13);
        if (r < 13) break;
        name[12] = 0;
        uint32_t elength = Reader->ReadUInt32();

        FResourceEntry Entry;
        Entry.Position = Reader->Tell();
        Entry.CompressedSize = Entry.Length = elength;
        Entry.Flags = 0;
        Entry.CRC32 = 0;
        Entry.Namespace = ns_global;
        Entry.ResourceID = -1;
        Entry.Method = METHOD_STORED;
        Entry.FileName = rf->NormalizeFileName(name);
        entries.push_back(Entry);
        Reader->Seek(elength, FileReader::SeekCur);
    }
    auto Entries = rf->AllocateEntries((int)entries.size());
    memcpy(Entries, entries.data(), entries.size() * sizeof(Entries[0]));
    rf->GenerateHash();
    return true;
}


//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile* CheckHog(const char* filename, FileReader& file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
    char head[3];

    if (file.GetLength() >= 20)
    {
        file.Seek(0, FileReader::SeekSet);
        file.Read(&head, 3);
        if (!memcmp(head, "DHF", 3))
        {
            auto rf = new FResourceFile(filename, file, sp);
            if (OpenHog(rf, filter)) return rf;
            file = rf->Destroy();
        }
        file.Seek(0, FileReader::SeekSet);
    }
    return nullptr;
}


}