#include <zlib.h>
#include "doomtype.h"

// zlib includes some CRC32 stuff, so just use that

inline const DWORD *GetCRCTable () { return (const DWORD *)get_crc_table(); }
inline DWORD CalcCRC32 (const BYTE *buf, unsigned int len)
{
	return crc32 (0, buf, len);
}
inline DWORD CRC1 (DWORD crc, const BYTE c, const DWORD *crcTable)
{
	return crcTable[(crc & 0xff) ^ c] ^ (crc >> 8);
}
