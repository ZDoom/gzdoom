/*
** farchive.cpp
** Implements an archiver for DObject serialization.
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
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
** The structure of the archive file generated is influenced heavily by the
** description of the MFC archive format published somewhere in the MSDN
** library.
**
** Two major shortcomings of the format I use are that there is no version
** control and no support for storing the non-default portions of objects.
** The latter would allow for easier extension of objects in future
** releases even without a versioning system.
*/

#include <stddef.h>
#include <string.h>
#include <zlib.h>
#include <stdlib.h>

#include "doomtype.h"
#include "farchive.h"
#include "m_swap.h"
#include "m_crc32.h"
#include "cmdlib.h"
#include "i_system.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "d_player.h"
#include "m_misc.h"
#include "dobject.h"

// These are special tokens found in the data stream of an archive.
// Whenever a new object is encountered, it gets created using new and
// is then asked to serialize itself before processing of the previous
// object continues. This can result in some very deep recursion if
// you aren't careful about how you organize your data.

#define NEW_OBJ				((BYTE)1)	// Data for a new object follows
#define NEW_CLS_OBJ			((BYTE)2)	// Data for a new class and object follows
#define OLD_OBJ				((BYTE)3)	// Reference to an old object follows
#define NULL_OBJ			((BYTE)4)	// Load as NULL
#define M1_OBJ				((BYTE)44)	// Load as (DObject*)-1

#define NEW_PLYR_OBJ		((BYTE)5)	// Data for a new player follows
#define NEW_PLYR_CLS_OBJ	((BYTE)6)	// Data for a new class and player follows

#define NEW_NAME			((BYTE)27)	// A new name follows
#define OLD_NAME			((BYTE)28)	// Reference to an old name follows
#define NIL_NAME			((BYTE)33)	// Load as NULL

#define NEW_SPRITE			((BYTE)11)	// A new sprite name follows
#define OLD_SPRITE			((BYTE)12)	// Reference to an old sprite name follows

#ifdef __BIG_ENDIAN__
static inline WORD SWAP_WORD(WORD x) { return x; }
static inline DWORD SWAP_DWORD(DWORD x) { return x; }
static inline QWORD SWAP_QWORD(QWORD x) { return x; }
static inline void SWAP_FLOAT(float x) { }
static inline void SWAP_DOUBLE(double &dst, double src) { dst = src; }
#else
#ifdef _MSC_VER
static inline WORD  SWAP_WORD(WORD x)		{ return _byteswap_ushort(x); }
static inline DWORD SWAP_DWORD(DWORD x)		{ return _byteswap_ulong(x); }
static inline QWORD SWAP_QWORD(QWORD x)		{ return _byteswap_uint64(x); }
static inline void SWAP_DOUBLE(double &dst, double &src)
{
	union twiddle { QWORD q; double d; } tdst, tsrc;
	tsrc.d = src;
	tdst.q = _byteswap_uint64(tsrc.q);
	dst = tdst.d;
}
#else
static inline WORD  SWAP_WORD(WORD x)		{ return (((x)<<8) | ((x)>>8)); }
static inline DWORD SWAP_DWORD(DWORD x)		{ return x = (((x)>>24) | (((x)>>8)&0xff00) | (((x)<<8)&0xff0000) | ((x)<<24)); }
static inline QWORD SWAP_QWORD(QWORD x)
{
	union { QWORD q; DWORD d[2]; } t, u;
	t.q = x;
	u.d[0] = SWAP_DWORD(t.d[1]);
	u.d[1] = SWAP_DWORD(t.d[0]);
	return u.q;
}
static inline void SWAP_DOUBLE(double &dst, double &src)
{
	union twiddle { double f; DWORD d[2]; } tdst, tsrc;
	DWORD t;

	tsrc.f = src;
	t = tsrc.d[0];
	tdst.d[0] = SWAP_DWORD(tsrc.d[1]);
	tdst.d[1] = SWAP_DWORD(t);
	dst = tdst.f;
}
#endif
static inline void SWAP_FLOAT(float &x)
{
	union twiddle { DWORD i; float f; } t;
	t.f = x;
	t.i = SWAP_DWORD(t.i);
	x = t.f;
}
#endif

// Output buffer size for compression; need some extra space.
// I assume the description in zlib.h is accurate.
#define OUT_LEN(a)		((a) + (a) / 1000 + 12)

void FCompressedFile::BeEmpty ()
{
	m_Pos = 0;
	m_BufferSize = 0;
	m_MaxBufferSize = 0;
	m_Buffer = NULL;
	m_File = NULL;
	m_NoCompress = false;
	m_Mode = ENotOpen;
}

static const char LZOSig[4] = { 'F', 'L', 'Z', 'O' };
static const char ZSig[4] = { 'F', 'L', 'Z', 'L' };

FCompressedFile::FCompressedFile ()
{
	BeEmpty ();
}

FCompressedFile::FCompressedFile (const char *name, EOpenMode mode, bool dontCompress)
{
	BeEmpty ();
	Open (name, mode);
	m_NoCompress = dontCompress;
}

FCompressedFile::FCompressedFile (FILE *file, EOpenMode mode, bool dontCompress, bool postopen)
{
	BeEmpty ();
	m_Mode = mode;
	m_File = file;
	m_NoCompress = dontCompress;
	if (postopen)
	{
		PostOpen ();
	}
}

