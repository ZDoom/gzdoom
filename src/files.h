#ifndef FILES_H
#define FILES_H

#include <stdio.h>
#include <zlib.h>
#include "bzlib.h"
#include "doomtype.h"
#include "m_swap.h"

class FileReaderBase
{
public:
	virtual ~FileReaderBase() {}
	virtual long Read (void *buffer, long len) = 0;

	FileReaderBase &operator>> (BYTE &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderBase &operator>> (SBYTE &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderBase &operator>> (WORD &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderBase &operator>> (SWORD &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderBase &operator>> (DWORD &v)
	{
		Read (&v, 4);
		v = LittleLong(v);
		return *this;
	}

	FileReaderBase &operator>> (int &v)
	{
		Read (&v, 4);
		v = LittleLong(v);
		return *this;
	}

};


class FileReader : public FileReaderBase
{
public:
	FileReader ();
	FileReader (const char *filename);
	FileReader (FILE *file);
	FileReader (FILE *file, long length);
	bool Open (const char *filename);
	virtual ~FileReader ();

	virtual long Tell () const;
	virtual long Seek (long offset, int origin);
	virtual long Read (void *buffer, long len);
	virtual char *Gets(char *strbuf, int len);
	long GetLength () const { return Length; }

	// If you use the underlying FILE without going through this class,
	// you must call ResetFilePtr() before using this class again.
	void ResetFilePtr ();

	FILE *GetFile () const { return File; }
	virtual const char *GetBuffer() const { return NULL; }

	FileReader &operator>> (BYTE &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReader &operator>> (SBYTE &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReader &operator>> (WORD &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReader &operator>> (SWORD &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReader &operator>> (DWORD &v)
	{
		Read (&v, 4);
		v = LittleLong(v);
		return *this;
	}


protected:
	FileReader (const FileReader &other, long length);

	char *GetsFromBuffer(const char * bufptr, char *strbuf, int len);

	FILE *File;
	long Length;
	long StartPos;
	long FilePos;

private:
	long CalcFileLen () const;
protected:
	bool CloseOnDestruct;
};

// Wraps around a FileReader to decompress a zlib stream
class FileReaderZ : public FileReaderBase
{
public:
	FileReaderZ (FileReader &file, bool zip=false);
	~FileReaderZ ();

	virtual long Read (void *buffer, long len);

	FileReaderZ &operator>> (BYTE &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderZ &operator>> (SBYTE &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderZ &operator>> (WORD &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderZ &operator>> (SWORD &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderZ &operator>> (DWORD &v)
	{
		Read (&v, 4);
		v = LittleLong(v);
		return *this;
	}

	FileReaderZ &operator>> (int &v)
	{
		Read (&v, 4);
		v = LittleLong(v);
		return *this;
	}

private:
	enum { BUFF_SIZE = 4096 };

	FileReader &File;
	bool SawEOF;
	z_stream Stream;
	BYTE InBuff[BUFF_SIZE];

	void FillBuffer ();

	FileReaderZ &operator= (const FileReaderZ &) { return *this; }
};

// Wraps around a FileReader to decompress a bzip2 stream
class FileReaderBZ2 : public FileReaderBase
{
public:
	FileReaderBZ2 (FileReader &file);
	~FileReaderBZ2 ();

	long Read (void *buffer, long len);

	FileReaderBZ2 &operator>> (BYTE &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderBZ2 &operator>> (SBYTE &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderBZ2 &operator>> (WORD &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderBZ2 &operator>> (SWORD &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderBZ2 &operator>> (DWORD &v)
	{
		Read (&v, 4);
		v = LittleLong(v);
		return *this;
	}

	FileReaderBZ2 &operator>> (int &v)
	{
		Read (&v, 4);
		v = LittleLong(v);
		return *this;
	}

private:
	enum { BUFF_SIZE = 4096 };

	FileReader &File;
	bool SawEOF;
	bz_stream Stream;
	BYTE InBuff[BUFF_SIZE];

	void FillBuffer ();

	FileReaderBZ2 &operator= (const FileReaderBZ2 &) { return *this; }
};

// Wraps around a FileReader to decompress a lzma stream
class FileReaderLZMA : public FileReaderBase
{
	struct StreamPointer;

public:
	FileReaderLZMA (FileReader &file, size_t uncompressed_size, bool zip);
	~FileReaderLZMA ();

	long Read (void *buffer, long len);

	FileReaderLZMA &operator>> (BYTE &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderLZMA &operator>> (SBYTE &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderLZMA &operator>> (WORD &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderLZMA &operator>> (SWORD &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderLZMA &operator>> (DWORD &v)
	{
		Read (&v, 4);
		v = LittleLong(v);
		return *this;
	}

	FileReaderLZMA &operator>> (int &v)
	{
		Read (&v, 4);
		v = LittleLong(v);
		return *this;
	}

private:
	enum { BUFF_SIZE = 4096 };

	FileReader &File;
	bool SawEOF;
	StreamPointer *Streamp;	// anonymous pointer to LKZA decoder struct - to avoid including the LZMA headers globally
	size_t Size;
	size_t InPos, InSize;
	size_t OutProcessed;
	BYTE InBuff[BUFF_SIZE];

	void FillBuffer ();

	FileReaderLZMA &operator= (const FileReaderLZMA &) { return *this; }
};

class MemoryReader : public FileReader
{
public:
	MemoryReader (const char *buffer, long length);
	~MemoryReader ();

	virtual long Tell () const;
	virtual long Seek (long offset, int origin);
	virtual long Read (void *buffer, long len);
	virtual char *Gets(char *strbuf, int len);
	virtual const char *GetBuffer() const { return bufptr; }

protected:
	const char * bufptr;
};

class MemoryArrayReader : public FileReader
{
public:
    MemoryArrayReader (const char *buffer, long length);
    ~MemoryArrayReader ();

    virtual long Tell () const;
    virtual long Seek (long offset, int origin);
    virtual long Read (void *buffer, long len);
    virtual char *Gets(char *strbuf, int len);
    virtual const char *GetBuffer() const { return (char*)&buf[0]; }
    TArray<BYTE> &GetArray() { return buf; }

    void UpdateLength() { Length = buf.Size(); }

protected:
    TArray<BYTE> buf;
};


#endif
