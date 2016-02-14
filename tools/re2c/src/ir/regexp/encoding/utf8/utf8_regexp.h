#ifndef _RE2C_IR_REGEXP_ENCODING_UTF8_REGEXP_
#define _RE2C_IR_REGEXP_ENCODING_UTF8_REGEXP_

#include "src/ir/regexp/encoding/utf8/utf8.h"

namespace re2c {

class Range;
class RegExp;

RegExp * UTF8Symbol(utf8::rune r);
RegExp * UTF8Range(const Range * r);

} // namespace re2c

#endif // _RE2C_IR_REGEXP_ENCODING_UTF8_REGEXP_
