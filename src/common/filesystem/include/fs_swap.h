//
// DESCRIPTION:
//		Endianess handling, swapping 16bit and 32bit.
//
//-----------------------------------------------------------------------------


#ifndef __FS_SWAP_H__
#define __FS_SWAP_H__

#include <stdlib.h>

// Endianess handling.
// WAD files are stored little endian.

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#endif

namespace FileSys {
namespace byteswap {
	
#ifdef __APPLE__

inline unsigned short LittleShort(unsigned short x)
{
	return OSSwapLittleToHostInt16(x);
}

inline unsigned int LittleLong(unsigned int x)
{
	return OSSwapLittleToHostInt32(x);
}

inline unsigned short BigShort(unsigned short x)
{
	return OSSwapBigToHostInt16(x);
}

inline unsigned int BigLong(unsigned int x)
{
	return OSSwapBigToHostInt32(x);
}


#elif defined __BIG_ENDIAN__

// Swap 16bit, that is, MSB and LSB byte.
// No masking with 0xFF should be necessary. 
inline unsigned short LittleShort (unsigned short x)
{
	return (unsigned short)((x>>8) | (x<<8));
}

inline unsigned int LittleLong (unsigned int x)
{
	return (unsigned int)(
		(x>>24)
		| ((x>>8) & 0xff00)
		| ((x<<8) & 0xff0000)
		| (x<<24));
}

inline unsigned short BigShort(unsigned short x)
{
	return x;
}

inline unsigned int BigLong(unsigned int x)
{
	return x;
}

#else

inline unsigned short LittleShort(unsigned short x)
{
	return x;
}

inline unsigned int LittleLong(unsigned int x)
{
	return x;
}

#ifdef _MSC_VER

inline unsigned short BigShort(unsigned short x)
{
	return _byteswap_ushort(x);
}

inline unsigned int BigLong(unsigned int x)
{
	return (unsigned int)_byteswap_ulong((unsigned long)x);
}

#pragma warning (default: 4035)

#else

inline unsigned short BigShort (unsigned short x)
{
	return (unsigned short)((x>>8) | (x<<8));
}

inline unsigned int BigLong (unsigned int x)
{
	return (unsigned int)(
		(x>>24)
		| ((x>>8) & 0xff00)
		| ((x<<8) & 0xff0000)
		| (x<<24));
}

#endif

#endif // __BIG_ENDIAN__
}
}

#endif // __M_SWAP_H__
