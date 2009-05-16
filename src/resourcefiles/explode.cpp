/*
    gunzip.c by Pasi Ojala,	a1bert@iki.fi
				http://www.iki.fi/a1bert/

    A hopefully easier to understand guide to GZip
    (deflate) decompression routine than the GZip
    source code.

 */

/*----------------------------------------------------------------------*/


#include <stdlib.h>
#include "explode.h"

static const unsigned char BitReverse[] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/****************************************************************
    Bit-I/O variables and routines/macros

    These routines work in the bit level because the target
    environment does not have a barrel shifter. Trying to
    handle several bits at once would've only made the code
    slower.

    If the environment supports multi-bit shifts, you should
    write these routines again (see e.g. the GZIP sources).

	[RH] Since the target environment is not a C64, I did as
	suggested and rewrote these using zlib as a reference.

 ****************************************************************/

#define READBYTE(c) \
	do { \
		c = 0; \
		if (InLeft) { \
			InLeft--; \
			In->Read(&c, 1); \
		} \
	} while (0)

/* Get a byte of input into the bit accumulator. */
#define PULLBYTE() \
    do { \
        unsigned char next; \
		READBYTE(next); \
        Hold += (unsigned int)(next) << Bits; \
        Bits += 8; \
    } while (0)

/* Assure that there are at least n bits in the bit accumulator. */
#define NEEDBITS(n) \
    do { \
        while (Bits < (unsigned)(n)) \
            PULLBYTE(); \
    } while (0)

/* Return the low n bits of the bit accumulator (n < 16) */
#define BITS(n) \
    ((unsigned)Hold & ((1U << (n)) - 1))

/* Remove n bits from the bit accumulator */
#define DROPBITS(n) \
    do { \
        Hold >>= (n); \
        Bits -= (unsigned)(n); \
    } while (0)

#define READBITS(c, a) \
	do { \
		NEEDBITS(a); \
		c = BITS(a); \
		DROPBITS(a); \
	} while (0)


/* SetCodeLengths() and DecodeSFValue() are from 7-Zip, which is LGPL. */

bool FZipExploder::DecoderBase::SetCodeLengths(const Byte *codeLengths, const int kNumSymbols)
{
	int lenCounts[kNumBitsInLongestCode + 2], tmpPositions[kNumBitsInLongestCode + 1];
	int i;
	for(i = 0; i <= kNumBitsInLongestCode; i++)
		lenCounts[i] = 0;
	int symbolIndex;
	for (symbolIndex = 0; symbolIndex < kNumSymbols; symbolIndex++)
		lenCounts[codeLengths[symbolIndex]]++;
  
	Limits[kNumBitsInLongestCode + 1] = 0;
	Positions[kNumBitsInLongestCode + 1] = 0;
	lenCounts[kNumBitsInLongestCode + 1] =  0;

	int startPos = 0;
	static const UInt32 kMaxValue = (1 << kNumBitsInLongestCode);

	for (i = kNumBitsInLongestCode; i > 0; i--)
	{
		startPos += lenCounts[i] << (kNumBitsInLongestCode - i);
		if (startPos > kMaxValue)
			return false;
		Limits[i] = startPos;
		Positions[i] = Positions[i + 1] + lenCounts[i + 1];
		tmpPositions[i] = Positions[i] + lenCounts[i];
	}

	if (startPos != kMaxValue)
		return false;

	for (symbolIndex = 0; symbolIndex < kNumSymbols; symbolIndex++)
		if (codeLengths[symbolIndex] != 0)
			Symbols[--tmpPositions[codeLengths[symbolIndex]]] = symbolIndex;
	return true;
}

int FZipExploder::DecodeSFValue(const DecoderBase &decoder, const unsigned int kNumSymbols)
{
	unsigned int numBits = 0;
	unsigned int value;
	int i;
	NEEDBITS(kNumBitsInLongestCode);
//	value = BITS(kNumBitsInLongestCode);
// TODO: Rewrite this so it doesn't need the BitReverse table.
//		 (It should be possible, right?)
	value = (BitReverse[Hold & 0xFF] << 8) | BitReverse[(Hold >> 8) & 0xFF];
	for(i = kNumBitsInLongestCode; i > 0; i--)
	{
		if (value < decoder.Limits[i])
		{
			numBits = i;
			break;
		}
	}
	if (i == 0)
		return -1;
	DROPBITS(numBits);
	unsigned int index = decoder.Positions[numBits] +
		((value - decoder.Limits[numBits + 1]) >> (kNumBitsInLongestCode - numBits));
	if (index >= kNumSymbols)
		return -1;
	return decoder.Symbols[index];
}


int FZipExploder::DecodeSF(unsigned char *table)
{
	unsigned char a, c;
	int i, n, v = 0;

	READBYTE(c);
	n = c + 1;
	for (i = 0; i < n; i++) {
		int nv, bl;
		READBYTE(a);
		nv = ((a >> 4) & 15) + 1;
		bl = (a & 15) + 1;
		while (nv--) {
			table[v++] = bl;
		}
	}
	return v; /* entries used */
}

