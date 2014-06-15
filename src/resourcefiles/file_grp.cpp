/*
** file_grp.cpp
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

#include "resourcefile.h"
#include "cmdlib.h"
#include "v_text.h"
#include "w_wad.h"

//==========================================================================
//
//
//
//==========================================================================

struct GrpInfo
{
	DWORD		Magic[3];
	DWORD		NumLumps;
};

struct GrpLump
{
	union
	{
		struct
		{
			char		Name[12];
			DWORD		Size;
		};
		char NameWithZero[13];
	};
};


//==========================================================================
//
// Build GRP file
//
//==========================================================================

class FGrpFile : public FUncompressedFile
{
public:
	FGrpFile(const char * filename, FileReader *file);
	bool Open(bool quiet);
};


//==========================================================================
//
// Initializes a Build GRP file
//
//==========================================================================

FGrpFile::FGrpFile(const char *filename, FileReader *file)
: FUncompressedFile(filename, file)
{
	Lumps = NULL;
}

//==========================================================================
//
// Open it
//
//==========================================================================

bool FGrpFile::Open(bool quiet)
{
	GrpInfo header;

	Reader->Read(&header, sizeof(header));
	NumLumps = LittleLong(header.NumLumps);
	
	GrpLump *fileinfo = new GrpLump[NumLumps];
	Reader->Read (fileinfo, NumLumps * sizeof(GrpLump));

	Lumps = new FUncompressedLump[NumLumps];

	int Position = sizeof(GrpInfo) + NumLumps * sizeof(GrpLump);

	for(DWORD i = 0; i < NumLumps; i++)
	{
		Lumps[i].Owner = this;
		Lumps[i].Position = Position;
		Lumps[i].LumpSize = LittleLong(fileinfo[i].Size);
		Position += fileinfo[i].Size;
		Lumps[i].Namespace = ns_global;
		Lumps[i].Flags = 0;
		fileinfo[i].NameWithZero[12] = '\0';	// Be sure filename is null-terminated
		Lumps[i].LumpNameSetup(fileinfo[i].NameWithZero);
	}
	if (!quiet) Printf(", %d lumps\n", NumLumps);

	delete[] fileinfo;
	return true;
}


//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckGRP(const char *filename, FileReader *file, bool quiet)
{
	char head[12];

	if (file->GetLength() >= 12)
	{
		file->Seek(0, SEEK_SET);
		file->Read(&head, 12);
		file->Seek(0, SEEK_SET);
		if (!memcmp(head, "KenSilverman", 12))
		{
			FResourceFile *rf = new FGrpFile(filename, file);
			if (rf->Open(quiet)) return rf;

			rf->Reader = NULL; // to avoid destruction of reader
			delete rf;
		}
	}
	return NULL;
}