FCompressedFile::~FCompressedFile ()
{
	Close ();
}

bool FCompressedFile::Open (const char *name, EOpenMode mode)
{
	Close ();
	if (name == NULL)
		return false;
	m_Mode = mode;
	m_File = fopen (name, mode == EReading ? "rb" : "wb");
	PostOpen ();
	return !!m_File;
}

void FCompressedFile::PostOpen ()
{
	if (m_File && m_Mode == EReading)
	{
		char sig[4];
		fread (sig, 4, 1, m_File);
		if (sig[0] != ZSig[0] || sig[1] != ZSig[1] || sig[2] != ZSig[2] || sig[3] != ZSig[3])
		{
			fclose (m_File);
			m_File = NULL;
			if (sig[0] == LZOSig[0] && sig[1] == LZOSig[1] && sig[2] == LZOSig[2] && sig[3] == LZOSig[3])
			{
				Printf ("Compressed files from older ZDooms are not supported.\n");
			}
			return;
		}
		else
		{
			DWORD sizes[2];
			fread (sizes, sizeof(DWORD), 2, m_File);
			sizes[0] = SWAP_DWORD (sizes[0]);
			sizes[1] = SWAP_DWORD (sizes[1]);
			unsigned int len = sizes[0] == 0 ? sizes[1] : sizes[0];
			m_Buffer = (BYTE *)M_Malloc (len+8);
			fread (m_Buffer+8, len, 1, m_File);
			sizes[0] = SWAP_DWORD (sizes[0]);
			sizes[1] = SWAP_DWORD (sizes[1]);
			((DWORD *)m_Buffer)[0] = sizes[0];
			((DWORD *)m_Buffer)[1] = sizes[1];
			Explode ();
		}
	}
}

void FCompressedFile::Close ()
{
	if (m_File)
	{
		if (m_Mode == EWriting)
		{
			Implode ();
			fwrite (ZSig, 4, 1, m_File);
			fwrite (m_Buffer, m_BufferSize + 8, 1, m_File);
		}
		fclose (m_File);
		m_File = NULL;
	}
	if (m_Buffer)
	{
		M_Free (m_Buffer);
		m_Buffer = NULL;
	}
	BeEmpty ();
}

void FCompressedFile::Flush ()
{
}

FFile::EOpenMode FCompressedFile::Mode () const
{
	return m_Mode;
}

bool FCompressedFile::IsOpen () const
{
	return !!m_File;
}

FFile &FCompressedFile::Write (const void *mem, unsigned int len)
{
	if (m_Mode == EWriting)
	{
		if (m_Pos + len > m_MaxBufferSize)
		{
			do
			{
				m_MaxBufferSize = m_MaxBufferSize ? m_MaxBufferSize * 2 : 16384;
			}
			while (m_Pos + len > m_MaxBufferSize);
			m_Buffer = (BYTE *)M_Realloc (m_Buffer, m_MaxBufferSize);
		}
		if (len == 1)
			m_Buffer[m_Pos] = *(BYTE *)mem;
		else
			memcpy (m_Buffer + m_Pos, mem, len);
		m_Pos += len;
		if (m_Pos > m_BufferSize)
			m_BufferSize = m_Pos;
	}
	else
	{
		I_Error ("Tried to write to reading cfile");
	}
	return *this;
}

FFile &FCompressedFile::Read (void *mem, unsigned int len)
{
	if (m_Mode == EReading)
	{
		if (m_Pos + len > m_BufferSize)
		{
			I_Error ("Attempt to read past end of cfile");
		}
		if (len == 1)
			*(BYTE *)mem = m_Buffer[m_Pos];
		else
			memcpy (mem, m_Buffer + m_Pos, len);
		m_Pos += len;
	}
	else
	{
		I_Error ("Tried to read from writing cfile");
	}
	return *this;
}

unsigned int FCompressedFile::Tell () const
{
	return m_Pos;
}

FFile &FCompressedFile::Seek (int pos, ESeekPos ofs)
{
	if (ofs == ESeekRelative)
		pos += m_Pos;
	else if (ofs == ESeekEnd)
		pos = m_BufferSize - pos;

	if (pos < 0)
		m_Pos = 0;
	else if ((unsigned)pos > m_BufferSize)
		m_Pos = m_BufferSize;
	else
		m_Pos = pos;

	return *this;
}

