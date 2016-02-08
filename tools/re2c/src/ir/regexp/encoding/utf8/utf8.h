#ifndef _RE2C_IR_REGEXP_ENCODING_UTF8_UTF8_
#define _RE2C_IR_REGEXP_ENCODING_UTF8_UTF8_

#include "src/util/c99_stdint.h"

namespace re2c {

class utf8
{
public:
	typedef uint32_t rune;

	// maximum characters per rune
	// enum instead of static const member because of [-Wvla]
	enum { MAX_RUNE_LENGTH = 4u };

	// decoding error
	static const uint32_t ERROR;

	// maximal runes for each rune length
	static const rune MAX_1BYTE_RUNE;
	static const rune MAX_2BYTE_RUNE;
	static const rune MAX_3BYTE_RUNE;
	static const rune MAX_4BYTE_RUNE;
	static const rune MAX_RUNE;

	static const uint32_t PREFIX_1BYTE;
	static const uint32_t INFIX;
	static const uint32_t PREFIX_2BYTE;
	static const uint32_t PREFIX_3BYTE;
	static const uint32_t PREFIX_4BYTE;

	static const uint32_t SHIFT;
	static const uint32_t MASK;

	// UTF-8 bytestring for given Unicode rune
	static uint32_t rune_to_bytes(uint32_t * s, rune r);

	// length of UTF-8 bytestring for given Unicode rune
	static uint32_t rune_length(rune r);

	// maximal Unicode rune with given length of UTF-8 bytestring
	static rune max_rune(uint32_t i);
};

}  // namespace re2c

#endif // _RE2C_IR_REGEXP_ENCODING_UTF8_UTF8_
