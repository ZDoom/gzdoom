/*
** file_7z.cpp
**
**---------------------------------------------------------------------------
** Copyright 2009 Randy Heit
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
#include "templates.h"
#include "v_text.h"
#include "w_zip.h"
#include "i_system.h"
#include "w_wad.h"

#include "7z.h"
#include "7zCrc.h"


//-----------------------------------------------------------------------
//
// Interface classes to 7z library
//
//-----------------------------------------------------------------------

extern ISzAlloc g_Alloc;

struct CZDFileInStream
{
	ISeekInStream s;
	FileReader *File;

	CZDFileInStream(FileReader *_file)
	{
		s.Read = Read;
		s.Seek = Seek;
		File = _file;
	}

	static SRes Read(void *pp, void *buf, size_t *size)
	{
		CZDFileInStream *p = (CZDFileInStream *)pp;
		long numread = p->File->Read(buf, (long)*size);
		if (numread < 0)
		{
			*size = 0;
			return SZ_ERROR_READ;
		}
		*size = numread;
		return SZ_OK;
	}

	static SRes Seek(void *pp, Int64 *pos, ESzSeek origin)
	{
		CZDFileInStream *p = (CZDFileInStream *)pp;
		int move_method;
		int res;
		if (origin == SZ_SEEK_SET)
		{
			move_method = SEEK_SET;
		}
		else if (origin == SZ_SEEK_CUR)
		{
			move_method = SEEK_CUR;
		}
		else if (origin == SZ_SEEK_END)
		{
			move_method = SEEK_END;
		}
		else
		{
			return 1;
		}
		res = p->File->Seek((long)*pos, move_method);
		*pos = p->File->Tell();
		return res;
	}
};

struct C7zArchive
{
	CSzArEx DB;
	CZDFileInStream ArchiveStream;
	CLookToRead LookStream;
	UInt32 BlockIndex;
	Byte *OutBuffer;
	size_t OutBufferSize;

	C7zArchive(FileReader *file) : ArchiveStream(file)
	{
		if (g_CrcTable[1] == 0)
		{
			CrcGenerateTable();
		}
		file->Seek(0, SEEK_SET);
		LookToRead_CreateVTable(&LookStream, false);
		LookStream.realStream = &ArchiveStream.s;
		LookToRead_Init(&LookStream);
		SzArEx_Init(&DB);
		BlockIndex = 0xFFFFFFFF;
		OutBuffer = NULL;
		OutBufferSize = 0;
	}

	~C7zArchive()
	{
		if (OutBuffer != NULL)
		{
			IAlloc_Free(&g_Alloc, OutBuffer);
		}
		SzArEx_Free(&DB, &g_Alloc);
	}

	SRes Open()
	{
		return SzArEx_Open(&DB, &LookStream.s, &g_Alloc, &g_Alloc);
	}

	SRes Extract(UInt32 file_index, char *buffer)
	{
		size_t offset, out_size_processed;
		SRes res = SzArEx_Extract(&DB, &LookStream.s, file_index,
			&BlockIndex, &OutBuffer, &OutBufferSize,
			&offset, &out_size_processed,
			&g_Alloc, &g_Alloc);
		if (res == SZ_OK)
		{
			memcpy(buffer, OutBuffer + offset, out_size_processed);
		}
		return res;
	}
};
//==========================================================================
//
// Zip Lump
//
//==========================================================================

struct F7ZLump : public FResourceLump
{
	int		Position;

	virtual int FillCache();

};


//==========================================================================
//
// Zip file
//
//==========================================================================

class F7ZFile : public FResourceFile
{
	friend struct F7ZLump;

	F7ZLump *Lumps;
	C7zArchive *Archive;

	static int STACK_ARGS lumpcmp(const void * a, const void * b);

public:
	F7ZFile(const char * filename, FileReader *filer);
	bool Open(bool quiet);
	virtual ~F7ZFile();
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
};



int STACK_ARGS F7ZFile::lumpcmp(const void * a, const void * b)
{
	F7ZLump * rec1 = (F7ZLump *)a;
	F7ZLump * rec2 = (F7ZLump *)b;

	return stricmp(rec1->FullName, rec2->FullName);
}


//==========================================================================
//
// 7Z file
//
//==========================================================================

F7ZFile::F7ZFile(const char * filename, FileReader *filer)
	: FResourceFile(filename, filer) 
{
	Lumps = NULL;
	Archive = NULL;
}


//==========================================================================
//
// Open it
//
//==========================================================================

bool F7ZFile::Open(bool quiet)
{
	Archive = new C7zArchive(Reader);
	int skipped = 0;
	SRes res;

	res = Archive->Open();
	if (res != SZ_OK)
	{
		delete Archive;
		Archive = NULL;
		if (!quiet)
		{
			Printf("\n" TEXTCOLOR_RED "%s: ", Filename);
			if (res == SZ_ERROR_UNSUPPORTED)
			{
				Printf("Decoder does not support this archive\n");
			}
			else if (res == SZ_ERROR_MEM)
			{
				Printf("Cannot allocate memory\n");
			}
			else if (res == SZ_ERROR_CRC)
			{
				Printf("CRC error\n");
			}
			else
			{
				Printf("error #%d\n", res);
			}
		}
		return false;
	}
	NumLumps = Archive->DB.db.NumFiles;

	Lumps = new F7ZLump[NumLumps];

	F7ZLump *lump_p = Lumps;
	TArray<UInt16> nameUTF16;
	TArray<char> nameASCII;
	for (DWORD i = 0; i < NumLumps; ++i)
	{
		CSzFileItem *file = &Archive->DB.db.Files[i];

		// skip Directories
		if (file->IsDir)
		{
			skipped++;
			continue;
		}

		const size_t nameLength = SzArEx_GetFileNameUtf16(&Archive->DB, i, NULL);

		if (0 == nameLength)
		{
			++skipped;
			continue;
		}

		nameUTF16.Resize(nameLength);
		nameASCII.Resize(nameLength);
		SzArEx_GetFileNameUtf16(&Archive->DB, i, &nameUTF16[0]);
		for (size_t c = 0; c < nameLength; ++c)
		{
			nameASCII[c] = static_cast<char>(nameUTF16[c]);
		}
		FixPathSeperator(&nameASCII[0]);

		FString name = &nameASCII[0];
		name.ToLower();

		lump_p->LumpNameSetup(name);
		lump_p->LumpSize = int(file->Size);
		lump_p->Owner = this;
		lump_p->Flags = LUMPF_ZIPFILE;
		lump_p->Position = i;
		lump_p->CheckEmbedded();
		lump_p++;
	}
	// Resize the lump record array to its actual size
	NumLumps -= skipped;

	if (NumLumps > 0)
	{
		// Quick check for unsupported compression method

		TArray<char> temp;
		temp.Resize(Lumps[0].LumpSize);

		if (SZ_OK != Archive->Extract(Lumps[0].Position, &temp[0]))
		{
			if (!quiet) Printf("\n%s: unsupported 7z/LZMA file!\n", Filename);
			return false;
		}
	}

	if (!quiet) Printf(", %d lumps\n", NumLumps);

	// Entries in archives are sorted alphabetically
	qsort(&Lumps[0], NumLumps, sizeof(F7ZLump), lumpcmp);
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

F7ZFile::~F7ZFile()
{
	if (Lumps != NULL)
	{
		delete[] Lumps;
	}
	if (Archive != NULL)
	{
		delete Archive;
	}
}

//==========================================================================
//
// Fills the lump cache and performs decompression
//
//==========================================================================

int F7ZLump::FillCache()
{
	Cache = new char[LumpSize];
	static_cast<F7ZFile*>(Owner)->Archive->Extract(Position, Cache);
	RefCount = 1;
	return 1;
}

//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *Check7Z(const char *filename, FileReader *file, bool quiet)
{
	char head[k7zSignatureSize];

	if (file->GetLength() >= k7zSignatureSize)
	{
		file->Seek(0, SEEK_SET);
		file->Read(&head, k7zSignatureSize);
		file->Seek(0, SEEK_SET);
		if (!memcmp(head, k7zSignature, k7zSignatureSize))
		{
			FResourceFile *rf = new F7ZFile(filename, file);
			if (rf->Open(quiet)) return rf;

			rf->Reader = NULL; // to avoid destruction of reader
			delete rf;
		}
	}
	return NULL;
}



