/*
** ancientzip.cpp
**
**---------------------------------------------------------------------------
** Copyright 2010-2011 Randy Heit
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
** Based in information from
**
    gunzip.c by Pasi Ojala,	a1bert@iki.fi
				http://www.iki.fi/a1bert/

    A hopefully easier to understand guide to GZip
    (deflate) decompression routine than the GZip
    source code.

 */

/*----------------------------------------------------------------------*/


#include <stdlib.h>
#include "ancientzip.h"

namespace FileSys {
	
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
			if (bs < be) \
				c = ReadBuf[bs++]; \
			else { \
				be = (decltype(be))In->Read(&ReadBuf, sizeof(ReadBuf)); \
				c = ReadBuf[0]; \
				bs = 1; \
			} \
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

/****************************************************************
   Shannon-Fano tree routines
 ****************************************************************/

static const unsigned char BitReverse4[] = {
	0x00, 0x08, 0x04, 0x0c, 0x02, 0x0a, 0x06, 0x0e,
	0x01, 0x09, 0x05, 0x0d, 0x03, 0x0b, 0x07, 0x0f
};

#define FIRST_BIT_LEN	8
#define REST_BIT_LEN	4

void FZipExploder::InsertCode(TArray<HuffNode> &decoder, unsigned int pos, int bits, unsigned short code, int len, unsigned char value)
{
	assert(len > 0);
	unsigned int node = pos + (code & ((1 << bits) - 1));

	if (len > bits)
	{ // This code uses more bits than this level has room for. Store the bottom bits
	  // in this table, then proceed to the next one.
		unsigned int child = decoder[node].ChildTable;
		if (child == 0)
		{ // Need to create child table.
			child = InitTable(decoder, 1 << REST_BIT_LEN);
			decoder[node].ChildTable = child;
			decoder[node].Length = bits;
			decoder[node].Value = 0;
		}
		else
		{
			assert(decoder[node].Length == bits);
			assert(decoder[node].Value == 0);
		}
		InsertCode(decoder, child, REST_BIT_LEN, code >> bits, len - bits, value);
	}
	else
	{ // If this code uses fewer bits than this level of the table, it is
	  // inserted repeatedly for each value that matches it at its lower
	  // bits.
		for (int i = 1 << (bits - len); --i >= 0; node += 1 << len)
		{
			decoder[node].Length = len;
			decoder[node].Value = value;
			assert(decoder[node].ChildTable == 0);
		}
	}
}

unsigned int FZipExploder::InitTable(TArray<HuffNode> &decoder, int numspots)
{
	unsigned int start = decoder.Size();
	decoder.Reserve(numspots);
	memset(&decoder[start], 0, sizeof(HuffNode)*numspots);
	return start;
}

int FZipExploder::buildercmp(const void *a, const void *b)
{
	const TableBuilder *v1 = (const TableBuilder *)a;
	const TableBuilder *v2 = (const TableBuilder *)b;
	int d = v1->Length - v2->Length;
	if (d == 0) {
		d = v1->Value - v2->Value;
	}
	return d;
}

int FZipExploder::BuildDecoder(TArray<HuffNode> &decoder, TableBuilder *values, int numvals)
{
	int i;

	qsort(values, numvals, sizeof(*values), buildercmp);

	// Generate the Shannon-Fano tree:
	unsigned short code = 0;
	unsigned short code_increment = 0;
	unsigned short last_bit_length = 0;
	for (i = numvals - 1; i >= 0; --i)
	{
		code += code_increment;
		if (values[i].Length != last_bit_length)
		{
			last_bit_length = values[i].Length;
			code_increment = 1 << (16 - last_bit_length);
		}
		// Reverse the order of the bits in the code before storing it.
		values[i].Code = BitReverse4[code >> 12] |
						(BitReverse4[(code >> 8) & 0xf] << 4) |
						(BitReverse4[(code >> 4) & 0xf] << 8) |
						(BitReverse4[code & 0xf] << 12);
	}

	// Insert each code into the hierarchical table. The top level is FIRST_BIT_LEN bits,
	// and the other levels are REST_BIT_LEN bits. If a code does not completely fill
	// a level, every permutation for the remaining bits is filled in to
	// match this one.
	InitTable(decoder, 1 << FIRST_BIT_LEN);		// Set up the top level.
	for (i = 0; i < numvals; ++i)
	{
		InsertCode(decoder, 0, FIRST_BIT_LEN, values[i].Code, values[i].Length, values[i].Value);
	}
	return 0;
}


int FZipExploder::DecodeSFValue(const TArray<HuffNode> &decoder)
{
	unsigned int bits = FIRST_BIT_LEN, table = 0, code;
	const HuffNode *pos;
	do
	{
		NEEDBITS(bits);
		code = BITS(bits);
		bits = REST_BIT_LEN;
		pos = &decoder[table + code];
		DROPBITS(pos->Length);
		table = pos->ChildTable;
	}
	while (table != 0);
	return pos->Value;
}


int FZipExploder::DecodeSF(TArray<HuffNode> &decoder, int numvals)
{
	TableBuilder builder[256];
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
			builder[v].Length = bl;
			builder[v].Value = v;
			v++;
		}
	}
	if (v != numvals)
		return 1;	/* bad table */
	return BuildDecoder(decoder, builder, v);
}

int FZipExploder::Explode(unsigned char *out, unsigned int outsize,
						  FileReader &in, unsigned int insize,
						  int flags)
{
	int c, i, minMatchLen = 3, len, dist;
	int lowDistanceBits;
	unsigned int bIdx = 0;

	Hold = 0;
	Bits = 0;
	In = &in;
	InLeft = insize;
	bs = be = 0;

	if ((flags & 4)) {
		/* 3 trees: literals, lengths, distance top 6 */
		minMatchLen = 3;
		if (DecodeSF(LiteralDecoder, 256))
			return 1;
	} else {
		/* 2 trees: lengths, distance top 6 */
		minMatchLen = 2;
	}
	if (DecodeSF(LengthDecoder, 64))
		return 1;
	if (DecodeSF(DistanceDecoder, 64))
		return 1;

	lowDistanceBits = (flags & 2) ? /* 8k dictionary */ 7 : /* 4k dictionary */ 6;
	while (bIdx < outsize)
	{
		READBITS(c, 1);
		if (c) {
			/* literal data */
			if ((flags & 4)) {
				c = DecodeSFValue(LiteralDecoder);
			} else {
				READBITS(c, 8);
			}
			out[bIdx++] = c;
		} else {
			READBITS(dist, lowDistanceBits);
			c = DecodeSFValue(DistanceDecoder);
			dist |= (c << lowDistanceBits);
			len = DecodeSFValue(LengthDecoder);
			if (len == 63) {
				READBITS(c, 8);
				len += c;
			}
			len += minMatchLen;
			dist++;
			if (bIdx + len > outsize) {
				return -1;
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

int ShrinkLoop(unsigned char *out, unsigned int outsize, FileReader &_In, unsigned int InLeft)
{
	FileReader *In = &_In;
	unsigned char ReadBuf[256];
	unsigned short Parent[HSIZE];
	unsigned char Value[HSIZE], Stack[HSIZE];
	unsigned char *newstr;
	int len;
	int KwKwK, codesize = 9;	/* start at 9 bits/code */
	int code, oldcode, freecode, curcode;
	unsigned int Bits = 0, Hold = 0;
	unsigned int size = 0;
	unsigned int bs = 0, be = 0;

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

}
