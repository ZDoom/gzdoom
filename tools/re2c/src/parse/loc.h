#ifndef _RE2C_PARSE_LOC_
#define _RE2C_PARSE_LOC_

#include <string>

#include "src/util/c99_stdint.h"

namespace re2c
{

struct Loc
{
	std::string filename;
	uint32_t line;

	inline Loc (const std::string & f, uint32_t l)
		: filename (f)
		, line (l)
	{}
};

} // namespace re2c

#endif // _RE2C_PARSE_LOC_
