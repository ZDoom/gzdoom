#ifndef _RE2C_IR_REGEXP_ENCODING_UTF16_RANGE_
#define _RE2C_IR_REGEXP_ENCODING_UTF16_RANGE_

#include "src/util/c99_stdint.h"

#include "src/ir/regexp/encoding/utf16/utf16.h"

namespace re2c {

struct RangeSuffix;

void UTF16addContinuous1(RangeSuffix * & root, uint32_t l, uint32_t h);
void UTF16addContinuous2(RangeSuffix * & root, uint32_t l_ld, uint32_t h_ld, uint32_t l_tr, uint32_t h_tr);
void UTF16splitByContinuity(RangeSuffix * & root, uint32_t l_ld, uint32_t h_ld, uint32_t l_tr, uint32_t h_tr);
void UTF16splitByRuneLength(RangeSuffix * & root, utf16::rune l, utf16::rune h);

} // namespace re2c

#endif // _RE2C_IR_REGEXP_ENCODING_UTF16_RANGE_
