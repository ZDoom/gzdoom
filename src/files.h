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

	FileReaderBase &operator>> (uint8_t &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderBase &operator>> (int8_t &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderBase &operator>> (uint16_t &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderBase &operator>> (int16_t &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderBase &operator>> (uint32_t &v)
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

	FileReader &operator>> (uint8_t &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReader &operator>> (int8_t &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReader &operator>> (uint16_t &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReader &operator>> (int16_t &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReader &operator>> (uint32_t &v)
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

	FileReaderZ &operator>> (uint8_t &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderZ &operator>> (int8_t &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderZ &operator>> (uint16_t &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderZ &operator>> (int16_t &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderZ &operator>> (uint32_t &v)
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
	uint8_t InBuff[BUFF_SIZE];

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

	FileReaderBZ2 &operator>> (uint8_t &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderBZ2 &operator>> (int8_t &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderBZ2 &operator>> (uint16_t &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderBZ2 &operator>> (int16_t &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderBZ2 &operator>> (uint32_t &v)
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
	uint8_t InBuff[BUFF_SIZE];

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

	FileReaderLZMA &operator>> (uint8_t &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderLZMA &operator>> (int8_t &v)
	{
		Read (&v, 1);
		return *this;
	}

	FileReaderLZMA &operator>> (uint16_t &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderLZMA &operator>> (int16_t &v)
	{
		Read (&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileReaderLZMA &operator>> (uint32_t &v)
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
	uint8_t InBuff[BUFF_SIZE];

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
    TArray<uint8_t> &GetArray() { return buf; }

    void UpdateLength() { Length = buf.Size(); }

protected:
    TArray<uint8_t> buf;
};


class FileWriter
{
protected:
	bool OpenDirect(const char *filename);

	FileWriter()
	{
		File = NULL;
	}
public:
	virtual ~FileWriter()
	{
		if (File != NULL) fclose(File);
	}

	static FileWriter *Open(const char *filename);

	virtual size_t Write(const void *buffer, size_t len);
	size_t Printf(const char *fmt, ...) GCCPRINTF(2,3);

protected:

	FILE *File;

protected:
	bool CloseOnDestruct;
};

class BufferWriter : public FileWriter
{
protected:
	TArray<unsigned char> mBuffer;
public:

	BufferWriter() {}
	virtual size_t Write(const void *buffer, size_t len) override;
	TArray<unsigned char> *GetBuffer() { return &mBuffer; }
};




#endif
