#ifndef __W_ZIP
#define __W_ZIP

#pragma pack(1)
// FZipCentralInfo
struct FZipEndOfCentralDirectory
{
	DWORD	Magic;
	WORD	DiskNumber;
	WORD	FirstDisk;
	WORD	NumEntries;
	WORD	NumEntriesOnAllDisks;
	DWORD	DirectorySize;
	DWORD	DirectoryOffset;
	WORD	ZipCommentLength;
};

// FZipFileInfo
struct FZipCentralDirectoryInfo
{
	DWORD	Magic;
	BYTE	VersionMadeBy[2];
	BYTE	VersionToExtract[2];
	WORD	Flags;
	WORD	Method;
	WORD	ModTime;
	WORD	ModDate;
	DWORD	CRC32;
	DWORD	CompressedSize;
	DWORD	UncompressedSize;
	WORD	NameLength;
	WORD	ExtraLength;
	WORD	CommentLength;
	WORD	StartingDiskNumber;
	WORD	InternalAttributes;
	DWORD	ExternalAttributes;
	DWORD	LocalHeaderOffset;
	// file name and other variable length info follows
};

// FZipLocalHeader
struct FZipLocalFileHeader
{
	DWORD	Magic;
	BYTE	VersionToExtract[2];
	WORD	Flags;
	WORD	Method;
	WORD	ModTime;
	WORD	ModDate;
	DWORD	CRC32;
	DWORD	CompressedSize;
	DWORD	UncompressedSize;
	WORD	NameLength;
	WORD	ExtraLength;
	// file name and other variable length info follows
};


#pragma pack()

#define ZIP_LOCALFILE	MAKE_ID('P','K',3,4)
#define ZIP_CENTRALFILE	MAKE_ID('P','K',1,2)
#define ZIP_ENDOFDIR	MAKE_ID('P','K',5,6)

#define METHOD_STORED	0
#define METHOD_SHRINK	1
#define METHOD_IMPLODE	6
#define METHOD_DEFLATE	8
#define METHOD_BZIP2	12
#define METHOD_LZMA		14
#define METHOD_PPMD		98

// File header flags.
#define ZF_ENCRYPTED			0x1

#endif
