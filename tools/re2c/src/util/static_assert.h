#ifndef _RE2C_UTIL_STATIC_ASSERT_
#define _RE2C_UTIL_STATIC_ASSERT_

namespace re2c {

template<bool> struct static_assert_t;
template<> struct static_assert_t<true> {};

} // namespace re2c

#define RE2C_STATIC_ASSERT(e) \
    { re2c::static_assert_t<e> _; (void) _; }

#endif // _RE2C_UTIL_STATIC_ASSERT_
