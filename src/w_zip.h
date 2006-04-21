#ifndef __W_ZIP
#define __W_ZIP

#pragma pack(1)
struct FZipCentralInfo
{
	// patched together from information from Q3's unzip.c
	DWORD dwSignature;
	WORD  wNumberDisk;
	WORD  wNumberDiskWithCD;
	WORD  wEntryCount;
	WORD  wTotalEntryCount;
	DWORD dwCDSize;
	DWORD dwCDOffset;
	WORD  wCommentSize;
};

struct FZipFileInfo
{
	// patched together from information from Q3's unzip.c
	DWORD dwSignature;
	WORD  wVersion;
	WORD  wRequiredVersion;
	WORD  wFlags;
	WORD  wCompression;
	DWORD dwLastModified;
	DWORD dwCrc;
	DWORD dwCompressedSize;
	DWORD dwSize;
	WORD  wFileNameSize;
	WORD  wExtraSize;
	WORD  wCommentSize;
	WORD  wDiskStart;
	WORD  wInternalAttributes;
	DWORD wExternalAttributes;
	DWORD dwFileOffset;
	// file name and other variable length info follows
};

struct FZipLocalHeader
{
	// patched together from information from Q3's unzip.c
	DWORD dwSignature;
	WORD  wRequiredVersion;
	WORD  wFlags;
	WORD  wCompression;
	DWORD dwLastModified;
	DWORD dwCrc;
	DWORD dwCompressedSize;
	DWORD dwSize;
	WORD  wFileNameSize;
	WORD  wExtraSize;
	// file name and other variable length info follows
};


#pragma pack()

#define Z_DEFLATED   8

// File header flags.
#define ZF_ENCRYPTED			0x1

#endif
