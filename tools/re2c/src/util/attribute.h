#ifndef _RE2C_UTIL_ATTRIBUTE_
#define _RE2C_UTIL_ATTRIBUTE_

#ifdef __GNUC__
#    define RE2C_GXX_ATTRIBUTE(x) __attribute__(x)
#else
#    define RE2C_GXX_ATTRIBUTE(x)
#endif

#endif // _RE2C_UTIL_ATTRIBUTE_
