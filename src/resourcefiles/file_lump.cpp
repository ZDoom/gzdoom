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
#include "cmdlib.h"
#include "v_text.h"
#include "w_wad.h"

//==========================================================================
//
// Single lump
//
//==========================================================================

class FLumpFile : public FUncompressedFile
{
public:
	FLumpFile(const char * filename, FileReader *file);
	bool Open(bool quiet);
};


//==========================================================================
//
// FLumpFile::FLumpFile
//
//==========================================================================

FLumpFile::FLumpFile(const char *filename, FileReader *file) : FUncompressedFile(filename, file)
{
}

//==========================================================================
//
// Open it
//
//==========================================================================

bool FLumpFile::Open(bool quiet)
{
	FString name(ExtractFileBase (Filename));

	Lumps = new FUncompressedLump[1];	// must use array allocator
	uppercopy(Lumps->Name, name);
	Lumps->Name[8] = 0;
	Lumps->Owner = this;
	Lumps->Position = 0;
	Lumps->LumpSize = Reader->GetLength();
	Lumps->Namespace = ns_global;
	Lumps->Flags = 0;
	Lumps->FullName = NULL;
	NumLumps = 1;
	if (!quiet)
	{
		Printf("\n");
	}
	return true;
}

//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckLump(const char *filename, FileReader *file, bool quiet)
{
	// always succeeds
	FResourceFile *rf = new FLumpFile(filename, file);
	if (rf->Open(quiet)) return rf;
	delete rf;
	return NULL;
}

