#ifndef _RE2C_IR_REGEXP_ENCODING_UTF8_RANGE_
#define _RE2C_IR_REGEXP_ENCODING_UTF8_RANGE_

#include "src/util/c99_stdint.h"

#include "src/ir/regexp/encoding/utf8/utf8.h"

namespace re2c {

struct RangeSuffix;

void UTF8addContinuous(RangeSuffix * & p, utf8::rune l, utf8::rune h, uint32_t n);
void UTF8splitByContinuity(RangeSuffix * & p, utf8::rune l, utf8::rune h, uint32_t n);
void UTF8splitByRuneLength(RangeSuffix * & p, utf8::rune l, utf8::rune h);

} // namespace re2c

#endif // _RE2C_IR_REGEXP_ENCODING_UTF8_RANGE_
