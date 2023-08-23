#ifndef __W_ZIP
#define __W_ZIP

#if defined(__GNUC__)
#define FORCE_PACKED __attribute__((__packed__))
#else
#define FORCE_PACKED
#endif

namespace FileSys {
	
#pragma pack(1)
// FZipCentralInfo
struct FZipEndOfCentralDirectory
{
	uint32_t	Magic;
	uint16_t	DiskNumber;
	uint16_t	FirstDisk;
	uint16_t	NumEntries;
	uint16_t	NumEntriesOnAllDisks;
	uint32_t	DirectorySize;
	uint32_t	DirectoryOffset;
	uint16_t	ZipCommentLength;
} FORCE_PACKED;

struct FZipEndOfCentralDirectory64
{
	uint32_t	Magic;
	uint64_t	StructSize;
	uint16_t	VersionMadeBy;
	uint16_t	VersionNeeded;
	uint32_t	DiskNumber;
	uint32_t	FirstDisk;
	uint64_t	NumEntries;
	uint64_t	NumEntriesOnAllDisks;
	uint64_t	DirectorySize;
	uint64_t	DirectoryOffset;
	uint16_t	ZipCommentLength;
} FORCE_PACKED;


// FZipFileInfo
struct FZipCentralDirectoryInfo
{
	uint32_t	Magic;
	uint8_t	VersionMadeBy[2];
	uint8_t	VersionToExtract[2];
	uint16_t	Flags;
	uint16_t	Method;
	uint16_t	ModTime;
	uint16_t	ModDate;
	uint32_t	CRC32;
	uint32_t	CompressedSize32;
	uint32_t	UncompressedSize32;
	uint16_t	NameLength;
	uint16_t	ExtraLength;
	uint16_t	CommentLength;
	uint16_t	StartingDiskNumber;
	uint16_t	InternalAttributes;
	uint32_t	ExternalAttributes;
	uint32_t	LocalHeaderOffset32;
	// file name and other variable length info follows
} FORCE_PACKED;

struct FZipCentralDirectoryInfo64BitExt
{
	uint16_t Type;
	uint16_t Length;
	uint64_t UncompressedSize;
	uint64_t CompressedSize;
	uint64_t LocalHeaderOffset;
	uint32_t DiskNo;
} FORCE_PACKED;

// FZipLocalHeader
struct FZipLocalFileHeader
{
	uint32_t	Magic;
	uint8_t	VersionToExtract[2];
	uint16_t	Flags;
	uint16_t	Method;
	uint16_t	ModTime;
	uint16_t	ModDate;
	uint32_t	CRC32;
	uint32_t	CompressedSize;
	uint32_t	UncompressedSize;
	uint16_t	NameLength;
	uint16_t	ExtraLength;
	// file name and other variable length info follows
} FORCE_PACKED;

#pragma pack()

#ifndef MAKE_ID
#ifndef __BIG_ENDIAN__
#define MAKE_ID(a,b,c,d)	((uint32_t)((a)|((b)<<8)|((c)<<16)|((d)<<24)))
#else
#define MAKE_ID(a,b,c,d)	((uint32_t)((d)|((c)<<8)|((b)<<16)|((a)<<24)))
#endif
#endif


#define ZIP_LOCALFILE	MAKE_ID('P','K',3,4)
#define ZIP_CENTRALFILE	MAKE_ID('P','K',1,2)
#define ZIP_ENDOFDIR	MAKE_ID('P','K',5,6)

// File header flags.
#define ZF_ENCRYPTED			0x1

}
#endif
