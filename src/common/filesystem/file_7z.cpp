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

// Note that 7z made the unwise decision to include windows.h :(
#include "7z.h"
#include "7zCrc.h"

#include "resourcefile.h"
#include "cmdlib.h"



//-----------------------------------------------------------------------
//
// Interface classes to 7z library
//
//-----------------------------------------------------------------------

extern ISzAlloc g_Alloc;

struct CZDFileInStream
{
	ISeekInStream s;
	FileReader &File;

	CZDFileInStream(FileReader &_file) 
		: File(_file)
	{
		s.Read = Read;
		s.Seek = Seek;
	}

	static SRes Read(const ISeekInStream *pp, void *buf, size_t *size)
	{
		CZDFileInStream *p = (CZDFileInStream *)pp;
		auto numread = p->File.Read(buf, (long)*size);
		if (numread < 0)
		{
			*size = 0;
			return SZ_ERROR_READ;
		}
		*size = numread;
		return SZ_OK;
	}

	static SRes Seek(const ISeekInStream *pp, Int64 *pos, ESzSeek origin)
	{
		CZDFileInStream *p = (CZDFileInStream *)pp;
		FileReader::ESeek move_method;
		int res;
		if (origin == SZ_SEEK_SET)
		{
			move_method = FileReader::SeekSet;
		}
		else if (origin == SZ_SEEK_CUR)
		{
			move_method = FileReader::SeekCur;
		}
		else if (origin == SZ_SEEK_END)
		{
			move_method = FileReader::SeekEnd;
		}
		else
		{
			return 1;
		}
		res = (int)p->File.Seek((long)*pos, move_method);
		*pos = p->File.Tell();
		return res;
	}
};

struct C7zArchive
{
	CSzArEx DB;
	CZDFileInStream ArchiveStream;
	CLookToRead2 LookStream;
	Byte StreamBuffer[1<<14];
	UInt32 BlockIndex;
	Byte *OutBuffer;
	size_t OutBufferSize;

	C7zArchive(FileReader &file) : ArchiveStream(file)
	{
		if (g_CrcTable[1] == 0)
		{
			CrcGenerateTable();
		}
		file.Seek(0, FileReader::SeekSet);
		LookToRead2_CreateVTable(&LookStream, false);
		LookStream.realStream = &ArchiveStream.s;
		LookToRead2_Init(&LookStream);
		LookStream.bufSize = sizeof(StreamBuffer);
		LookStream.buf = StreamBuffer;
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
		return SzArEx_Open(&DB, &LookStream.vt, &g_Alloc, &g_Alloc);
	}

	SRes Extract(UInt32 file_index, char *buffer)
	{
		size_t offset, out_size_processed;
		SRes res = SzArEx_Extract(&DB, &LookStream.vt, file_index,
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
// 7-zip file
//
//==========================================================================

class F7ZFile : public FResourceFile
{
	friend struct F7ZLump;

	F7ZLump *Lumps;
	C7zArchive *Archive;

public:
	F7ZFile(const char * filename, FileReader &filer);
	bool Open(LumpFilterInfo* filter, FileSystemMessageFunc Printf);
	virtual ~F7ZFile();
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
};



//==========================================================================
//
// 7Z file
//
//==========================================================================

F7ZFile::F7ZFile(const char * filename, FileReader &filer)
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

bool F7ZFile::Open(LumpFilterInfo *filter, FileSystemMessageFunc Printf)
{
	Archive = new C7zArchive(Reader);
	int skipped = 0;
	SRes res;

	res = Archive->Open();
	if (res != SZ_OK)
	{
		delete Archive;
		Archive = NULL;
		if (res == SZ_ERROR_UNSUPPORTED)
		{
			Printf(FSMessageLevel::Error, "%s: Decoder does not support this archive\n", FileName.GetChars());
		}
		else if (res == SZ_ERROR_MEM)
		{
			Printf(FSMessageLevel::Error, "Cannot allocate memory\n");
		}
		else if (res == SZ_ERROR_CRC)
		{
			Printf(FSMessageLevel::Error, "CRC error\n");
		}
		else
		{
			Printf(FSMessageLevel::Error, "error #%d\n", res);
		}
		return false;
	}

	CSzArEx* const archPtr = &Archive->DB;

	NumLumps = archPtr->NumFiles;
	Lumps = new F7ZLump[NumLumps];

	F7ZLump *lump_p = Lumps;
	TArray<UInt16> nameUTF16;
	TArray<char> nameASCII;

	for (uint32_t i = 0; i < NumLumps; ++i)
	{
		// skip Directories
		if (SzArEx_IsDir(archPtr, i))
		{
			skipped++;
			continue;
		}

		const size_t nameLength = SzArEx_GetFileNameUtf16(archPtr, i, NULL);

		if (0 == nameLength)
		{
			++skipped;
			continue;
		}

		nameUTF16.Resize((unsigned)nameLength);
		nameASCII.Resize((unsigned)nameLength);
		SzArEx_GetFileNameUtf16(archPtr, i, &nameUTF16[0]);
		for (size_t c = 0; c < nameLength; ++c)
		{
			nameASCII[c] = static_cast<char>(nameUTF16[c]);
		}
		FixPathSeperator(&nameASCII[0]);

		FString name = &nameASCII[0];
		name.ToLower();

		lump_p->LumpNameSetup(name);
		lump_p->LumpSize = static_cast<int>(SzArEx_GetFileSize(archPtr, i));
		lump_p->Owner = this;
		lump_p->Flags = LUMPF_FULLPATH|LUMPF_COMPRESSED;
		lump_p->Position = i;
		lump_p->CheckEmbedded(filter);
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
			Printf(FSMessageLevel::Error, "%s: unsupported 7z/LZMA file!\n", FileName.GetChars());
			return false;
		}
	}

	GenerateHash();
	PostProcessArchive(&Lumps[0], sizeof(F7ZLump), filter);
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

FResourceFile *Check7Z(const char *filename, FileReader &file, LumpFilterInfo* filter, FileSystemMessageFunc Printf)
{
	char head[k7zSignatureSize];

	if (file.GetLength() >= k7zSignatureSize)
	{
		file.Seek(0, FileReader::SeekSet);
		file.Read(&head, k7zSignatureSize);
		file.Seek(0, FileReader::SeekSet);
		if (!memcmp(head, k7zSignature, k7zSignatureSize))
		{
			auto rf = new F7ZFile(filename, file);
			if (rf->Open(filter, Printf)) return rf;

			file = std::move(rf->Reader); // to avoid destruction of reader
			delete rf;
		}
	}
	return NULL;
}



