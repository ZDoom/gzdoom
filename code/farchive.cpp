#include <stddef.h>
#include <string.h>
#include <zlib.h>

#include "doomtype.h"
#include "farchive.h"
#include "m_alloc.h"
#include "m_swap.h"
#include "cmdlib.h"
#include "i_system.h"
#include "c_cvars.h"
#include "d_player.h"
#include "dobject.h"

#ifdef __BIG_ENDIAN__
#define SWAP_WORD(x)
#define SWAP_DWORD(x)
#define SWAP_QWORD(x)
#define SWAP_FLOAT(x)
#define SWAP_DOUBLE(x)
#else
#define SWAP_WORD(x)		{ x = (((x)<<8) | ((x)>>8)); }
#define SWAP_DWORD(x)		{ x = (((x)>>24) | (((x)>>8)&0xff00) | ((x)<<8)&0xff0000 | ((x)<<24)); }
#if 0
#define SWAP_QWORD(x)		{ x = (((x)>>56) | (((x)>>40)&(0xff<<8)) | (((x)>>24)&(0xff<<16)) | (((x)>>8)&(0xff<<24)) |\
								   (((x)<<8)&(QWORD)0xff00000000) | (((x)<<24)&(QWORD)0xff0000000000) | (((x)<<40)&(QWORD)0xff000000000000) | ((x)<<56))); }
#else
#define SWAP_QWORD(x)		{ DWORD *y = (DWORD *)&x; DWORD t=y[0]; y[0]=y[1]; y[1]=t; SWAP_DWORD(y[0]); SWAP_DWORD(y[1]); }
#endif
#define SWAP_FLOAT(x)		{ DWORD dw = *(DWORD *)&x; SWAP_DWORD(dw); x = *(float *)&dw; }
#define SWAP_DOUBLE(x)		{ QWORD qw = *(QWORD *)&x; SWAP_QWORD(qw); x = *(double *)&qw; }
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

FCompressedFile::FCompressedFile (FILE *file, EOpenMode mode, bool dontCompress)
{
	BeEmpty ();
	m_Mode = mode;
	m_File = file;
	m_NoCompress = dontCompress;
	PostOpen ();
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
				Printf (PRINT_HIGH, "Compressed files from older ZDooms are not supported.\n");
			}
		}
		else
		{
			DWORD sizes[2];
			fread (sizes, sizeof(DWORD), 2, m_File);
			SWAP_DWORD (sizes[0]);
			SWAP_DWORD (sizes[1]);
			unsigned int len = sizes[0] == 0 ? sizes[1] : sizes[0];
			m_Buffer = (byte *)Malloc (len+8);
			fread (m_Buffer+8, len, 1, m_File);
			SWAP_DWORD (sizes[0]);
			SWAP_DWORD (sizes[1]);
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
		free (m_Buffer);
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
			m_Buffer = (byte *)Realloc (m_Buffer, m_MaxBufferSize);
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
			I_Error ("Attempt to read past end of cfilen");
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
	byte *oldbuf = m_Buffer;
	int r;

	if (!*nofilecompression && !m_NoCompress)
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
		if (r != Z_OK || outlen > len)
		{
			DPrintf ("cfile could not be deflated\n");
			outlen = 0;
		}
		else
		{
			DPrintf ("cfile shrank from %u to %u bytes\n", len, outlen);
		}
	}
	else
	{
		outlen = 0;
	}

	m_MaxBufferSize = m_BufferSize = ((outlen == 0) ? len : outlen);
	m_Buffer = (BYTE *)Malloc (m_BufferSize + 8);
	m_Pos = 0;

	DWORD *lens = (DWORD *)(m_Buffer);
	lens[0] = BELONG((unsigned int)outlen);
	lens[1] = BELONG((unsigned int)len);

	if (outlen == 0)
		memcpy (m_Buffer + 8, oldbuf, len);
	else
		memcpy (m_Buffer + 8, compressed, outlen);
	if (compressed)
		delete[] compressed;
	free (oldbuf);
}

