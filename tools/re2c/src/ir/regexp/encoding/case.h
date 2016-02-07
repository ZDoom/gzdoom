#ifndef _RE2C_IR_REGEXP_ENCODING_CASE_
#define _RE2C_IR_REGEXP_ENCODING_CASE_

#include "src/util/c99_stdint.h"

namespace re2c {

// TODO: support non-ASCII encodings
bool is_alpha (uint32_t c);
uint32_t to_lower_unsafe (uint32_t c);
uint32_t to_upper_unsafe (uint32_t c);

inline bool is_alpha (uint32_t c)
{
	return (c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z');
}

inline uint32_t to_lower_unsafe (uint32_t c)
{
	return c | 0x20u;
}

inline uint32_t to_upper_unsafe (uint32_t c)
{
	return c & ~0x20u;
}

}

#endif // _RE2C_IR_REGEXP_ENCODING_CASE_
