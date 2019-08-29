//
// DESCRIPTION:
//		Endianess handling, swapping 16bit and 32bit.
//
//-----------------------------------------------------------------------------


#ifndef __M_SWAP_H__
#define __M_SWAP_H__

#include <stdlib.h>

// Endianess handling.
// WAD files are stored little endian.

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>

inline short LittleShort(short x)
{
	return (short)OSSwapLittleToHostInt16((uint16_t)x);
}

inline unsigned short LittleShort(unsigned short x)
{
	return OSSwapLittleToHostInt16(x);
}

inline short LittleShort(int x)
{
	return OSSwapLittleToHostInt16((uint16_t)x);
}

inline unsigned short LittleShort(unsigned int x)
{
	return OSSwapLittleToHostInt16((uint16_t)x);
}

inline int LittleLong(int x)
{
	return OSSwapLittleToHostInt32((uint32_t)x);
}

inline unsigned int LittleLong(unsigned int x)
{
	return OSSwapLittleToHostInt32(x);
}

inline short BigShort(short x)
{
	return (short)OSSwapBigToHostInt16((uint16_t)x);
}

inline unsigned short BigShort(unsigned short x)
{
	return OSSwapBigToHostInt16(x);
}

inline int BigLong(int x)
{
	return OSSwapBigToHostInt32((uint32_t)x);
}

inline unsigned int BigLong(unsigned int x)
{
	return OSSwapBigToHostInt32(x);
}

#elif defined __BIG_ENDIAN__

// Swap 16bit, that is, MSB and LSB byte.
// No masking with 0xFF should be necessary. 
inline short LittleShort (short x)
{
	return (short)((((unsigned short)x)>>8) | (((unsigned short)x)<<8));
}

inline unsigned short LittleShort (unsigned short x)
{
	return (unsigned short)((x>>8) | (x<<8));
}

inline short LittleShort (int x)
{
	return LittleShort((short)x);
}

inline unsigned short LittleShort (unsigned int x)
{
	return LittleShort((unsigned short)x);
}

// Swapping 32bit.
inline unsigned int LittleLong (unsigned int x)
{
	return (unsigned int)(
		(x>>24)
		| ((x>>8) & 0xff00)
		| ((x<<8) & 0xff0000)
		| (x<<24));
}

inline int LittleLong (int x)
{
	return (int)(
		(((unsigned int)x)>>24)
		| ((((unsigned int)x)>>8) & 0xff00)
		| ((((unsigned int)x)<<8) & 0xff0000)
		| (((unsigned int)x)<<24));
}

inline short BigShort(short x)
{
	return x;
}

inline unsigned short BigShort(unsigned short x)
{
	return x;
}

inline unsigned int BigLong(unsigned int x)
{
	return x;
}

inline int BigLong(int x)
{
	return x;
}

#else

inline short LittleShort(short x)
{
	return x;
}

inline unsigned short LittleShort(unsigned short x)
{
	return x;
}

inline unsigned int LittleLong(unsigned int x)
{
	return x;
}

inline int LittleLong(int x)
{
	return x;
}

#ifdef _MSC_VER

inline short BigShort(short x)
{
	return (short)_byteswap_ushort((unsigned short)x);
}

inline unsigned short BigShort(unsigned short x)
{
	return _byteswap_ushort(x);
}

inline int BigLong(int x)
{
	return (int)_byteswap_ulong((unsigned long)x);
}

inline unsigned int BigLong(unsigned int x)
{
	return (unsigned int)_byteswap_ulong((unsigned long)x);
}
#pragma warning (default: 4035)

#else

inline short BigShort (short x)
{
	return (short)((((unsigned short)x)>>8) | (((unsigned short)x)<<8));
}

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

inline int BigLong (int x)
{
	return (int)(
		(((unsigned int)x)>>24)
		| ((((unsigned int)x)>>8) & 0xff00)
		| ((((unsigned int)x)<<8) & 0xff0000)
		| (((unsigned int)x)<<24));
}

#endif

#endif // __BIG_ENDIAN__

// These may be destructive so they should create errors
unsigned long BigLong(unsigned long) = delete;
long BigLong(long) = delete;
unsigned long LittleLong(unsigned long) = delete;
long LittleLong(long) = delete;


// Data accessors, since some data is highly likely to be unaligned.
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
inline int GetShort(const unsigned char *foo)
{
	return *(const short *)foo;
}
inline int GetInt(const unsigned char *foo)
{
	return *(const int *)foo;
}
#else
inline int GetShort(const unsigned char *foo)
{
	return short(foo[0] | (foo[1] << 8));
}
inline int GetInt(const unsigned char *foo)
{
	return int(foo[0] | (foo[1] << 8) | (foo[2] << 16) | (foo[3] << 24));
}
#endif
inline int GetBigInt(const unsigned char *foo)
{
	return int((foo[0] << 24) | (foo[1] << 16) | (foo[2] << 8) | foo[3]);
}

#ifdef __BIG_ENDIAN__
inline int GetNativeInt(const unsigned char *foo)
{
	return GetBigInt(foo);
}
#else
inline int GetNativeInt(const unsigned char *foo)
{
	return GetInt(foo);
}
#endif

#endif // __M_SWAP_H__
