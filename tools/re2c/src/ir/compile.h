#ifndef _RE2C_IR_COMPILE_
#define _RE2C_IR_COMPILE_

#include "src/util/c99_stdint.h"
#include <string>

#include "src/util/smart_ptr.h"

namespace re2c
{

class DFA;
struct Output;
struct Spec;

smart_ptr<DFA> compile (Spec & spec, Output & output, const std::string & cond, uint32_t cunits);

} // namespace re2c

#endif // _RE2C_IR_COMPILE_
