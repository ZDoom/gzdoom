#ifndef _RE2C_CODEGEN_PRINT_
#define _RE2C_CODEGEN_PRINT_

#include "src/util/c99_stdint.h"
#include <iosfwd>

namespace re2c
{

bool is_print (uint32_t c);
bool is_space (uint32_t c);
char hexCh(uint32_t c);
void prtCh(std::ostream&, uint32_t);
void prtHex(std::ostream&, uint32_t);
void prtChOrHex(std::ostream&, uint32_t);
void printSpan(std::ostream&, uint32_t, uint32_t);

} // end namespace re2c

#endif // _RE2C_CODEGEN_PRINT_