void FCompressedFile::Explode ()
{
	uLong expandsize, cprlen;
	unsigned char *expand;

	if (m_Buffer)
	{
		unsigned int *ints = (unsigned int *)(m_Buffer);
		cprlen = BELONG(ints[0]);
		expandsize = BELONG(ints[1]);
		
		expand = (unsigned char *)Malloc (expandsize);
		if (cprlen)
		{
			int r;
			uLong newlen;

			newlen = expandsize;
			r = uncompress (expand, &newlen, m_Buffer + 8, cprlen);
			if (r != Z_OK || newlen != expandsize)
			{
				free (expand);
				I_Error ("Could not decompress cfile");
			}
		}
		else
		{
			memcpy (expand, m_Buffer + 8, expandsize);
		}
		if (FreeOnExplode ())
			free (m_Buffer);
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
	m_Buffer = (unsigned char *)Malloc (16384);
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
		Explode ();
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
			I_Error ("FCompressedMemFile must be deflated before storing");
		}
		arc.Write (ZSig, 4);

		DWORD sizes[2];
		sizes[0] = ((DWORD *)m_ImplodedBuffer)[0];
		sizes[1] = ((DWORD *)m_ImplodedBuffer)[1];
		SWAP_DWORD (sizes[0]);
		SWAP_DWORD (sizes[1]);
		arc.Write (m_ImplodedBuffer, (sizes[0] ? sizes[0] : sizes[1])+8);
	}
	else
	{
		Close ();
		m_Mode = EReading;

		char sig[4];
		DWORD sizes[2];

		arc.Read (sig, 4);

		if (sig[0] != ZSig[0] || sig[1] != ZSig[1] || sig[2] != ZSig[2] || sig[3] != ZSig[3])
			I_Error ("Expected to extract a compressed file");

		arc << sizes[0] << sizes[1];
		DWORD len = sizes[0] == 0 ? sizes[1] : sizes[0];

		m_Buffer = (BYTE *)Malloc (len+8);
		SWAP_DWORD (sizes[0]);
		SWAP_DWORD (sizes[1]);
		((DWORD *)m_Buffer)[0] = sizes[0];
		((DWORD *)m_Buffer)[1] = sizes[1];
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

//============================================
//
// FArchive
//
//============================================

FArchive::FArchive (FFile &file)
{
	int i;

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
	m_TypeMap = new TypeMap[TypeInfo::m_NumTypes];
	for (i = 0; i < TypeInfo::m_NumTypes; i++)
	{
		m_TypeMap[i].toArchive = ~0;
		m_TypeMap[i].toCurrent = NULL;
	}
	m_ClassCount = 0;
	for (i = 0; i < EObjectHashSize; i++)
		m_ObjectHash[i] = ~0;
}

FArchive::~FArchive ()
{
	Close ();
	if (m_TypeMap)
		delete[] m_TypeMap;
	if (m_ObjectMap)
		free (m_ObjectMap);
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
		DPrintf ("Processed %d objects\n", m_ObjectCount);
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

void FArchive::WriteString (const char *str)
{
	if (str == NULL)
	{
		WriteCount (0);
	}
	else
	{
		DWORD size = strlen (str) + 1;
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
		if (size == 0)
		{
			ReplaceString ((char **)&str, NULL);
		}
		else
		{
			char *str2 = new char[size];
			size--;
			Read (str2, size);
			str2[size] = 0;
			ReplaceString ((char **)&str, str2);
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
		WORD temp = w;
		SWAP_WORD(temp);
		Write (&temp, sizeof(WORD));
	}
	else
	{
		Read (&w, sizeof(WORD));
		SWAP_WORD(w);
	}
	return *this;
}

FArchive &FArchive::operator<< (DWORD &w)
{
	if (m_Storing)
	{
		DWORD temp = w;
		SWAP_DWORD(temp);
		Write (&temp, sizeof(DWORD));
	}
	else
	{
		Read (&w, sizeof(DWORD));
		SWAP_DWORD(w);
	}
	return *this;
}

FArchive &FArchive::operator<< (QWORD &w)
{
	if (m_Storing)
	{
		QWORD temp = w;
		SWAP_QWORD(temp);
		Write (&temp, sizeof(QWORD));
	}
	else
	{
		Read (&w, sizeof(QWORD));
		SWAP_QWORD(w);
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
		double temp = w;
		SWAP_DOUBLE(temp);
		Write (&temp, sizeof(double));
	}
	else
	{
		Read (&w, sizeof(double));
		SWAP_DOUBLE(w);
	}
	return *this;
}

FArchive &FArchive::SerializePointer (void *ptrbase, BYTE **ptr, DWORD elemSize)
{
	WORD w;

	if (m_Storing)
	{
		if (*ptr)
		{
			w = (*ptr - (byte *)ptrbase) / elemSize;
			SWAP_WORD(w);
		}
		else
		{
			w = 0xffff;
		}
		Write (&w, sizeof(WORD));
	}
	else
	{
		Read (&w, sizeof(WORD));
		SWAP_WORD (w);
		if (w != 0xffff)
		{
			*ptr = (byte *)ptrbase + w * elemSize;
		}
		else
		{
			*ptr = NULL;
		}
	}
	return *this;
}

FArchive &FArchive::SerializeObject (DObject *&object, TypeInfo *type)
{
	if (IsStoring ())
		return WriteObject (object);
	else
		return ReadObject (object, type);
}

#define NEW_OBJ				((BYTE)1)
#define NEW_CLS_OBJ			((BYTE)2)
#define OLD_OBJ				((BYTE)3)
#define NULL_OBJ			((BYTE)4)
#define NEW_PLYR_OBJ		((BYTE)5)
#define NEW_PLYR_CLS_OBJ	((BYTE)6)

FArchive &FArchive::WriteObject (DObject *obj)
{
	player_t *player;
	BYTE id[2];

	if (obj == NULL)
	{
		id[0] = NULL_OBJ;
		Write (id, 1);
	}
	else
	{
		const TypeInfo *type = RUNTIME_TYPE(obj);

		if (type == RUNTIME_CLASS(DObject))
		{
			//I_Error ("Tried to save an instance of DObject.\n"
			//		 "This should not happen.\n");
			id[0] = NULL_OBJ;
			Write (id, 1);
		}
		else if (m_TypeMap[type->TypeIndex].toArchive == ~0)
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
			MapObject (obj);
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

			if (index == ~0)
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
				WriteCount (m_TypeMap[type->TypeIndex].toArchive);
				MapObject (obj);
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

FArchive &FArchive::ReadObject (DObject* &obj, TypeInfo *wanttype)
{
	BYTE objHead;
	const TypeInfo *type;
	BYTE playerNum;
	DWORD index;

	operator<< (objHead);

	switch (objHead)
	{
	case NULL_OBJ:
		obj = NULL;
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
			obj = players[playerNum].mo;
			MapObject (obj);

			// But also create a new one so that we can get past the one
			// stored in the archive.
			DObject *tempobj = type->CreateNew ();
			tempobj->Serialize (*this);
			tempobj->CheckIfSerialized ();
			tempobj->Destroy ();
			break;
		}
		/* fallthrough */
	case NEW_CLS_OBJ:
		type = ReadClass (wanttype);
		obj = type->CreateNew ();
		MapObject (obj);
		obj->Serialize (*this);
		obj->CheckIfSerialized ();
		break;

	case NEW_PLYR_OBJ:
		operator<< (playerNum);
		if (m_HubTravel)
		{
			type = ReadStoredClass (wanttype);
			obj = players[playerNum].mo;
			MapObject (obj);

			DObject *tempobj = type->CreateNew ();
			tempobj->Serialize (*this);
			tempobj->CheckIfSerialized ();
			tempobj->Destroy ();
			break;
		}
		/* fallthrough */
	case NEW_OBJ:
		type = ReadStoredClass (wanttype);
		obj = type->CreateNew ();
		MapObject (obj);
		obj->Serialize (*this);
		obj->CheckIfSerialized ();
		break;

	default:
		I_Error ("Unknown object code (%d) in archive\n", objHead);
	}
	return *this;
}

DWORD FArchive::WriteClass (const TypeInfo *info)
{
	if (m_ClassCount >= TypeInfo::m_NumTypes)
	{
		I_Error ("Too many unique classes have been written.\nOnly %u were registered\n",
			TypeInfo::m_NumTypes);
	}
	if (m_TypeMap[info->TypeIndex].toArchive != ~0)
	{
		I_Error ("Attempt to write '%s' twice.\n", info->Name);
	}
	m_TypeMap[info->TypeIndex].toArchive = m_ClassCount;
	m_TypeMap[m_ClassCount].toCurrent = info;
	WriteString (info->Name);
	return m_ClassCount++;
}

const TypeInfo *FArchive::ReadClass ()
{
	struct String {
		String() { val = NULL; }
		~String() { if (val) delete[] val; }
		char *val;
	} typeName;
	int i;

	if (m_ClassCount >= TypeInfo::m_NumTypes)
	{
		I_Error ("Too many unique classes have been read.\nOnly %u were registered\n",
			TypeInfo::m_NumTypes);
	}
	operator<< (typeName.val);
	for (i = 0; i < TypeInfo::m_NumTypes; i++)
	{
		if (!strcmp (TypeInfo::m_Types[i]->Name, typeName.val))
		{
			m_TypeMap[i].toArchive = m_ClassCount;
			m_TypeMap[m_ClassCount].toCurrent = TypeInfo::m_Types[i];
			m_ClassCount++;
			return TypeInfo::m_Types[i];
		}
	}
	I_Error ("Unknown class '%s'\n", typeName);
	return NULL;
}

const TypeInfo *FArchive::ReadClass (const TypeInfo *wanttype)
{
	const TypeInfo *type = ReadClass ();
	if (!type->IsDescendantOf (wanttype))
	{
		I_Error ("Expected to extract an object of type '%s'.\n"
				 "Found one of type '%s' instead.\n",
			wanttype->Name, type->Name);
	}
	return type;
}

const TypeInfo *FArchive::ReadStoredClass (const TypeInfo *wanttype)
{
	DWORD index = ReadCount ();
	if (index >= m_ClassCount)
	{
		I_Error ("Class reference too high (%u; max is %u)\n", index, m_ClassCount);
	}
	const TypeInfo *type = m_TypeMap[index].toCurrent;
	if (!type->IsDescendantOf (wanttype))
	{
		I_Error ("Expected to extract an object of type '%s'.\n"
				 "Found one of type '%s' instead.\n",
			wanttype->Name, type->Name);
	}
	return type;
}

DWORD FArchive::MapObject (const DObject *obj)
{
	DWORD i;

	if (m_ObjectCount >= m_MaxObjectCount)
	{
		m_MaxObjectCount = m_MaxObjectCount ? m_MaxObjectCount * 2 : 1024;
		m_ObjectMap = (ObjectMap *)Realloc (m_ObjectMap, sizeof(ObjectMap)*m_MaxObjectCount);
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
	return (DWORD)((ptrdiff_t)obj % EObjectHashSize);
}

DWORD FArchive::FindObjectIndex (const DObject *obj) const
{
	size_t index = m_ObjectHash[HashObject (obj)];
	while (index != ~0 && m_ObjectMap[index].object != obj)
	{
		index = m_ObjectMap[index].hashNext;
	}
	return index;
}

void FArchive::UserWriteClass (const TypeInfo *type)
{
	BYTE id;

	if (type == NULL)
	{
		id = 2;
		Write (&id, 1);
	}
	else
	{
		if (m_TypeMap[type->TypeIndex].toArchive == ~0)
		{
			id = 1;
			Write (&id, 1);
			WriteClass (type);
		}
		else
		{
			id = 0;
			Write (&id, 1);
			WriteCount (m_TypeMap[type->TypeIndex].toArchive);
		}
	}
}

void FArchive::UserReadClass (const TypeInfo *&type)
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
	}
}