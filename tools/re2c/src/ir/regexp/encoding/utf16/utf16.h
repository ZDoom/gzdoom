#ifndef _RE2C_IR_REGEXP_ENCODING_UTF16_UTF16_
#define _RE2C_IR_REGEXP_ENCODING_UTF16_UTF16_

#include "src/util/c99_stdint.h"

namespace re2c {

class utf16
{
public:
	typedef uint32_t rune;

	static const uint32_t MAX_1WORD_RUNE;
	static const uint32_t MIN_LEAD_SURR;
	static const uint32_t MIN_TRAIL_SURR;
	static const uint32_t MAX_TRAIL_SURR;

	/* leading surrogate of UTF-16 symbol */
	static inline uint32_t lead_surr(rune r);

	/* trailing surrogate of UTF-16 symbol */
	static inline uint32_t trail_surr(rune r);
};

inline uint32_t utf16::lead_surr(rune r)
{
	return ((r - 0x10000u) / 0x400u) + MIN_LEAD_SURR;
}

inline uint32_t utf16::trail_surr(rune r)
{
	return ((r - 0x10000u) % 0x400u) + MIN_TRAIL_SURR;
}

}  // namespace re2c

#endif // _RE2C_IR_REGEXP_ENCODING_UTF16_UTF16_