int FZipExploder::Explode(unsigned char *out, unsigned int outsize,
						  FileReader *in, unsigned int insize,
						  int flags)
{
	int c, i, minMatchLen = 3, len, dist;
	int lowDistanceBits;
	unsigned char ll[256];
	unsigned int bIdx = 0;

	Hold = 0;
	Bits = 0;
	In = in;
	InLeft = insize;

	if ((flags & 4)) {
		/* 3 trees: literals, lengths, distance top 6 */
		minMatchLen = 3;
		if (!LiteralDecoder.SetCodeLengths(ll, DecodeSF(ll)))
			return 1;
	} else {
		/* 2 trees: lengths, distance top 6 */
		minMatchLen = 2;
	}
	if (!LengthDecoder.SetCodeLengths(ll, DecodeSF(ll)))
		return 1;
	if (!DistanceDecoder.SetCodeLengths(ll, DecodeSF(ll)))
		return 1;

	lowDistanceBits = (flags & 2) ? /* 8k dictionary */ 7 : /* 4k dictionary */ 6;
	while (bIdx < outsize)
	{
		READBITS(c, 1);
		if (c) {
			/* literal data */
			if ((flags & 4)) {
				c = DecodeSFValue(LiteralDecoder, 256);
			} else {
				READBITS(c, 8);
			}
			out[bIdx++] = c;
		} else {
			READBITS(dist, lowDistanceBits);
			c = DecodeSFValue(DistanceDecoder, 64);
			dist |= (c << lowDistanceBits);
			len = DecodeSFValue(LengthDecoder, 64);
			if (len == 63) {
				READBITS(c, 8);
				len += c;
			}
			len += minMatchLen;
			dist++;
			if (bIdx + len > outsize) {
				throw CExplosionError("Not enough output space");
			}
			if ((unsigned int)dist > bIdx) {
				/* Anything before the first input byte is zero. */
				int zeros = dist - bIdx;
				if (len < zeros)
					zeros = len;
				for(i = zeros; i; i--)
					out[bIdx++] = 0;
				len -= zeros;
			}
			for(i = len; i; i--, bIdx++) {
				out[bIdx] = out[bIdx - dist];
			}
		}
	}
	return 0;
}


/* HSIZE is defined as 2^13 (8192) in unzip.h */
#define HSIZE      8192
#define BOGUSCODE  256
#define CODE_MASK  (HSIZE - 1)   /* 0x1fff (lower bits are parent's index) */
#define FREE_CODE  HSIZE         /* 0x2000 (code is unused or was cleared) */
#define HAS_CHILD  (HSIZE << 1)  /* 0x4000 (code has a child--do not clear) */

int ShrinkLoop(unsigned char *out, unsigned int outsize,
			   FileReader *In, unsigned int InLeft)
{
	unsigned short Parent[HSIZE];
	unsigned char Value[HSIZE], Stack[HSIZE];
	unsigned char *newstr;
	int len;
	int KwKwK, codesize = 9;	/* start at 9 bits/code */
	int code, oldcode, freecode, curcode;
	unsigned int Bits = 0, Hold = 0;
	unsigned int size = 0;

	freecode = BOGUSCODE;
	for (code = 0; code < BOGUSCODE; code++)
	{
		Value[code] = code;
		Parent[code] = BOGUSCODE;
	}
	for (code = BOGUSCODE+1; code < HSIZE; code++)
		Parent[code] = FREE_CODE;

	READBITS(oldcode, codesize);
	if (size < outsize) {
		out[size++] = oldcode;
	}

	while (size < outsize)
	{
		READBITS(code, codesize);
		if (code == BOGUSCODE) {	/* possible to have consecutive escapes? */
			READBITS(code, codesize);
			if (code == 1) {
				codesize++;
			} else if (code == 2) {
				/* clear leafs (nodes with no children) */
				/* first loop:  mark each parent as such */
				for (code = BOGUSCODE+1;  code < HSIZE;  ++code) {
					curcode = (Parent[code] & CODE_MASK);

					if (curcode > BOGUSCODE)
						Parent[curcode] |= HAS_CHILD;		/* set parent's child-bit */
				}

				/* second loop:  clear all nodes *not* marked as parents; reset flag bits */
				for (code = BOGUSCODE+1;  code < HSIZE;  ++code) {
					if (Parent[code] & HAS_CHILD) {		/* just clear child-bit */
						Parent[code] &= ~HAS_CHILD;
					} else {							/* leaf:  lose it */
						Parent[code] = FREE_CODE;
					}
				}
				freecode = BOGUSCODE;
			}
			continue;
		}

		newstr = &Stack[HSIZE-1];
		curcode = code;

		if (Parent[curcode] == FREE_CODE) {
			KwKwK = 1;
			newstr--;	/* last character will be same as first character */
			curcode = oldcode;
			len = 1;
		} else {
			KwKwK = 0;
			len = 0;
		}

		do {
			*newstr-- = Value[curcode];
			len++;
			curcode = (Parent[curcode] & CODE_MASK);
		} while (curcode != BOGUSCODE);

		newstr++;
		if (KwKwK) {
			Stack[HSIZE-1] = *newstr;
		}

		do {
			freecode++;
		} while (Parent[freecode] != FREE_CODE);

		Parent[freecode] = oldcode;
		Value[freecode] = *newstr;
		oldcode = code;

		while (len--) {
			out[size++] = *newstr++;
		}
	}
	return 0;
}
