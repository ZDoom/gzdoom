#include <limits.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>

#include "zstring.h"

#ifndef _MSC_VER
#include <stdint.h>
#else
typedef unsigned __int64 uint64_t;
typedef signed __int64 int64_t;
#endif

// Even though the standard C library has a function to do printf-style formatting in a
// generic way, there is no standard interface to this function. So if you want to do
// some printf formatting that doesn't fit in the context of the provided functions,
// you need to roll your own. Why is that?
//
// Maybe Microsoft wants you to write a better one yourself? When used as part of a
// sprintf replacement, this function is significantly faster than Microsoft's
// offering. When used as part of a fprintf replacement, this function turns out to
// be slower, but that's probably because the CRT's fprintf can interact with the
// FILE object on a low level for better perfomance. If you sprintf into a buffer
// and then fwrite that buffer, this routine wins again, though the difference isn't
// great.

namespace StringFormat
{
	int Worker (OutputFunc output, void *outputData, const char *fmt, ...)
	{
		va_list arglist;
		int len;

		va_start (arglist, fmt);
		len = VWorker (output, outputData, fmt, arglist);
		va_end (arglist);
		return len;
	}

	static inline int writepad (OutputFunc output, void *outputData, const char *pad, int padsize, int spaceToFill)
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

	// Gasp! This is supposed to be a replacement for sprintf formatting, but
	// I used sprintf for doubles anyway! Oh no!
	static int fmt_fp (OutputFunc output, void *outputData, int flags, int precision, int width, double number, char type)
	{
		char *buff;
		char format[16];
		int i;

		format[0]   = '%';
		i = 1;
		if (flags & F_MINUS) format[i++] = '-';
		if (flags & F_PLUS)  format[i++] = '+';
		if (flags & F_ZERO)  format[i++] = '0';
		if (flags & F_BLANK) format[i++] = ' ';
		if (flags & F_HASH)  format[i++] = '#';
		format[i++] = '*';
		format[i++] = '.';
		format[i++] = '*';
		format[i++] = type;
		format[i++] = '\0';
		buff = (char *)alloca (1000 + precision);
		i = sprintf (buff, format, width, precision, number);
		return output (outputData, buff, i);
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

			base = c+1;

			// Now that that's all out of the way, we should be pointing at the type specifier
	{
		static const char hexits[18] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f','0','x'};
		static const char HEXits[18] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','0','X'};
		static const char spaces[16] = {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
		static const char zeroes[17] = {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','.'};
		static const char plusprefix = '+';
		static const char minusprefix = '-';
		static const char dotchar = '.';
		const char *prefix = NULL;
		int prefixlen = 0;
		int postprefixzeros = 0;
		int size = flags & 0xF000;
		char buffer[32], *ibuff;
		const char *obuff = 0;
		char type = *c++;
		int bufflen = 0;
		int outlen = 0;
		unsigned int intarg = 0;
		uint64_t int64arg = 0;
		const void *voidparg;
		const char *charparg;
		const char *xits = hexits;
		int inlen = len;

		// Using a bunch of if/else if statements is faster than a switch, because a switch generates
		// a jump table. A jump table means a possible data cache miss and a hefty penalty while the
		// cache line is loaded.

		if (type == 'x' || type == 'X' ||
			type == 'p' ||
			type == 'd' || type == 'u' || type == 'i' ||
			type == 'o')
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
				if (size == F_HALFHALF)
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
				while (intarg != 0)
				{
					*--ibuff = char(intarg % 10) + '0'; intarg /= 10;
				}
			}
			else if (type == 'o')
			{ // Octal
				while (intarg != 0)
				{
					*--ibuff = char(intarg & 7) + '0'; intarg >>= 3;
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
						prefix = zeroes;
						prefixlen = 1;
					}
				}
				else if (type == 'x' || type == 'X')
				{
					prefix = xits + 16;
					prefixlen = 2;
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
			buffer[0] = char(intarg);
			bufflen = 1;
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
		else if (type == 'e' || type == 'E' ||
				 type == 'f' || type == 'F' ||
				 type == 'g' || type == 'G')
		{
			// IEEE 754 floating point numbers
#ifdef _MSC_VER
#define FP_SIGN_MASK		(1ui64<<63)
#define FP_EXPONENT_MASK	(2047ui64<<52)
#define FP_FRACTION_MASK	((1ui64<<52)-1)
#else
#define FP_SIGN_MASK		(1llu<<63)
#define FP_EXPONENT_MASK	(2047llu<<52)
#define FP_FRACTION_MASK	((1llu<<52)-1)
#endif
			union
			{
				double d;
				uint64_t i64;
			} number;
			number.d = va_arg (arglist, double);

			if (precision < 0) precision = 6;

			flags |= F_SIGNED;

			if ((number.i64 & FP_EXPONENT_MASK) == FP_EXPONENT_MASK)
			{
				if (number.i64 & FP_SIGN_MASK)
				{
					flags |= F_NEGATIVE;
				}
				if ((number.i64 & FP_FRACTION_MASK) == 0)
				{
					obuff = "Infinity";
					bufflen = 8;
				}
				else if ((number.i64 & ((FP_FRACTION_MASK+1)>>1)) == 0)
				{
					obuff = "NaN";
					bufflen = 3;
				}
				else
				{
					obuff = "Ind";
				}
			}
			else
			{
				// Converting a binary floating point number to an ASCII decimal
				// representation is non-trivial, so I'm not going to do it myself.
				// (At least for now.)
				len += fmt_fp (output, outputData, flags, precision, width, number.d, type);
				continue;
			}
		}

		// Check for sign prefix (only for signed numbers)
		if (flags & F_SIGNED)
		{
			if (flags & F_NEGATIVE)
			{
				prefix = &minusprefix;
				prefixlen = 1;
			}
			else if (flags & F_PLUS)
			{
				prefix = &plusprefix;
				prefixlen = 1;
			}
			else if (flags & F_BLANK)
			{
				prefix = spaces;
				prefixlen = 1;
			}
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
		if (bufflen > 0)
		{
			outlen += output (outputData, obuff, bufflen);
		}

        if ((flags & F_MINUS) && fieldlen < width)
		{ // Field is left-justified, so padding comes last
			outlen += writepad (output, outputData, pad, sizeof(spaces), width - fieldlen);
		}
		len += outlen;
	}
	}
	}
};
