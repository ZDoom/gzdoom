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

#include "resourcefile.h"
#include "printf.h"
#include "cmdlib.h"

//==========================================================================
//
//
//
//==========================================================================

struct whresentry
{
	int		filepospage, filelen, priority;
} ;

struct dpackheader_t
{
	int		ident;		// == IDPAKHEADER
	int		dirofs;
	int		dirlen;
} ;


//==========================================================================
//
// Wad file
//
//==========================================================================

class FWHResFile : public FUncompressedFile
{
	FString basename;
public:
	FWHResFile(const char * filename, FileReader &file);
	bool Open(bool quiet, LumpFilterInfo* filter);
};


//==========================================================================
//
// FWadFile::FWadFile
//
// Initializes a WAD file
//
//==========================================================================

FWHResFile::FWHResFile(const char *filename, FileReader &file) 
	: FUncompressedFile(filename, file)
{
	basename = ExtractFileBase(filename, false);
}

//==========================================================================
//
// Open it
//
//==========================================================================

bool FWHResFile::Open(bool quiet, LumpFilterInfo*)
{
	int directory[1024];

	Reader.Seek(-4096, FileReader::SeekEnd);
	Reader.Read(directory, 4096);

	int nl =1024/3;
	Lumps.Resize(nl);


	int i = 0;
	for(int k = 0; k < nl; k++)
	{
		int offset = LittleLong(directory[k*3]) * 4096;
		int length = LittleLong(directory[k*3+1]);
		if (length <= 0) break;
		FStringf synthname("%s/%04d", basename.GetChars(), k);
		Lumps[i].LumpNameSetup(synthname);
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

FResourceFile *CheckWHRes(const char *filename, FileReader &file, bool quiet, LumpFilterInfo* filter)
{
	if (file.GetLength() >= 8192) // needs to be at least 8192 to contain one file and the directory.
	{
		int directory[1024];
		int nl =1024/3;

		file.Seek(-4096, FileReader::SeekEnd);
		file.Read(directory, 4096);

		int checkpos = 0;
		for(int k = 0; k < nl; k++)
		{
			int offset = LittleLong(directory[k*3]);
			int length = LittleLong(directory[k*3+1]);
			if (length <= 0 && offset == 0) break;
			if (offset != checkpos || length <= 0) return nullptr;
			checkpos += (length+4095) / 4096;
		}
		auto rf = new FWHResFile(filename, file);
		if (rf->Open(quiet, filter)) return rf;
		file = std::move(rf->Reader); // to avoid destruction of reader
		delete rf;
	}
	return NULL;
}
 