#ifndef _RE2C_TEST_RANGE_TEST_
#define _RE2C_TEST_RANGE_TEST_

#include "src/util/c99_stdint.h"

namespace re2c { class Range; }

namespace re2c_test {

/*
 * If encoding has N code units (characters), character class can be
 * represented as an N-bit integer: k-th bit is set iff k-th character
 * belongs to the class.
 *
 * Addition and subtraction can be implemented trivially for such
 * integer representation of character classes: addition is simply
 * bitwise OR of two classes, subtraction is bitwise AND of the first
 * class and negated second class.
 */
template <uint8_t BITS> re2c::Range * range (uint32_t n);
template <uint8_t BITS> re2c::Range * add (uint32_t n1, uint32_t n2);
template <uint8_t BITS> re2c::Range * sub (uint32_t n1, uint32_t n2);

} // namespace re2c_test

#endif // _RE2C_TEST_RANGE_TEST_
