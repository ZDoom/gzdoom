#include "files.h"
#include "i_system.h"
#include "templates.h"

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

long FileReader::CalcFileLen() const
{
	long endpos;

	fseek (File, 0, SEEK_END);
	endpos = ftell (File);
	fseek (File, 0, SEEK_SET);
	return endpos;
}

// Now for the zlib wrapper -------------------------------------------------

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
