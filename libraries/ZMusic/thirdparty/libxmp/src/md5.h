/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 */

#ifndef LIBXMP_MD5_H
#define LIBXMP_MD5_H

#include "common.h"

#define	MD5_BLOCK_LENGTH		64
#define	MD5_DIGEST_LENGTH		16
#define	MD5_DIGEST_STRING_LENGTH	(MD5_DIGEST_LENGTH * 2 + 1)

typedef struct MD5Context {
	uint32 state[4];		/* state */
	uint64 count;			/* number of bits, mod 2^64 */
	uint8 buffer[MD5_BLOCK_LENGTH];	/* input buffer */
} MD5_CTX;

LIBXMP_BEGIN_DECLS

void	 MD5Init(MD5_CTX *);
void	 MD5Update(MD5_CTX *, const unsigned char *, size_t);
void	 MD5Final(uint8[MD5_DIGEST_LENGTH], MD5_CTX *);

LIBXMP_END_DECLS

#endif /* LIBXMP_MD5_H */

