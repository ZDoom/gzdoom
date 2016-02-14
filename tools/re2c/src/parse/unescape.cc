#include "src/parse/unescape.h"

namespace re2c {

// expected characters: [0-9a-zA-Z]
static inline uint32_t hex_digit (const char c)
{
	switch (c)
	{
		case '0': return 0;
		case '1': return 1;
		case '2': return 2;
		case '3': return 3;
		case '4': return 4;
		case '5': return 5;
		case '6': return 6;
		case '7': return 7;
		case '8': return 8;
		case '9': return 9;
		case 'a':
		case 'A': return 0xA;
		case 'b':
		case 'B': return 0xB;
		case 'c':
		case 'C': return 0xC;
		case 'd':
		case 'D': return 0xD;
		case 'e':
		case 'E': return 0xE;
		case 'f':
		case 'F': return 0xF;
		default:  return ~0u; // unexpected
	}
}

// expected string format: "\" [xXuU] [0-9a-zA-Z]*
uint32_t unesc_hex (const char * s, const char * s_end)
{
	uint32_t n = 0;
	for (s += 2; s != s_end; ++s)
	{
		n <<= 4;
		n += hex_digit (*s);
	}
	return n;
}

// expected string format: "\" [0-7]*
uint32_t unesc_oct (const char * s, const char * s_end)
{
	uint32_t n = 0;
	for (++s; s != s_end; ++s)
	{
		n <<= 3;
		n += static_cast<uint8_t> (*s - '0');
	}
	return n;
}

} // namespace re2c
