/*
** file_mvl.cpp
**
** reads Descent2 .mvl files
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



static bool OpenMvl(FResourceFile* rf, FileSystemFilterInfo* filter)
{
    auto Reader = rf->GetContainerReader();
    auto count = Reader->ReadUInt32();
    auto Entries = rf->AllocateEntries(count);
    size_t pos = 8 + (17 * count);   // files start after the directory

    for (uint32_t i = 0; i < count; i++)
    {
        char name[13];
        Reader->Read(&name, 13);
        name[12] = 0;
        uint32_t elength = Reader->ReadUInt32();

        Entries[i].Position = pos;
        Entries[i].CompressedSize = Entries[i].Length = elength;
        Entries[i].ResourceID = -1;
        Entries[i].FileName = rf->NormalizeFileName(name);

        pos += elength;
    }

    return true;
}


//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile* CheckMvl(const char* filename, FileReader& file, FileSystemFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
    char head[4];

    if (file.GetLength() >= 20)
    {
        file.Seek(0, FileReader::SeekSet);
        file.Read(&head, 4);
        if (!memcmp(head, "DMVL", 4))
        {
            auto rf = new FResourceFile(filename, file, sp, FResourceFile::NO_FOLDERS | FResourceFile::SHORTNAMES);
            if (OpenMvl(rf, filter)) return rf;
            file = rf->Destroy();
        }
        file.Seek(0, FileReader::SeekSet);
    }
    return nullptr;
}


}