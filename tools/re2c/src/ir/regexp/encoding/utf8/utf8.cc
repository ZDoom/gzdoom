#include "src/ir/regexp/encoding/utf8/utf8.h"

namespace re2c {

const uint32_t utf8::ERROR = 0xFFFDu;

const utf8::rune utf8::MAX_1BYTE_RUNE = 0x7Fu;
const utf8::rune utf8::MAX_2BYTE_RUNE = 0x7FFu;
const utf8::rune utf8::MAX_3BYTE_RUNE = 0xFFFFu;
const utf8::rune utf8::MAX_4BYTE_RUNE = 0x10FFFFu;
const utf8::rune utf8::MAX_RUNE       = utf8::MAX_4BYTE_RUNE;

const uint32_t utf8::PREFIX_1BYTE = 0u;    // 0000 0000
const uint32_t utf8::INFIX        = 0x80u; // 1000 0000
const uint32_t utf8::PREFIX_2BYTE = 0xC0u; // 1100 0000
const uint32_t utf8::PREFIX_3BYTE = 0xE0u; // 1110 0000
const uint32_t utf8::PREFIX_4BYTE = 0xF0u; // 1111 0000

const uint32_t utf8::SHIFT = 6u;
const uint32_t utf8::MASK = 0x3Fu; // 0011 1111

uint32_t utf8::rune_to_bytes(uint32_t *str, rune c)
{
	// one byte sequence: 0-0x7F => 0xxxxxxx
	if (c <= MAX_1BYTE_RUNE)
	{
		str[0] = PREFIX_1BYTE | c;
		return 1;
	}

	// two byte sequence: 0x80-0x7FF => 110xxxxx 10xxxxxx
	if (c <= MAX_2BYTE_RUNE)
	{
		str[0] = PREFIX_2BYTE | (c >> 1*SHIFT);
		str[1] = INFIX        | (c & MASK);
		return 2;
	}

	// If the Rune is out of range, convert it to the error rune.
	// Do this test here because the error rune encodes to three bytes.
	// Doing it earlier would duplicate work, since an out of range
	// Rune wouldn't have fit in one or two bytes.
	if (c > MAX_RUNE)
		c = ERROR;

	// three byte sequence: 0x800 - 0xFFFF => 1110xxxx 10xxxxxx 10xxxxxx
	if (c <= MAX_3BYTE_RUNE)
	{
		str[0] = PREFIX_3BYTE | (c >> 2*SHIFT);
		str[1] = INFIX        | ((c >> 1*SHIFT) & MASK);
		str[2] = INFIX        | (c & MASK);
		return 3;
	}

	// four byte sequence (21-bit value):
	// 0x10000 - 0x1FFFFF => 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
	str[0] = PREFIX_4BYTE | (c >> 3*SHIFT);
	str[1] = INFIX        | ((c >> 2*SHIFT) & MASK);
	str[2] = INFIX        | ((c >> 1*SHIFT) & MASK);
	str[3] = INFIX        | (c & MASK);
	return 4;
}

uint32_t utf8::rune_length(rune r)
{
	if (r <= MAX_2BYTE_RUNE)
		return r <= MAX_1BYTE_RUNE ? 1 : 2;
	else
		return r <= MAX_3BYTE_RUNE ? 3 : 4;
}

utf8::rune utf8::max_rune(uint32_t i)
{
	switch (i)
	{
		case 1:  return MAX_1BYTE_RUNE;
		case 2:  return MAX_2BYTE_RUNE;
		case 3:  return MAX_3BYTE_RUNE;
		case 4:  return MAX_4BYTE_RUNE;
		default: return ERROR;
	}
}

} // namespace re2c