CVAR (Bool, nofilecompression, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

void FCompressedFile::Implode ()
{
	uLong outlen;
	uLong len = m_BufferSize;
	Byte *compressed = NULL;
	BYTE *oldbuf = m_Buffer;
	int r;

	if (!nofilecompression && !m_NoCompress)
	{
		outlen = OUT_LEN(len);
		do
		{
			compressed = new Bytef[outlen];
			r = compress (compressed, &outlen, m_Buffer, len);
			if (r == Z_BUF_ERROR)
			{
				delete[] compressed;
				outlen += 1024;
			}
		} while (r == Z_BUF_ERROR);

		// If the data could not be compressed, store it as-is.
		if (r != Z_OK || outlen >= len)
		{
			DPrintf ("cfile could not be compressed\n");
			outlen = 0;
		}
		else
		{
			DPrintf ("cfile shrank from %lu to %lu bytes\n", len, outlen);
		}
	}
	else
	{
		outlen = 0;
	}

	m_MaxBufferSize = m_BufferSize = ((outlen == 0) ? len : outlen);
	m_Buffer = (BYTE *)M_Malloc (m_BufferSize + 8);
	m_Pos = 0;

	DWORD *lens = (DWORD *)(m_Buffer);
	lens[0] = BigLong((unsigned int)outlen);
	lens[1] = BigLong((unsigned int)len);

	if (outlen == 0)
		memcpy (m_Buffer + 8, oldbuf, len);
	else
		memcpy (m_Buffer + 8, compressed, outlen);
	if (compressed)
		delete[] compressed;
	M_Free (oldbuf);
}

void FCompressedFile::Explode ()
{
	uLong expandsize, cprlen;
	unsigned char *expand;

	if (m_Buffer)
	{
		unsigned int *ints = (unsigned int *)(m_Buffer);
		cprlen = BigLong(ints[0]);
		expandsize = BigLong(ints[1]);
		
		expand = (unsigned char *)M_Malloc (expandsize);
		if (cprlen)
		{
			int r;
			uLong newlen;

			newlen = expandsize;
			r = uncompress (expand, &newlen, m_Buffer + 8, cprlen);
			if (r != Z_OK || newlen != expandsize)
			{
				M_Free (expand);
				I_Error ("Could not decompress buffer: %s", M_ZLibError(r).GetChars());
			}
		}
		else
		{
			memcpy (expand, m_Buffer + 8, expandsize);
		}
		if (FreeOnExplode ())
			M_Free (m_Buffer);
		m_Buffer = expand;
		m_BufferSize = expandsize;
	}
}

FCompressedMemFile::FCompressedMemFile ()
{
	m_SourceFromMem = false;
	m_ImplodedBuffer = NULL;
}

/*
FCompressedMemFile::FCompressedMemFile (const char *name, EOpenMode mode)
	: FCompressedFile (name, mode)
{
	m_SourceFromMem = false;
	m_ImplodedBuffer = NULL;
}
*/

FCompressedMemFile::~FCompressedMemFile ()
{
	if (m_ImplodedBuffer != NULL)
	{
		M_Free (m_ImplodedBuffer);
	}
}

bool FCompressedMemFile::Open (const char *name, EOpenMode mode)
{
	if (mode == EWriting)
	{
		if (name)
		{
			I_Error ("FCompressedMemFile cannot write to disk");
		}
		else
		{
			return Open ();
		}
	}
	else
	{
		bool res = FCompressedFile::Open (name, EReading);
		if (res)
		{
			fclose (m_File);
			m_File = NULL;
		}
		return res;
	}
	return false;
}

bool FCompressedMemFile::Open (void *memblock)
{
	Close ();
	m_Mode = EReading;
	m_Buffer = (BYTE *)memblock;
	m_SourceFromMem = true;
	Explode ();
	m_SourceFromMem = false;
	return !!m_Buffer;
}

bool FCompressedMemFile::Open ()
{
	Close ();
	m_Mode = EWriting;
	m_BufferSize = 0;
	m_MaxBufferSize = 16384;
	m_Buffer = (unsigned char *)M_Malloc (16384);
	m_Pos = 0;
	return true;
}

bool FCompressedMemFile::Reopen ()
{
	if (m_Buffer == NULL && m_ImplodedBuffer)
	{
		m_Mode = EReading;
		m_Buffer = m_ImplodedBuffer;
		m_SourceFromMem = true;
		try
		{
			Explode ();
		}
		catch(...)
		{
			// If we just leave things as they are, m_Buffer and m_ImplodedBuffer
			// both point to the same memory block and both will try to free it.
			m_Buffer = NULL;
			m_SourceFromMem = false;
			throw;
		}
		m_SourceFromMem = false;
		return true;
	}
	return false;
}

void FCompressedMemFile::Close ()
{
	if (m_Mode == EWriting)
	{
		Implode ();
		m_ImplodedBuffer = m_Buffer;
		m_Buffer = NULL;
	}
}

void FCompressedMemFile::Serialize (FArchive &arc)
{
	if (arc.IsStoring ())
	{
		if (m_ImplodedBuffer == NULL)
		{
			I_Error ("FCompressedMemFile must be compressed before storing");
		}
		arc.Write (ZSig, 4);

		DWORD sizes[2];
		sizes[0] = SWAP_DWORD (((DWORD *)m_ImplodedBuffer)[0]);
		sizes[1] = SWAP_DWORD (((DWORD *)m_ImplodedBuffer)[1]);
		arc.Write (m_ImplodedBuffer, (sizes[0] ? sizes[0] : sizes[1])+8);
	}
	else
	{
		Close ();
		m_Mode = EReading;

		char sig[4];
		DWORD sizes[2] = { 0, 0 };

		arc.Read (sig, 4);

		if (sig[0] != ZSig[0] || sig[1] != ZSig[1] || sig[2] != ZSig[2] || sig[3] != ZSig[3])
			I_Error ("Expected to extract a compressed file");

		arc << sizes[0] << sizes[1];
		DWORD len = sizes[0] == 0 ? sizes[1] : sizes[0];

		m_Buffer = (BYTE *)M_Malloc (len+8);
		((DWORD *)m_Buffer)[0] = SWAP_DWORD(sizes[0]);
		((DWORD *)m_Buffer)[1] = SWAP_DWORD(sizes[1]);
		arc.Read (m_Buffer+8, len);
		m_ImplodedBuffer = m_Buffer;
		m_Buffer = NULL;
		m_Mode = EWriting;
	}
}

bool FCompressedMemFile::IsOpen () const
{
	return !!m_Buffer;
}

void FCompressedMemFile::GetSizes(unsigned int &compressed, unsigned int &uncompressed) const
{
	if (m_ImplodedBuffer != NULL)
	{
		compressed = BigLong(*(unsigned int *)m_ImplodedBuffer);
		uncompressed = BigLong(*(unsigned int *)(m_ImplodedBuffer + 4));
	}
	else
	{
		compressed = 0;
		uncompressed = m_BufferSize;
	}
}

FPNGChunkFile::FPNGChunkFile (FILE *file, DWORD id)
	: FCompressedFile (file, EWriting, true, false), m_ChunkID (id)
{
}

FPNGChunkFile::FPNGChunkFile (FILE *file, DWORD id, size_t chunklen)
	: FCompressedFile (file, EReading, true, false), m_ChunkID (id)
{
	m_Buffer = (BYTE *)M_Malloc (chunklen);
	m_BufferSize = (unsigned int)chunklen;
	fread (m_Buffer, chunklen, 1, m_File);
	// Skip the CRC for now. Maybe later it will be used.
	fseek (m_File, 4, SEEK_CUR);
}

// Unlike FCompressedFile::Close, m_File is left open
void FPNGChunkFile::Close ()
{
	DWORD data[2];
	DWORD crc;

	if (m_File)
	{
		if (m_Mode == EWriting)
		{
			crc = CalcCRC32 ((BYTE *)&m_ChunkID, 4);
			crc = AddCRC32 (crc, (BYTE *)m_Buffer, m_BufferSize);

			data[0] = BigLong(m_BufferSize);
			data[1] = m_ChunkID;
			fwrite (data, 8, 1, m_File);
			fwrite (m_Buffer, m_BufferSize, 1, m_File);
			crc = SWAP_DWORD (crc);
			fwrite (&crc, 4, 1, m_File);
		}
		m_File = NULL;
	}
	FCompressedFile::Close ();
}

FPNGChunkArchive::FPNGChunkArchive (FILE *file, DWORD id)
	: FArchive (), Chunk (file, id)
{
	AttachToFile (Chunk);
}

FPNGChunkArchive::FPNGChunkArchive (FILE *file, DWORD id, size_t len)
	: FArchive (), Chunk (file, id, len)
{
	AttachToFile (Chunk);
}

FPNGChunkArchive::~FPNGChunkArchive ()
{
	// Close before FArchive's destructor, because Chunk will be
	// destroyed before the FArchive is destroyed.
	Close ();
}

//============================================
//
// FArchive
//
//============================================

FArchive::FArchive ()
{
}

FArchive::FArchive (FFile &file)
{
	AttachToFile (file);
}

void FArchive::AttachToFile (FFile &file)
{
	unsigned int i;

	m_HubTravel = false;
	m_File = &file;
	m_MaxObjectCount = m_ObjectCount = 0;
	m_ObjectMap = NULL;
	if (file.Mode() == FFile::EReading)
	{
		m_Loading = true;
		m_Storing = false;
	}
	else
	{
		m_Loading = false;
		m_Storing = true;
	}
	m_Persistent = file.IsPersistent();
	m_TypeMap = NULL;
	m_TypeMap = new TypeMap[PClass::m_Types.Size()];
	for (i = 0; i < PClass::m_Types.Size(); i++)
	{
		m_TypeMap[i].toArchive = TypeMap::NO_INDEX;
		m_TypeMap[i].toCurrent = NULL;
	}
	m_ClassCount = 0;
	for (i = 0; i < EObjectHashSize; i++)
	{
		m_ObjectHash[i] = ~0;
		m_NameHash[i] = NameMap::NO_INDEX;
	}
	m_NumSprites = 0;
	m_SpriteMap = new int[sprites.Size()];
	for (size_t s = 0; s < sprites.Size(); ++s)
	{
		m_SpriteMap[s] = -1;
	}
}

FArchive::~FArchive ()
{
	Close ();
	if (m_TypeMap)
		delete[] m_TypeMap;
	if (m_ObjectMap)
		M_Free (m_ObjectMap);
	if (m_SpriteMap)
		delete[] m_SpriteMap;
}

void FArchive::Write (const void *mem, unsigned int len)
{
	m_File->Write (mem, len);
}

void FArchive::Read (void *mem, unsigned int len)
{
	m_File->Read (mem, len);
}

void FArchive::Close ()
{
	if (m_File)
	{
		m_File->Close ();
		m_File = NULL;
		DPrintf ("Processed %u objects\n", m_ObjectCount);
	}
}

void FArchive::WriteCount (DWORD count)
{
	BYTE out;

	do
	{
		out = count & 0x7f;
		if (count >= 0x80)
			out |= 0x80;
		Write (&out, sizeof(BYTE));
		count >>= 7;
	} while (count);

}

DWORD FArchive::ReadCount ()
{
	BYTE in;
	DWORD count = 0;
	int ofs = 0;

	do
	{
		Read (&in, sizeof(BYTE));
		count |= (in & 0x7f) << ofs;
		ofs += 7;
	} while (in & 0x80);

	return count;
}

void FArchive::WriteName (const char *name)
{
	BYTE id;

	if (name == NULL)
	{
		id = NIL_NAME;
		Write (&id, 1);
	}
	else
	{
		DWORD index = FindName (name);
		if (index != NameMap::NO_INDEX)
		{
			id = OLD_NAME;
			Write (&id, 1);
			WriteCount (index);
		}
		else
		{
			AddName (name);
			id = NEW_NAME;
			Write (&id, 1);
			WriteString (name);
		}
	}
}

const char *FArchive::ReadName ()
{
	BYTE id;

	operator<< (id);
	if (id == NIL_NAME)
	{
		return NULL;
	}
	else if (id == OLD_NAME)
	{
		DWORD index = ReadCount ();
		if (index >= m_Names.Size())
		{
			I_Error ("Name %u has not been read yet\n", index);
		}
		return &m_NameStorage[m_Names[index].StringStart];
	}
	else if (id == NEW_NAME)
	{
		DWORD index;
		DWORD size = ReadCount ();
		char *str;

		index = (DWORD)m_NameStorage.Reserve (size);
		str = &m_NameStorage[index];
		Read (str, size-1);
		str[size-1] = 0;
		AddName (index);
		return str;
	}
	else
	{
		I_Error ("Expected a name but got something else\n");
		return NULL;
	}
}

void FArchive::WriteString (const char *str)
{
	if (str == NULL)
	{
		WriteCount (0);
	}
	else
	{
		DWORD size = (DWORD)(strlen (str) + 1);
		WriteCount (size);
		Write (str, size - 1);
	}
}

FArchive &FArchive::operator<< (char *&str)
{
	if (m_Storing)
	{
		WriteString (str);
	}
	else
	{
		DWORD size = ReadCount ();
		char *str2;

		if (size == 0)
		{
			str2 = NULL;
		}
		else
		{
			str2 = new char[size];
			size--;
			Read (str2, size);
			str2[size] = 0;
			ReplaceString ((char **)&str, str2);
		}
		if (str)
		{
			delete[] str;
		}
		str = str2;
	}
	return *this;
}

FArchive &FArchive::operator<< (FString &str)
{
	if (m_Storing)
	{
		WriteString (str.GetChars());
	}
	else
	{
		DWORD size = ReadCount();

		if (size == 0)
		{
			str = "";
		}
		else
		{
			char *str2 = (char *)alloca(size*sizeof(char));
			size--;
			Read (str2, size);
			str2[size] = 0;
			str = str2;
		}
	}
	return *this;
}

FArchive &FArchive::operator<< (BYTE &c)
{
	if (m_Storing)
		Write (&c, sizeof(BYTE));
	else
		Read (&c, sizeof(BYTE));
	return *this;
}

FArchive &FArchive::operator<< (WORD &w)
{
	if (m_Storing)
	{
		WORD temp = SWAP_WORD(w);
		Write (&temp, sizeof(WORD));
	}
	else
	{
		Read (&w, sizeof(WORD));
		w = SWAP_WORD(w);
	}
	return *this;
}

FArchive &FArchive::operator<< (DWORD &w)
{
	if (m_Storing)
	{
		DWORD temp = SWAP_DWORD(w);
		Write (&temp, sizeof(DWORD));
	}
	else
	{
		Read (&w, sizeof(DWORD));
		w = SWAP_DWORD(w);
	}
	return *this;
}

FArchive &FArchive::operator<< (QWORD &w)
{
	if (m_Storing)
	{
		QWORD temp = SWAP_QWORD(w);
		Write (&temp, sizeof(QWORD));
	}
	else
	{
		Read (&w, sizeof(QWORD));
		w = SWAP_QWORD(w);
	}
	return *this;
}

FArchive &FArchive::operator<< (float &w)
{
	if (m_Storing)
	{
		float temp = w;
		SWAP_FLOAT(temp);
		Write (&temp, sizeof(float));
	}
	else
	{
		Read (&w, sizeof(float));
		SWAP_FLOAT(w);
	}
	return *this;
}

FArchive &FArchive::operator<< (double &w)
{
	if (m_Storing)
	{
		double temp;
		SWAP_DOUBLE(temp,w);
		Write (&temp, sizeof(double));
	}
	else
	{
		Read (&w, sizeof(double));
		SWAP_DOUBLE(w,w);
	}
	return *this;
}

FArchive &FArchive::operator<< (FName &n)
{ // In an archive, a "name" is a string that might be stored multiple times,
  // so it is only stored once. It is still treated as a normal string. In the
  // rest of the game, a name is a unique identifier for a number.
	if (m_Storing)
	{
		WriteName (n.GetChars());
	}
	else
	{
		n = FName(ReadName());
	}
	return *this;
}

FArchive &FArchive::SerializePointer (void *ptrbase, BYTE **ptr, DWORD elemSize)
{
	DWORD w;

	if (m_Storing)
	{
		if (*(void **)ptr)
		{
			w = DWORD(((size_t)*ptr - (size_t)ptrbase) / elemSize);
		}
		else
		{
			w = ~0u;
		}
		WriteCount (w);
	}
	else
	{
		w = ReadCount ();
		if (w != ~0u)
		{
			*(void **)ptr = (BYTE *)ptrbase + w * elemSize;
		}
		else
		{
			*(void **)ptr = NULL;
		}
	}
	return *this;
}

FArchive &FArchive::SerializeObject (DObject *&object, PClass *type)
{
	if (IsStoring ())
	{
		return WriteObject (object);
	}
	else
	{
		return ReadObject (object, type);
	}
}

FArchive &FArchive::WriteObject (DObject *obj)
{
	player_t *player;
	BYTE id[2];

	if (obj == NULL)
	{
		id[0] = NULL_OBJ;
		Write (id, 1);
	}
	else if (obj == (DObject*)~0)
	{
		id[0] = M1_OBJ;
		Write (id, 1);
	}
	else if (obj->ObjectFlags & OF_EuthanizeMe)
	{
		// Objects that want to die are not saved to the archive, but
		// we leave the pointers to them alone.
		id[0] = NULL_OBJ;
		Write (id, 1);
	}
	else
	{
		const PClass *type = RUNTIME_TYPE(obj);

		if (type == RUNTIME_CLASS(DObject))
		{
			//I_Error ("Tried to save an instance of DObject.\n"
			//		 "This should not happen.\n");
			id[0] = NULL_OBJ;
			Write (id, 1);
		}
		else if (m_TypeMap[type->ClassIndex].toArchive == TypeMap::NO_INDEX)
		{
			// No instances of this class have been written out yet.
			// Write out the class, then write out the object. If this
			// is an actor controlled by a player, make note of that
			// so that it can be overridden when moving around in a hub.
			if (obj->IsKindOf (RUNTIME_CLASS (AActor)) &&
				(player = static_cast<AActor *>(obj)->player) &&
				player->mo == obj)
			{
				id[0] = NEW_PLYR_CLS_OBJ;
				id[1] = (BYTE)(player - players);
				Write (id, 2);
			}
			else
			{
				id[0] = NEW_CLS_OBJ;
				Write (id, 1);
			}
			WriteClass (type);
//			Printf ("Make class %s (%u)\n", type->Name, m_File->Tell());
			MapObject (obj);
			obj->SerializeUserVars (*this);
			obj->Serialize (*this);
			obj->CheckIfSerialized ();
		}
		else
		{
			// An instance of this class has already been saved. If
			// this object has already been written, save a reference
			// to the saved object. Otherwise, save a reference to the
			// class, then save the object. Again, if this is a player-
			// controlled actor, remember that.
			DWORD index = FindObjectIndex (obj);

			if (index == TypeMap::NO_INDEX)
			{

				if (obj->IsKindOf (RUNTIME_CLASS (AActor)) &&
					(player = static_cast<AActor *>(obj)->player) &&
					player->mo == obj)
				{
					id[0] = NEW_PLYR_OBJ;
					id[1] = (BYTE)(player - players);
					Write (id, 2);
				}
				else
				{
					id[0] = NEW_OBJ;
					Write (id, 1);
				}
				WriteCount (m_TypeMap[type->ClassIndex].toArchive);
//				Printf ("Reuse class %s (%u)\n", type->Name, m_File->Tell());
				MapObject (obj);
				obj->SerializeUserVars (*this);
				obj->Serialize (*this);
				obj->CheckIfSerialized ();
			}
			else
			{
				id[0] = OLD_OBJ;
				Write (id, 1);
				WriteCount (index);
			}
		}
	}
	return *this;
}

FArchive &FArchive::ReadObject (DObject* &obj, PClass *wanttype)
{
	BYTE objHead;
	const PClass *type;
	BYTE playerNum;
	DWORD index;

	operator<< (objHead);

	switch (objHead)
	{
	case NULL_OBJ:
		obj = NULL;
		break;

	case M1_OBJ:
		obj = (DObject *)~0;
		break;

	case OLD_OBJ:
		index = ReadCount ();
		if (index >= m_ObjectCount)
		{
			I_Error ("Object reference too high (%u; max is %u)\n", index, m_ObjectCount);
		}
		obj = (DObject *)m_ObjectMap[index].object;
		break;

	case NEW_PLYR_CLS_OBJ:
		operator<< (playerNum);
		if (m_HubTravel)
		{
			// If travelling inside a hub, use the existing player actor
			type = ReadClass (wanttype);
//			Printf ("New player class: %s (%u)\n", type->Name, m_File->Tell());
			obj = players[playerNum].mo;

			// But also create a new one so that we can get past the one
			// stored in the archive.
			AActor *tempobj = static_cast<AActor *>(type->CreateNew ());
			MapObject (obj != NULL ? obj : tempobj);
			tempobj->SerializeUserVars (*this);
			tempobj->Serialize (*this);
			tempobj->CheckIfSerialized ();
			// If this player is not present anymore, keep the new body
			// around just so that the load will succeed.
			if (obj != NULL)
			{
				// When the temporary player's inventory items were loaded,
				// they became owned by the real player. Undo that now.
				for (AInventory *item = tempobj->Inventory; item != NULL; item = item->Inventory)
				{
					item->Owner = tempobj;
				}
				tempobj->Destroy ();
			}
			else
			{
				obj = tempobj;
				players[playerNum].mo = static_cast<APlayerPawn *>(obj);
			}
			break;
		}
		/* fallthrough when not travelling to a previous level */
	case NEW_CLS_OBJ:
		type = ReadClass (wanttype);
//		Printf ("New class: %s (%u)\n", type->Name, m_File->Tell());
		obj = type->CreateNew ();
		MapObject (obj);
		obj->SerializeUserVars (*this);
		obj->Serialize (*this);
		obj->CheckIfSerialized ();
		break;

	case NEW_PLYR_OBJ:
		operator<< (playerNum);
		if (m_HubTravel)
		{
			type = ReadStoredClass (wanttype);
//			Printf ("Use player class: %s (%u)\n", type->Name, m_File->Tell());
			obj = players[playerNum].mo;

			AActor *tempobj = static_cast<AActor *>(type->CreateNew ());
			MapObject (obj != NULL ? obj : tempobj);
			tempobj->SerializeUserVars (*this);
			tempobj->Serialize (*this);
			tempobj->CheckIfSerialized ();
			if (obj != NULL)
			{
				for (AInventory *item = tempobj->Inventory;
					item != NULL; item = item->Inventory)
				{
					item->Owner = tempobj;
				}
				tempobj->Destroy ();
			}
			else
			{
				obj = tempobj;
				players[playerNum].mo = static_cast<APlayerPawn *>(obj);
			}
			break;
		}
		/* fallthrough when not travelling to a previous level */
	case NEW_OBJ:
		type = ReadStoredClass (wanttype);
//		Printf ("Use class: %s (%u)\n", type->Name, m_File->Tell());
		obj = type->CreateNew ();
		MapObject (obj);
		obj->SerializeUserVars (*this);
		obj->Serialize (*this);
		obj->CheckIfSerialized ();
		break;

	default:
		I_Error ("Unknown object code (%d) in archive\n", objHead);
	}
	return *this;
}

void FArchive::WriteSprite (int spritenum)
{
	BYTE id;

	if ((unsigned)spritenum >= (unsigned)sprites.Size())
	{
		spritenum = 0;
	}

	if (m_SpriteMap[spritenum] < 0)
	{
		m_SpriteMap[spritenum] = (int)(m_NumSprites++);
		id = NEW_SPRITE; 
		Write (&id, 1);
		Write (sprites[spritenum].name, 4);

		// Write the current sprite number as a hint, because
		// these will only change between different versions.
		WriteCount (spritenum);
	}
	else
	{
		id = OLD_SPRITE;
		Write (&id, 1);
		WriteCount (m_SpriteMap[spritenum]);
	}
}

int FArchive::ReadSprite ()
{
	BYTE id;

	Read (&id, 1);
	if (id == OLD_SPRITE)
	{
		DWORD index = ReadCount ();
		if (index >= m_NumSprites)
		{
			I_Error ("Sprite %u has not been read yet\n", index);
		}
		return m_SpriteMap[index];
	}
	else if (id == NEW_SPRITE)
	{
		DWORD name;
		DWORD hint;

		Read (&name, 4);
		hint = ReadCount ();

		if (hint >= NumStdSprites || sprites[hint].dwName != name)
		{
			for (hint = NumStdSprites; hint-- != 0; )
			{
				if (sprites[hint].dwName == name)
				{
					break;
				}
			}
			if (hint >= sprites.Size())
			{ // Don't know this sprite, so just use the first one
				hint = 0;
			}
		}
		m_SpriteMap[m_NumSprites++] = hint;
		return hint;
	}
	else
	{
		I_Error ("Expected a sprite but got something else\n");
		return 0;
	}
}

DWORD FArchive::AddName (const char *name)
{
	DWORD index;
	unsigned int hash = MakeKey (name) % EObjectHashSize;

	index = FindName (name, hash);
	if (index == NameMap::NO_INDEX)
	{
		DWORD namelen = (DWORD)(strlen (name) + 1);
		DWORD strpos = (DWORD)m_NameStorage.Reserve (namelen);
		NameMap mapper = { strpos, (DWORD)m_NameHash[hash] };

		memcpy (&m_NameStorage[strpos], name, namelen);
		m_NameHash[hash] = index = (DWORD)m_Names.Push (mapper);
	}
	return index;
}

DWORD FArchive::AddName (unsigned int start)
{
	DWORD hash = MakeKey (&m_NameStorage[start]) % EObjectHashSize;
	NameMap mapper = { (DWORD)start, (DWORD)m_NameHash[hash] };
	return (DWORD)(m_NameHash[hash] = m_Names.Push (mapper));
}

DWORD FArchive::FindName (const char *name) const
{
	return FindName (name, MakeKey (name) % EObjectHashSize);
}

DWORD FArchive::FindName (const char *name, unsigned int bucket) const
{
	unsigned int map = m_NameHash[bucket];

	while (map != NameMap::NO_INDEX)
	{
		const NameMap *mapping = &m_Names[map];
		if (strcmp (name, &m_NameStorage[mapping->StringStart]) == 0)
		{
			return (DWORD)map;
		}
		map = mapping->HashNext;
	}
	return (DWORD)map;
}

DWORD FArchive::WriteClass (const PClass *info)
{
	if (m_ClassCount >= PClass::m_Types.Size())
	{
		I_Error ("Too many unique classes have been written.\nOnly %u were registered\n",
			PClass::m_Types.Size());
	}
	if (m_TypeMap[info->ClassIndex].toArchive != TypeMap::NO_INDEX)
	{
		I_Error ("Attempt to write '%s' twice.\n", info->TypeName.GetChars());
	}
	m_TypeMap[info->ClassIndex].toArchive = m_ClassCount;
	m_TypeMap[m_ClassCount].toCurrent = info;
	WriteString (info->TypeName.GetChars());
	return m_ClassCount++;
}

const PClass *FArchive::ReadClass ()
{
	struct String {
		String() { val = NULL; }
		~String() { if (val) delete[] val; }
		char *val;
	} typeName;

	if (m_ClassCount >= PClass::m_Types.Size())
	{
		I_Error ("Too many unique classes have been read.\nOnly %u were registered\n",
			PClass::m_Types.Size());
	}
	operator<< (typeName.val);
	FName zaname(typeName.val, true);
	if (zaname != NAME_None)
	{
		for (unsigned int i = PClass::m_Types.Size(); i-- > 0; )
		{
			if (PClass::m_Types[i]->TypeName == zaname)
			{
				m_TypeMap[i].toArchive = m_ClassCount;
				m_TypeMap[m_ClassCount].toCurrent = PClass::m_Types[i];
				m_ClassCount++;
				return PClass::m_Types[i];
			}
		}
	}
	I_Error ("Unknown class '%s'\n", typeName.val);
	return NULL;
}

const PClass *FArchive::ReadClass (const PClass *wanttype)
{
	const PClass *type = ReadClass ();
	if (!type->IsDescendantOf (wanttype))
	{
		I_Error ("Expected to extract an object of type '%s'.\n"
				 "Found one of type '%s' instead.\n",
			wanttype->TypeName.GetChars(), type->TypeName.GetChars());
	}
	return type;
}

const PClass *FArchive::ReadStoredClass (const PClass *wanttype)
{
	DWORD index = ReadCount ();
	if (index >= m_ClassCount)
	{
		I_Error ("Class reference too high (%u; max is %u)\n", index, m_ClassCount);
	}
	const PClass *type = m_TypeMap[index].toCurrent;
	if (!type->IsDescendantOf (wanttype))
	{
		I_Error ("Expected to extract an object of type '%s'.\n"
				 "Found one of type '%s' instead.\n",
			wanttype->TypeName.GetChars(), type->TypeName.GetChars());
	}
	return type;
}

DWORD FArchive::MapObject (const DObject *obj)
{
	DWORD i;

	if (m_ObjectCount >= m_MaxObjectCount)
	{
		m_MaxObjectCount = m_MaxObjectCount ? m_MaxObjectCount * 2 : 1024;
		m_ObjectMap = (ObjectMap *)M_Realloc (m_ObjectMap, sizeof(ObjectMap)*m_MaxObjectCount);
		for (i = m_ObjectCount; i < m_MaxObjectCount; i++)
		{
			m_ObjectMap[i].hashNext = ~0;
			m_ObjectMap[i].object = NULL;
		}
	}

	DWORD index = m_ObjectCount++;
	DWORD hash = HashObject (obj);

	m_ObjectMap[index].object = obj;
	m_ObjectMap[index].hashNext = m_ObjectHash[hash];
	m_ObjectHash[hash] = index;

	return index;
}

DWORD FArchive::HashObject (const DObject *obj) const
{
	return (DWORD)((size_t)obj % EObjectHashSize);
}

DWORD FArchive::FindObjectIndex (const DObject *obj) const
{
	DWORD index = m_ObjectHash[HashObject (obj)];
	while (index != TypeMap::NO_INDEX && m_ObjectMap[index].object != obj)
	{
		index = m_ObjectMap[index].hashNext;
	}
	return index;
}

void FArchive::UserWriteClass (const PClass *type)
{
	BYTE id;

	if (type == NULL)
	{
		id = 2;
		Write (&id, 1);
	}
	else
	{
		if (m_TypeMap[type->ClassIndex].toArchive == TypeMap::NO_INDEX)
		{
			id = 1;
			Write (&id, 1);
			WriteClass (type);
		}
		else
		{
			id = 0;
			Write (&id, 1);
			WriteCount (m_TypeMap[type->ClassIndex].toArchive);
		}
	}
}

void FArchive::UserReadClass (const PClass *&type)
{
	BYTE newclass;

	Read (&newclass, 1);
	switch (newclass)
	{
	case 0:
		type = ReadStoredClass (RUNTIME_CLASS(DObject));
		break;
	case 1:
		type = ReadClass ();
		break;
	case 2:
		type = NULL;
		break;
	default:
		I_Error ("Unknown class type %d in archive.\n", newclass);
		break;
	}
}

FArchive &operator<< (FArchive &arc, const PClass * &info)
{
	if (arc.IsStoring ())
	{
		arc.UserWriteClass (info);
	}
	else
	{
		arc.UserReadClass (info);
	}
	return arc;
}

FArchive &operator<< (FArchive &arc, sector_t *&sec)
{
	return arc.SerializePointer (sectors, (BYTE **)&sec, sizeof(*sectors));
}

FArchive &operator<< (FArchive &arc, const sector_t *&sec)
{
	return arc.SerializePointer (sectors, (BYTE **)&sec, sizeof(*sectors));
}

FArchive &operator<< (FArchive &arc, line_t *&line)
{
	return arc.SerializePointer (lines, (BYTE **)&line, sizeof(*lines));
}

FArchive &operator<< (FArchive &arc, vertex_t *&vert)
{
	return arc.SerializePointer (vertexes, (BYTE **)&vert, sizeof(*vertexes));
}

FArchive &operator<< (FArchive &arc, side_t *&side)
{
	return arc.SerializePointer (sides, (BYTE **)&side, sizeof(*sides));
}
