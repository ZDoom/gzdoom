/*
** files.cpp
** Implements classes for reading from files or memory blocks
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** Copyright 2005-2008 Christoph Oelckers
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

#include "files.h"
#include "i_system.h"
#include "templates.h"

//==========================================================================
//
// FileReader
//
// reads data from an uncompressed file or part of it
//
//==========================================================================

FileReader::FileReader ()
: File(NULL), Length(0), CloseOnDestruct(false)
{
}

FileReader::FileReader (const FileReader &other, long length)
: File(other.File), Length(length), CloseOnDestruct(false)
{
	FilePos = StartPos = ftell (other.File);
}

FileReader::FileReader (const char *filename)
: File(NULL), Length(0), StartPos(0), FilePos(0), CloseOnDestruct(false)
{
	File = fopen (filename, "rb");
	if (File == NULL)
	{
		I_Error ("Could not open %s", filename);
	}
	CloseOnDestruct = true;
	Length = CalcFileLen();
}

FileReader::FileReader (FILE *file)
: File(file), Length(0), StartPos(0), FilePos(0), CloseOnDestruct(false)
{
	Length = CalcFileLen();
}

FileReader::FileReader (FILE *file, long length)
: File(file), Length(length), CloseOnDestruct(true)
{
	FilePos = StartPos = ftell (file);
}

FileReader::~FileReader ()
{
	if (CloseOnDestruct && File != NULL)
	{
		fclose (File);
		File = NULL;
	}
}

void FileReader::ResetFilePtr ()
{
	FilePos = ftell (File);
}

long FileReader::Tell () const
{
	return FilePos - StartPos;
}

long FileReader::Seek (long offset, int origin)
{
	if (origin == SEEK_SET)
	{
		offset += StartPos;
	}
	else if (origin == SEEK_CUR)
	{
		offset += FilePos;
	}
	else if (origin == SEEK_END)
	{
		offset += StartPos + Length;
	}
	if (0 == fseek (File, offset, SEEK_SET))
	{
		FilePos = offset;
		return 0;
	}
	return -1;
}

long FileReader::Read (void *buffer, long len)
{
	if (FilePos + len > StartPos + Length)
	{
		len = Length - FilePos + StartPos;
	}
	len = (long)fread (buffer, 1, len, File);
	FilePos += len;
	return len;
}

char *FileReader::Gets(char *strbuf, int len)
{
	if (FilePos + len > StartPos + Length)
	{
		len = Length - FilePos + StartPos;
	}
	if (len <= 0) return 0;
	char *p = fgets(strbuf, len, File);
	FilePos += len;
	return p;
}

char *FileReader::GetsFromBuffer(const char * bufptr, char *strbuf, int len)
{
	if (len>Length-FilePos) len=Length-FilePos;
	if (len <= 0) return NULL;

	char *p = strbuf;
	while (len > 1 && bufptr[FilePos] != 0)
	{
		if (bufptr[FilePos] != '\r')
		{
			*p++ = bufptr[FilePos];
			len--;
			if (bufptr[FilePos] == '\n') break;
		}
		FilePos++;
	}
	*p++=0;
	return strbuf;
}

long FileReader::CalcFileLen() const
{
	long endpos;

	fseek (File, 0, SEEK_END);
	endpos = ftell (File);
	fseek (File, 0, SEEK_SET);
	return endpos;
}

//==========================================================================
//
// FileReaderZ
//
// The zlib wrapper
// reads data from a ZLib compressed stream
//
//==========================================================================

FileReaderZ::FileReaderZ (FileReader &file, bool zip)
: File(file), SawEOF(false)
{
	int err;

	FillBuffer ();

	Stream.zalloc = Z_NULL;
	Stream.zfree = Z_NULL;

	if (!zip) err = inflateInit (&Stream);
	else err = inflateInit2 (&Stream, -MAX_WBITS);

	if (err != Z_OK)
	{
		I_Error ("FileReaderZ: inflateInit failed: %d\n", err);
	}
}

FileReaderZ::~FileReaderZ ()
{
	inflateEnd (&Stream);
}

long FileReaderZ::Read (void *buffer, long len)
{
	int err;

	Stream.next_out = (Bytef *)buffer;
	Stream.avail_out = len;

	do
	{
		err = inflate (&Stream, Z_SYNC_FLUSH);
		if (Stream.avail_in == 0 && !SawEOF)
		{
			FillBuffer ();
		}
	} while (err == Z_OK && Stream.avail_out != 0);

	if (err != Z_OK && err != Z_STREAM_END)
	{
		I_Error ("Corrupt zlib stream");
	}

	if (Stream.avail_out != 0)
	{
		I_Error ("Ran out of data in zlib stream");
	}

	return len - Stream.avail_out;
}

void FileReaderZ::FillBuffer ()
{
	long numread = File.Read (InBuff, BUFF_SIZE);

	if (numread < BUFF_SIZE)
	{
		SawEOF = true;
	}
	Stream.next_in = InBuff;
	Stream.avail_in = numread;
}

//==========================================================================
//
// MemoryReader
//
// reads data from a block of memory
//
//==========================================================================

MemoryReader::MemoryReader (const char *buffer, long length)
{
	bufptr=buffer;
	Length=length;
	FilePos=0;
}

MemoryReader::~MemoryReader ()
{
}

long MemoryReader::Tell () const
{
	return FilePos;
}

long MemoryReader::Seek (long offset, int origin)
{
	switch (origin)
	{
	case SEEK_CUR:
		offset+=FilePos;
		break;

	case SEEK_END:
		offset+=Length;
		break;

	}
	FilePos=clamp<long>(offset,0,Length-1);
	return 0;
}

long MemoryReader::Read (void *buffer, long len)
{
	if (len>Length-FilePos) len=Length-FilePos;
	if (len<0) len=0;
	memcpy(buffer,bufptr+FilePos,len);
	FilePos+=len;
	return len;
}

char *MemoryReader::Gets(char *strbuf, int len)
{
	return GetsFromBuffer(bufptr, strbuf, len);
}

