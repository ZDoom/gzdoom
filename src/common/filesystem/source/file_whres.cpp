/*
** file_whres.cpp
**
** reads a Witchaven/TekWar sound resource file
**
**---------------------------------------------------------------------------
** Copyright 2009-2019 Christoph Oelckers
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

#include "resourcefile_internal.h"
#include "fs_stringpool.h"
#include "fs_swap.h"

namespace FileSys {
	using namespace byteswap;

//==========================================================================
//
// WH resource file
//
//==========================================================================

class FWHResFile : public FUncompressedFile
{
	const char* BaseName;
public:
	FWHResFile(const char * filename, FileReader &file, StringPool* sp);
	bool Open(LumpFilterInfo* filter);
};


//==========================================================================
//
//
//
//==========================================================================

FWHResFile::FWHResFile(const char *filename, FileReader &file, StringPool* sp)
	: FUncompressedFile(filename, file, sp)
{
	BaseName = stringpool->Strdup(ExtractBaseName(filename, false).c_str());
}

//==========================================================================
//
// Open it
//
//==========================================================================

bool FWHResFile::Open(LumpFilterInfo*)
{
	uint32_t directory[1024];

	Reader.Seek(-4096, FileReader::SeekEnd);
	Reader.Read(directory, 4096);

	int nl =1024/3;
	Lumps.Resize(nl);


	int i = 0;
	for(int k = 0; k < nl; k++)
	{
		uint32_t offset = LittleLong(directory[k*3]) * 4096;
		uint32_t length = LittleLong(directory[k*3+1]);
		if (length == 0) break;
		char num[6];
		snprintf(num, 6, "/%04d", k);
		std::string synthname = BaseName;
		synthname += num;
		Lumps[i].LumpNameSetup(synthname.c_str(), stringpool);
		Lumps[i].Owner = this;
		Lumps[i].Position = offset;
		Lumps[i].LumpSize = length;
		i++;
	}
	NumLumps = i;
	Lumps.Clamp(NumLumps);
	Lumps.ShrinkToFit();
	return true;
}


//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckWHRes(const char *filename, FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	if (file.GetLength() >= 8192) // needs to be at least 8192 to contain one file and the directory.
	{
		unsigned directory[1024];
		int nl =1024/3;

		file.Seek(-4096, FileReader::SeekEnd);
		file.Read(directory, 4096);
		auto size = file.GetLength();

		uint32_t checkpos = 0;
		for(int k = 0; k < nl; k++)
		{
			unsigned offset = LittleLong(directory[k*3]);
			unsigned length = LittleLong(directory[k*3+1]);
			if (length <= 0 && offset == 0) break;
			if (offset != checkpos || length == 0 || offset + length >= (size_t)size - 4096 ) return nullptr;
			checkpos += (length+4095) / 4096;
		}
		auto rf = new FWHResFile(filename, file, sp);
		if (rf->Open(filter)) return rf;
		file = std::move(rf->Reader); // to avoid destruction of reader
		delete rf;
	}
	return NULL;
}
 
}