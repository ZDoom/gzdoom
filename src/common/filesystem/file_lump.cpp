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
#include "printf.h"

//==========================================================================
//
// Single lump
//
//==========================================================================

class FLumpFile : public FUncompressedFile
{
public:
	FLumpFile(const char * filename, FileReader &file);
	bool Open(bool quiet, LumpFilterInfo* filter);
};


//==========================================================================
//
// FLumpFile::FLumpFile
//
//==========================================================================

FLumpFile::FLumpFile(const char *filename, FileReader &file) 
	: FUncompressedFile(filename, file)
{
}

//==========================================================================
//
// Open it
//
//==========================================================================

bool FLumpFile::Open(bool quiet, LumpFilterInfo*)
{
	FString name(ExtractFileBase(FileName, true));

	Lumps.Resize(1);
	Lumps[0].LumpNameSetup(name);
	Lumps[0].Owner = this;
	Lumps[0].Position = 0;
	Lumps[0].LumpSize = (int)Reader.GetLength();
	Lumps[0].Flags = 0;
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

FResourceFile *CheckLump(const char *filename, FileReader &file, bool quiet, LumpFilterInfo* filter)
{
	// always succeeds
	FResourceFile *rf = new FLumpFile(filename, file);
	if (rf->Open(quiet, filter)) return rf;
	file = std::move(rf->Reader); // to avoid destruction of reader
	delete rf;
	return NULL;
}

