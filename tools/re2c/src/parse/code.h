#ifndef _RE2C_PARSE_CODE_
#define _RE2C_PARSE_CODE_

#include "src/util/c99_stdint.h"
#include <stddef.h>
#include <string>

#include "src/parse/loc.h"
#include "src/util/free_list.h"

namespace re2c
{

struct Code
{
	static free_list<const Code *> freelist;

	const Loc loc;
	const std::string text;

	inline Code (const char * t, size_t t_len, const std::string & f, uint32_t l)
		: loc (f, l)
		, text (t, t_len)
	{
		freelist.insert (this);
	}
};

} // namespace re2c

#endif // _RE2C_PARSE_CODE_
