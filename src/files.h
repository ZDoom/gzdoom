#ifndef FILES_H
#define FILES_H

#include <stdio.h>
#include <zlib.h>
#include "doomtype.h"
#include "m_swap.h"

class FileReader
{
public:
	FileReader (const char *filename);
	FileReader (FILE *file);
	FileReader (FILE *file, long length);
	~FileReader ();

	long Tell () const;
	long Seek (long offset, int origin);
	long Read (void *buffer, long len);
	long GetLength () const { return Length; }

	// If you use the underlying FILE without going through this class,
	// you must call ResetFilePtr() before using this class again.
	void ResetFilePtr ();

	FILE *GetFile () const { return File; }

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
		v = SHORT(v);
		return *this;
	}

	FileReader &operator>> (SWORD &v)
	{
		Read (&v, 2);
		v = SHORT(v);
		return *this;
	}

	FileReader &operator>> (DWORD &v)
	{
		Read (&v, 4);
		v = LONG(v);
		return *this;
	}

protected:
	FileReader (const FileReader &other, long length);
	FileReader ();

	FILE *File;
	long Length;
	long StartPos;
	long FilePos;

private:
	long CalcFileLen () const;
	bool CloseOnDestruct;
};

// Wraps around a FileReader to decompress a zlib stream
class FileReaderZ
{
public:
	FileReaderZ (FileReader &file);
	~FileReaderZ ();

	long Read (void *buffer, long len);

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
		v = SHORT(v);
		return *this;
	}

	FileReaderZ &operator>> (SWORD &v)
	{
		Read (&v, 2);
		v = SHORT(v);
		return *this;
	}

	FileReaderZ &operator>> (DWORD &v)
	{
		Read (&v, 4);
		v = LONG(v);
		return *this;
	}

	FileReaderZ &operator>> (fixed_t &v)
	{
		Read (&v, 4);
		v = LONG(v);
		return *this;
	}

private:
	enum { BUFF_SIZE = 4096 };

	FileReader &File;
	bool SawEOF;
	z_stream Stream;
	BYTE InBuff[BUFF_SIZE];

	void FillBuffer ();
};

#endif
