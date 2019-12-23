/*
** file_pak.cpp
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
#include "w_wad.h"
#include "doomtype.h"

//==========================================================================
//
//
//
//==========================================================================

struct dpackfile_t
{
	char	name[56];
	int		filepos, filelen;
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

class FPakFile : public FUncompressedFile
{
public:
	FPakFile(const char * filename, FileReader &file);
	bool Open(bool quiet);
};


//==========================================================================
//
// FWadFile::FWadFile
//
// Initializes a WAD file
//
//==========================================================================

FPakFile::FPakFile(const char *filename, FileReader &file) 
	: FUncompressedFile(filename, file)
{
}

//==========================================================================
//
// Open it
//
//==========================================================================

bool FPakFile::Open(bool quiet)
{
	dpackheader_t header;

	Reader.Read(&header, sizeof(header));
	NumLumps = LittleLong(header.dirlen) / sizeof(dpackfile_t);
	header.dirofs = LittleLong(header.dirofs);
	
	TArray<dpackfile_t> fileinfo(NumLumps, true);
	Reader.Seek (header.dirofs, FileReader::SeekSet);
	Reader.Read (fileinfo.Data(), NumLumps * sizeof(dpackfile_t));

	Lumps.Resize(NumLumps);

	if (!quiet && !batchrun) Printf(", %d lumps\n", NumLumps);

	for(uint32_t i = 0; i < NumLumps; i++)
	{
		Lumps[i].LumpNameSetup(fileinfo[i].name);
		Lumps[i].Owner = this;
		Lumps[i].Position = LittleLong(fileinfo[i].filepos);
		Lumps[i].LumpSize = LittleLong(fileinfo[i].filelen);
		Lumps[i].CheckEmbedded();
	}
	GenerateHash();
	return true;
}


//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckPak(const char *filename, FileReader &file, bool quiet)
{
	char head[4];

	if (file.GetLength() >= 12)
	{
		file.Seek(0, FileReader::SeekSet);
		file.Read(&head, 4);
		file.Seek(0, FileReader::SeekSet);
		if (!memcmp(head, "PACK", 4))
		{
			FResourceFile *rf = new FPakFile(filename, file);
			if (rf->Open(quiet)) return rf;

			file = std::move(rf->Reader); // to avoid destruction of reader
			delete rf;
		}
	}
	return NULL;
}

