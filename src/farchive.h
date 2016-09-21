/*
** farchive.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
*/

#ifndef __FARCHIVE_H__
#define __FARCHIVE_H__

#include <stdio.h>
#include "dobject.h"
#include "r_state.h"
#include "tflags.h"

class FFile
{
public:
		enum EOpenMode
		{
			EReading,
			EWriting,
			ENotOpen
		};

		enum ESeekPos
		{
			ESeekSet,
			ESeekRelative,
			ESeekEnd
		};

virtual	~FFile () {}

virtual	bool Open (const char *name, EOpenMode mode) = 0;
virtual	void Close () = 0;
virtual	void Flush () = 0;
virtual EOpenMode Mode () const = 0;
virtual bool IsPersistent () const = 0;
virtual bool IsOpen () const = 0;

virtual	FFile& Write (const void *, unsigned int) = 0;
virtual	FFile& Read (void *, unsigned int) = 0;

virtual	unsigned int Tell () const = 0;
virtual	FFile& Seek (int, ESeekPos) = 0;
inline	FFile& Seek (unsigned int i, ESeekPos p) { return Seek ((int)i, p); }
};

class FCompressedFile : public FFile
{
public:
	FCompressedFile ();
	FCompressedFile (const char *name, EOpenMode mode, bool dontcompress = false);
	FCompressedFile (FILE *file, EOpenMode mode, bool dontcompress = false, bool postopen=true);
	~FCompressedFile ();

	bool Open (const char *name, EOpenMode mode);
	void Close ();
	void Flush ();
	EOpenMode Mode () const;
	bool IsPersistent () const { return true; }
	bool IsOpen () const;
	unsigned int GetSize () const { return m_BufferSize; }

	FFile &Write (const void *, unsigned int);
	FFile &Read (void *, unsigned int);
	unsigned int Tell () const;
	FFile &Seek (int, ESeekPos);

protected:
	unsigned int m_Pos;
	unsigned int m_BufferSize;
	unsigned int m_MaxBufferSize;
	unsigned char *m_Buffer;
	bool m_NoCompress;
	EOpenMode m_Mode;
	FILE *m_File;

	void Implode ();
	void Explode ();
	virtual bool FreeOnExplode () { return true; }
	void PostOpen ();

private:
	void BeEmpty ();
};

class FPNGChunkFile : public FCompressedFile
{
public:
	FPNGChunkFile (FILE *file, DWORD id);					// Create for writing
	FPNGChunkFile (FILE *file, DWORD id, size_t chunklen);	// Create for reading

	void Close ();

private:
	DWORD m_ChunkID;
};

class FArchive
{
public:
		FArchive (FFile &file);
		virtual ~FArchive ();

		inline bool IsLoading () const { return m_Loading; }
		inline bool IsStoring () const { return m_Storing; }
		inline bool IsPeristent () const { return m_Persistent; }
		
		void SetHubTravel () { m_HubTravel = true; }

		void Close ();

virtual	void Write (const void *mem, unsigned int len);
virtual void Read (void *mem, unsigned int len);

		void WriteString(const FString &str);
		void WriteString (const char *str);

		void WriteByte(BYTE val);
		void WriteInt16(WORD val);
		void WriteInt32(DWORD val);
		void WriteInt64(QWORD val);

		void WriteCount (DWORD count);
		DWORD ReadCount ();

		void UserReadClass (PClass *&info);
		template<typename T> void UserReadClass(T *&info)
		{
			PClass *myclass;
			UserReadClass(myclass);
			info = dyn_cast<T>(myclass);
		}

		FArchive& operator<< (BYTE &c);
		FArchive& operator<< (WORD &s);
		FArchive& operator<< (DWORD &i);
		FArchive& operator<< (QWORD &i);
		FArchive& operator<< (QWORD_UNION &i) { return operator<< (i.AsOne); }
		FArchive& operator<< (float &f);
		FArchive& operator<< (double &d);
		FArchive& operator<< (char *&str);
		FArchive& operator<< (FName &n);
		FArchive& operator<< (FString &str);

		void WriteName (const char *name);
		const char *ReadName ();	// The returned name disappears with the archive, unlike strings


inline	FArchive& operator<< (SBYTE &c) { return operator<< ((BYTE &)c); }
inline	FArchive& operator<< (SWORD &s) { return operator<< ((WORD &)s); }
inline	FArchive& operator<< (SDWORD &i) { return operator<< ((DWORD &)i); }
inline	FArchive& operator<< (SQWORD &i) { return operator<< ((QWORD &)i); }
inline	FArchive& operator<< (unsigned char *&str) { return operator<< ((char *&)str); }
inline	FArchive& operator<< (signed char *&str) { return operator<< ((char *&)str); }
inline	FArchive& operator<< (bool &b) { return operator<< ((BYTE &)b); }

	void EnableThinkers()
	{
		m_ThinkersAllowed = true;
	}

	bool ThinkersAllowed()
	{
		return m_ThinkersAllowed;
	}

protected:
		enum { EObjectHashSize = 137 };

		DWORD WriteClass (PClass *info);
		PClass *ReadClass ();
		PClass *ReadClass (const PClass *wanttype);
		PClass *ReadStoredClass (const PClass *wanttype);
		DWORD AddName (const char *name);
		DWORD AddName (unsigned int start);	// Name has already been added to storage
		DWORD FindName (const char *name) const;
		DWORD FindName (const char *name, unsigned int bucket) const;

		bool m_Persistent;		// meant for persistent storage (disk)?
		bool m_Loading;			// extracting objects?
		bool m_Storing;			// inserting objects?
		bool m_HubTravel;		// travelling inside a hub?
		FFile *m_File;			// unerlying file object

		TMap<PClass *, DWORD> ClassToArchive;	// Maps PClass to archive type index
		TArray<PClass *>	  ArchiveToClass;	// Maps archive type index to PClass

		TMap<DObject *, DWORD> ObjectToArchive;	// Maps objects to archive index
		TArray<DObject *>	   ArchiveToObject;	// Maps archive index to objects

		struct NameMap
		{
			DWORD StringStart;	// index into m_NameStorage
			DWORD HashNext;		// next in hash bucket
			enum { NO_INDEX = 0xffffffff };
		};
		TArray<NameMap> m_Names;
		TArray<char> m_NameStorage;
		unsigned int m_NameHash[EObjectHashSize];

		int *m_SpriteMap;
		size_t m_NumSprites;
		bool m_ThinkersAllowed = false;

		FArchive ();
		void AttachToFile (FFile &file);

private:
		FArchive (const FArchive &) {}
		void operator= (const FArchive &) {}
};

// Create an FPNGChunkFile and FArchive in one step
class FPNGChunkArchive : public FArchive
{
public:
	FPNGChunkArchive (FILE *file, DWORD chunkid);
	FPNGChunkArchive (FILE *file, DWORD chunkid, size_t chunklen);
	~FPNGChunkArchive ();
	FPNGChunkFile Chunk;
};


#endif //__FARCHIVE_H__
