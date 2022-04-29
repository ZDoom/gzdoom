/*
** zstrformat.cpp
** Routines for generic printf-style formatting.
**
**---------------------------------------------------------------------------
** Copyright 2005-2008 Randy Heit
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
** Portions of this file relating to printing floating point numbers
** are covered by the following copyright:
**
**---------------------------------------------------------------------------
** Copyright (c) 1990, 1993
**	The Regents of the University of California.  All rights reserved.
**
** This code is derived from software contributed to Berkeley by
** Chris Torek.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 4. Neither the name of the University nor the names of its contributors
**    may be used to endorse or promote products derived from this software
**    without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
**---------------------------------------------------------------------------
**
** Even though the standard C library has a function to do printf-style
** formatting in a generic way, there is no standard interface to this
** function. So if you want to do some printf formatting that doesn't fit in
** the context of the provided functions, you need to roll your own. Why is
** that?
**
** Maybe Microsoft wants you to write a better one yourself? When used as
** part of a sprintf replacement, this function is significantly faster than
** Microsoft's offering. When used as part of a fprintf replacement, this
** function turns out to be slower, but that's probably because the CRT's
** fprintf can interact with the FILE object on a low level for better
** perfomance. If you sprintf into a buffer and then fwrite that buffer, this
** routine wins again, though the difference isn't great.
*/

#include <limits.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <locale.h>

#include "zstring.h"
#include "gdtoa.h"
#include "utf8.h"


/*
 * MAXEXPDIG is the maximum number of decimal digits needed to store a
 * floating point exponent in the largest supported format.  It should
 * be ceil(log10(LDBL_MAX_10_EXP)) or, if hexadecimal floating point
 * conversions are supported, ceil(log10(LDBL_MAX_EXP)).  But since it
 * is presently never greater than 5 in practice, we fudge it.
 */
#define	MAXEXPDIG	6
#if LDBL_MAX_EXP > 999999
#error "floating point buffers too small"
#endif

#define DEFPREC		6

