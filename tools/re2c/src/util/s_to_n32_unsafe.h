#ifndef _RE2C_UTIL_S_TO_N32_UNSAFE_
#define _RE2C_UTIL_S_TO_N32_UNSAFE_

#include "src/util/attribute.h"
#include "src/util/c99_stdint.h"

bool s_to_u32_unsafe (const char * s, const char * s_end, uint32_t & number) RE2C_GXX_ATTRIBUTE ((warn_unused_result));
bool s_to_i32_unsafe (const char * s, const char * s_end, int32_t & number) RE2C_GXX_ATTRIBUTE ((warn_unused_result));

#endif // _RE2C_UTIL_S_TO_N32_UNSAFE_
