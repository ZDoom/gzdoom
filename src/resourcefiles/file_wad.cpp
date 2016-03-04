/*
** file_wad.cpp
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
#include "templates.h"
#include "v_text.h"
#include "w_wad.h"
#include "gi.h"
#include "i_system.h"

// Console Doom LZSS wrapper.
class FileReaderLZSS : public FileReaderBase
{
private:
	enum { BUFF_SIZE = 4096, WINDOW_SIZE = 4096, INTERNAL_BUFFER_SIZE = 128 };

	FileReader &File;
	bool SawEOF;
	BYTE InBuff[BUFF_SIZE];

	enum StreamState
	{
		STREAM_EMPTY,
		STREAM_BITS,
		STREAM_FLUSH,
		STREAM_FINAL
	};
	struct
	{
		StreamState State;

		BYTE *In;
		unsigned int AvailIn;
		unsigned int InternalOut;

		BYTE CFlags, Bits;

		BYTE Window[WINDOW_SIZE+INTERNAL_BUFFER_SIZE];
		const BYTE *WindowData;
		BYTE *InternalBuffer;
	} Stream;

	void FillBuffer()
	{
		if(Stream.AvailIn)
			memmove(InBuff, Stream.In, Stream.AvailIn);

		long numread = File.Read(InBuff+Stream.AvailIn, BUFF_SIZE-Stream.AvailIn);

		if (numread < BUFF_SIZE)
		{
			SawEOF = true;
		}
		Stream.In = InBuff;
		Stream.AvailIn = numread+Stream.AvailIn;
	}

	// Reads a flag byte.
	void PrepareBlocks()
	{
		assert(Stream.InternalBuffer == Stream.WindowData);
		Stream.CFlags = *Stream.In++;
		--Stream.AvailIn;
		Stream.Bits = 0xFF;
		Stream.State = STREAM_BITS;
	}

	// Reads the next chunk in the block. Returns true if successful and
	// returns false if it ran out of input data.
	bool UncompressBlock()
	{
		if(Stream.CFlags & 1)
		{
			// Check to see if we have enough input
			if(Stream.AvailIn < 2)
				return false;
			Stream.AvailIn -= 2;

			WORD pos = BigShort(*(WORD*)Stream.In);
			BYTE len = (pos & 0xF)+1;
			pos >>= 4;
			Stream.In += 2;
			if(len == 1)
			{
				// We've reached the end of the stream.
				Stream.State = STREAM_FINAL;
				return true;
			}

			const BYTE* copyStart = Stream.InternalBuffer-pos-1;

			// Complete overlap: Single byte repeated
			if(pos == 0)
				memset(Stream.InternalBuffer, *copyStart, len);
			// No overlap: One copy
			else if(pos >= len)
				memcpy(Stream.InternalBuffer, copyStart, len);
			else
			{
				// Partial overlap: Copy in 2 or 3 chunks.
				do
				{
					unsigned int copy = MIN<unsigned int>(len, pos+1);
					memcpy(Stream.InternalBuffer, copyStart, copy);
					Stream.InternalBuffer += copy;
					Stream.InternalOut += copy;
					len -= copy;
					pos += copy; // Increase our position since we can copy twice as much the next round.
				}
				while(len);
			}

			Stream.InternalOut += len;
			Stream.InternalBuffer += len;
		}
		else
		{
			// Uncompressed byte.
			*Stream.InternalBuffer++ = *Stream.In++;
			--Stream.AvailIn;
			++Stream.InternalOut;
		}

		Stream.CFlags >>= 1;
		Stream.Bits >>= 1;

		// If we're done with this block, flush the output
		if(Stream.Bits == 0)
			Stream.State = STREAM_FLUSH;

		return true;
	}

public:
	FileReaderLZSS(FileReader &file) : File(file), SawEOF(false)
	{
		Stream.State = STREAM_EMPTY;
		Stream.WindowData = Stream.InternalBuffer = Stream.Window+WINDOW_SIZE;
		Stream.InternalOut = 0;
		Stream.AvailIn = 0;

		FillBuffer();
	}

	~FileReaderLZSS()
	{
	}

	long Read(void *buffer, long len)
	{

		BYTE *Out = (BYTE*)buffer;
		long AvailOut = len;

		do
		{
			while(Stream.AvailIn)
			{
				if(Stream.State == STREAM_EMPTY)
					PrepareBlocks();
				else if(Stream.State == STREAM_BITS && !UncompressBlock())
					break;
				else
					break;
			}

			unsigned int copy = MIN<unsigned int>(Stream.InternalOut, AvailOut);
			if(copy > 0)
			{
				memcpy(Out, Stream.WindowData, copy);
				Out += copy;
				AvailOut -= copy;

				// Slide our window
				memmove(Stream.Window, Stream.Window+copy, WINDOW_SIZE+INTERNAL_BUFFER_SIZE-copy);
				Stream.InternalBuffer -= copy;
				Stream.InternalOut -= copy;
			}

			if(Stream.State == STREAM_FINAL)
				break;

			if(Stream.InternalOut == 0 && Stream.State == STREAM_FLUSH)
				Stream.State = STREAM_EMPTY;

			if(Stream.AvailIn < 2)
				FillBuffer();
		}
		while(AvailOut && Stream.State != STREAM_FINAL);

		assert(AvailOut == 0);
		return (long)(Out - (BYTE*)buffer);
	}
};

//==========================================================================
//
// Wad Lump (with console doom LZSS support)
//
//==========================================================================

class FWadFileLump : public FResourceLump
{
public:
	bool Compressed;
	int	Position;

	int GetFileOffset() { return Position; }
	FileReader *GetReader()
	{
		if(!Compressed)
		{
			Owner->Reader->Seek(Position, SEEK_SET);
			return Owner->Reader;
		}
		return NULL;
	}
	int FillCache()
	{
		if(!Compressed)
		{
			const char * buffer = Owner->Reader->GetBuffer();

			if (buffer != NULL)
			{
				// This is an in-memory file so the cache can point directly to the file's data.
				Cache = const_cast<char*>(buffer) + Position;
				RefCount = -1;
				return -1;
			}
		}

		Owner->Reader->Seek(Position, SEEK_SET);
		Cache = new char[LumpSize];

		if(Compressed)
		{
			FileReaderLZSS lzss(*Owner->Reader);
			lzss.Read(Cache, LumpSize);
		}
		else
			Owner->Reader->Read(Cache, LumpSize);

		RefCount = 1;
		return 1;
	}
};

//==========================================================================
//
// Wad file
//
//==========================================================================

class FWadFile : public FResourceFile
{
	FWadFileLump *Lumps;

	bool IsMarker(int lump, const char *marker);
	void SetNamespace(const char *startmarker, const char *endmarker, namespace_t space, bool flathack=false);
	void SkinHack ();

public:
	FWadFile(const char * filename, FileReader *file);
	~FWadFile();
	void FindStrifeTeaserVoices ();
	FResourceLump *GetLump(int lump) { return &Lumps[lump]; }
	bool Open(bool quiet);
};


//==========================================================================
//
// FWadFile::FWadFile
//
// Initializes a WAD file
//
//==========================================================================

FWadFile::FWadFile(const char *filename, FileReader *file) : FResourceFile(filename, file)
{
	Lumps = NULL;
}

FWadFile::~FWadFile()
{
	delete[] Lumps;
}

//==========================================================================
//
// Open it
//
//==========================================================================

bool FWadFile::Open(bool quiet)
{
	wadinfo_t header;
	DWORD InfoTableOfs;
	bool isBigEndian = false; // Little endian is assumed until proven otherwise
	const long wadSize = Reader->GetLength();

	Reader->Read(&header, sizeof(header));
	NumLumps = LittleLong(header.NumLumps);
	InfoTableOfs = LittleLong(header.InfoTableOfs);

	// Check to see if the little endian interpretation is valid
	// This should be sufficient to detect big endian wads.
	if (InfoTableOfs + NumLumps*sizeof(wadlump_t) > (unsigned)wadSize)
	{
		NumLumps = BigLong(header.NumLumps);
		InfoTableOfs = BigLong(header.InfoTableOfs);
		isBigEndian = true;

		// Check again to detect broken wads
		if (InfoTableOfs + NumLumps*sizeof(wadlump_t) > (unsigned)wadSize)
		{
			I_Error("Cannot load broken WAD file %s\n", Filename);
		}
	}

	wadlump_t *fileinfo = new wadlump_t[NumLumps];
	Reader->Seek (InfoTableOfs, SEEK_SET);
	Reader->Read (fileinfo, NumLumps * sizeof(wadlump_t));

	Lumps = new FWadFileLump[NumLumps];

	for(DWORD i = 0; i < NumLumps; i++)
	{
		uppercopy (Lumps[i].Name, fileinfo[i].Name);
		Lumps[i].Name[8] = 0;
		Lumps[i].Compressed = !(gameinfo.flags & GI_SHAREWARE) && (Lumps[i].Name[0] & 0x80) == 0x80;
		Lumps[i].Name[0] &= ~0x80;

		Lumps[i].Owner = this;
		Lumps[i].Position = isBigEndian ? BigLong(fileinfo[i].FilePos) : LittleLong(fileinfo[i].FilePos);
		Lumps[i].LumpSize = isBigEndian ? BigLong(fileinfo[i].Size) : LittleLong(fileinfo[i].Size);
		Lumps[i].Namespace = ns_global;
		Lumps[i].Flags = 0;
		Lumps[i].FullName = NULL;
	}

	delete[] fileinfo;

	if (!quiet)
	{
		if (!batchrun) Printf(", %d lumps\n", NumLumps);

		// don't bother with namespaces here. We won't need them.
		SetNamespace("S_START", "S_END", ns_sprites);
		SetNamespace("F_START", "F_END", ns_flats, true);
		SetNamespace("C_START", "C_END", ns_colormaps);
		SetNamespace("A_START", "A_END", ns_acslibrary);
		SetNamespace("TX_START", "TX_END", ns_newtextures);
		SetNamespace("V_START", "V_END", ns_strifevoices);
		SetNamespace("HI_START", "HI_END", ns_hires);
		SetNamespace("VX_START", "VX_END", ns_voxels);
		SkinHack();
	}
	return true;
}

//==========================================================================
//
// IsMarker
//
// (from BOOM)
//
//==========================================================================

inline bool FWadFile::IsMarker(int lump, const char *marker)
{
	if (Lumps[lump].Name[0] == marker[0])
	{
		return (!strcmp(Lumps[lump].Name, marker) ||
			(marker[1] == '_' && !strcmp(Lumps[lump].Name+1, marker)));
	}
	else return false;
}

//==========================================================================
//
// SetNameSpace
//
// Sets namespace information for the lumps. It always looks for the first
// x_START and the last x_END lump, except when loading flats. In this case
// F_START may be absent and if that is the case all lumps with a size of
// 4096 will be flagged appropriately.
//
//==========================================================================

// This class was supposed to be local in the function but GCC
// does not like that.
struct Marker
{
	int markertype;
	unsigned int index;
};

void FWadFile::SetNamespace(const char *startmarker, const char *endmarker, namespace_t space, bool flathack)
{
	bool warned = false;
	int numstartmarkers = 0, numendmarkers = 0;
	unsigned int i;
	TArray<Marker> markers;
	
	for(i = 0; i < NumLumps; i++)
	{
		if (IsMarker(i, startmarker))
		{
			Marker m = { 0, i };
			markers.Push(m);
			numstartmarkers++;
		}
		else if (IsMarker(i, endmarker))
		{
			Marker m = { 1, i };
			markers.Push(m);
			numendmarkers++;
		}
	}

	if (numstartmarkers == 0)
	{
		if (numendmarkers == 0) return;	// no markers found

		Printf(TEXTCOLOR_YELLOW"WARNING: %s marker without corresponding %s found.\n", endmarker, startmarker);

		
		if (flathack)
		{
			// We have found no F_START but one or more F_END markers.
			// mark all lumps before the last F_END marker as potential flats.
			unsigned int end = markers[markers.Size()-1].index;
			for(unsigned int i = 0; i < end; i++)
			{
				if (Lumps[i].LumpSize == 4096)
				{
					// We can't add this to the flats namespace but 
					// it needs to be flagged for the texture manager.
					DPrintf("Marking %s as potential flat\n", Lumps[i].Name);
					Lumps[i].Flags |= LUMPF_MAYBEFLAT;
				}
			}
		}
		return;
	}

	i = 0;
	while (i < markers.Size())
	{
		int start, end;
		if (markers[i].markertype != 0)
		{
			Printf(TEXTCOLOR_YELLOW"WARNING: %s marker without corresponding %s found.\n", endmarker, startmarker);
			i++;
			continue;
		}
		start = i++;

		// skip over subsequent x_START markers
		while (i < markers.Size() && markers[i].markertype == 0)
		{
			Printf(TEXTCOLOR_YELLOW"WARNING: duplicate %s marker found.\n", startmarker);
			i++;
			continue;
		}
		// same for x_END markers
		while (i < markers.Size()-1 && (markers[i].markertype == 1 && markers[i+1].markertype == 1))
		{
			Printf(TEXTCOLOR_YELLOW"WARNING: duplicate %s marker found.\n", endmarker);
			i++;
			continue;
		}
		// We found a starting marker but no end marker. Ignore this block.
		if (i >= markers.Size())
		{
			Printf(TEXTCOLOR_YELLOW"WARNING: %s marker without corresponding %s found.\n", startmarker, endmarker);
			end = NumLumps;
		}
		else
		{
			end = markers[i++].index;
		}

		// we found a marked block
		DPrintf("Found %s block at (%d-%d)\n", startmarker, markers[start].index, end);
		for(int j = markers[start].index + 1; j < end; j++)
		{
			if (Lumps[j].Namespace != ns_global)
			{
				if (!warned)
				{
					Printf(TEXTCOLOR_YELLOW"WARNING: Overlapping namespaces found (lump %d)\n", j);
				}
				warned = true;
			}
			else if (space == ns_sprites && Lumps[j].LumpSize < 8)
			{
				// sf 26/10/99:
				// ignore sprite lumps smaller than 8 bytes (the smallest possible)
				// in size -- this was used by some dmadds wads
				// as an 'empty' graphics resource
				DPrintf(" Skipped empty sprite %s (lump %d)\n", Lumps[j].Name, j);
			}
			else
			{
				Lumps[j].Namespace = space;
			}
		}
	}
}


//==========================================================================
//
// W_SkinHack
//
// Tests a wad file to see if it contains an S_SKIN marker. If it does,
// every lump in the wad is moved into a new namespace. Because skins are
// only supposed to replace player sprites, sounds, or faces, this should
// not be a problem. Yes, there are skins that replace more than that, but
// they are such a pain, and breaking them like this was done on purpose.
// This also renames any S_SKINxx lumps to just S_SKIN.
//
//==========================================================================

void FWadFile::SkinHack ()
{
	static int namespc = ns_firstskin;
	bool skinned = false;
	bool hasmap = false;
	DWORD i;

	for (i = 0; i < NumLumps; i++)
	{
		FResourceLump *lump = &Lumps[i];

		if (lump->Name[0] == 'S' &&
			lump->Name[1] == '_' &&
			lump->Name[2] == 'S' &&
			lump->Name[3] == 'K' &&
			lump->Name[4] == 'I' &&
			lump->Name[5] == 'N')
		{ // Wad has at least one skin.
			lump->Name[6] = lump->Name[7] = 0;
			if (!skinned)
			{
				skinned = true;
				DWORD j;

				for (j = 0; j < NumLumps; j++)
				{
					Lumps[j].Namespace = namespc;
				}
				namespc++;
			}
		}
		if ((lump->Name[0] == 'M' &&
			 lump->Name[1] == 'A' &&
			 lump->Name[2] == 'P' &&
			 lump->Name[3] >= '0' && lump->Name[3] <= '9' &&
			 lump->Name[4] >= '0' && lump->Name[4] <= '9' &&
			 lump->Name[5] >= '\0')
			||
			(lump->Name[0] == 'E' &&
			 lump->Name[1] >= '0' && lump->Name[1] <= '9' &&
			 lump->Name[2] == 'M' &&
			 lump->Name[3] >= '0' && lump->Name[3] <= '9' &&
			 lump->Name[4] >= '\0'))
		{
			hasmap = true;
		}
	}
	if (skinned && hasmap)
	{
		Printf (TEXTCOLOR_BLUE
			"The maps in %s will not be loaded because it has a skin.\n"
			TEXTCOLOR_BLUE
			"You should remove the skin from the wad to play these maps.\n",
			Filename);
	}
}


//==========================================================================
//
// FindStrifeTeaserVoices
//
// Strife0.wad does not have the voices between V_START/V_END markers, so
// figure out which lumps are voices based on their names.
//
//==========================================================================

void FWadFile::FindStrifeTeaserVoices ()
{
	for (DWORD i = 0; i <= NumLumps; ++i)
	{
		if (Lumps[i].Name[0] == 'V' &&
			Lumps[i].Name[1] == 'O' &&
			Lumps[i].Name[2] == 'C')
		{
			int j;

			for (j = 3; j < 8; ++j)
			{
				if (Lumps[i].Name[j] != 0 && !isdigit(Lumps[i].Name[j]))
					break;
			}
			if (j == 8)
			{
				Lumps[i].Namespace = ns_strifevoices;
			}
		}
	}
}


//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckWad(const char *filename, FileReader *file, bool quiet)
{
	char head[4];

	if (file->GetLength() >= 12)
	{
		file->Seek(0, SEEK_SET);
		file->Read(&head, 4);
		file->Seek(0, SEEK_SET);
		if (!memcmp(head, "IWAD", 4) || !memcmp(head, "PWAD", 4))
		{
			FResourceFile *rf = new FWadFile(filename, file);
			if (rf->Open(quiet)) return rf;

			rf->Reader = NULL; // to avoid destruction of reader
			delete rf;
		}
	}
	return NULL;
}