static const char hexits[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
static const char HEXits[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
static const char spaces[16] = {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
static const char zeroes[17] = {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','.'};

namespace StringFormat
{
	static int writepad (OutputFunc output, void *outputData, const char *pad, int padsize, int spaceToFill);
	static int printandpad (OutputFunc output, void *outputData, const char *p, const char *ep, int len, const char *with, int padsize);
	static int exponent (char *p0, int exp, int fmtch);

	int Worker (OutputFunc output, void *outputData, const char *fmt, ...)
	{
		va_list arglist;
		int len;

		va_start (arglist, fmt);
		len = VWorker (output, outputData, fmt, arglist);
		va_end (arglist);
		return len;
	}

	int VWorker (OutputFunc output, void *outputData, const char *fmt, va_list arglist)
	{
		const char *c;
		const char *base;
		int len = 0;
		int width;
		int precision;
		int flags;

		base = c = fmt;
		for (;;)
		{
			while (*c && *c != '%')
			{
				++c;
			}
			if (*c == '\0')
			{
				return len + output (outputData, base, int(c - base));
			}

			if (c - base > 0)
			{
				len += output (outputData, base, int(c - base));
			}
			c++;

			// Gather the flags, if any
			for (flags = 0;; ++c)
			{
				if (*c == '-')
				{
					flags |= F_MINUS;		// bit 0
				}
				else if (*c == '+')
				{
					flags |= F_PLUS;		// bit 1
				}
				else if (*c == '0')
				{
					flags |= F_ZERO;		// bit 2
				}
				else if (*c == ' ')
				{
					flags |= F_BLANK;		// bit 3
				}
				else if (*c == '#')
				{
					flags |= F_HASH;		// bit 4
				}
				else
				{
					break;
				}
			}

			width = precision = -1;

			// Read the width, if any
			if (*c == '*')
			{
				++c;
				width = va_arg (arglist, int);
				if (width < 0)
				{ // Negative width means minus flag and positive width
					flags |= F_MINUS;
					width = -width;
				}
			}
			else if (*c >= '0' && *c <= '9')
			{
				width = *c++ - '0';
				while (*c >= '0' && *c <= '9')
				{
					width = width * 10 + *c++ - '0';
				}
			}

			// If 0 and - both appear, 0 is ignored.
			// If the blank and + both appear, the blank is ignored.
			flags &= ~((flags & 3) << 2);

			// Read the precision, if any
			if (*c == '.')
			{
				precision = 0;
				if (*++c == '*')
				{
					++c;
					precision = va_arg (arglist, int);
				}
				else if (*c >= '0' && *c <= '9')
				{
					precision = *c++ - '0';
					while (*c >= '0' && *c <= '9')
					{
						precision = precision * 10 + *c++ - '0';
					}
				}
			}

			// Read the size prefix, if any
			if (*c == 'h')
			{
				if (*++c == 'h')
				{
					flags |= F_HALFHALF;
					++c;
				}
				else
				{
					flags |= F_HALF;
				}
			}
			else if (*c == 'l')
			{
				if (*++c == 'l')
				{
					flags |= F_LONGLONG;
					++c;
				}
				else
				{
					flags |= F_LONG;
				}
			}
			else if (*c == 'I')
			{
				if (*++c == '6')
				{
					if (*++c == '4')
					{
						flags |= F_LONGLONG;
                        ++c;
					}
				}
				else
				{
					flags |= F_BIGI;
				}
			}
			else if (*c == 't')
			{
				flags |= F_PTRDIFF;
				++c;
			}
			else if (*c == 'z')
			{
				flags |= F_SIZE;
				++c;
			}

			base = c+1;

			// Now that that's all out of the way, we should be pointing at the type specifier
	{
		char prefix[3];
		int prefixlen;
		char hexprefix = '\0';
		char sign = '\0';
		int postprefixzeros = 0;
		int size = flags & 0xF000;
		char buffer[80], *ibuff;
		const char *obuff = 0;
		char type = *c++;
		int bufflen = 0;
		int outlen = 0;
		unsigned int intarg = 0;
		uint64_t int64arg = 0;
		const void *voidparg;
		const char *charparg;
		double dblarg;
		const char *xits = hexits;
		int inlen = len;
		/*
		 * We can decompose the printed representation of floating
		 * point numbers into several parts, some of which may be empty:
		 *
		 * [+|-| ] [0x|0X] MMM . NNN [e|E|p|P] [+|-] ZZ
		 *    A       B     ---C---      D       E   F
		 *
		 * A:	'sign' holds this value if present; '\0' otherwise
		 * B:	hexprefix holds the 'x' or 'X'; '\0' if not hexadecimal
		 * C:	obuff points to the string MMMNNN.  Leading and trailing
		 *		zeros are not in the string and must be added.
		 * D:	expchar holds this character; '\0' if no exponent, e.g. %f
		 * F:	at least two digits for decimal, at least one digit for hex
		 */
		const char *decimal_point = ".";/* locale specific decimal point */
		int signflag;					/* true if float is negative */
		int expt = 0;					/* integer value of exponent */
		char expchar = 'e';				/* exponent character: [eEpP\0] */
		char* dtoaend = nullptr;		/* pointer to end of converted digits */
		int expsize = 0;				/* character count for expstr */
		int ndig = 0;					/* actual number of digits returned by dtoa */
		char expstr[MAXEXPDIG+2];		/* buffer for exponent string: e+ZZZ */
		char *dtoaresult = NULL;		/* buffer allocated by dtoa */

		// Using a bunch of if/else if statements is faster than a switch, because a switch generates
		// a jump table. A jump table means a possible data cache miss and a hefty penalty while the
		// cache line is loaded.

		if (type == 'x' || type == 'X' ||
			type == 'p' ||
			type == 'd' || type == 'u' || type == 'i' ||
			type == 'o' ||
			type == 'B')
		{
			if (type == 'X' || type == 'p')
			{
				xits = HEXits;
			}
			if (type == 'p')
			{
				type = 'X';
				voidparg = va_arg (arglist, void *);
				if (sizeof(void*) == sizeof(int))
				{
					intarg = (unsigned int)(size_t)voidparg;
					precision = 8;
					size = 0;
				}
				else
				{
					int64arg = (uint64_t)(size_t)voidparg;
					precision = 16;
					size = F_LONGLONG;
				}
			}
			else
			{
				if (size == 0)
				{
					intarg = va_arg (arglist, int);
				}
				else if (size == F_HALFHALF)
				{
					intarg = va_arg (arglist, int);
					intarg = (signed char)intarg;
				}
				else if (size == F_HALF)
				{
					intarg = va_arg (arglist, int);
					intarg = (short)intarg;
				}
				else if (size == F_LONG)
				{
					if (sizeof(long) == sizeof(int)) intarg = va_arg (arglist, int);
					else { int64arg = va_arg (arglist, int64_t); size = F_LONGLONG; }
				}
				else if (size == F_BIGI)
				{
					if (sizeof(void*) == sizeof(int)) intarg = va_arg (arglist, int);
					else { int64arg = va_arg (arglist, int64_t); size = F_LONGLONG; }
				}
				else if (size == F_LONGLONG)
				{
					int64arg = va_arg (arglist, int64_t);
				}
				else if (size == F_PTRDIFF)
				{
					if (sizeof(ptrdiff_t) == sizeof(int)) intarg = va_arg (arglist, int);
					else { int64arg = va_arg (arglist, int64_t); size = F_LONGLONG; }
				}
				else if (size == F_SIZE)
				{
					if (sizeof(size_t) == sizeof(int)) intarg = va_arg (arglist, int);
					else { int64arg = va_arg (arglist, int64_t); size = F_LONGLONG; }
				}
				else
				{
					intarg = va_arg (arglist, int);
				}
			}

			if (precision < 0) precision = 1;

			ibuff = &buffer[sizeof(buffer)];

			if (size == F_LONGLONG)
			{
				if (int64arg == 0)
				{
					flags |= F_ZEROVALUE;
				}
				else
				{
					if (type == 'o')
					{ // Octal: Dump digits until it fits in an unsigned int
						while (int64arg > UINT_MAX)
						{
							*--ibuff = char(int64arg & 7) + '0'; int64arg >>= 3;
						}
						intarg = int(int64arg);
					}
					else if (type == 'x' || type == 'X')
					{ // Hexadecimal: Dump digits until it fits in an unsigned int
						while (int64arg > UINT_MAX)
						{
							*--ibuff = xits[int64arg & 15]; int64arg >>= 4;
						}
						intarg = int(int64arg);
					}
					else if (type == 'B')
					{ // Binary: Dump digits until it fits in an unsigned int
						while (int64arg > UINT_MAX)
						{
							*--ibuff = char(int64arg & 1) + '0'; int64arg >>= 1;
						}
						intarg = int(int64arg);
					}
					else
					{
						if (type != 'u')
						{
							// If a signed number is negative, set the negative flag and make it positive.
							int64_t sint64arg = (int64_t)int64arg;
							if (sint64arg < 0)
							{
								flags |= F_NEGATIVE;
								sint64arg = -sint64arg;
								int64arg = sint64arg;
							}
							flags |= F_SIGNED;
							type = 'u';
						}
						// If an unsigned int64 is too big to fit in an unsigned int, dump out
						// digits until it is sufficiently small.
						while (int64arg > INT_MAX)
						{
							*--ibuff = char(int64arg % 10) + '0'; int64arg /= 10;
						}
						intarg = (unsigned int)(int64arg);
					}
				}
			}
			else
			{
				if (intarg == 0)
				{
					flags |= F_ZEROVALUE;
				}
				else if (type == 'i' || type == 'd')
				{ // If a signed int is negative, set the negative flag and make it positive.
					signed int sintarg = (signed int)intarg;
					if (sintarg < 0)
					{
						flags |= F_NEGATIVE;
						sintarg = -sintarg;
						intarg = sintarg;
					}
					flags |= F_SIGNED;
					type = 'u';
				}
			}
			if (flags & F_ZEROVALUE)
			{
				if (precision != 0)
				{
					*--ibuff = '0';
				}
			}
			else if (type == 'u')
			{ // Decimal
				int i;

				// Unsigned division is typically slower than signed division.
				// Do it at most once.
				if (intarg > INT_MAX)
				{
					*--ibuff = char(intarg % 10) + '0'; intarg /= 10;
				}
				i = (int)intarg;
				while (i != 0)
				{
					*--ibuff = char(i % 10) + '0'; i /= 10;
				}
			}
			else if (type == 'o')
			{ // Octal
				while (intarg != 0)
				{
					*--ibuff = char(intarg & 7) + '0'; intarg >>= 3;
				}
			}
			else if (type == 'B')
			{ // Binary
				while (intarg != 0)
				{
					*--ibuff = char(intarg & 1) + '0'; intarg >>= 1;
				}
			}
			else
			{ // Hexadecimal
				while (intarg != 0)
				{
					*--ibuff = xits[intarg & 15]; intarg >>= 4;
				}
			}
			// Check for prefix (only for non-decimal, which are always unsigned)
			if ((flags & (F_HASH|F_ZEROVALUE)) == F_HASH)
			{
				if (type == 'o')
				{
					if (bufflen >= precision)
					{
						sign = '0';
					}
				}
				else if (type == 'x' || type == 'X')
				{
					hexprefix = type;
				}
				else if (type == 'B')
				{
					hexprefix = '!';
				}
			}
			bufflen = (int)(ptrdiff_t)(&buffer[sizeof(buffer)] - ibuff);
			obuff = ibuff;
			if (precision >= 0)
			{
				postprefixzeros = precision - bufflen;
				if (postprefixzeros < 0) postprefixzeros = 0;
//				flags &= ~F_ZERO;
			}
		}
		else if (type == 'c')
		{
			intarg = va_arg (arglist, int);
			if (utf8_encode(intarg, (uint8_t*)buffer, &bufflen) != 0)
			{
				buffer[0] = '?';
				bufflen = 1;
			}
			obuff = buffer;
		}
		else if (type == 's')
		{
			charparg = va_arg (arglist, const char *);
			if (charparg == NULL)
			{
				obuff = "(null)";
				bufflen = 6;
			}
			else
			{
				obuff = charparg;
				if (precision < 0)
				{
					bufflen = (int)strlen (charparg);
				}
				else
				{
					for (bufflen = 0; bufflen < precision && charparg[bufflen] != '\0'; ++bufflen)
					{ /* empty */ }
				}
			}
		}
		else if (type == '%')
		{ // Just print a '%': Output it with the next stage.
			base--;
			continue;
		}
		else if (type == 'n')
		{
			if (size == F_HALFHALF)
			{
				*va_arg (arglist, char *) = (char)inlen;
			}
			else if (size == F_HALF)
			{
				*va_arg (arglist, short *) = (short)inlen;
			}
			else if (size == F_LONG)
			{
				*va_arg (arglist, long *) = inlen;
			}
			else if (size == F_LONGLONG)
			{
				*va_arg (arglist, int64_t *) = inlen;
			}
			else if (size == F_BIGI)
			{
				*va_arg (arglist, ptrdiff_t *) = inlen;
			}
			else
			{
				*va_arg (arglist, int *) = inlen;
			}
		}
		else if (type == 'f' || type == 'F')
		{
			expchar = '\0';
			goto fp_begin;
		}
		else if (type == 'g' || type == 'G')
		{
			expchar = type - ('g' - 'e');
			if (precision == 0)
			{
				precision = 1;
			}
			goto fp_begin;
		}
		else if (type == 'H')
		{ // %H is an extension that behaves similarly to %g, except it automatically
		  // selects precision based on whatever will produce the smallest string.
			expchar = 'e';
			goto fp_begin;
		}
#if 0
		// The hdtoa function provided with FreeBSD uses a hexadecimal FP constant.
		// Microsoft's compiler does not support these, so I would need to hack it
		// together with ints instead. It's very do-able, but until I actually have
		// some reason to print hex FP numbers, I won't bother.
		else if (type == 'a' || type == 'A')
		{
			if (type == 'A')
			{
				xits = HEXits;
				hexprefix = 'X';
				expchar = 'P';
			}
			else
			{
				hexprefix = 'x';
				expchar = 'p';
			}
			if (precision >= 0)
			{
				precision++;
			}
			dblarg = va_arg(arglist, double);
			dtoaresult = obuff = hdtoa(dblarg, xits, precision, &expt, &signflag, &dtoaend);
			if (precision < 0)
			{
				precision = (int)(dtoaend - obuff);
			}
			if (expt == INT_MAX)
			{
				hexprefix = '\0';
			}
			goto fp_common;
		}
#endif
		else if (type == 'e' || type == 'E')
		{
			expchar = type;
			if (precision < 0)	// account for digit before decpt
			{
				precision = DEFPREC + 1;
			}
			else
			{
				precision++;
			}
fp_begin:
			if (precision < 0)
			{
				precision = DEFPREC;
			}
			dblarg = va_arg(arglist, double);
			obuff = dtoaresult = dtoa(dblarg, type != 'H' ? (expchar ? 2 : 3) : 0, precision, &expt, &signflag, &dtoaend);
//fp_common:
			decimal_point = localeconv()->decimal_point;
			flags |= F_SIGNED;
			if (signflag)
			{
				flags |= F_NEGATIVE;
			}
			if (expt == 9999)	// inf or nan
			{
				if (*obuff == 'N')
				{
					obuff = (type >= 'a') ? "nan" : "NAN";
					flags &= ~F_SIGNED;
				}
				else
				{
					obuff = (type >= 'a') ? "inf" : "INF";
				}
				bufflen = 3;
				flags &= ~F_ZERO;
			}
			else
			{
				flags |= F_FPT;
				ndig = (int)(dtoaend - obuff);
				if (type == 'g' || type == 'G')
				{
					if (expt > -4 && expt <= precision)
					{ // Make %[gG] smell like %[fF].
						expchar = '\0';
						if (flags & F_HASH)
						{
							precision -= expt;
						}
						else
						{
							precision = ndig - expt;
						}
						if (precision < 0)
						{
							precision = 0;
						}
					}
					else
					{ // Make %[gG] smell like %[eE], but trim trailing zeroes if no # flag.
						if (!(flags & F_HASH))
						{
							precision = ndig;
						}
					}
				}
				else if (type == 'H')
				{
					if (expt > -(ndig + 2) && expt <= (ndig + 4))
					{ // Make %H smell like %f
						expchar = '\0';
						precision = ndig - expt;
						if (precision < 0)
						{
							precision = 0;
						}
					}
					else
					{// Make %H smell like %e
						precision = ndig;
					}
				}
				if (expchar)
				{
					expsize = exponent(expstr, expt - 1, expchar);
					bufflen = expsize + precision;
					if (precision > 1 || (flags & F_HASH))
					{
						++bufflen;
					}
				}
				else
				{ // space for digits before decimal point
					if (expt > 0)
					{
						bufflen = expt;
					}
					else	// "0"
					{
						bufflen = 1;
					}
					// space for decimal pt and following digits
					if (precision != 0 || (flags & F_HASH))
					{
						bufflen += precision + 1;
					}
				}
			}
		}

		// Check for sign prefix (only for signed numbers)
		if (flags & F_SIGNED)
		{
			if (flags & F_NEGATIVE)
			{
				sign = '-';
			}
			else if (flags & F_PLUS)
			{
				sign = '+';
			}
			else if (flags & F_BLANK)
			{
				sign = ' ';
			}
		}

		// Construct complete prefix from sign and hex prefix character
		prefixlen = 0;
		if (sign != '\0')
		{
			prefix[0] = sign;
			prefixlen = 1;
		}
		if (hexprefix != '\0')
		{
			prefix[prefixlen] = '0';
			prefix[prefixlen + 1] = hexprefix;
			prefixlen += 2;
		}

		// Pad the output to the field width, if needed
		int fieldlen = prefixlen + postprefixzeros + bufflen;
		const char *pad = (flags & F_ZERO) ? zeroes : spaces;

		// If the output is right aligned and zero-padded, then the prefix must come before the padding.
		if ((flags & (F_ZERO|F_MINUS)) == F_ZERO && prefixlen > 0)
		{
			outlen += output (outputData, prefix, prefixlen);
			prefixlen = 0;
		}
        if (!(flags & F_MINUS) && fieldlen < width)
		{ // Field is right-justified, so padding comes first
			outlen += writepad (output, outputData, pad, sizeof(spaces), width - fieldlen);
			width = -1;
		}

		// Output field: Prefix, post-prefix zeros, buffer text
		if (prefixlen > 0)
		{
			outlen += output (outputData, prefix, prefixlen);
		}
		outlen += writepad (output, outputData, zeroes, sizeof(spaces), postprefixzeros);
		if (!(flags & F_FPT))
		{
			if (bufflen > 0)
			{
				outlen += output (outputData, obuff, bufflen);
			}
		}
		else
		{
			if (expchar == '\0')	// %[fF] or sufficiently short %[gG]
			{
				if (expt <= 0)
				{
					outlen += output (outputData, zeroes, 1);
					if (precision != 0 || (flags & F_HASH))
					{
						outlen += output (outputData, decimal_point, 1);
					}
					outlen += writepad (output, outputData, zeroes, sizeof(zeroes), -expt);
					// already handled initial 0's
					precision += expt;
				}
				else
				{
					outlen += printandpad (output, outputData, obuff, dtoaend, expt, zeroes, sizeof(zeroes));
					obuff += expt;
					if (precision || (flags & F_HASH))
					{
						outlen += output (outputData, decimal_point, 1);
					}
				}
				outlen += printandpad (output, outputData, obuff, dtoaend, precision, zeroes, sizeof(zeroes));
			}
			else						// %[eE] or sufficiently long %[gG]
			{
				if (precision > 1 || (flags & F_HASH))
				{
					buffer[0] = *obuff++;
					buffer[1] = *decimal_point;
					outlen += output (outputData, buffer, 2);
					outlen += output (outputData, obuff, ndig - 1);
					outlen += writepad (output, outputData, zeroes, sizeof(zeroes), precision - ndig);
				}
				else		// XeYY
				{
					outlen += output (outputData, obuff, 1);
				}
				outlen += output (outputData, expstr, expsize);
			}
		}

        if ((flags & F_MINUS) && fieldlen < width)
		{ // Field is left-justified, so padding comes last
			outlen += writepad (output, outputData, pad, sizeof(spaces), width - fieldlen);
		}
		len += outlen;
		if (dtoaresult != NULL)
		{
			freedtoa(dtoaresult);
			dtoaresult = NULL;
		}
	}
	}
	}

	static int writepad (OutputFunc output, void *outputData, const char *pad, int padsize, int spaceToFill)
	{
		int outlen = 0;
		while (spaceToFill > 0)
		{
			int count = spaceToFill > padsize ? padsize : spaceToFill;
			outlen += output (outputData, pad, count);
			spaceToFill -= count;
		}
		return outlen;
	}

	static int printandpad (OutputFunc output, void *outputData, const char *p, const char *ep, int len, const char *with, int padsize)
	{
		int outlen = 0;
		int n2 = (int)(ep - p);
		if (n2 > len)
		{
			n2 = len;
		}
		if (n2 > 0)
		{
			outlen = output (outputData, p, n2);	
		}
		return outlen + writepad (output, outputData, with, padsize, len - (n2 > 0 ? n2 : 0));
	}

	static int exponent (char *p0, int exp, int fmtch)
	{
		char *p, *t;
		char expbuf[MAXEXPDIG];

		p = p0;
		*p++ = fmtch;
		if (exp < 0)
		{
			exp = -exp;
			*p++ = '-';
		}
		else
		{
			*p++ = '+';
		}
		t = expbuf + MAXEXPDIG;
		if (exp > 9)
		{
			do
			{
				*--t = '0' + (exp % 10);
			}
			while ((exp /= 10) > 9);
			*--t = '0' + exp;
			for(; t < expbuf + MAXEXPDIG; *p++ = *t++)
			{ }
		}
		else
		{
			// Exponents for decimal floating point conversions
			// (%[eEgG]) must be at least two characters long,
			// whereas exponents for hexadecimal conversions can
			// be only one character long.
			if (fmtch == 'e' || fmtch == 'E')
			{
				*p++ = '0';
			}
			*p++ = '0' + exp;
		}
		return (int)(p - p0);
	}
};

//========================================================================//
// snprintf / vsnprintf imitations

#ifdef __GNUC__
#define GCCPRINTF(stri,firstargi)	__attribute__((format(printf,stri,firstargi)))
#define GCCFORMAT(stri)				__attribute__((format(printf,stri,0)))
#define GCCNOWARN					__attribute__((unused))
#else
#define GCCPRINTF(a,b)
#define GCCFORMAT(a)
#define GCCNOWARN
#endif

struct snprintf_state
{
	char *buffer;
	size_t maxlen;
	size_t curlen;
	int ideallen;
};

static int myvsnprintf_helper(void *data, const char *cstr, int cstr_len)
{
	snprintf_state *state = (snprintf_state *)data;

	if (INT_MAX - cstr_len < state->ideallen)
	{
		state->ideallen = INT_MAX;
	}
	else
	{
		state->ideallen += cstr_len;
	}
	if (state->curlen + cstr_len > state->maxlen)
	{
		cstr_len = (int)(state->maxlen - state->curlen);
	}
	if (cstr_len > 0)
	{
		memcpy(state->buffer + state->curlen, cstr, cstr_len);
		state->curlen += cstr_len;
	}
	return cstr_len;
}

extern "C"
{

// Unlike the MS CRT function snprintf, this one always writes a terminating
// null character to the buffer. It also returns the full length of the string
// that would have been output if the buffer had been large enough. In other
// words, it follows BSD/Linux rules and not MS rules.
int myvsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
	size_t originalcount = count;
	if (count != 0)
	{
		count--;
	}
	if (count > INT_MAX)
	{ // This is probably an error. Output nothing.
		originalcount = 0;
		count = 0;
	}
	snprintf_state state = { buffer, count, 0, 0 };
	StringFormat::VWorker(myvsnprintf_helper, &state, format, argptr);
	if (originalcount > 0)
	{
		buffer[state.curlen] = '\0';
	}
	return state.ideallen;
}

int mysnprintf(char *buffer, size_t count, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	int len = myvsnprintf(buffer, count, format, argptr);
	va_end(argptr);
	return len;
}

}
