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

int FZipExploder::READBYTE()
{
	if (InLeft)
	{
		unsigned char c;
		InLeft--;
		if (1 != In->Read(&c, 1))
			throw CExplosionError("Out of input");
		return c;
	}
	throw CExplosionError("Out of input");
}

/* Get a byte of input into the bit accumulator, or return from inflate()
   if there is no input available. */
#define PULLBYTE() \
    do { \
        int next = READBYTE(); \
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

int FZipExploder::IsPat()
{
	for(;;)
	{
		if (fpos[len] >= fmax)
			return -1;
		if (flens[fpos[len]] == len)
			return fpos[len]++;
		fpos[len]++;
	}
}


/*
    A recursive routine which creates the Huffman decode tables

    No presorting of code lengths are needed, because a counting
    sort is perfomed on the fly.
 */

/* Maximum recursion depth is equal to the maximum
   Huffman code length, which is 15 in the deflate algorithm.
   (16 in Inflate!) */
int FZipExploder::Rec()
{
	struct HufNode *curplace = Places;
	int tmp;

	if(len == 17)
	{
		return -1;
	}
	Places++;
	len++;

	tmp = IsPat();
	if(tmp >= 0) {
		curplace->b0 = tmp;		/* leaf cell for 0-bit */
	} else {
		/* Not a Leaf cell */
		curplace->b0 = 0x8000;
		if(Rec())
			return -1;
	}
	tmp = IsPat();
	if(tmp >= 0) {
		curplace->b1 = tmp;		/* leaf cell for 1-bit */
		curplace->jump = NULL;	/* Just for the display routine */
	} else {
		/* Not a Leaf cell */
		curplace->b1 = 0x8000;
		curplace->jump = Places;
		if(Rec())
			return -1;
	}
	len--;
	return 0;
}


/* In C64 return the most significant bit in Carry */
/* The same as DecodeValue(), except that 0/1 is reversed */
int FZipExploder::DecodeSFValue(struct HufNode *currentTree)
{
	struct HufNode *X = currentTree;
	int c;

	/* decode one symbol of the data */
	for(;;)
	{
		READBITS(c, 1);
		if(!c) {	/* Only the decision is reversed! */
			if(!(X->b1 & 0x8000))
				return X->b1;	/* If leaf node, return data */
			X = X->jump;
		} else {
			if(!(X->b0 & 0x8000))
				return X->b0;	/* If leaf node, return data */
			X++;
		}
	}
	return -1;
}


/*
    Note:
	The tree create and distance code trees <= 32 entries
	and could be represented with the shorter tree algorithm.
	I.e. use a X/Y-indexed table for each struct member.
 */
int FZipExploder::CreateTree(struct HufNode *currentTree, int numval, int *lengths)
{
	int i;

	/* Create the Huffman decode tree/table */
	Places = currentTree;
	flens = lengths;
	fmax  = numval;
	for (i=0;i<17;i++)
		fpos[i] = 0;
	len = 0;
	if(Rec()) {
/*		fprintf(stderr, "invalid huffman tree\n");*/
		return -1;
	}

/*	fprintf(stderr, "%d table entries used (max code length %d)\n",
		Places-currentTree, maxlen);*/
	return 0;
}


int FZipExploder::DecodeSF(int *table)
{
	int i, a, n = READBYTE() + 1, v = 0;

	for (i = 0; i < n; i++) {
		int nv, bl;
		a = READBYTE();
		nv = ((a >> 4) & 15) + 1;
		bl = (a & 15) + 1;
		while (nv--) {
			table[v++] = bl;
		}
	}
	return v; /* entries used */
}

/* Note: Imploding could use the lighter huffman tree routines, as the
		 max number of entries is 256. But too much code would need to
		 be duplicated.
 */
int FZipExploder::Explode(unsigned char *out, unsigned int outsize,
						  FileReader *in, unsigned int insize,
						  int flags)
{
	int c, i, minMatchLen = 3, len, dist;
	int ll[256];
	unsigned int bIdx = 0;

	Hold = 0;
	Bits = 0;
	In = in;
	InLeft = insize;

	if ((flags & 4)) {
		/* 3 trees: literals, lengths, distance top 6 */
		minMatchLen = 3;
		if (CreateTree(LiteralTree, DecodeSF(ll), ll))
			return 1;
	} else {
		/* 2 trees: lengths, distance top 6 */
		minMatchLen = 2;
	}
	if (CreateTree(LengthTree, DecodeSF(ll), ll))
		return 1;
	if (CreateTree(DistanceTree, DecodeSF(ll), ll))
		return 1;

	while (bIdx < outsize)
	{
		READBITS(c, 1);
		if (c) {
			/* literal data */
			if ((flags & 4)) {
				c = DecodeSFValue(LiteralTree);
			} else {
				READBITS(c, 8);
			}
			out[bIdx++] = c;
		} else {
			if ((flags & 2)) {
				/* 8k dictionary */
				READBITS(dist, 7);
				c = DecodeSFValue(DistanceTree);
				dist |= (c<<7);
			} else {
				/* 4k dictionary */
				READBITS(dist, 6);
				c = DecodeSFValue(DistanceTree);
				dist |= (c<<6);
			}
			len = DecodeSFValue(LengthTree);
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
