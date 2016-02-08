#ifndef _RE2C_CONF_MSG_
#define _RE2C_CONF_MSG_

#include <string>

#include "src/util/attribute.h"
#include "src/util/c99_stdint.h"

namespace re2c {

void error (const char * fmt, ...) RE2C_GXX_ATTRIBUTE ((format (printf, 1, 2)));
void error_encoding ();
void error_arg (const char * option);
void warning_start (uint32_t line, bool error);
void warning_end (const char * type, bool error);
void warning (const char * type, uint32_t line, bool error, const char * fmt, ...) RE2C_GXX_ATTRIBUTE ((format (printf, 4, 5)));
void usage ();
void vernum ();
void version ();
std::string incond (const std::string & cond);

} // namespace re2c

#endif // _RE2C_CONF_MSG_
