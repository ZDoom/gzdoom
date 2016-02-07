#ifndef _RE2C_PARSE_UNESCAPE_
#define _RE2C_PARSE_UNESCAPE_

#include "src/util/c99_stdint.h"

namespace re2c {

uint32_t unesc_hex (const char * s, const char * s_end);
uint32_t unesc_oct (const char * s, const char * s_end);

} // namespace re2c

#endif // _RE2C_PARSE_UNESCAPE_
