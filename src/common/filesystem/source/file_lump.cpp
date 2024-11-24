/*
** file_lump.cpp
**
**---------------------------------------------------------------------------
** Copyright 2009 Christoph Oelckers
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
//
//==========================================================================

static bool OpenLump(FResourceFile* file, FileSystemFilterInfo*)
{
	auto Entries = file->AllocateEntries(1);
	Entries[0].FileName = file->NormalizeFileName(ExtractBaseName(file->GetFileName(), true).c_str());
	Entries[0].ResourceID = -1;
	Entries[0].Position = 0;
	Entries[0].CompressedSize = Entries[0].Length = file->GetContainerReader()->GetLength();
	Entries[0].Method = METHOD_STORED;
	Entries[0].Flags = 0;
	return true;
}

//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckLump(const char *filename, FileReader &file, FileSystemFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	// always succeeds
	auto rf = new FResourceFile(filename, file, sp, FResourceFile::NO_FOLDERS);
	if (OpenLump(rf, filter)) return rf;
	file = rf->Destroy();
	return NULL;
}

}
