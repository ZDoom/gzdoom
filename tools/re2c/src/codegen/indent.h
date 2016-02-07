#ifndef _RE2C_CODEGEN_INDENT_
#define _RE2C_CODEGEN_INDENT_

#include <string>

#include "src/globals.h"

namespace re2c
{

inline std::string indent (uint32_t ind)
{
	std::string str;

	while (opts->target != opt_t::DOT && ind-- > 0)
	{
		str += opts->indString;
	}
	return str;
}

} // end namespace re2c

#endif // _RE2C_CODEGEN_INDENT_
