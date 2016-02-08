#ifndef _RE2C_IR_REGEXP_ENCODING_UTF16_REGEXP_
#define _RE2C_IR_REGEXP_ENCODING_UTF16_REGEXP_

#include "src/ir/regexp/encoding/utf16/utf16.h"

namespace re2c {

class Range;
class RegExp;

RegExp * UTF16Symbol(utf16::rune r);
RegExp * UTF16Range(const Range * r);

} // namespace re2c

#endif // _RE2C_IR_REGEXP_ENCODING_UTF16_REGEXP_
